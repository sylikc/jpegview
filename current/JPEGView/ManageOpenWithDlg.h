// Manage 'Open with' commands dialog
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "resource.h"

class CUserCommand;

// Dialog to manage the "Open with" menu entries
class CManageOpenWithDlg : public CDialogImpl<CManageOpenWithDlg>
{
public:

	enum { IDD = IDD_OPENWITH };

	BEGIN_MSG_MAP(CManageOpenWithDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
		COMMAND_ID_HANDLER(IDC_OW_CLOSE, OnCloseDialog)
		COMMAND_ID_HANDLER(IDC_OW_SHORTCUT_HELP, OnShortcutHelp)
		COMMAND_ID_HANDLER(IDC_OW_DELETE, OnDeleteEntry)
		COMMAND_ID_HANDLER(IDC_OW_NEW, OnNewEntry)
		COMMAND_ID_HANDLER(IDC_OW_EDIT, OnEditEntry)
		COMMAND_ID_HANDLER(IDC_OW_BROWSE, OnBrowse)
		COMMAND_HANDLER(IDC_OPENWITHENTRIES, LBN_SELCHANGE, OnListBoxSelectionChanged)
		COMMAND_HANDLER(IDC_OW_TITLE, EN_CHANGE, OnTitleChanged)
		COMMAND_HANDLER(IDC_OW_SHORTCUT, EN_CHANGE, OnShortcutChanged)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseDialog(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnShortcutHelp(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDeleteEntry(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnNewEntry(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditEntry(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBrowse(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnListBoxSelectionChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTitleChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnShortcutChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CManageOpenWithDlg();

private:

	CStatic m_lblMenuEntries;
	CStatic m_lblTitle;
	CStatic m_lblApplication;
	CStatic m_lblShortcut;
	CButton m_btnClose;
	CButton m_btnBrowse;
	CButton m_btnNew;
	CButton m_btnEdit;
	CButton m_btnDelete;
	CButton m_btnShortcutHelp;
	CEdit m_edtTitle;
	CEdit m_edtApplication;
	CEdit m_edtShortcut;
	CListBox m_lbMenuEntries;

	bool m_editMode;

	void EnterEditMode(bool addNewEntry);
	void EnableControls(BOOL enable);
	void ClearControls();
	CUserCommand* GetSelectedCommand();
	void EnableDisableSaveButton();
};
