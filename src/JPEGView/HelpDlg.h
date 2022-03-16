// Help dialog
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "resource.h"

class CMainDlg;

// Dialog to show help screen for JPEGView
class CHelpDlg : public CDialogImpl<CHelpDlg>, public CScrollImpl<CHelpDlg>
{
public:

	enum { IDD = IDD_HELP };

	BEGIN_MSG_MAP(CHelpDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
		MESSAGE_HANDLER(WM_VSCROLL, CScrollImpl<CHelpDlg>::OnVScroll)
		MESSAGE_HANDLER(WM_HSCROLL, CScrollImpl<CHelpDlg>::OnHScroll)
		MESSAGE_HANDLER(WM_MOUSEWHEEL, CScrollImpl<CHelpDlg>::OnMouseWheel)
		MESSAGE_HANDLER(WM_MOUSEHWHEEL, CScrollImpl<CHelpDlg>::OnMouseHWheel)
		MESSAGE_HANDLER(WM_SETTINGCHANGE, CScrollImpl<CHelpDlg>::OnSettingChange)
		MESSAGE_HANDLER(WM_SIZE, CScrollImpl<CHelpDlg>::OnSize)
		MESSAGE_HANDLER(WM_PAINT, CScrollImpl<CHelpDlg>::OnPaint)
		MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown)
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_SCROLL_UP, CScrollImpl<CHelpDlg>::OnScrollUp)
		COMMAND_ID_HANDLER(ID_SCROLL_DOWN, CScrollImpl<CHelpDlg>::OnScrollDown)
		COMMAND_ID_HANDLER(ID_SCROLL_PAGE_UP, CScrollImpl<CHelpDlg>::OnScrollPageUp)
		COMMAND_ID_HANDLER(ID_SCROLL_PAGE_DOWN, CScrollImpl<CHelpDlg>::OnScrollPageDown)
		COMMAND_ID_HANDLER(ID_SCROLL_TOP, CScrollImpl<CHelpDlg>::OnScrollTop)
		COMMAND_ID_HANDLER(ID_SCROLL_BOTTOM, CScrollImpl<CHelpDlg>::OnScrollBottom)
		COMMAND_ID_HANDLER(ID_SCROLL_LEFT, CScrollImpl<CHelpDlg>::OnScrollLeft)
		COMMAND_ID_HANDLER(ID_SCROLL_RIGHT, CScrollImpl<CHelpDlg>::OnScrollRight)
		COMMAND_ID_HANDLER(ID_SCROLL_PAGE_LEFT, CScrollImpl<CHelpDlg>::OnScrollPageLeft)
		COMMAND_ID_HANDLER(ID_SCROLL_PAGE_RIGHT, CScrollImpl<CHelpDlg>::OnScrollPageRight)
		COMMAND_ID_HANDLER(ID_SCROLL_ALL_LEFT, CScrollImpl<CHelpDlg>::OnScrollAllLeft)
		COMMAND_ID_HANDLER(ID_SCROLL_ALL_RIGHT, CScrollImpl<CHelpDlg>::OnScrollAllRight)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnKeyDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEraseBackground(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	void DoPaint(HDC hDC);

	bool IsDestroyed() { return m_isDestoyed; }
	void DestroyDialog();

	CHelpDlg(CMainDlg* pOwner);
	~CHelpDlg();

private:
	CMainDlg* m_pOwner;
	bool m_isDestoyed;
};
