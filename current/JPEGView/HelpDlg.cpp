#include "StdAfx.h"
#include "HelpDlg.h"
#include "NLS.h"
#include "MainDlg.h"
#include "HelpDisplayCtl.h"
#include "MultiMonitorSupport.h"

///////////////////////////////////////////////////////////////////////////////////
// Class implementation
///////////////////////////////////////////////////////////////////////////////////

CHelpDlg::CHelpDlg(CMainDlg* pOwner) {
	m_pOwner = pOwner;
	m_isDestoyed = false;
}

CHelpDlg::~CHelpDlg() {
}

LRESULT CHelpDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	HICON hIconSmall = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME),
		IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	SetIcon(hIconSmall, FALSE);

	CRect windowRect, clientRect;
	GetWindowRect(&windowRect);
	GetClientRect(&clientRect);
	int inflateX = (windowRect.Width() - clientRect.Width()) / 2;
	int inflateY = (windowRect.Height() - clientRect.Height()) / 2;

	CDC dc(this->GetDC());
	CHelpDisplayCtl helpDisplayCtl(m_pOwner, dc, m_pOwner->GetImageProcessingParams());
	CRect panelRect = helpDisplayCtl.PanelRect();
	SetScrollSize(panelRect.Width() - 1, panelRect.Height() - 1);
	CRect monitorRect = CMultiMonitorSupport::GetWorkingRect(m_hWnd);
	int scrollbarWidth = (panelRect.Height() > monitorRect.Height()) ? ::GetSystemMetrics(SM_CXVSCROLL) : 0;
	panelRect.InflateRect(inflateX, inflateY, inflateX + scrollbarWidth, inflateY);
	MoveWindow(CRect(0, 0, min(monitorRect.Width(), panelRect.Width()), min(monitorRect.Height(), panelRect.Height())));

	this->SetWindowText(CNLS::GetString(_T("JPEGView Help")));
	CenterWindow();

	return TRUE;
}

LRESULT CHelpDlg::OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	DestroyDialog();
	return 0;
}

LRESULT CHelpDlg::OnKeyDown(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	if (wParam == VK_ESCAPE || wParam == VK_F1) {
		DestroyDialog();
	}
	return 1;
}

void CHelpDlg::DoPaint(HDC hDC) {
	CDC dc(hDC);
	CHelpDisplayCtl helpDisplayCtl(m_pOwner, dc, m_pOwner->GetImageProcessingParams());
	helpDisplayCtl.Show();
	dc.Detach();
}

LRESULT CHelpDlg::OnEraseBackground(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled) {
	// prevent erasing background
	bHandled = TRUE;
	return TRUE;
}

void CHelpDlg::DestroyDialog() {
	DestroyWindow();
	m_isDestoyed = true;
}