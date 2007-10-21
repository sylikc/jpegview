#pragma once

#define MYWM_POSTINIT WM_USER+1

class CFileOpenDialog : public CFileDialog
{
public:
	CFileOpenDialog(HWND parentWindow, LPCTSTR sInitialFileName, LPCTSTR sFileEndings);

	BEGIN_MSG_MAP(CFileOpenDialog)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(MYWM_POSTINIT, OnPostInitDialog)
		NOTIFY_CODE_HANDLER(CDN_SELCHANGE, OnSelChange)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnPostInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSelChange(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);

private:
	static bool m_bSized;
};
