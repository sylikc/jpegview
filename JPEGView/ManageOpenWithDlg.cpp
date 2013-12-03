#include "StdAfx.h"
#include "ManageOpenWithDlg.h"
#include "NLS.h"
#include "UserCommand.h"
#include "SettingsProvider.h"
#include "HelpersGUI.h"
#include "KeyMap.h"

///////////////////////////////////////////////////////////////////////////////////
// Class implementation
///////////////////////////////////////////////////////////////////////////////////

CManageOpenWithDlg::CManageOpenWithDlg() {
	m_editMode = false;
}

LRESULT CManageOpenWithDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	m_lblMenuEntries.Attach(GetDlgItem(IDC_OPENWITHMENULABEL));
	m_lblTitle.Attach(GetDlgItem(IDC_OW_TITLE_LABEL));
	m_lblApplication.Attach(GetDlgItem(IDC_OW_APP_LABEL));
	m_lblShortcut.Attach(GetDlgItem(IDC_OW_SHORTCUT_LABEL));
	m_btnClose.Attach(GetDlgItem(IDC_OW_CLOSE));
	m_btnBrowse.Attach(GetDlgItem(IDC_OW_BROWSE));
	m_btnNew.Attach(GetDlgItem(IDC_OW_NEW));
	m_btnEdit.Attach(GetDlgItem(IDC_OW_EDIT));
	m_btnDelete.Attach(GetDlgItem(IDC_OW_DELETE));
	m_btnShortcutHelp.Attach(GetDlgItem(IDC_OW_SHORTCUT_HELP));
	m_edtTitle.Attach(GetDlgItem(IDC_OW_TITLE));
	m_edtApplication.Attach(GetDlgItem(IDC_OW_APP));
	m_edtShortcut.Attach(GetDlgItem(IDC_OW_SHORTCUT));
	m_lbMenuEntries.Attach(GetDlgItem(IDC_OPENWITHENTRIES));

	CenterWindow(GetParent());

	HICON hIconSmall = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	SetIcon(hIconSmall, FALSE);

	CString title(CNLS::GetString(_T("Manage 'Open image with' menu...")));
	title.Replace(_T("..."), _T(""));
	this->SetWindowText(title);

	m_lblMenuEntries.SetWindowText(CNLS::GetString(_T("Menu entries:")));
	m_lblTitle.SetWindowText(CNLS::GetString(_T("Title:")));
	m_lblApplication.SetWindowText(CNLS::GetString(_T("Application:")));
	m_lblShortcut.SetWindowText(CNLS::GetString(_T("Shortcut key:")));
	m_btnClose.SetWindowText(CNLS::GetString(_T("Close")));
	m_btnBrowse.SetWindowText(CNLS::GetString(_T("Browse...")));
	m_btnNew.SetWindowText(CNLS::GetString(_T("New")));
	m_btnEdit.SetWindowText(CNLS::GetString(_T("Edit")));
	m_btnDelete.SetWindowText(CNLS::GetString(_T("Delete")));

	m_edtTitle.SetLimitText(128);
	m_edtShortcut.SetLimitText(64);

	std::list<CUserCommand*> openWithEntries = CSettingsProvider::This().OpenWithCommandList();
	std::list<CUserCommand*>::iterator iter;
	for (iter = openWithEntries.begin( ); iter != openWithEntries.end( ); iter++ ) {
		LPCTSTR menuItemText = (*iter)->MenuItemText();
		m_lbMenuEntries.AddString(menuItemText);
	}

	EnableControls(FALSE);

	return TRUE;
}

LRESULT CManageOpenWithDlg::OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	EndDialog(IDCANCEL);
	return 0;
}

