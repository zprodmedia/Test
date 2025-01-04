#include "stdafx.h"
#include "OptionsWindow.h"
#include "SandMan.h"
#include "SettingsWindow.h"
#include "../MiscHelpers/Common/Settings.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MiscHelpers/Common/ComboInputDialog.h"
#include "../MiscHelpers/Common/SettingsWidgets.h"
#include "../MiscHelpers/Common/CheckableMessageBox.h"
#include "Helpers/WinAdmin.h"


void COptionsWindow::CreateAccess()
{
	// Resource Access
	connect(ui.chkPrivacy, SIGNAL(clicked(bool)), this, SLOT(OnAccessChangedEx()));
	connect(ui.chkUseSpecificity, SIGNAL(clicked(bool)), this, SLOT(OnAccessChangedEx()));
	connect(ui.chkBlockWMI, SIGNAL(clicked(bool)), this, SLOT(OnAccessChangedEx()));
	connect(ui.chkHideHostApps, SIGNAL(clicked(bool)), this, SLOT(OnAccessChangedEx()));
	connect(ui.chkCloseForBox, SIGNAL(clicked(bool)), this, SLOT(OnAccessChangedEx()));
	connect(ui.chkNoOpenForBox, SIGNAL(clicked(bool)), this, SLOT(OnAccessChangedEx()));
	//

	connect(ui.btnAddFile, SIGNAL(clicked(bool)), this, SLOT(OnAddFile()));
	QMenu* pFileBtnMenu = new QMenu(ui.btnAddFile);
	pFileBtnMenu->addAction(tr("Browse for File"), this, SLOT(OnBrowseFile()));
	pFileBtnMenu->addAction(tr("Browse for Folder"), this, SLOT(OnBrowseFolder()));
	ui.btnAddFile->setPopupMode(QToolButton::MenuButtonPopup);
	ui.btnAddFile->setMenu(pFileBtnMenu);
	connect(ui.chkShowFilesTmpl, SIGNAL(clicked(bool)), this, SLOT(OnShowFilesTmpl()));
	connect(ui.btnDelFile, SIGNAL(clicked(bool)), this, SLOT(OnDelFile()));
	connect(ui.btnAddKey, SIGNAL(clicked(bool)), this, SLOT(OnAddKey()));
	connect(ui.chkShowKeysTmpl, SIGNAL(clicked(bool)), this, SLOT(OnShowKeysTmpl()));
	connect(ui.btnDelKey, SIGNAL(clicked(bool)), this, SLOT(OnDelKey()));
	connect(ui.btnAddIPC, SIGNAL(clicked(bool)), this, SLOT(OnAddIPC()));
	connect(ui.chkShowIPCTmpl, SIGNAL(clicked(bool)), this, SLOT(OnShowIPCTmpl()));
	connect(ui.btnDelIPC, SIGNAL(clicked(bool)), this, SLOT(OnDelIPC()));
	connect(ui.btnAddWnd, SIGNAL(clicked(bool)), this, SLOT(OnAddWnd()));
	connect(ui.chkShowWndTmpl, SIGNAL(clicked(bool)), this, SLOT(OnShowWndTmpl()));
	connect(ui.btnDelWnd, SIGNAL(clicked(bool)), this, SLOT(OnDelWnd()));
	connect(ui.btnAddCOM, SIGNAL(clicked(bool)), this, SLOT(OnAddCOM()));
	connect(ui.chkShowCOMTmpl, SIGNAL(clicked(bool)), this, SLOT(OnShowCOMTmpl()));
	connect(ui.btnDelCOM, SIGNAL(clicked(bool)), this, SLOT(OnDelCOM()));
	//connect(ui.chkShowAccessTmpl, SIGNAL(clicked(bool)), this, SLOT(OnShowAccessTmpl()));
	//connect(ui.btnDelAccess, SIGNAL(clicked(bool)), this, SLOT(OnDelAccess()));

	QTreeWidget* pTrees[] = { ui.treeFiles, ui.treeKeys , ui.treeIPC, ui.treeWnd, ui.treeCOM, NULL};
	for (QTreeWidget** pTree = pTrees; *pTree; pTree++) {
		//connect(*pTree, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(OnAccessItemClicked(QTreeWidgetItem*, int)));
		connect(*pTree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnAccessItemDoubleClicked(QTreeWidgetItem*, int)));
		connect(*pTree, SIGNAL(itemSelectionChanged()), this, SLOT(OnAccessSelectionChanged()));
		connect(*pTree, SIGNAL(itemChanged(QTreeWidgetItem *, int)), this, SLOT(OnAccessChanged(QTreeWidgetItem *, int)));
	}

	connect(ui.tabsAccess, SIGNAL(currentChanged(int)), this, SLOT(OnAccessTab()));
}

