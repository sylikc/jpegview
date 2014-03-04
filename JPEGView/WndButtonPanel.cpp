#include "StdAfx.h"
#include "WndButtonPanel.h"
#include "ImageProcessingPanel.h"
#include "Helpers.h"
#include "SettingsProvider.h"

#define WNDBUTTON_PANEL_HEIGHT 24
#define WNDBUTTON_BORDER 1

/////////////////////////////////////////////////////////////////////////////////////////////
// CWndButtonPanel
/////////////////////////////////////////////////////////////////////////////////////////////

CWndButtonPanel::CWndButtonPanel(HWND hWnd, INotifiyMouseCapture* pNotifyMouseCapture, CPanel* pImageProcPanel) : CPanel(hWnd, pNotifyMouseCapture) {
	m_pImageProcPanel = pImageProcPanel;
	m_clientRect = CRect(0, 0, 0, 0);
	m_fDPIScale *= CSettingsProvider::This().ScaleFactorNavPanel();
	m_nWidth = 0;
	m_nHeight = (int)(WNDBUTTON_PANEL_HEIGHT*m_fDPIScale);

	CButtonCtrl* pMinimizeBtn = AddUserPaintButton(ID_btnMinimize, (LPCTSTR)NULL, &PaintMinimizeBtn);
	pMinimizeBtn->SetExtendedActiveArea(CRect(0, -2, 0, 0));
	CButtonCtrl* pRestoreBtn = AddUserPaintButton(ID_btnRestore, (LPCTSTR)NULL, &PaintRestoreBtn);
	pRestoreBtn->SetExtendedActiveArea(CRect(0, -2, 0, 0));
	CButtonCtrl* pCloseBtn = AddUserPaintButton(ID_btnClose, (LPCTSTR)NULL, &PaintCloseBtn);
	pCloseBtn->SetExtendedActiveArea(CRect(0, -2, 2, 0)); // extend to make sure that moving mouse to top, right screen corner closes the application
}

CRect CWndButtonPanel::PanelRect() {
	if (m_nWidth == 0) {
		int nButtonSize = m_nHeight - 4 * WNDBUTTON_BORDER;
		m_nWidth = 4 * WNDBUTTON_BORDER + ((int)m_controls.size() - 1) * WNDBUTTON_BORDER * 3 + (int)m_controls.size() * nButtonSize;
	}

	CRect sliderRect = m_pImageProcPanel->PanelRect();
	m_clientRect = CRect(CPoint(sliderRect.right - m_nWidth, 0), CSize(m_nWidth, m_nHeight));
	return m_clientRect;
}

void CWndButtonPanel::RequestRepositioning() {
	RepositionAll();
}

void CWndButtonPanel::RepositionAll() {
	m_clientRect = PanelRect();
	int nButtonSize = m_nHeight - 4 * WNDBUTTON_BORDER;
	int nStartX = m_clientRect.right - m_nWidth + WNDBUTTON_BORDER * 2;
	int nStartY = WNDBUTTON_BORDER * 2;
	ControlsIterator iter;
	for (iter = m_controls.begin( ); iter != m_controls.end( ); iter++ ) {
		CButtonCtrl* pButton = dynamic_cast<CButtonCtrl*>(iter->second);
		if (pButton != NULL) {
			pButton->SetPosition(CRect(CPoint(nStartX, nStartY), CSize(nButtonSize, nButtonSize)));
			nStartX += nButtonSize + WNDBUTTON_BORDER * 3;
		}
	}
}

void CWndButtonPanel::PaintMinimizeBtn(void* pContext, const CRect& rect, CDC& dc) {
	CRect r = Helpers::InflateRect(rect, 0.3f);
	CPoint p1(r.left, r.bottom-1);
	dc.MoveTo(p1);
	CPoint p2(r.right, r.bottom-1);
	dc.LineTo(p2);
	CPoint p3(r.left, r.bottom);
	dc.MoveTo(p3);
	CPoint p4(r.right, r.bottom);
	dc.LineTo(p4);
}

void CWndButtonPanel::PaintRestoreBtn(void* pContext, const CRect& rect, CDC& dc) {
	CRect r = Helpers::InflateRect(rect, 0.25f);
	int nXt = rect.left + (int)(rect.Width()*0.7f);
	int nYt = rect.top + (int)(rect.Height()*0.4f);
	dc.Rectangle(r.left, nYt, nXt, r.bottom);
	CPoint p1(r.left+1, nYt+1);
	dc.MoveTo(p1);
	CPoint p2(nXt-1, nYt+1);
	dc.LineTo(p2);
	int nXm = (r.left + nXt) >> 1;
	int nYm = (nYt + r.bottom) >> 1;
	CPoint p11(nXm, r.top);
	dc.MoveTo(p11);
	CPoint p12(r.right, r.top);
	dc.LineTo(p12);
	CPoint p21(nXm, r.top-1);
	dc.MoveTo(p21);
	CPoint p22(r.right, r.top-1);
	dc.LineTo(p22);
	CPoint p31(r.right, r.top-1);
	dc.MoveTo(p31);
	CPoint p32(r.right, nYm);
	dc.LineTo(p32);
	CPoint p41(r.right-2, nYm);
	dc.LineTo(p41);
}

void CWndButtonPanel::PaintCloseBtn(void* pContext, const CRect& rect, CDC& dc) {
	CRect r = Helpers::InflateRect(rect, 0.25f);

	CPoint p1(r.left, r.top);
	dc.MoveTo(p1);
	CPoint p2(r.right, r.bottom);
	dc.LineTo(p2);
	CPoint p3(r.right, r.top);
	dc.MoveTo(p3);
	CPoint p4(r.left, r.bottom);
	dc.LineTo(p4);
	CPoint p11(r.left, r.top+1);
	dc.MoveTo(p11);
	CPoint p21(r.right-1, r.bottom);
	dc.LineTo(p21);
	CPoint p51(r.left+1, r.top);
	dc.MoveTo(p51);
	CPoint p52(r.right, r.bottom-1);
	dc.LineTo(p52);
	CPoint p31(r.right, r.top+1);
	dc.MoveTo(p31);
	CPoint p41(r.left+1, r.bottom);
	dc.LineTo(p41);
	CPoint p61(r.right-1, r.top);
	dc.MoveTo(p61);
	CPoint p62(r.left, r.bottom-1);
	dc.LineTo(p62);
}