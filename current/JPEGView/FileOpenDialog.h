#pragma once

class CFileOpenDialog : public CFileDialog
{
public:
	CFileOpenDialog(HWND parentWindow, LPCTSTR sInitialFileName, LPCTSTR sFileEndings);

	BEGIN_MSG_MAP(CFileOpenDialog)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		NOTIFY_CODE_HANDLER(CDN_SELCHANGE, OnSelChange)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSelChange(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);

private:
	static bool m_bSized;
};