void COptionsWindow::OnAccessChangedEx()
{
	if (sender() == ui.chkPrivacy || sender() == ui.chkUseSpecificity) {
		if (ui.chkPrivacy->isChecked() || (ui.chkUseSpecificity->isEnabled() && ui.chkUseSpecificity->isChecked()))
			theGUI->CheckCertificate(this, 0);
	}

	UpdateAccessPolicy();

	if ((sender() == ui.chkPrivacy || sender() == ui.chkRestrictDevices) && !(ui.chkPrivacy->isChecked() || ui.chkRestrictDevices->isChecked())) {
		ui.chkUseSpecificity->setChecked(m_pBox->GetBool("UseRuleSpecificity", false));
	}

	OnAccessChanged();
}

void COptionsWindow::OnAccessChanged()
{ 
	UpdateJobOptions();

	m_AccessChanged = true;
	OnOptChanged();
}

void COptionsWindow::UpdateAccessPolicy()
{ 
	ui.chkUseSpecificity->setEnabled(!(ui.chkPrivacy->isChecked() || ui.chkRestrictDevices->isChecked()));

	if (ui.chkPrivacy->isChecked() || ui.chkRestrictDevices->isChecked()) {
		ui.chkUseSpecificity->setChecked(true);
	}
}

QTreeWidgetItem* COptionsWindow::GetAccessEntry(EAccessType Type, const QString& Program, EAccessMode Mode, const QString& Path)
{
	QTreeWidget* pTree = GetAccessTree(Type);
	for (int i = 0; i < pTree->topLevelItemCount(); i++)
	{
		QTreeWidgetItem* pItem = pTree->topLevelItem(i);
		if (pItem->data(0, Qt::UserRole).toInt() == Type
			&& pItem->data(1, Qt::UserRole).toString().compare(Program, Qt::CaseInsensitive) == 0
			&& pItem->data(2, Qt::UserRole).toInt() == Mode
			&& pItem->data(3, Qt::UserRole).toString().compare(Path, Qt::CaseInsensitive) == 0)
			return pItem;
	}
	return NULL;
}

bool COptionsWindow::IsAccessEntrySet(EAccessType Type, const QString& Program, EAccessMode Mode, const QString& Path)
{
	QTreeWidgetItem* pItem = GetAccessEntry(Type, Program, Mode, Path);
	return pItem && pItem->checkState(0) == Qt::Checked;
}

void COptionsWindow::SetAccessEntry(EAccessType Type, const QString& Program, EAccessMode Mode, const QString& Path)
{
	if (GetAccessEntry(Type, Program, Mode, Path) != NULL)
		return; // already set
	OnAccessChanged();
	AddAccessEntry(Type, Mode, Program, Path);
}

void COptionsWindow::DelAccessEntry(EAccessType Type, const QString& Program, EAccessMode Mode, const QString& Path)
{
	if(QTreeWidgetItem* pItem = GetAccessEntry(Type, Program, Mode, Path))
	{
		delete pItem;
		OnAccessChanged();
	}
}

QString COptionsWindow::AccessTypeToName(EAccessEntry Type)
{
	switch (Type)
	{
	case eNormalFilePath:	return "NormalFilePath";
	case eOpenFilePath:		return "OpenFilePath";
	case eOpenPipePath:		return "OpenPipePath";
	case eClosedFilePath:	return "ClosedFilePath";
	case eReadFilePath:		return "ReadFilePath";
	case eWriteFilePath:	return "WriteFilePath";

	case eNormalKeyPath:	return "NormalKeyPath";
	case eOpenKeyPath:		return "OpenKeyPath";
	case eOpenConfPath:		return "OpenConfPath";
	case eClosedKeyPath:	return "ClosedKeyPath";
	case eReadKeyPath:		return "ReadKeyPath";
	case eWriteKeyPath:		return "WriteKeyPath";

	case eNormalIpcPath:	return "NormalIpcPath";
	case eOpenIpcPath:		return "OpenIpcPath";
	case eClosedIpcPath:	return "ClosedIpcPath";
	case eReadIpcPath:		return "ReadIpcPath";

	case eOpenWinClass:		return "OpenWinClass";
	case eNoRenameWinClass:	return "NoRenameWinClass";

	case eOpenCOM:			return "OpenClsid";
	case eClosedCOM:		return "ClosedClsid";
	case eClosedCOM_RT:		return "ClosedRT";
	}
	return "Unknown";
}

