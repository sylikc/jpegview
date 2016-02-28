#include "StdAfx.h"
#include "UnsharpMaskPanel.h"
#include "ImageProcessingPanel.h"
#include "Helpers.h"
#include "ProcessParams.h"
#include "NLS.h"

#define PANEL_BORDER 12
#define PANEL_GAPTITLE 10
#define USM_PANEL_ADDX 40
#define USM_PANEL_GAPSLIDER 7
#define USM_PANEL_GAPBUTTONY 4
#define USM_PANEL_GAPBUTTONX 12

/////////////////////////////////////////////////////////////////////////////////////////////
// CUnsharpMaskPanel
/////////////////////////////////////////////////////////////////////////////////////////////

CUnsharpMaskPanel::CUnsharpMaskPanel(HWND hWnd, INotifiyMouseCapture* pNotifyMouseCapture, 
									 CPanel* pImageProcPanel, CUnsharpMaskParams* pParams) : CPanel(hWnd, pNotifyMouseCapture, true) {
	m_pImageProcPanel = pImageProcPanel;
	m_clientRect = CRect(0, 0, 0, 0);
	m_nWidth = 0;
	m_nHeight = 0;
	m_nMaxSliderWidth = 0;

	CTextCtrl* pTextUSM = AddText(ID_txtTitle, CNLS::GetString(_T("Apply Unsharp Mask")), false);
	pTextUSM->SetBold(true);
	AddSlider(ID_slRadius, CNLS::GetString(_T("Radius")), &(pParams->Radius), NULL, 0.0, 5.0, 1.0, true, false, false, 200);
	AddSlider(ID_slAmount, CNLS::GetString(_T("Amount")), &(pParams->Amount), NULL, 0.0, 10.0, 0.0, true, false, false, 200);
	AddSlider(ID_slThreshold, CNLS::GetString(_T("Threshold")), &(pParams->Threshold), NULL, 0, 20, 4.0, true, false, false, 200);
	AddButton(ID_btnCancel, CNLS::GetString(_T("Cancel")));
	AddButton(ID_btnApply, CNLS::GetString(_T("Apply")));
}

CRect CUnsharpMaskPanel::PanelRect() {
	if (m_nWidth == 0) {
		m_nHeight = 0;
		int nLastMinSize = 0;
		ControlsIterator iter;
		for (iter = m_controls.begin( ); iter != m_controls.end( ); iter++ ) {
			CSize minSize = iter->second->GetMinSize();
			m_nWidth = max(m_nWidth, minSize.cx);
			m_nHeight += minSize.cy;
			nLastMinSize = minSize.cy;
			if (dynamic_cast<CSliderDouble*>(iter->second) != NULL) {
				m_nMaxSliderWidth = max(m_nMaxSliderWidth, minSize.cx);
			}
		}
		m_nHeight -= nLastMinSize; // last button height added twice - subtract again one
		m_nWidth += (int)(m_fDPIScale*(2*PANEL_BORDER + USM_PANEL_ADDX));
		m_nHeight += (int)(m_fDPIScale*(2*PANEL_BORDER + PANEL_GAPTITLE + 3*USM_PANEL_GAPSLIDER + USM_PANEL_GAPBUTTONY));
	}
	CRect sliderRect = m_pImageProcPanel->PanelRect();
	m_clientRect = CRect(CPoint((sliderRect.right + sliderRect.left) / 2 - m_nWidth / 2, sliderRect.bottom - m_nHeight - 30),
		CSize(m_nWidth, m_nHeight));
	return m_clientRect;
}

void CUnsharpMaskPanel::RequestRepositioning() {
	m_nWidth = 0;
}

void CUnsharpMaskPanel::RepositionAll() {
	PanelRect();
	int nOffset = (int)(m_fDPIScale*PANEL_BORDER);
	int nStartX = m_clientRect.left + nOffset;
	int nStartY = m_clientRect.top + nOffset;
	bool bStartButtons = true;
	ControlsIterator iter;
	for (iter = m_controls.begin( ); iter != m_controls.end( ); iter++ ) {
		CTextCtrl* pText = dynamic_cast<CTextCtrl*>(iter->second);
		if (pText != NULL) {
			pText->SetPosition(CRect(CPoint(nStartX, nStartY), pText->GetMinSize()));
			nStartY += pText->GetMinSize().cy;
			nStartY += (int)(m_fDPIScale*PANEL_GAPTITLE);
		} else {
			CSliderDouble* pSlider = dynamic_cast<CSliderDouble*>(iter->second);
			if (pSlider != NULL) {
				pSlider->SetPosition(CRect(CPoint((m_clientRect.left + m_clientRect.right)/2 - m_nMaxSliderWidth/2, nStartY), 
					CSize(m_nMaxSliderWidth, pSlider->GetMinSize().cy)));
				nStartY += pSlider->GetMinSize().cy;
				nStartY += (int)(m_fDPIScale*USM_PANEL_GAPSLIDER);
			} else {
				CButtonCtrl* pButton = dynamic_cast<CButtonCtrl*>(iter->second);
				if (pButton != NULL) {
					if (bStartButtons) {
						nStartX = m_clientRect.right - (int)(m_fDPIScale*PANEL_BORDER);
						nStartY += (int)(m_fDPIScale*USM_PANEL_GAPBUTTONY);
					}
					pButton->SetPosition(CRect(CPoint(nStartX - pButton->GetMinSize().cx, nStartY), pButton->GetMinSize()));
					nStartX -= pButton->GetMinSize().cx;
					nStartX -= (int)(m_fDPIScale*USM_PANEL_GAPBUTTONX);
					bStartButtons = false;
				}
			}
		}
	}
}
