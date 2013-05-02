// FileExtensionsDlg.h : interface of the CFileExtensionsDlg class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "resource.h"

class CFileExtensionsRegistry;

// Dialog to define the file extensions that are bound to JPEGView
class CFileExtensionsDlg : public CDialogImpl<CFileExtensionsDlg>
{
public:

	enum { IDD = IDD_SETFILEEXTENSIONS };

	BEGIN_MSG_MAP(CFileExtensionsDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
        COMMAND_ID_HANDLER(IDC_FE_OK, OnOK)
		COMMAND_ID_HANDLER(IDC_FE_CANCEL, OnCancelDialog)
		NOTIFY_HANDLER(IDC_LIST_FILES, LVN_ITEMCHANGED, OnListViewItemChanged)
		NOTIFY_HANDLER(IDC_LIST_FILES, LVN_KEYDOWN, OnListViewKeyDown)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
    LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancelDialog(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnListViewItemChanged(WPARAM wParam, LPNMHDR lpnmhdr, BOOL& bHandled);
	LRESULT OnListViewKeyDown(WPARAM wParam, LPNMHDR lpnmhdr, BOOL& bHandled);

	CFileExtensionsDlg();
	~CFileExtensionsDlg();

private:
	CStatic m_lblMessage;
	CButton m_btnOk;
	CButton m_btnCancel;
	CListViewCtrl m_lvFileExtensions;
    bool m_bInOnListViewItemChanged;
    CFileExtensionsRegistry* m_pRegistry;

    void FillFileExtensionsList();
    void InsertExtension(LPCTSTR sExtension, LPCTSTR sHint);
    void InsertExtensions(LPCTSTR sExtensionList, LPCTSTR sHint);
};