void COptionsWindow::LoadAccessList()
{
	ui.chkPrivacy->setChecked(m_pBox->GetBool("UsePrivacyMode", false));
	ui.chkUseSpecificity->setChecked(m_pBox->GetBool("UseRuleSpecificity", false));
	ui.chkBlockWMI->setChecked(m_BoxTemplates.contains("BlockAccessWMI"));
	ui.chkHideHostApps->setChecked(m_BoxTemplates.contains("HideInstalledPrograms"));
	ui.chkCloseForBox->setChecked(m_pBox->GetBool("AlwaysCloseForBoxed", true));
	ui.chkNoOpenForBox->setChecked(m_pBox->GetBool("DontOpenForBoxed", true));

	QTreeWidget* pTrees[] = { ui.treeFiles, ui.treeKeys , ui.treeIPC, ui.treeWnd, ui.treeCOM, NULL};
	for (QTreeWidget** pTree = pTrees; *pTree; pTree++)
		(*pTree )->clear();

	for (int i = 0; i < eMaxAccessEntry; i++)
	{
		foreach(const QString& Value, m_pBox->GetTextList(AccessTypeToName((EAccessEntry)i), m_Template))
			ParseAndAddAccessEntry((EAccessEntry)i, Value);

		foreach(const QString& Value, m_pBox->GetTextList(AccessTypeToName((EAccessEntry)i) + "Disabled", m_Template))
			ParseAndAddAccessEntry((EAccessEntry)i, Value, true);
	}

	LoadAccessListTmpl();

	UpdateAccessPolicy();

	m_AccessChanged = false;
}

void COptionsWindow::LoadAccessListTmpl(bool bUpdate)
{
	for (int i = 0; i < eMaxAccessType; i++) {
		QCheckBox* pCheck = NULL;
		switch (i)
		{
		case eFile:	pCheck = ui.chkShowFilesTmpl; break;
		case eKey:	pCheck = ui.chkShowKeysTmpl; break;
		case eIPC:	pCheck = ui.chkShowIPCTmpl; break;
		case eWnd:	pCheck = ui.chkShowWndTmpl; break;
		case eCOM:	pCheck = ui.chkShowCOMTmpl;	break;
		}
		LoadAccessListTmpl((EAccessType)i, pCheck->isChecked(), bUpdate);
	}
}

QTreeWidget* COptionsWindow::GetAccessTree(EAccessType Type)
{
	QTreeWidget* pTree = NULL;
	switch (Type)
	{
	case eFile:	pTree = ui.treeFiles; break;
	case eKey:	pTree = ui.treeKeys; break;
	case eIPC:	pTree = ui.treeIPC; break;
	case eWnd:	pTree = ui.treeWnd; break;
	case eCOM:	pTree = ui.treeCOM; break;
	}
	return pTree;
}

void COptionsWindow::LoadAccessListTmpl(EAccessType Type, bool bChecked, bool bUpdate)
{
	if (bChecked)
	{
		foreach(EAccessMode Mode, GetAccessModes((EAccessType)Type))
		{
			foreach(const QString & Template, m_pBox->GetTemplates())
			{
				foreach(const QString & Value, m_pBox->GetTextListTmpl(MakeAccessStr(Type, Mode), Template))
					ParseAndAddAccessEntry(Type, Mode, Value, false, Template);
			}
		}
	}
	else if (bUpdate)
	{
		QTreeWidget* pTree = GetAccessTree(Type);
		for (int i = 0; i < pTree->topLevelItemCount(); )
		{
			QTreeWidgetItem* pItem = pTree->topLevelItem(i);
			int Type = pItem->data(0, Qt::UserRole).toInt();
			if (Type == -1) {
				delete pItem;
				continue; // entry from template
			}
			i++;
		}
	}
}

