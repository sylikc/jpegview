#include "StdAfx.h"
#include "ImageProcessingPanel.h"
#include "Helpers.h"
#include "ProcessParams.h"
#include "NLS.h"

#define SLIDER_WIDTH 130
#define MIN_SLIDER_WIDTH 40
#define SLIDER_HEIGHT 30
#define WIDTH_ADD_PIXELS 63
#define SLIDER_GAP 23
#define SCREEN_Y_OFFSET 5

/////////////////////////////////////////////////////////////////////////////////////////////
// static helper functions
/////////////////////////////////////////////////////////////////////////////////////////////

// Gets the screen size of the screen containing the point with given virtual desktop coordinates
static CSize GetScreenSize(int nX, int nY) {
	HMONITOR hMonitor = ::MonitorFromPoint(CPoint(nX, nY), MONITOR_DEFAULTTONEAREST);
	MONITORINFO monitorInfo;
	monitorInfo.cbSize = sizeof(MONITORINFO);
	if (::GetMonitorInfo(hMonitor, &monitorInfo)) {
		return CSize(monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top);
	} else {
		return CSize(::GetSystemMetrics(SM_CXSCREEN), ::GetSystemMetrics(SM_CYSCREEN));
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////
// CImageProcessingPanel
/////////////////////////////////////////////////////////////////////////////////////////////

CImageProcessingPanel::CImageProcessingPanel(HWND hWnd, INotifiyMouseCapture* pNotifyMouseCapture, CImageProcessingParams* pParams,
											 bool* pEnableLDC, bool* pEnableContrastCorr) : CPanel(hWnd, pNotifyMouseCapture) {
	// slider width is smaller for very small screens
	::GetClientRect(hWnd, &m_clientRect);
	DetermineSliderWidth();
	m_nSliderHeight = (int)(SLIDER_HEIGHT*m_fDPIScale);
	m_nSliderGap = (int) (SLIDER_GAP*m_fDPIScale);
	m_nTotalAreaHeight = m_nSliderHeight*3 + SCREEN_Y_OFFSET;
	m_sliderAreaRect = CRect(0, 0, 0, 0);

	m_nYStart = 0xFFFF;
	m_nTotalSliders = 0;

	AddSlider(ID_slContrast, CNLS::GetString(_T("Contrast")), &(pParams->Contrast), NULL, -0.5, 0.5, 0.0, true, false, false);
	AddSlider(ID_slBrightness, CNLS::GetString(_T("Brightness")), &(pParams->Gamma), NULL, 0.5, 2.0, 1.0, true, true, true);
	AddSlider(ID_slSaturation, CNLS::GetString(_T("Saturation")), &(pParams->Saturation), NULL, 0.0, 2.0, 1.0, true, false, false);
	AddSlider(ID_slCR, CNLS::GetString(_T("C - R")), &(pParams->CyanRed), NULL, -1.0, 1.0, 0.0, true, false, false);
	AddSlider(ID_slMG, CNLS::GetString(_T("M - G")), &(pParams->MagentaGreen), NULL, -1.0, 1.0, 0.0, true, false, false);
	AddSlider(ID_slYB, CNLS::GetString(_T("Y - B")), &(pParams->YellowBlue), NULL, -1.0, 1.0, 0.0, true, false, false);
	AddSlider(ID_slLightenShadows, CNLS::GetString(_T("Lighten Shadows")), &(pParams->LightenShadows), pEnableLDC, 0.0, 1.0, 0.0, true, false, false);
	AddSlider(ID_slDarkenHighlights, CNLS::GetString(_T("Darken Highlights")), &(pParams->DarkenHighlights), pEnableLDC, 0.0, 1.0, 0.0, true, false, false);
	AddSlider(ID_slDeepShadows, CNLS::GetString(_T("Deep Shadows")), &(pParams->LightenShadowSteepness), pEnableLDC, 0.0, 1.0, 0.0, true, false, false);
	AddSlider(ID_slColorCorr, CNLS::GetString(_T("Color Correction")), &(pParams->ColorCorrectionFactor), pEnableContrastCorr, -0.5, 0.5, 0.0, true, false, false);
	AddSlider(ID_slContrastCorr, CNLS::GetString(_T("Contrast Correction")), &(pParams->ContrastCorrectionFactor), pEnableContrastCorr, 0.0, 1.0, 0.0, true, false, false);
	AddSlider(ID_slSharpen, CNLS::GetString(_T("Sharpen")), &(pParams->Sharpen), NULL, 0.0, 0.5, 0.0, false, false, false);
	
	AddButton(ID_btnUnsharpMask, CNLS::GetString(_T("Unsharp mask...")));
	AddText(ID_txtParamDB, CNLS::GetString(_T("Parameter DB:")), false);
	AddButton(ID_btnSaveTo, CNLS::GetString(_T("Save to")));
	AddButton(ID_btnRemoveFrom, CNLS::GetString(_T("Remove from")));
	AddText(ID_txtRename, CNLS::GetString(_T("Rename:")), false);
	AddText(ID_txtFilename, NULL, true);
	CTextCtrl* pAcqDate = AddText(ID_txtAcqDate, NULL, false);
	pAcqDate->SetRightAligned(true);
}

void CImageProcessingPanel::DetermineSliderWidth() {
	m_nSliderWidth = (int)(SLIDER_WIDTH*m_fDPIScale);
	int nNeededSpace = (int)(1200*m_fDPIScale);
	if (m_clientRect.Width() < nNeededSpace) {
		int nNeededGain = nNeededSpace - m_clientRect.Width();
		m_nSliderWidth = m_nSliderWidth - nNeededGain/4;
		m_nSliderWidth = max(MIN_SLIDER_WIDTH, m_nSliderWidth);
	}

	m_nNoLabelWidth = m_nSliderWidth + (int)(WIDTH_ADD_PIXELS*m_fDPIScale);
}

CRect CImageProcessingPanel::PanelRect() {
	CRect clientRect;
	::GetClientRect(m_hWnd, &clientRect);

	// Check if client rectangle unchanged - no repositioning needed
	if (m_clientRect == clientRect) {
		return m_sliderAreaRect;
	}
	m_clientRect = clientRect;

	CRect wndRect;
	::GetWindowRect(m_hWnd, &wndRect);
	int nYStart = clientRect.Height() - m_nTotalAreaHeight;
	int nXStart = 0;

	// Place on second monitor if its height is larger
	CSize sizeScreen = GetScreenSize(wndRect.left + 10, wndRect.top + nYStart);
	if (sizeScreen.cy < clientRect.Height() - 5) {
		CSize sizeScreen2 = GetScreenSize(wndRect.left + sizeScreen.cx + 10, wndRect.top + nYStart);
		if (sizeScreen2.cy > sizeScreen.cy) {
			nXStart += sizeScreen.cx;
			sizeScreen = sizeScreen2;
		}
	}

	// Get the new width of the sliders and modify all existing sliders
	DetermineSliderWidth();
	ControlsIterator iter;
	for (iter = m_controls.begin( ); iter != m_controls.end( ); iter++ ) {
		CSliderDouble* pSlider = dynamic_cast<CSliderDouble*>(iter->second);
		if (pSlider != NULL) {
			pSlider->SetSliderLen(m_nSliderWidth);
		}
	}

	m_sliderAreaRect = CRect(nXStart, nYStart, min(clientRect.Width(), sizeScreen.cx) + nXStart, nYStart + m_nTotalAreaHeight);
	return m_sliderAreaRect;
}

void CImageProcessingPanel::AddSlider(int nID, LPCTSTR sName, double* pdValue, bool* pbEnable, 
						   double dMin, double dMax, double dDefaultValue, bool bAllowPreviewAndReset, bool bLogarithmic, bool bInvert) {
	CPanel::AddSlider(nID, sName, pdValue, pbEnable, dMin, dMax, dDefaultValue, bAllowPreviewAndReset, bLogarithmic, bInvert, m_nSliderWidth);
	m_nTotalSliders++;
}

void CImageProcessingPanel::RepositionAll() {
	const int X_OFFSET = 5;

	CRect sliderAreaRect = PanelRect();
	int nXStart = sliderAreaRect.left + X_OFFSET;
	int nYStart = sliderAreaRect.top;

	// Place at bottom of window, grouping sliders in three rows and n-columns,
	// add remaining controls afterwards
	
	m_nYStart = nYStart;
	int nXLimitScreen = sliderAreaRect.right;
	int nX = nXStart, nY = nYStart + 2*m_nSliderHeight;
	int nSliderIdx = 0;
	int nTotalSliderIdx = 0;
	int nSliderEndX = 0;
	int nXButtonStart, nXButton2Start;
	CSliderDouble* pSliderColumn[3];
	ControlsIterator iter;
	for (iter = m_controls.begin( ); iter != m_controls.end( ); iter++ ) {
		CSliderDouble* pSlider = dynamic_cast<CSliderDouble*>(iter->second);
		if (pSlider == NULL) {
			continue;
		}
		nTotalSliderIdx++;
		pSliderColumn[nSliderIdx] = pSlider;
		nSliderIdx++;
		if (nSliderIdx == 3 || nTotalSliderIdx >= m_nTotalSliders - 1) {
			int nTotalWidth = 0;
			for (int i = 0; i < nSliderIdx; i++) {
				nTotalWidth = max(nTotalWidth, pSliderColumn[i]->GetNameLabelWidth());
			}
			nTotalWidth += m_nNoLabelWidth;
			nSliderEndX = nX + nTotalWidth;
			// the last slider is 'Sharpen', only show if we have enough space
			if (nTotalSliderIdx == m_nTotalSliders) {
				bool bShowSharpen = nXLimitScreen - nSliderEndX > (int)(150*m_fDPIScale);
				pSlider->SetShow(bShowSharpen, false);
				if (!bShowSharpen) nSliderEndX -= nTotalWidth;
			}
			for (int i = 0; i < nSliderIdx; i++) {
				pSliderColumn[i]->SetPosition(CRect(nX, nYStart + i*m_nSliderHeight, nX + nTotalWidth - m_nSliderGap, nYStart + (i+1)*m_nSliderHeight));
			}
			if (nTotalSliderIdx == m_nTotalSliders - 1) {
				nXButtonStart = nX;
			}
			if (nTotalSliderIdx == m_nTotalSliders) {
				nXButton2Start = nX;
				nX = nXButtonStart;
				nY = nYStart + 2*m_nSliderHeight;
			} else {
				nX += nTotalWidth;
				nSliderIdx = 0;
			}
		}
	}
	// layout other controls
	CTextCtrl* txtAcqDate = GetTextAcqDate();
	CButtonCtrl* btnUnsharpMask = GetBtnUnsharpMask();
	for (iter = m_controls.begin( ); iter != m_controls.end( ); iter++ ) {
		CTextCtrl* pTextCtrl = dynamic_cast<CTextCtrl*>(iter->second);
		if (pTextCtrl != NULL) {
			if (pTextCtrl == txtAcqDate) {
				int nAcqDateWidth = max(0, nXLimitScreen - nSliderEndX - 10);
				txtAcqDate->SetPosition(CRect(nSliderEndX, nYStart, nSliderEndX + nAcqDateWidth, nYStart + m_nSliderHeight));
				txtAcqDate->SetShow(nAcqDateWidth > 30);
				continue;
			}
			int nTextWidth = pTextCtrl->GetTextLabelWidth() + 16;
			// Special behaviour if not enough room - remove label 'Rename'
			if (pTextCtrl == GetTextFilename()) {
				if (nX + nTextWidth > nXLimitScreen) {
					GetTextRename()->SetShow(false, false);
					nX -= GetTextRename()->GetPosition().Width();
				} else {
					GetTextRename()->SetShow(true, false);
				}
			}
			// limit text on screen boundary
			if (nX + nTextWidth >= nXLimitScreen) {
				nTextWidth = nXLimitScreen - nX - 1;
			}
			// only show editable control if large enough
			if (pTextCtrl == GetTextFilename()) {
				pTextCtrl->SetShow(nTextWidth > 40, false);
			} else {
				pTextCtrl->SetShow(nTextWidth > 0, false);
			}
			pTextCtrl->SetPosition(CRect(nX, nY, nX + nTextWidth, nY + m_nSliderHeight));
			pTextCtrl->SetMaxTextWidth(nXLimitScreen - nX);
			nX += nTextWidth;
		} else {
			CButtonCtrl* pButtonCtrl = dynamic_cast<CButtonCtrl*>(iter->second);
			if (pButtonCtrl != NULL) {
				int nButtonWidth = pButtonCtrl->GetMinSize().cx;
				int nButtonHeigth = pButtonCtrl->GetMinSize().cy;
				if (pButtonCtrl == btnUnsharpMask) {
					int nYUSM = nYStart + m_nSliderHeight + ((m_nSliderHeight - nButtonHeigth) >> 1);
					pButtonCtrl->SetPosition(CRect(nXButton2Start, nYUSM, nXButton2Start + nButtonWidth, nYUSM + nButtonHeigth));
					continue;
				}
				int nStartY = nY + ((m_nSliderHeight - nButtonHeigth) >> 1);
				pButtonCtrl->SetPosition(CRect(nX, nStartY, nX + nButtonWidth, nStartY + nButtonHeigth));
				nX += nButtonWidth + 16;
			}
		}
	}
}