LRESULT CManageOpenWithDlg::OnCloseDialog(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (m_editMode) {
		m_btnClose.SetWindowText(CNLS::GetString(_T("Close")));
		EnableControls(FALSE);
		m_btnNew.EnableWindow(TRUE);
		m_btnEdit.EnableWindow(TRUE);
		m_btnDelete.EnableWindow(TRUE);
		m_lbMenuEntries.EnableWindow(TRUE);
		
		TCHAR title[127];
		int numChars = m_edtTitle.GetLine(0, (LPTSTR)title, 127);
		title[numChars] = 0;
		
		TCHAR application[MAX_PATH + 1];
		numChars = m_edtApplication.GetLine(0, (LPTSTR)application, MAX_PATH);
		application[numChars] = 0;

		TCHAR shortcut[63];
		numChars = m_edtShortcut.GetLine(0, (LPTSTR)shortcut, 63);
		shortcut[numChars] = 0;

		CString keyCode;
		if (shortcut[0] != 0) {
			keyCode.Format(_T("KeyCode: %s "), shortcut);
		}

		CString sCommandLine;
		sCommandLine.Format(_T("\"Cmd: '\"%s\" %%filename%%' %s Menuitem: '%s' Flags: 'ShellExecute'\""), application, (LPCTSTR)keyCode, title);
		
		CUserCommand* pSelectedCommand = GetSelectedCommand();
		if (pSelectedCommand == NULL) {
			// add new command
			CSettingsProvider::This().AddOpenWithCommand(sCommandLine);

			int index = m_lbMenuEntries.AddString(title);
			m_lbMenuEntries.DeleteString(index - 1);
			m_lbMenuEntries.SetCurSel(index - 1);
		} else {
			// change existing command
			CSettingsProvider::This().ChangeOpenWithCommand(pSelectedCommand, sCommandLine);

			int index = m_lbMenuEntries.GetCurSel();
			m_lbMenuEntries.DeleteString(index);
			m_lbMenuEntries.InsertString(index, title);
			m_lbMenuEntries.SetCurSel(index);
		}

		m_editMode = false;
	} else {
		EndDialog(IDOK);
	}
	return 0;
}

LRESULT CManageOpenWithDlg::OnListBoxSelectionChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CUserCommand* pUserCommand = GetSelectedCommand();
	m_btnDelete.EnableWindow(pUserCommand != NULL);
	m_btnEdit.EnableWindow(pUserCommand != NULL);
	if (pUserCommand != NULL) {
		m_edtTitle.SetWindowText(pUserCommand->MenuItemText());
		m_edtApplication.SetWindowText(pUserCommand->GetExecutable());
		CString shortcutKey = CKeyMap::GetShortcutKey(pUserCommand->GetKeyCode());
		m_edtShortcut.SetWindowText(shortcutKey);
	} else {
		ClearControls();
	}

	return 0;
}

LRESULT CManageOpenWithDlg::OnShortcutHelp(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	::MessageBox(m_hWnd, CString(CNLS::GetString(_T("Modifier keys:"))) + _T(" Alt, Ctrl, Shift\n\n") + 
		CNLS::GetString(_T("Known keys:")) + + _T(" Esc, Return, Space, End, Home, Back, Tab, PgDn, PgUp, Left, Right, Up, Down, Insert, Del, Plus, Minus, Mul, Div, Comma, Period, A .. Z, F1 .. F12\n\n") +
		CNLS::GetString(_T("Combine keys with '+'")) + _T("\n\n") +
		CNLS::GetString(_T("Example:")) + _T("'Alt+Ctrl+F7'")
		, CNLS::GetString(_T("Shortcut Help")), MB_OK | MB_ICONINFORMATION);
	return 0;
}

LRESULT CManageOpenWithDlg::OnDeleteEntry(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CUserCommand* pUserCommand = GetSelectedCommand();
	if (pUserCommand != NULL) {
		int index = CSettingsProvider::This().DeleteOpenWithCommand(pUserCommand);
		if (index >= 0) {
			m_lbMenuEntries.DeleteString(index);
			EnableControls(FALSE);
			ClearControls();
		}
	}
	return 0;
}

