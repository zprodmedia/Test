/*
 * Copyright 2004-2020 Sandboxie Holdings, LLC
 * Copyright 2020-2022 David Xanatos, xanasoft.com
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


 //---------------------------------------------------------------------------
 // File (Copy)
 //---------------------------------------------------------------------------

#include "common\pattern.h"

 //---------------------------------------------------------------------------
 // Functions
 //---------------------------------------------------------------------------

static void File_InitCopyLimit(void);

static BOOLEAN File_InitFileMigration(void);

//---------------------------------------------------------------------------
// Variables
//---------------------------------------------------------------------------

extern POOL* Dll_Pool;
extern POOL* Dll_PoolTemp;

typedef enum { // Note: thisorder defines the config priority
    FILE_DONT_COPY,
    FILE_COPY_CONTENT,
    FILE_COPY_EMPTY,
    NUM_COPY_MODES
} ENUM_COPY_MODES;

static LIST File_MigrationOptions[NUM_COPY_MODES];

static BOOLEAN File_MigrationDenyWrite = FALSE;

static ULONGLONG File_CopyLimitKb = (80 * 1024);        // 80 MB
static BOOLEAN File_CopyLimitSilent = FALSE;
static BOOLEAN File_NotifyNoCopy = FALSE;

//---------------------------------------------------------------------------
// File_InitFileMigration
//---------------------------------------------------------------------------


_FX BOOLEAN File_InitFileMigration(void)
{
    for(ULONG i=0; i < NUM_COPY_MODES; i++)
        List_Init(&File_MigrationOptions[i]);

    Config_InitPatternList(NULL, L"CopyEmpty", &File_MigrationOptions[FILE_COPY_EMPTY], FALSE);
    Config_InitPatternList(NULL, L"CopyAlways", &File_MigrationOptions[FILE_COPY_CONTENT], FALSE);
    Config_InitPatternList(NULL, L"DontCopy", &File_MigrationOptions[FILE_DONT_COPY], FALSE);

    File_MigrationDenyWrite = Config_GetSettingsForImageName_bool(L"CopyBlockDenyWrite", FALSE);

    File_InitCopyLimit();

    File_NotifyNoCopy = SbieApi_QueryConfBool(NULL, L"NotifyNoCopy", FALSE);

    return TRUE;
}


//---------------------------------------------------------------------------
// File_MigrateFile_Message
//---------------------------------------------------------------------------


_FX VOID File_MigrateFile_Message(const WCHAR* TruePath, ULONGLONG file_size, int MsgID)
{
    const WCHAR* name = wcsrchr(TruePath, L'\\');
    if (name)
        ++name;
    else
        name = TruePath;

    ULONG TruePathNameLen = wcslen(name);
    WCHAR* text = Dll_AllocTemp(
        (TruePathNameLen + 64) * sizeof(WCHAR));
    Sbie_snwprintf(text, (TruePathNameLen + 64), L"%s [%s / %I64u]",
        name, Dll_BoxName, file_size);

    SbieApi_Log(MsgID, text);

    Dll_Free(text);
}


//---------------------------------------------------------------------------
// File_MigrateFile_GetMode
//---------------------------------------------------------------------------


_FX ULONG File_MigrateFile_GetMode(const WCHAR* TruePath, ULONGLONG file_size)
{
    ULONG mode = NUM_COPY_MODES;

    ULONG path_len = (wcslen(TruePath) + 1) * sizeof(WCHAR);
    WCHAR* path_lwr = Dll_AllocTemp(path_len);
    if (!path_lwr) {
        SbieApi_Log(2305, NULL);
        return FILE_DONT_COPY;
    }
    memcpy(path_lwr, TruePath, path_len);
    _wcslwr(path_lwr);
    path_len = wcslen(path_lwr);

    //
    // Check what preset applies to this file type/path
    //

    for (ULONG i = 0; i < NUM_COPY_MODES; i++)
    {
        PATTERN* pat = List_Head(&File_MigrationOptions[i]);
        while (pat) 
        {
            if (Pattern_Match(pat, path_lwr, path_len))
            {
                mode = i;
                goto found_match;
            }
            pat = List_Next(pat);
        }
    }

found_match:

    Dll_Free(path_lwr);

    if (mode != NUM_COPY_MODES) {

        if (File_NotifyNoCopy) {
            if (mode == FILE_DONT_COPY) {
                if(File_MigrationDenyWrite)
                    File_MigrateFile_Message(TruePath, file_size, 2114);
                else // else open read only
                    File_MigrateFile_Message(TruePath, file_size, 2115);
            }
            else if (mode == FILE_COPY_EMPTY) 
                File_MigrateFile_Message(TruePath, file_size, 2113);
        }

        return mode;
    }

    //
    // if tere is no configuration for this file type/path decide based on the file size
    //
    
    if (File_CopyLimitKb == -1 || file_size < ((ULONGLONG)File_CopyLimitKb * 1024))
        return FILE_COPY_CONTENT;

    //
    // ask the user to decide if the large file should be coped into the sandbox
    //
    
    MAN_FILE_MIGRATION_REQ req;
    MAN_FILE_MIGRATION_RPL* rpl = NULL;
    BOOLEAN ok = FALSE;

    if (SbieApi_QueryConfBool(NULL, L"PromptForFileMigration", TRUE))
    {
        req.msgid = MAN_FILE_MIGRATION;
        req.file_size = file_size;
        wcscpy(req.file_path, TruePath);

        rpl = SbieDll_CallServerQueue(INTERACTIVE_QUEUE_NAME, &req, sizeof(req), sizeof(*rpl));
    }

    if (rpl)
    {
        ok = rpl->retval != 0;
        Dll_Free(rpl);

        if(ok)
            return FILE_COPY_CONTENT;
    }

    //
    // issue appropriate message if so configured, and user wasn't asked
    //

    else if (!File_CopyLimitSilent) 
        File_MigrateFile_Message(TruePath, file_size, 2102);

    return FILE_DONT_COPY;
}


//---------------------------------------------------------------------------
// File_InitCopyLimit
//---------------------------------------------------------------------------


_FX void File_InitCopyLimit(void)
{
    static const WCHAR* _CopyLimitKb = L"CopyLimitKb";
    static const WCHAR* _CopyLimitSilent = L"CopyLimitSilent";

    //
    // if this is one of SandboxieCrypto, SandboxieWUAU or WUAUCLT,
    // or TrustedInstaller, then we don't impose a CopyLimit
    //

    BOOLEAN SetMaxCopyLimit = FALSE;

    if (Dll_ImageType == DLL_IMAGE_SANDBOXIE_CRYPTO ||
        Dll_ImageType == DLL_IMAGE_SANDBOXIE_WUAU ||
        Dll_ImageType == DLL_IMAGE_WUAUCLT ||
        Dll_ImageType == DLL_IMAGE_TRUSTED_INSTALLER) {

        SetMaxCopyLimit = TRUE;
    }

    if (SetMaxCopyLimit) {

        File_CopyLimitKb = -1;
        File_CopyLimitSilent = FALSE;
        return;
    }

    //
    // get configuration settings for CopyLimitKb and CopyLimitSilent
    //

    File_CopyLimitKb = SbieApi_QueryConfNumber64(NULL, _CopyLimitKb, -1);
    File_CopyLimitSilent = SbieApi_QueryConfBool(NULL, _CopyLimitSilent, FALSE);
}


//---------------------------------------------------------------------------
// File_MigrateFile
//---------------------------------------------------------------------------


_FX NTSTATUS File_MigrateFile(
    const WCHAR* TruePath, const WCHAR* CopyPath,
    BOOLEAN IsWritePath, BOOLEAN WithContents)
{
    NTSTATUS status;
    HANDLE TrueHandle, CopyHandle;
    OBJECT_ATTRIBUTES objattrs;
    UNICODE_STRING objname;
    IO_STATUS_BLOCK IoStatusBlock;
    FILE_NETWORK_OPEN_INFORMATION open_info;
    ULONGLONG file_size;
    ACCESS_MASK DesiredAccess;
    ULONG CreateOptions;
    PSECURITY_DESCRIPTOR pSecurityDescriptor = NULL;

    InitializeObjectAttributes(
        &objattrs, &objname, OBJ_CASE_INSENSITIVE, NULL, Secure_NormalSD);

    //
    // open TruePath.  if we get a sharing violation trying to open it,
    // try to get the driver to open it bypassing share access.  if even
    // this fails, then we can't copy the data, but can still create an
    // empty file
    //

    RtlInitUnicodeString(&objname, TruePath);

    status = __sys_NtCreateFile(
        &TrueHandle, FILE_GENERIC_READ, &objattrs, &IoStatusBlock,
        NULL, 0, FILE_SHARE_VALID_FLAGS,
        FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);

    if (IsWritePath && status == STATUS_ACCESS_DENIED)
        status = STATUS_SHARING_VIOLATION;

    if (status == STATUS_SHARING_VIOLATION) {

        status = SbieApi_OpenFile(&TrueHandle, TruePath);

        if (!NT_SUCCESS(status)) {

            WithContents = FALSE;

            status = __sys_NtCreateFile(
                &TrueHandle, FILE_READ_ATTRIBUTES | SYNCHRONIZE,
                &objattrs, &IoStatusBlock, NULL, 0, FILE_SHARE_VALID_FLAGS,
                FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
        }
    }

    if (!NT_SUCCESS(status))
        return status;

    //
    // query attributes and size of the TruePath file
    //

    status = __sys_NtQueryInformationFile(
        TrueHandle, &IoStatusBlock, &open_info,
        sizeof(FILE_NETWORK_OPEN_INFORMATION), FileNetworkOpenInformation);

    if (!NT_SUCCESS(status)) {
        NtClose(TrueHandle);
        return status;
    }

    if (WithContents) {

        file_size = open_info.EndOfFile.QuadPart;

        ULONG mode = File_MigrateFile_GetMode(TruePath, file_size);

        if (mode == FILE_COPY_EMPTY)
            file_size = 0;
        else if (mode == FILE_DONT_COPY)
        {
            NtClose(TrueHandle);

            if (File_MigrationDenyWrite)
                return STATUS_ACCESS_DENIED;
            else
                return STATUS_BAD_INITIAL_PC;
        }

    }
    else
        file_size = 0;

    if (Secure_CopyACLs) {

        //
        // Query the security descriptor of the source file
        //

        ULONG lengthNeeded = 0;
        status = NtQuerySecurityObject(TrueHandle, DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION | /*OWNER_SECURITY_INFORMATION |*/ GROUP_SECURITY_INFORMATION, NULL, 0, &lengthNeeded);
        if (status == STATUS_BUFFER_TOO_SMALL) {
            pSecurityDescriptor = (PSECURITY_DESCRIPTOR)Dll_AllocTemp(lengthNeeded);
            status = NtQuerySecurityObject(TrueHandle, DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION | /*OWNER_SECURITY_INFORMATION |*/ GROUP_SECURITY_INFORMATION, pSecurityDescriptor, lengthNeeded, &lengthNeeded);
            if (NT_SUCCESS(status)) 
                File_AddCurrentUserToSD(&pSecurityDescriptor);
            else {
                Dll_Free(pSecurityDescriptor);
                pSecurityDescriptor = NULL;
            }
        }

        if (!NT_SUCCESS(status)) {
            NtClose(TrueHandle);
            return status;
        }

        InitializeObjectAttributes(
            &objattrs, &objname, OBJ_CASE_INSENSITIVE, NULL, pSecurityDescriptor);
    }

    //
    // create the CopyPath file
    //

    RtlInitUnicodeString(&objname, CopyPath);

    if (open_info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        DesiredAccess = FILE_GENERIC_READ;
        CreateOptions = FILE_DIRECTORY_FILE;
    }
    else {
        DesiredAccess = FILE_GENERIC_WRITE;
        CreateOptions = FILE_NON_DIRECTORY_FILE;
    }

    status = __sys_NtCreateFile(
        &CopyHandle, DesiredAccess, &objattrs, &IoStatusBlock,
        NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_VALID_FLAGS,
        FILE_CREATE, FILE_SYNCHRONOUS_IO_NONALERT | CreateOptions,
        NULL, 0);

    if (!NT_SUCCESS(status)) {
        NtClose(TrueHandle);
        if(pSecurityDescriptor)
            Dll_Free(pSecurityDescriptor);
        return status;
    }

    //
    // copy the file, if so desired
    //

    if (file_size) {

        ULONG Next_Status = GetTickCount() + 3000; // wait 3 seconds

        void* buffer = Dll_AllocTemp(PAGE_SIZE);
        if (!buffer) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            file_size = 0;
        }

        while (file_size > 0) {

            ULONG buffer_size =
                (file_size > PAGE_SIZE) ? PAGE_SIZE : (ULONG)file_size;

            status = NtReadFile(
                TrueHandle, NULL, NULL, NULL, &IoStatusBlock,
                buffer, buffer_size, NULL, NULL);

            if (NT_SUCCESS(status)) {

                buffer_size = (ULONG)IoStatusBlock.Information;
                file_size -= (ULONGLONG)buffer_size;

                status = NtWriteFile(
                    CopyHandle, NULL, NULL, NULL, &IoStatusBlock,
                    buffer, buffer_size, NULL, NULL);
            }

            if (!NT_SUCCESS(status))
                break;

            ULONG Cur_Ticks = GetTickCount();
            if (Next_Status < Cur_Ticks) {
                Next_Status = Cur_Ticks + 1000; // update progress every second

                WCHAR size_str[32];
                Sbie_snwprintf(size_str, 32, L"%I64u", file_size);
                const WCHAR* strings[] = { Dll_BoxName, TruePath, size_str, NULL };
                SbieApi_LogMsgExt(-1, 2198, strings);
            }
        }

        if (buffer)
            Dll_Free(buffer);
    }

    //
    // set the short name on the file.  we must do this before we copy
    // its attributes, as this may make the file read-only
    //

    if (NT_SUCCESS(status)) {

        status = File_CopyShortName(TruePath, CopyPath);

        if (IsWritePath && status == STATUS_ACCESS_DENIED)
            status = STATUS_SUCCESS;
    }

    //
    // set information on the CopyPath file
    //

    if (NT_SUCCESS(status)) {

        FILE_BASIC_INFORMATION info;

        info.CreationTime.QuadPart = open_info.CreationTime.QuadPart;
        info.LastAccessTime.QuadPart = open_info.LastAccessTime.QuadPart;
        info.LastWriteTime.QuadPart = open_info.LastWriteTime.QuadPart;
        info.ChangeTime.QuadPart = open_info.ChangeTime.QuadPart;
        info.FileAttributes = open_info.FileAttributes;

        status = File_SetAttributes(CopyHandle, CopyPath, &info);
    }

    NtClose(TrueHandle);
    if(pSecurityDescriptor)
        Dll_Free(pSecurityDescriptor);
    NtClose(CopyHandle);

    return status;
}