QPair<COptionsWindow::EAccessType, COptionsWindow::EAccessMode> COptionsWindow::SplitAccessType(EAccessEntry EntryType)
{
	EAccessType	Type = eMaxAccessType;
	EAccessMode	Mode = eMaxAccessMode;
	switch (EntryType)
	{
	case eNormalFilePath:	Type = eFile;	Mode = eNormal;	break;
	case eOpenFilePath:		Type = eFile;	Mode = eOpen;	break;
	case eOpenPipePath:		Type = eFile;	Mode = eOpen4All; break;
	case eClosedFilePath:	Type = eFile;	Mode = eClosed;	break;
	case eReadFilePath:		Type = eFile;	Mode = eReadOnly; break;
	case eWriteFilePath:	Type = eFile;	Mode = eBoxOnly; break;

	case eNormalKeyPath:	Type = eKey;	Mode = eNormal;	break;
	case eOpenKeyPath:		Type = eKey;	Mode = eOpen;	break;
	case eOpenConfPath:		Type = eKey;	Mode = eOpen4All; break;
	case eClosedKeyPath:	Type = eKey;	Mode = eClosed;	break;
	case eReadKeyPath:		Type = eKey;	Mode = eReadOnly; break;
	case eWriteKeyPath:		Type = eKey;	Mode = eBoxOnly; break;

	case eNormalIpcPath:	Type = eIPC;	Mode = eNormal;	break;
	case eOpenIpcPath:		Type = eIPC;	Mode = eOpen;	break;
	case eClosedIpcPath:	Type = eIPC;	Mode = eClosed;	break;
	case eReadIpcPath:		Type = eIPC;	Mode = eReadOnly; break;

	case eOpenWinClass:		Type = eWnd;	Mode = eOpen;	break;
	case eNoRenameWinClass:	Type = eWnd;	Mode = eNoRename;	break;

	case eOpenCOM:			Type = eCOM;	Mode = eOpen;	break;
	case eClosedCOM:		Type = eCOM;	Mode = eClosed;	break;
	case eClosedCOM_RT:		Type = eCOM;	Mode = eClosedRT; break;
	}

	return qMakePair(Type, Mode);
}

void COptionsWindow::ParseAndAddAccessEntry(EAccessEntry EntryType, const QString& Value, bool disabled, const QString& Template)
{
	QPair<EAccessType, EAccessMode> Type = SplitAccessType(EntryType);
	if (Type.first == eMaxAccessType || Type.first == eMaxAccessMode)
		return;

	ParseAndAddAccessEntry(Type.first, Type.second, Value, disabled, Template);
}

void COptionsWindow::ParseAndAddAccessEntry(EAccessType Type, EAccessMode Mode, const QString& Value, bool disabled, const QString& Template)
{
	//
	// Mind this special cases
	// OpenIpcPath=$:program.exe <- full access into the address space of a target process running outside the sandbox. 
	// OpenWinClass=$:program.exe <- permits to use the PostThreadMessage API to send a message directly to a thread running outside the sandbox. 
	// This form of the setting does not support wildcards.
	//

	QStringList Values = Value.split(",");

	if (Type == eWnd && Mode == eOpen) {
		int pos = Values.count() >= 2 ? 1 : 0;
		if (Values[pos].right(11).compare("/IgnoreUIPI", Qt::CaseInsensitive) == 0) {
			Mode = eIgnoreUIPI;
			Values[pos].truncate(Values[pos].length() - 11);
		}
	}

	if (Values.count() >= 2) 
		AddAccessEntry(Type, Mode, Values[0], Values[1], disabled, Template);
	else // all programs
		AddAccessEntry(Type, Mode, "", Values[0], disabled, Template);
}

QString COptionsWindow::GetAccessModeStr(EAccessMode Mode)
{
	switch (Mode)
	{
	case eNormal:		return tr("Normal");
	case eOpen:			return tr("Open");
	case eOpen4All:		return tr("Open for All");
	case eNoRename:		return tr("No Rename");
	case eClosed:		return tr("Closed");
	case eClosedRT:		return tr("Closed RT");
	case eReadOnly:		return tr("Read Only");
	case eBoxOnly:		return tr("Box Only (Write Only)");
	case eIgnoreUIPI:	return tr("Ignore UIPI");
	}
	return tr("Unknown");
}

