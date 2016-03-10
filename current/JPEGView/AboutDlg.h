// About dialog
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "resource.h"

// About dialog
class CAboutDlg : public CDialogImpl<CAboutDlg>
{
public:
	enum { IDD = IDD_ABOUT };

	BEGIN_MSG_MAP(CAboutDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
		COMMAND_ID_HANDLER(IDC_CLOSE, OnCloseDialog)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseDialog)
		NOTIFY_HANDLER(IDC_LICENSE, EN_LINK, OnLinkClicked)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseDialog(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnLinkClicked(WPARAM wParam, LPNMHDR lpnmhdr, BOOL& bHandled);

	CAboutDlg(void);
	~CAboutDlg(void);

private:
	CStatic m_lblVersion;
	CStatic m_lblSIMD;
	CStatic m_lblNumCores;
	CRichEditCtrl m_richEdit;
	CButton m_btnClose;
};
