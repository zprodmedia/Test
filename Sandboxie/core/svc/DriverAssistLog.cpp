/*
 * Copyright 2004-2020 Sandboxie Holdings, LLC 
 * Copyright 2020 David Xanatos, xanasoft.com
 *
 * This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <lmcons.h>

//---------------------------------------------------------------------------
// Driver Assistant, log messages
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// Structures and Types
//---------------------------------------------------------------------------


//typedef struct WORK_ITEM {
//    ULONG type;
//    ULONG data[1];
//} WORK_ITEM;


//---------------------------------------------------------------------------
// GetUserNameFromProcess
//---------------------------------------------------------------------------


bool GetUserNameFromProcess(DWORD pid, WCHAR* user, DWORD userSize, WCHAR* domain, DWORD domainSize)
{
    bool bRet = false;
    HANDLE hToken = (HANDLE)SbieApi_QueryProcessInfo((HANDLE)pid, 'ptok');
    if(hToken != NULL)
    {
        BYTE data[64]; // needed 44 = sizeof(TOKEN_USER) + sizeof(SID_AND_ATTRIBUTES) + sizeof(SID)
        DWORD tokenSize = sizeof(data);
        if(GetTokenInformation(hToken, TokenUser, data, tokenSize, &tokenSize))
        {
            TOKEN_USER* pUser = (TOKEN_USER*)data;
            PSID pSID = pUser->User.Sid;
            SID_NAME_USE sidName;
            if (LookupAccountSid(NULL, pSID, user, &userSize, domain, &domainSize, &sidName)) {
                user[userSize] = L'\0';
                domain[domainSize] = L'\0';
                bRet = true;
            }
        }
        CloseHandle(hToken);
    }
    return bRet;
}


//---------------------------------------------------------------------------
// LogMessage
//---------------------------------------------------------------------------


void DriverAssist::LogMessage(void *_msg)
{
    ULONG data = _msg ? *(ULONG*)_msg : 0;

    bool LogMessageEvents = (data & 0x01) != 0;

    EnterCriticalSection(&m_LogMessage_CritSec);

    ULONG m_MessageLen = 4096;
    void *m_MessageBuf = NULL;

    while (1) {

        m_MessageBuf = HeapAlloc(GetProcessHeap(), 0, m_MessageLen);
        if (! m_MessageBuf)
            break;

        ULONG len = m_MessageLen;
		ULONG message_number = m_last_message_number;
		ULONG code = -1;
		ULONG pid = 0;
		ULONG status = SbieApi_GetMessage(&message_number, -1, &code, &pid, (wchar_t*)m_MessageBuf, len);

        if (status == STATUS_BUFFER_TOO_SMALL) {
            HeapFree(GetProcessHeap(), 0, m_MessageBuf);
            m_MessageBuf = NULL;
            m_MessageLen += 4096;
            continue;
        }

        if (status != 0)
            break; // error or no more entries
		m_last_message_number = message_number;

        //
        // Skip hacky messages
        //

        if (code == MSG_2199) // Auto Recovery notification
            continue;
	    if (code == MSG_2198) // File Migration progress notifications
		    continue;
	    if (code == MSG_1399) // Process Start notification
		    continue;

        //
        // Add to event log
        //

        if (LogMessageEvents)
            LogMessage_Event(code, (wchar_t*)m_MessageBuf, pid);

        //
        // Add to log file
        //

		LogMessage_Single(code, (wchar_t*)m_MessageBuf, pid);
    }

    if (m_MessageBuf)
        HeapFree(GetProcessHeap(), 0, m_MessageBuf);

    LeaveCriticalSection(&m_LogMessage_CritSec);
}


//---------------------------------------------------------------------------
// LogMessage_Single
//---------------------------------------------------------------------------


void DriverAssist::LogMessage_Single(ULONG code, wchar_t* data, ULONG pid)
{
    //
    // check if logging is enabled
    //

    union {
        KEY_VALUE_PARTIAL_INFORMATION info;
        WCHAR space[MAX_PATH + 8];
    } u;

    if (! SbieDll_GetServiceRegistryValue(L"LogFile", &u.info, sizeof(u)))
        return;
    if (u.info.Type != REG_SZ || u.info.DataLength >= sizeof(u))
        return;

    WCHAR *path = (WCHAR *)u.info.Data;
    int LogVer = *path - L'0';
    if (LogVer < 0 || LogVer > 9 )
        return;
    ++path;
    if (*path != L';')
        return;
    ++path;

    //
    // get log message
    //

    WCHAR *str1 = data;
    ULONG str1_len = wcslen(str1);
    WCHAR *str2 = str1 + str1_len + 1;
    ULONG str2_len = wcslen(str2);

    WCHAR *text = SbieDll_FormatMessage2(code, str1, str2);
    if (! text)
        return;

    //
    // log version 2, add timestamp
    //

    if (LogVer >= 2) {

        WCHAR *text2 = (WCHAR *)LocalAlloc(
            LMEM_FIXED, (wcslen(text) + 64) * sizeof(WCHAR));
        if (! text2) {
            LocalFree(text);
            return;
        }

        SYSTEMTIME st;
        GetLocalTime(&st);

        wsprintf(text2, L"%04d-%02d-%02d %02d:%02d:%02d %s",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond,
            text);

        LocalFree(text);
        text = text2;
    }

    //
    // log version 3, add user name
    //

    if (LogVer >= 3) {

        WCHAR user[UNLEN + 1];
        WCHAR domain[DNLEN + 1];
        if (GetUserNameFromProcess(pid, user, UNLEN + 1, domain, DNLEN + 1)) {

            WCHAR *text2 = (WCHAR *)LocalAlloc(
                LMEM_FIXED, (wcslen(text) + UNLEN + DNLEN + 10) * sizeof(WCHAR));
            if (text2) {

                wsprintf(text2, L"%s (%s\\%s)", text, domain, user);

                LocalFree(text);
                text = text2;
            }
        }
    }

    //
    // write message to main log file and secondary log files
    //

    LogMessage_Write(path, text);

    LogMessage_Multi(code, path, text);

    LocalFree(text);
}


//---------------------------------------------------------------------------
// LogMessage_Multi
//---------------------------------------------------------------------------


void DriverAssist::LogMessage_Multi(
    ULONG msgid, const WCHAR *path, const WCHAR *text)
{
    union {
        KEY_VALUE_PARTIAL_INFORMATION info;
        WCHAR space[256];
    } u;

    if (! SbieDll_GetServiceRegistryValue(L"MultiLog", &u.info, sizeof(u)))
        return;
    if (u.info.Type != REG_SZ || u.info.DataLength >= sizeof(u))
        return;

    // go through a ',' or ';' separated list of message ID's, return message id is not listed
    WCHAR *ptr = (WCHAR *)u.info.Data;
    while (*ptr) {
        if (_wtoi(ptr) == (msgid & 0xFFFF))
            break;
        while (*ptr && *ptr != L',' && *ptr != L';')
            ++ptr;
        if (! (*ptr))
            return;
        ++ptr;
    }

    // get box name
    WCHAR *ptr2 = (WCHAR*)wcsrchr(text, L']');
    if (! ptr2)
        return;
    ptr = ptr2;
    while (ptr > text && *ptr != L'[')
        --ptr;
    if ((ptr == text) || (ptr2 - ptr <= 1) || (ptr2 - ptr > BOXNAME_COUNT))
        return;
    WCHAR boxname[BOXNAME_COUNT];
    wmemcpy(boxname, ptr + 1, ptr2 - ptr - 1);
    boxname[ptr2 - ptr - 1] = L'\0';

    LONG rc = SbieApi_IsBoxEnabled(boxname);
    if (rc != STATUS_SUCCESS && rc != STATUS_ACCOUNT_RESTRICTION)
        return;

    // append _boxname to log file name
    ptr = wcsrchr((WCHAR*)path, L'.');
    if (! ptr)
        return;
    ULONG len = wcslen(path) + 128;
    WCHAR *path2 = (WCHAR *)HeapAlloc(
                                GetProcessHeap(), 0, len * sizeof(WCHAR));
    if (! path2)
        return;
    wmemcpy(path2, path, ptr - path);
    path2[ptr - path] = L'_';
    wcscpy(&path2[ptr - path + 1], boxname);
    wcscat(path2, ptr);

    LogMessage_Write(path2, text);

    HeapFree(GetProcessHeap(), 0, path2);
}


//---------------------------------------------------------------------------
// LogMessage_Write
//---------------------------------------------------------------------------


void DriverAssist::LogMessage_Write(const WCHAR *path, const WCHAR *text)
{
    HANDLE hFile = CreateFile(
        path, FILE_GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
        return;

    SetFilePointer(hFile, 0, NULL, FILE_END);

    ULONG bytes;
    static const WCHAR *crlf = L"\r\n";
    WriteFile(hFile, text, wcslen(text) * sizeof(WCHAR), &bytes, NULL);
    WriteFile(hFile, crlf, wcslen(crlf) * sizeof(WCHAR), &bytes, NULL);

    CloseHandle(hFile);
}