QString COptionsWindow::GetAccessModeTip(EAccessMode Mode)
{
	switch (Mode)
	{
	case eNormal:		return tr("Regular Sandboxie behavior - allow read and also copy on write.");
	case eOpen:			return tr("Allow write-access outside the sandbox.");
	case eOpen4All:		return tr("Allow write-access outside the sandbox, also for applications installed inside the sandbox.");
	case eNoRename:		return tr("Don't rename window classes.");
	case eClosed:		return tr("Deny access to host location and prevent creation of sandboxed copies.");
	case eClosedRT:		return tr("Block access to WinRT class.");
	case eReadOnly:		return tr("Allow read-only access only.");
	case eBoxOnly:		return tr("Hide host files, folders or registry keys from sandboxed processes.");
	case eIgnoreUIPI:	return tr("Ignore UIPI restrictions for processes.");
	}
	return tr("Unknown");
}

QString COptionsWindow::GetAccessTypeStr(EAccessType Type)
{
	switch (Type)
	{
	case eFile:			return tr("File/Folder");
	case eKey:			return tr("Registry");
	case eIPC:			return tr("IPC Path");
	case eWnd:			return tr("Wnd Class");
	case eCOM:			return tr("COM Object");
	}
	return tr("Unknown");
}

void COptionsWindow::OnBrowseFile()
{
	QString Value = QFileDialog::getOpenFileName(this, tr("Select File"), "", tr("All Files (*.*)")).replace("/", "\\");
	if (Value.isEmpty())
		return;

	AddAccessEntry(eFile, eOpen, "", Value);

	OnAccessChanged();
}

void COptionsWindow::OnBrowseFolder()
{
	QString Value = QFileDialog::getExistingDirectory(this, tr("Select Directory")).replace("/", "\\");
	if (Value.isEmpty())
		return;

	// Add a trailing backslash if it does not exist
	if (!Value.endsWith("\\"))
		Value.append("\\");

	AddAccessEntry(eFile, eOpen, "", Value);

	OnAccessChanged();
}

QString COptionsWindow::ExpandPath(EAccessType Type, const QString& Path)
{
	QString sPath = Path;
	if (CSandBox* pBox = qobject_cast<CSandBox*>(m_pBox.data()))
		sPath = theAPI->Nt2DosPath(pBox->Expand(sPath));
	if ((Type == eFile || Type == eKey) && !sPath.isEmpty()) { 
		if (sPath.left(1) == "|")
			return sPath.mid(1);
		else if (!sPath.contains("*") && sPath.right(1) != "*")
			return sPath + "*";
	}
	return sPath;
}

void COptionsWindow::AddAccessEntry(EAccessType Type, EAccessMode Mode, QString Program, const QString& Path, bool disabled, const QString& Template)
{
	QTreeWidgetItem* pItem = new QTreeWidgetItem();

	pItem->setText(0, GetAccessTypeStr(Type) + (Template.isEmpty() ? "" : " (" + Template + ")"));
	pItem->setData(0, Qt::UserRole, !Template.isEmpty() ? -1 : (int)Type);

	pItem->setData(1, Qt::UserRole, Program);
	bool bAll = Program.isEmpty();
	if (bAll)
		Program = tr("All Programs");
	bool Not = Program.left(1) == "!";
	if (Not)
		Program.remove(0, 1);
	if (Program.left(1) == "<")
		Program = tr("Group: %1").arg(Program.mid(1, Program.length() - 2));
	else if(!bAll)
		m_Programs.insert(Program);
	pItem->setText(1, (Not ? "NOT " : "") + Program);
	
	pItem->setText(2, GetAccessModeStr(Mode));
	pItem->setData(2, Qt::UserRole, (int)Mode);

	//////////////////////////////////////////////////////////
	// File and Registry entries auto append a '*' wildcard 
	// when they don't contain any.
	// Prepending '|' disables this behaviour
	//

	pItem->setText(3, ExpandPath(Type, Path));
	pItem->setData(3, Qt::UserRole, Path);

	if(Template.isEmpty())
		pItem->setCheckState(0, disabled ? Qt::Unchecked : Qt::Checked);

	QTreeWidget* pTree = GetAccessTree(Type);
	pTree->addTopLevelItem(pItem);
}