LRESULT CManageOpenWithDlg::OnNewEntry(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	ClearControls();
	EnterEditMode(true);
	return 0;
}

LRESULT CManageOpenWithDlg::OnEditEntry(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	EnterEditMode(false);
	return 0;
}

LRESULT CManageOpenWithDlg::OnBrowse(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CFileDialog fileDlg(TRUE, _T(".exe"), _T(""), 
		OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_DONTADDTORECENT,
		Helpers::CReplacePipe(CString(CNLS::GetString(_T("Applications"))) + _T(" (*.exe)|*.exe|") +
		CNLS::GetString(_T("All Files")) + _T("|*.*|")), m_hWnd);

	wchar_t buffer[MAX_PATH + 1];
	::SHGetSpecialFolderPath(m_hWnd, (LPWSTR)buffer, CSIDL_PROGRAM_FILES, FALSE);
	fileDlg.m_ofn.lpstrInitialDir = buffer;
	fileDlg.m_ofn.lpstrTitle = CNLS::GetString(_T("Select application"));
	if (IDOK == fileDlg.DoModal(m_hWnd)) {
		m_edtApplication.SetWindowText(fileDlg.m_szFileName);
		EnableDisableSaveButton();
	}
	return 0;
}

LRESULT CManageOpenWithDlg::OnTitleChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (m_editMode) {
		EnableDisableSaveButton();
	}
	return 0;
}

LRESULT CManageOpenWithDlg::OnShortcutChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (m_editMode) {
		EnableDisableSaveButton();
	}
	return 0;
}

void CManageOpenWithDlg::EnterEditMode(bool addNewEntry) {
	m_btnBrowse.EnableWindow(TRUE);
	m_edtTitle.EnableWindow(TRUE);
	m_edtShortcut.EnableWindow(TRUE);
	m_btnNew.EnableWindow(FALSE);
	m_btnEdit.EnableWindow(FALSE);
	m_btnDelete.EnableWindow(FALSE);
	if (addNewEntry) {
		m_lbMenuEntries.AddString(_T("[new entry]"));
		m_lbMenuEntries.SetCurSel(m_lbMenuEntries.GetCount() - 1);
		m_btnClose.EnableWindow(FALSE);
	}
	m_lbMenuEntries.EnableWindow(FALSE);
	m_btnClose.SetWindowText(CNLS::GetString(_T("Save")));
	m_edtTitle.SetFocus();
	m_editMode = true;
}

void CManageOpenWithDlg::EnableControls(BOOL enable) {
	m_btnBrowse.EnableWindow(enable);
	m_btnDelete.EnableWindow(enable);
	m_btnEdit.EnableWindow(enable);
	m_edtTitle.EnableWindow(enable);
	m_edtShortcut.EnableWindow(enable);
}

void CManageOpenWithDlg::ClearControls() {
	m_edtTitle.SetWindowText(_T(""));
	m_edtApplication.SetWindowText(_T(""));
	m_edtShortcut.SetWindowText(_T(""));
}

CUserCommand* CManageOpenWithDlg::GetSelectedCommand() {
	int selectedItem = m_lbMenuEntries.GetCurSel();
	if (selectedItem != LB_ERR)
		return HelpersGUI::FindOpenWithCommand(selectedItem);
	return NULL;
}

void CManageOpenWithDlg::EnableDisableSaveButton() {
	TCHAR buffer[64];
	bool isValid = m_edtTitle.GetLine(0, (LPTSTR)buffer, 16) > 0 && m_edtApplication.GetLine(0, (LPTSTR)buffer, 16) > 0;
	if (isValid && m_edtShortcut.GetLine(0, (LPTSTR)buffer, 64) > 0) {
		buffer[63] = 0;
		isValid = CKeyMap::GetVirtualKeyCode(buffer) > 0;
	}
	m_btnClose.EnableWindow(isValid);
}