//---------------------------------------------------------------------------
// File_MigrateJunction
//---------------------------------------------------------------------------


_FX NTSTATUS File_MigrateJunction(
    const WCHAR* TruePath, const WCHAR* CopyPath,
    BOOLEAN IsWritePath)
{
    NTSTATUS status;
    HANDLE TrueHandle, CopyHandle;
    OBJECT_ATTRIBUTES objattrs;
    UNICODE_STRING objname;
    IO_STATUS_BLOCK IoStatusBlock;
    FILE_NETWORK_OPEN_INFORMATION open_info;
    PSECURITY_DESCRIPTOR pSecurityDescriptor = NULL;

    InitializeObjectAttributes(
        &objattrs, &objname, OBJ_CASE_INSENSITIVE, NULL, Secure_NormalSD);

    //
    // open TruePath.  if we get a sharing violation trying to open it,
    // try to get the driver to open it bypassing share access.  if even
    // this fails, then we can't copy the data, but can still create an
    // empty file
    //

    RtlInitUnicodeString(&objname, TruePath);

    status = __sys_NtCreateFile(
        &TrueHandle, FILE_GENERIC_READ, &objattrs, &IoStatusBlock,
        NULL, 0, FILE_SHARE_VALID_FLAGS,
        FILE_OPEN, FILE_OPEN_REPARSE_POINT | FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);

    /*if (IsWritePath && status == STATUS_ACCESS_DENIED)
        status = STATUS_SHARING_VIOLATION;

    if (status == STATUS_SHARING_VIOLATION) {

        status = SbieApi_OpenFile(&TrueHandle, TruePath);
    }*/

    if (!NT_SUCCESS(status))
        return status;

    //
    // query attributes and size of the TruePath file
    //

    status = __sys_NtQueryInformationFile(
        TrueHandle, &IoStatusBlock, &open_info,
        sizeof(FILE_NETWORK_OPEN_INFORMATION), FileNetworkOpenInformation);

    //
    // Get the reparse point data from the source
    //

    BYTE buf[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];  // We need a large buffer
    REPARSE_DATA_BUFFER* reparseDataBuffer = (REPARSE_DATA_BUFFER*)buf;
    status = __sys_NtFsControlFile(TrueHandle, NULL, NULL, NULL, &IoStatusBlock, FSCTL_GET_REPARSE_POINT, NULL, 0, reparseDataBuffer, MAXIMUM_REPARSE_DATA_BUFFER_SIZE);

    if (!NT_SUCCESS(status))
        return status;

    if (Secure_CopyACLs) {

        //
        // Query the security descriptor of the source file
        //

        ULONG lengthNeeded = 0;
        status = NtQuerySecurityObject(TrueHandle, DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION | /*OWNER_SECURITY_INFORMATION |*/ GROUP_SECURITY_INFORMATION, NULL, 0, &lengthNeeded);
        if (status == STATUS_BUFFER_TOO_SMALL) {
            pSecurityDescriptor = (PSECURITY_DESCRIPTOR)Dll_AllocTemp(lengthNeeded);
            status = NtQuerySecurityObject(TrueHandle, DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION | /*OWNER_SECURITY_INFORMATION |*/ GROUP_SECURITY_INFORMATION, pSecurityDescriptor, lengthNeeded, &lengthNeeded);
            if (NT_SUCCESS(status)) 
                File_AddCurrentUserToSD(&pSecurityDescriptor);
            else {
                Dll_Free(pSecurityDescriptor);
                pSecurityDescriptor = NULL;
            }
        }

        if (!NT_SUCCESS(status)) {
            NtClose(TrueHandle);
            return status;
        }

        InitializeObjectAttributes(
            &objattrs, &objname, OBJ_CASE_INSENSITIVE, NULL, pSecurityDescriptor);
    }

    //
    // Create the destination file with reparse point data
    //

    RtlInitUnicodeString(&objname, CopyPath);

    status = __sys_NtCreateFile(
        &CopyHandle, FILE_GENERIC_WRITE, &objattrs, &IoStatusBlock,
        NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_VALID_FLAGS,
        FILE_CREATE, FILE_SYNCHRONOUS_IO_NONALERT | FILE_DIRECTORY_FILE | FILE_OPEN_REPARSE_POINT,
        NULL, 0);

    if (!NT_SUCCESS(status)) {
        NtClose(TrueHandle);
        if (pSecurityDescriptor)
            Dll_Free(pSecurityDescriptor);
    }

    //
    // Set the reparse point data to the destination
    //

    #define REPARSE_MOUNTPOINT_HEADER_SIZE 8
    status = __sys_NtFsControlFile(CopyHandle, NULL, NULL, NULL, &IoStatusBlock, FSCTL_SET_REPARSE_POINT, reparseDataBuffer, REPARSE_MOUNTPOINT_HEADER_SIZE + reparseDataBuffer->ReparseDataLength, NULL, 0);

    //
    // set information on the CopyPath file
    //

    if (NT_SUCCESS(status)) {

        FILE_BASIC_INFORMATION info;

        info.CreationTime.QuadPart = open_info.CreationTime.QuadPart;
        info.LastAccessTime.QuadPart = open_info.LastAccessTime.QuadPart;
        info.LastWriteTime.QuadPart = open_info.LastWriteTime.QuadPart;
        info.ChangeTime.QuadPart = open_info.ChangeTime.QuadPart;
        info.FileAttributes = open_info.FileAttributes;

        status = File_SetAttributes(CopyHandle, CopyPath, &info);
    }

    NtClose(TrueHandle);
    if(pSecurityDescriptor)
        Dll_Free(pSecurityDescriptor);
    NtClose(CopyHandle);

    return status;
}