QString COptionsWindow::MakeAccessStr(EAccessType Type, EAccessMode Mode)
{
	switch (Type)
	{
	case eFile:
		switch (Mode)
		{
		case eNormal:		return "NormalFilePath";
		case eOpen:			return "OpenFilePath";
		case eOpen4All:		return "OpenPipePath";
		case eClosed:		return "ClosedFilePath";
		case eReadOnly:		return "ReadFilePath";
		case eBoxOnly:		return "WriteFilePath";
		}
		break;
	case eKey:
		switch (Mode)
		{
		case eNormal:		return "NormalKeyPath";
		case eOpen:			return "OpenKeyPath";
		case eOpen4All:		return "OpenConfPath";
		case eClosed:		return "ClosedKeyPath";
		case eReadOnly:		return "ReadKeyPath";
		case eBoxOnly:		return "WriteKeyPath";
		}
		break;
	case eIPC:
		switch (Mode)
		{
		case eNormal:		return "NormalIpcPath";
		case eOpen:			return "OpenIpcPath";
		case eClosed:		return "ClosedIpcPath";
		case eReadOnly:		return "ReadIpcPath";
		}
		break;
	case eWnd:
		switch (Mode)
		{
		case eOpen:			return "OpenWinClass";
		case eNoRename:		return "NoRenameWinClass";
		}
		break;
	case eCOM:
		switch (Mode)
		{
		case eOpen:			return "OpenClsid";
		case eClosed:		return "ClosedClsid";
		case eClosedRT:		return "ClosedRT";
		}
		break;
	}
	return "Unknown";
}

/*void COptionsWindow::OnAccessItemClicked(QTreeWidgetItem* pItem, int Column)
{
	if (Column != 0)
		return;

	CloseAccessEdit(pItem);
}*/

void COptionsWindow::CloseAccessEdit(bool bSave)
{
	QTreeWidget* pTrees[] = { ui.treeFiles, ui.treeKeys , ui.treeIPC, ui.treeWnd, ui.treeCOM, NULL};
	for (QTreeWidget** pTree = pTrees; *pTree; pTree++) {
		for (int i = 0; i < (*pTree)->topLevelItemCount(); i++)
		{
			QTreeWidgetItem* pItem = (*pTree)->topLevelItem(i);
			CloseAccessEdit(pItem, bSave);
		}
	}
}

void COptionsWindow::CloseAccessEdit(QTreeWidgetItem* pItem, bool bSave)
{
	QTreeWidget* pTree = pItem->treeWidget();

	QWidget* pProgram = pTree->itemWidget(pItem, 1);
	if (!pProgram)
		return;

	if (bSave)
	{
		QHBoxLayout* pLayout = (QHBoxLayout*)pProgram->layout();
		QToolButton* pNot = (QToolButton*)pLayout->itemAt(0)->widget();
		QComboBox* pCombo = (QComboBox*)pLayout->itemAt(1)->widget();

		QComboBox* pMode = (QComboBox*)pTree->itemWidget(pItem, 2);
		QLineEdit* pPath = (QLineEdit*)pTree->itemWidget(pItem, 3);

		QString Program = pCombo->currentText();
		int Index = pCombo->findText(Program);
		if (Index != -1)
			Program = pCombo->itemData(Index, Qt::UserRole).toString();
		if (!Program.isEmpty() && Program.left(1) != "<")
			m_Programs.insert(Program);

		EAccessMode Mode = (EAccessMode)pMode->currentData().toInt();
		QString Path = pPath->text();

		if (pItem->data(0, Qt::UserRole).toInt() == eCOM && !Path.isEmpty())
		{
			bool isGUID = Path.length() == 38 && Path.left(1) == "{" && Path.right(1) == "}";
			switch (Mode)
			{
			case eOpen:
			case eClosed:
				if (!isGUID) {
					QMessageBox::critical(this, "SandboxiePlus", tr("COM objects must be specified by their GUID, like: {00000000-0000-0000-0000-000000000000}"));
					return;
				}
				break;
			case eClosedRT:
				if (isGUID) {
					QMessageBox::critical(this, "SandboxiePlus", tr("RT interfaces must be specified by their name."));
					return;
				}
				break;
			}
		}

		if (pItem->data(0, Qt::UserRole).toInt() == eIPC && Mode == eOpen 
		  && ((Path == "*" && pItem->data(3, Qt::UserRole).toString() != "*") 
		   || (Path == "\\*" && pItem->data(3, Qt::UserRole).toString() != "\\*"))
		  && !m_BoxTemplates.contains("BoxedCOM"))  
		{
 			if (theConf->GetInt("Options/WarnOpenCOM", -1) == -1) {
				bool State = false;
				if (CCheckableMessageBox::question(this, "Sandboxie-Plus", tr("Opening all IPC access also opens COM access, do you still want to restrict COM to the sandbox?")
				 , tr("Don't ask in future"), &State, QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::Yes) == QDialogButtonBox::Yes)
					SetTemplate("BoxedCOM", true); // Normal overrides Open even without rule specificity :D
				if (State)
					theConf->SetValue("Options/WarnOpenCOM", 1);
			}
		}

		if (pItem->data(0, Qt::UserRole).toInt() == eWnd && Mode == eOpen && Path == "#" && !Program.isEmpty())
		{
			QMessageBox::warning(this, "Sandboxie-Plus", tr("'OpenWinClass=program.exe,#' is not supported, use 'NoRenameWinClass=program.exe,*' instead"));
			Mode = eNoRename;
			Path = "*";
		}

		EAccessType Type = (EAccessType)pItem->data(0, Qt::UserRole).toInt();
		pItem->setText(1, (pNot->isChecked() ? "NOT " : "") + pCombo->currentText());
		pItem->setData(1, Qt::UserRole, (pNot->isChecked() ? "!" : "") + Program);
		pItem->setText(2, GetAccessModeStr(Mode));
		pItem->setData(2, Qt::UserRole, (int)Mode);
		pItem->setText(3, ExpandPath(Type, Path));
		pItem->setData(3, Qt::UserRole, Path);

		OnAccessChanged();
	}

	pTree->setItemWidget(pItem, 1, NULL);
	pTree->setItemWidget(pItem, 2, NULL);
	pTree->setItemWidget(pItem, 3, NULL);
}

