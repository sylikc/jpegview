#include "StdAfx.h"
#include "InfoButtonPanel.h"
#include "ImageProcessingPanel.h"
#include "Helpers.h"
#include "SettingsProvider.h"
#include "NLS.h"

#define WNDBUTTON_PANEL_HEIGHT 24
#define WNDBUTTON_BORDER 1

/////////////////////////////////////////////////////////////////////////////////////////////
// CWndButtonPanel
/////////////////////////////////////////////////////////////////////////////////////////////

CInfoButtonPanel::CInfoButtonPanel(HWND hWnd, INotifiyMouseCapture* pNotifyMouseCapture, CPanel* pImageProcPanel) : CPanel(hWnd, pNotifyMouseCapture) {
	m_pImageProcPanel = pImageProcPanel;
	m_clientRect = CRect(0, 0, 0, 0);
	m_fDPIScale *= CSettingsProvider::This().ScaleFactorNavPanel();
	m_nWidth = 0;
	m_nHeight = (int)(WNDBUTTON_PANEL_HEIGHT*m_fDPIScale);

	CButtonCtrl* pInfoBtn = AddUserPaintButton(ID_btnEXIFInfo, CNLS::GetString(_T("Display image (EXIF) information")), &PaintInfoBtn);
	pInfoBtn->SetExtendedActiveArea(CRect(-2, -2, 0, 0));
}

CRect CInfoButtonPanel::PanelRect() {
	if (m_nWidth == 0) {
		int nButtonSize = m_nHeight - 4 * WNDBUTTON_BORDER;
		m_nWidth = 4 * WNDBUTTON_BORDER + (m_controls.size() - 1) * WNDBUTTON_BORDER * 3 + m_controls.size() * nButtonSize;
	}

	CRect sliderRect = m_pImageProcPanel->PanelRect();
	m_clientRect = CRect(CPoint(sliderRect.left, 0), CSize(m_nWidth, m_nHeight));
	return m_clientRect;
}

void CInfoButtonPanel::RequestRepositioning() {
	RepositionAll();
}

void CInfoButtonPanel::RepositionAll() {
	m_clientRect = PanelRect();
	int nButtonSize = m_nHeight - 4 * WNDBUTTON_BORDER;
	int nStartX = m_clientRect.left + WNDBUTTON_BORDER * 2;
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

void CInfoButtonPanel::PaintInfoBtn(void* pContext, const CRect& rect, CDC& dc) {
	CFont font;
	font.CreatePointFont(min(rect.Width() * 6, 160), _T("Courier New"), dc, true, true);
	HFONT oldFont = dc.SelectFont(font);
	dc.DrawText(_T("i"), 1, (LPRECT)&rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
	dc.SelectFont(oldFont);
}
