#include "StdAfx.h"
#include "RotationPanel.h"
#include "ImageProcessingPanel.h"
#include "Helpers.h"
#include "NLS.h"

#define PANEL_BORDER 12
#define PANEL_GAPTITLE 10
#define ROTATION_PANEL_WIDTH 360
#define ROTATION_PANEL_HEIGHT 100
#define ROTATION_PANEL_BUTTON_SIZE 21
#define ROTATION_PANEL_GAPBUTTONX1 6
#define ROTATION_PANEL_GAPBUTTONX2 12

/////////////////////////////////////////////////////////////////////////////////////////////
// CRotationPanel
/////////////////////////////////////////////////////////////////////////////////////////////

CRotationPanel::CRotationPanel(HWND hWnd, INotifiyMouseCapture* pNotifyMouseCapture, CPanel* pImageProcPanel) : CPanel(hWnd, pNotifyMouseCapture, true) {
	m_pImageProcPanel = pImageProcPanel;
	m_clientRect = CRect(0, 0, 0, 0);
	m_nWidth = (int)(ROTATION_PANEL_WIDTH*m_fDPIScale);
	m_nHeight = (int)(ROTATION_PANEL_HEIGHT*m_fDPIScale);
	m_nButtonSize = (int)(ROTATION_PANEL_BUTTON_SIZE*m_fDPIScale);

	CTextCtrl* pTitle = AddText(ID_txtTitle, CNLS::GetString(_T("Rotate Image")), false);
	pTitle->SetBold(true);
	CTextCtrl* pHint = AddText(ID_txtHint, _T(""), false);
	pHint->SetAllowMultiline(true);
	AddUserPaintButton(ID_btnShowGrid, CNLS::GetString(_T("Show grid lines")), &PaintShowGridBtn);
	AddUserPaintButton(ID_btnAutoCrop, CNLS::GetString(_T("Auto crop rotated image (avoids black border)")), &PaintAutoCropBtn);
	AddUserPaintButton(ID_btnAssistMode, CNLS::GetString(_T("Align image to horizontal or vertical line")), &PaintAssistedModeBtn);
	AddButton(ID_btnCancel, CNLS::GetString(_T("Cancel")));
    AddButton(ID_btnApply, CNLS::GetString(_T("Apply")));
}

CRect CRotationPanel::PanelRect() {
	CRect sliderRect = m_pImageProcPanel->PanelRect();
	m_clientRect = CRect(CPoint((sliderRect.right + sliderRect.left) / 2 - m_nWidth / 2, sliderRect.bottom - m_nHeight - 30),
		CSize(m_nWidth, m_nHeight));
	return m_clientRect;
}

void CRotationPanel::RequestRepositioning() {
}

void CRotationPanel::RepositionAll() {
	PanelRect();
	int nOffset = (int)(m_fDPIScale*PANEL_BORDER);
	int nStartX = m_clientRect.left + nOffset;
	int nStartY = m_clientRect.top + nOffset;
	int nStartYHint = 0;
	bool bFirstToolButton = true;
	bool bFirstTextButton = true;
	ControlsIterator iter;
	for (iter = m_controls.begin( ); iter != m_controls.end( ); iter++ ) {
		CTextCtrl* pText = dynamic_cast<CTextCtrl*>(iter->second);
		if (pText != NULL) {
			if (pText == GetTextTitle()) {
				pText->SetPosition(CRect(CPoint(nStartX, nStartY), pText->GetMinSize()));
				nStartY += pText->GetMinSize().cy;
			} else {
				nStartYHint = nStartY;
			}
		} else {
			CButtonCtrl* pButton = dynamic_cast<CButtonCtrl*>(iter->second);
			if (pButton != NULL) {
				if (pButton->IsUserPaintButton()) {
					if (bFirstToolButton) {
						nStartX = m_clientRect.left + (int)(m_fDPIScale*PANEL_BORDER);
						nStartY = m_clientRect.bottom - (int)(m_fDPIScale*PANEL_BORDER) - m_nButtonSize;
						bFirstToolButton = false;
					}
					pButton->SetPosition(CRect(CPoint(nStartX, nStartY), CSize(m_nButtonSize, m_nButtonSize)));
					nStartX += m_nButtonSize;
					nStartX += (int)(m_fDPIScale*ROTATION_PANEL_GAPBUTTONX1);
				} else {
					if (bFirstTextButton) {
						nStartX = m_clientRect.right - (int)(m_fDPIScale*PANEL_BORDER);
						nStartY = m_clientRect.bottom - (int)(m_fDPIScale*PANEL_BORDER) - pButton->GetMinSize().cy;
						bFirstTextButton = false;
					}
					pButton->SetPosition(CRect(CPoint(nStartX - pButton->GetMinSize().cx, nStartY), pButton->GetMinSize()));
					nStartX -= pButton->GetMinSize().cx;
					nStartX -= (int)(m_fDPIScale*ROTATION_PANEL_GAPBUTTONX2);
				}
			}
		}
	}
	CPoint topLeft(m_clientRect.left + (int)(m_fDPIScale*PANEL_BORDER), nStartYHint + (int)(m_nButtonSize * 0.2f));
	CPoint bottomRight(m_clientRect.right - (int)(m_fDPIScale*PANEL_BORDER),
		m_clientRect.bottom - (int)(m_fDPIScale*PANEL_BORDER) - (int)(m_nButtonSize * 1.2f));
	GetTextHint()->SetPosition(CRect(topLeft, bottomRight));
}

void CRotationPanel::PaintShowGridBtn(void* pContext, const CRect& rect, CDC& dc) {
	CRect r = Helpers::InflateRect(rect, 0.2f);

	int nDelta = r.Width() / 3;
	dc.MoveTo(r.left + nDelta, r.top);
	dc.LineTo(r.left + nDelta, r.bottom);
	dc.MoveTo(r.left + nDelta*2, r.top);
	dc.LineTo(r.left + nDelta*2, r.bottom);
	dc.MoveTo(r.left, r.top + nDelta);
	dc.LineTo(r.right, r.top + nDelta);
	dc.MoveTo(r.left, r.top + nDelta*2);
	dc.LineTo(r.right, r.top + nDelta*2);
	dc.Rectangle(r);
}

void CRotationPanel::PaintAutoCropBtn(void* pContext, const CRect& rect, CDC& dc) {
	CRect r = Helpers::InflateRect(rect, 0.35f);
	dc.Rectangle(r);
	dc.MoveTo(r.left + 1, r.bottom + 2);
	dc.LineTo(r.left - 3, r.top);
	dc.LineTo(r.right - 1, r.top - 3);
	dc.LineTo(r.right + 2, r.bottom - 1);
	dc.LineTo(r.left + 1, r.bottom + 2);
}

void CRotationPanel::PaintAssistedModeBtn(void* pContext, const CRect& rect, CDC& dc) {
	CRect r = Helpers::InflateRect(rect, 0.25f);

	int nAw = r.Width() / 3;
	dc.MoveTo(r.left, r.top);
	dc.LineTo(r.right, r.bottom);
	dc.MoveTo(r.left, r.bottom - nAw);
	dc.LineTo(r.left, r.bottom);
	dc.LineTo(r.left + nAw + 1, r.bottom);
	dc.MoveTo(r.left, r.bottom);
	dc.LineTo(r.left + nAw + 2, r.bottom - nAw - 2);
}