QList<COptionsWindow::EAccessMode> COptionsWindow::GetAccessModes(EAccessType Type)
{
	switch (Type)
	{
	case eFile:			return QList<EAccessMode>() << eNormal << eOpen << eOpen4All << eClosed << eReadOnly << eBoxOnly;
	case eKey:			return QList<EAccessMode>() << eNormal << eOpen << eOpen4All << eClosed << eReadOnly << eBoxOnly;
	case eIPC:			return QList<EAccessMode>() << eNormal << eOpen << eClosed << eReadOnly;
	case eWnd:			return QList<EAccessMode>() << eOpen << eNoRename << eIgnoreUIPI;
	case eCOM:			return QList<EAccessMode>() << eOpen << eClosed << eClosedRT;
	}
	return QList<EAccessMode>();
}

void COptionsWindow::OnAccessItemDoubleClicked(QTreeWidgetItem* pItem, int Column)
{
	//if (Column == 0)
	//	return;

	QTreeWidget* pTree = (QTreeWidget*)sender();

	int Type = pItem->data(0, Qt::UserRole).toInt();
	if (Type == -1) {
		QMessageBox::warning(this, "SandboxiePlus", tr("Template values can not be edited."));
		return;
	}

	QString Program = pItem->data(1, Qt::UserRole).toString();

	QWidget* pProgram = new QWidget();
	pProgram->setAutoFillBackground(true);
	QHBoxLayout* pLayout = new QHBoxLayout();
	pLayout->setContentsMargins(0,0,0,0);
	pLayout->setSpacing(0);
	pProgram->setLayout(pLayout);
	QToolButton* pNot = new QToolButton(pProgram);
	pNot->setText("!");
	pNot->setCheckable(true);
	if (Program.left(1) == "!"){
		pNot->setChecked(true);
		Program.remove(0, 1);
	}
	pLayout->addWidget(pNot);
	QComboBox* pCombo = new QComboBox(pProgram);
	pCombo->addItem(tr("All Programs"), "");

	foreach(const QString Group, GetCurrentGroups()){
		QString GroupName = Group.mid(1, Group.length() - 2);
		pCombo->addItem(tr("Group: %1").arg(GroupName), Group);
	}

	foreach(const QString & Name, m_Programs)
		pCombo->addItem(Name, Name);

	pCombo->setEditable(true);
	int Index = pCombo->findData(Program);
	pCombo->setCurrentIndex(Index);
	if(Index == -1)
		pCombo->setCurrentText(Program);
	pLayout->addWidget(pCombo);

	pTree->setItemWidget(pItem, 1, pProgram);

	QComboBox* pMode = new QComboBox();
	foreach(EAccessMode Mode, GetAccessModes((EAccessType)Type)) {
		pMode->addItem(GetAccessModeStr(Mode), (int)Mode);
		pMode->setItemData(pMode->count() - 1, GetAccessModeTip(Mode), Qt::ToolTipRole);
	}
	pMode->setCurrentIndex(pMode->findData(pItem->data(2, Qt::UserRole)));
	pTree->setItemWidget(pItem, 2, pMode);

	QLineEdit* pPath = new QLineEdit();
	pPath->setText(pItem->data(3, Qt::UserRole).toString());
	pTree->setItemWidget(pItem, 3, pPath);
}

void COptionsWindow::OnAccessChanged(QTreeWidgetItem* pItem, int Column)
{
	if (Column != 0)
		return;

	OnAccessChanged();
}

void COptionsWindow::DeleteAccessEntry(QTreeWidgetItem* pItem, int Column)
{
	if (!pItem)
		return;

	if (pItem->data(Column, Qt::UserRole).toInt() == -1) {
		QMessageBox::warning(this, "SandboxiePlus", tr("Template values can not be removed."));
		return;
	}

	delete pItem;
}

void COptionsWindow::SaveAccessList()
{
	WriteAdvancedCheck(ui.chkPrivacy, "UsePrivacyMode", "y", "");
	WriteAdvancedCheck(ui.chkUseSpecificity, "UseRuleSpecificity", "y", "");
	SetTemplate("BlockAccessWMI", ui.chkBlockWMI->isChecked());
	SetTemplate("HideInstalledPrograms", ui.chkHideHostApps->isChecked());
	WriteAdvancedCheck(ui.chkCloseForBox, "AlwaysCloseForBoxed", "", "n");
	WriteAdvancedCheck(ui.chkNoOpenForBox, "DontOpenForBoxed", "", "n");

	CloseAccessEdit(true);

	QStringList Keys = QStringList() 
		<< "NormalFilePath" << "OpenFilePath" << "OpenPipePath" << "ClosedFilePath" << "ReadFilePath" << "WriteFilePath"
		<< "NormalKeyPath" << "OpenKeyPath" << "OpenConfPath" << "ClosedKeyPath" << "ReadKeyPath" << "WriteKeyPath"
		<< "NormalIpcPath"<< "OpenIpcPath" << "ClosedIpcPath" << "ReadIpcPath" 
		<< "OpenWinClass" << "NoRenameWinClass"
		<< "OpenClsid" << "ClosedClsid" << "ClosedRT";

	QMap<QString, QList<QString>> AccessMap;


	QTreeWidget* pTrees[] = { ui.treeFiles, ui.treeKeys , ui.treeIPC, ui.treeWnd, ui.treeCOM, NULL};
	for (QTreeWidget** pTree = pTrees; *pTree; pTree++)
	{
		for (int i = 0; i < (*pTree)->topLevelItemCount(); i++)
		{
			QTreeWidgetItem* pItem = (*pTree)->topLevelItem(i);
			int Type = pItem->data(0, Qt::UserRole).toInt();
			if (Type == -1)
				continue; // entry from template
			int Mode = pItem->data(2, Qt::UserRole).toInt();
			QString Program = pItem->data(1, Qt::UserRole).toString();
			QString Value = pItem->data(3, Qt::UserRole).toString();
			if (!Program.isEmpty())
				Value.prepend(Program + ",");

			if (Type == eWnd && Mode == eIgnoreUIPI) {
				Mode = eOpen;
				Value.append("/IgnoreUIPI");
			}

			QString AccessStr = MakeAccessStr((EAccessType)Type, (EAccessMode)Mode);
			if (pItem->checkState(0) == Qt::Unchecked)
				AccessStr += "Disabled";
			AccessMap[AccessStr].append(Value);
		}
	}

	foreach(const QString & Key, Keys) {
		WriteTextList(Key, AccessMap[Key]);
		WriteTextList(Key + "Disabled", AccessMap[Key + "Disabled"]);
	}

	m_AccessChanged = false;
}
