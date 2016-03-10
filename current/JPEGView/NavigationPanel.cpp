#include "StdAfx.h"
#include "NavigationPanel.h"
#include "ImageProcessingPanel.h"
#include "Helpers.h"
#include "HelpersGUI.h"
#include "SettingsProvider.h"
#include "NLS.h"
#include "KeyMap.h"
#include "resource.h"

#define NAV_PANEL_HEIGHT 32
#define NAV_PANEL_BORDER 6
#define NAV_PANEL_GAP 5

/////////////////////////////////////////////////////////////////////////////////////////////
// CNavigationPanel
/////////////////////////////////////////////////////////////////////////////////////////////

CNavigationPanel::CNavigationPanel(HWND hWnd, INotifiyMouseCapture* pNotifyMouseCapture, CPanel* pImageProcPanel, CKeyMap* keyMap, bool* pFullScreenMode,
		DecisionMethod* isCurrentImageFitToScreen, void* pDecisionMethodParam) : CPanel(hWnd, pNotifyMouseCapture) {
	m_pImageProcPanel = pImageProcPanel;
	m_clientRect = CRect(0, 0, 0, 0);
	m_fDPIScale *= CSettingsProvider::This().ScaleFactorNavPanel();
	SetScaledWidth(1.0f);
	m_isCurrentImageFitToScreen = isCurrentImageFitToScreen;
	m_pDecisionMethodParam = pDecisionMethodParam;
	m_keyMap = keyMap;
	m_pFullScreenMode = pFullScreenMode;

	AddUserPaintButton(ID_btnHome, GetTooltip(keyMap, _T("Show first image in folder"), IDM_FIRST), &PaintHomeBtn);
	AddUserPaintButton(ID_btnPrev, GetTooltip(keyMap, _T("Show previous image"), IDM_PREV), &PaintPrevBtn);
	AddGap(ID_gap1, 4);
	AddUserPaintButton(ID_btnNext, GetTooltip(keyMap, _T("Show next image"), IDM_NEXT), &PaintNextBtn);
	AddUserPaintButton(ID_btnEnd, GetTooltip(keyMap, _T("Show last image in folder"), IDM_LAST), &PaintEndBtn);
	AddGap(ID_gap2, 8);
	if (CSettingsProvider::This().AllowFileDeletion()) {
		AddUserPaintButton(ID_btnDelete, GetTooltip(keyMap, _T("Delete image file"),
			keyMap->GetKeyStringForCommand(IDM_MOVE_TO_RECYCLE_BIN).IsEmpty() ? IDM_MOVE_TO_RECYCLE_BIN_CONFIRM : IDM_MOVE_TO_RECYCLE_BIN), &PaintDeleteBtn); 
		AddGap(ID_gap3, 8);
	}
	AddUserPaintButton(ID_btnZoomMode, GetTooltip(keyMap, _T("Zoom mode (drag mouse to zoom)"), IDM_ZOOM_MODE), &PaintZoomModeBtn);
	AddUserPaintButton(ID_btnFitToScreen, &ZoomFitToggleTooltip, &PaintZoomFitToggleBtn, NULL, this);
	AddUserPaintButton(ID_btnWindowMode, &WindowModeTooltip, &PaintWindowModeBtn, NULL, this);
	AddGap(ID_gap4, 8);
	AddUserPaintButton(ID_btnRotateCW, GetTooltip(keyMap, _T("Rotate image 90 deg clockwise"), IDM_ROTATE_90), &PaintRotateCWBtn);
	AddUserPaintButton(ID_btnRotateCCW, GetTooltip(keyMap, _T("Rotate image 90 deg counter-clockwise"), IDM_ROTATE_270), &PaintRotateCCWBtn);
	AddUserPaintButton(ID_btnRotateFree, GetTooltip(keyMap, _T("Rotate image by user-defined angle"), IDM_ROTATE), &PaintFreeRotBtn);
	AddUserPaintButton(ID_btnPerspectiveCorrection, GetTooltip(keyMap, _T("Correct converging lines (perspective correction)"), IDM_PERSPECTIVE), &PaintPerspectiveBtn);
	AddGap(ID_gap5, 8);
	AddUserPaintButton(ID_btnKeepParams, GetTooltip(keyMap, _T("Keep processing parameters between images"), IDM_KEEP_PARAMETERS), &PaintKeepParamsBtn);
	AddUserPaintButton(ID_btnLandscapeMode, GetTooltip(keyMap, _T("Landscape picture enhancement mode"), IDM_LANDSCAPE_MODE), &PaintLandscapeModeBtn);
	AddGap(ID_gap6, 16);
	AddUserPaintButton(ID_btnShowInfo, GetTooltip(keyMap, _T("Display image (EXIF) information"), IDM_SHOW_FILEINFO), &PaintInfoBtn);

	m_nOptimalWidth = PanelRect().Width();
}

CString CNavigationPanel::GetTooltip(CKeyMap* keyMap, LPCTSTR tooltip, int command)
{
	CString shortCut = keyMap->GetKeyStringForCommand(command);
	if (shortCut.IsEmpty()) {
		return CString(CNLS::GetString(tooltip));
	}
	CString result;
	result.Format(_T("%s (%s)"), CNLS::GetString(tooltip), shortCut);
	return result;
}

void CNavigationPanel::AddGap(int nID, int nGap) {
	CGapCtrl* pGapCtrl = new CGapCtrl(this, nGap);
	m_controls[nID] = pGapCtrl;
}

CRect CNavigationPanel::PanelRect() {
	if (m_nWidth == 0) {
		int nNumButtons = 0;
		int nTotalGap = 0;
		ControlsIterator iter;
		for (iter = m_controls.begin( ); iter != m_controls.end( ); iter++ ) {
			if (dynamic_cast<CButtonCtrl*>(iter->second) != NULL) {
				nNumButtons++;
			} else {
				CGapCtrl* pGapCtrl = dynamic_cast<CGapCtrl*>(iter->second);
				if (pGapCtrl != NULL) {
					nTotalGap += (int)(pGapCtrl->GapWidth()*m_fDPIScale*m_fAdditionalScale);
				}
			}
		}
		int nButtonSize = m_nHeight - m_nBorder;
		m_nWidth = m_nBorder * 2 + (nNumButtons - 1) * m_nGap + nNumButtons * nButtonSize + nTotalGap;
	}

	CRect sliderRect = m_pImageProcPanel->PanelRect();
	m_clientRect = CRect(CPoint((sliderRect.right + sliderRect.left - m_nWidth)/2, 
		((sliderRect.Width() < HelpersGUI::ScaleToScreen(800) || !CSettingsProvider::This().ShowBottomPanel()) ? (sliderRect.bottom - HelpersGUI::ScaleToScreen(26)) : sliderRect.top) - m_nHeight),
		CSize(m_nWidth, m_nHeight));
	return m_clientRect;
}

bool CNavigationPanel::AdjustMaximalWidth(int nMaxWidth) {
	int nUsedWidth = PanelRect().Width();
	if (nUsedWidth > nMaxWidth || (nUsedWidth < m_nOptimalWidth && nUsedWidth < nMaxWidth)) {
		float fScale = (float)min(nMaxWidth, m_nOptimalWidth)/m_nOptimalWidth;
		SetScaledWidth(fScale);
		RequestRepositioning();
		return true;
	}
	return false;
}

void CNavigationPanel::RequestRepositioning() {
	RepositionAll();
}

void CNavigationPanel::RepositionAll() {
	m_clientRect = PanelRect();
	int nButtonSize = m_nHeight - m_nBorder;
	int nStartX = m_clientRect.left + m_nBorder;
	int nStartY = m_clientRect.top + m_nBorder/2;
	ControlsIterator iter;
	for (iter = m_controls.begin( ); iter != m_controls.end( ); iter++ ) {
		CButtonCtrl* pButton = dynamic_cast<CButtonCtrl*>(iter->second);
		if (pButton != NULL) {
			pButton->SetPosition(CRect(CPoint(nStartX, nStartY), CSize(nButtonSize, nButtonSize)));
			nStartX += nButtonSize + m_nGap;
		} else {
			CGapCtrl* pGap = dynamic_cast<CGapCtrl*>(iter->second);
			if (pGap != NULL) {
				nStartX += (int)(pGap->GapWidth()*m_fDPIScale*m_fAdditionalScale);
			}
		}
	}
}

void CNavigationPanel::SetScaledWidth(float fScale) {
	m_fAdditionalScale = fScale;
	m_nWidth = 0;
	m_nHeight = (int)(NAV_PANEL_HEIGHT*m_fDPIScale*fScale);
	m_nBorder = (int)(NAV_PANEL_BORDER*m_fDPIScale*fScale);
	m_nGap = (int)(NAV_PANEL_GAP*m_fDPIScale*fScale);
}

void CNavigationPanel::PaintHomeBtn(void* pContext, const CRect& rect, CDC& dc) {
	CRect r = Helpers::InflateRect(rect, 0.3f);
	
	dc.MoveTo(r.left, r.top);
	dc.LineTo(r.left, r.bottom);

	int nX = r.left + (int)(r.Width()*0.4f);

	dc.MoveTo(nX, r.top);
	dc.LineTo(nX, r.bottom);

	int nW = r.Height() / 2;
	dc.MoveTo(r.right, r.top+1);
	dc.LineTo(r.right - nW + 1, r.top + nW);
	dc.LineTo(r.right+1, r.bottom);
}

void CNavigationPanel::PaintPrevBtn(void* pContext, const CRect& rect, CDC& dc) {
	CRect r = Helpers::InflateRect(rect, 0.3f);

	int nGapX = (int)(r.Width()*0.2f);
	int nX = r.left + nGapX;

	dc.MoveTo(nX, r.top);
	dc.LineTo(nX, r.bottom);

	int nX2 = r.right - nGapX;
	int nW = r.Height() / 2;
	dc.MoveTo(nX2, r.top+1);
	dc.LineTo(nX2 - nW + 1, r.top + nW);
	dc.LineTo(nX2+1, r.bottom);
}

void CNavigationPanel::PaintNextBtn(void* pContext, const CRect& rect, CDC& dc) {
	CRect r = Helpers::InflateRect(rect, 0.3f);

	int nGapX = (int)(r.Width()*0.2f);
	int nX = r.left + nGapX - 1;
	int nX2 = r.right - nGapX;
	int nW = r.Height() / 2;

	dc.MoveTo(nX+1, r.top+1);
	dc.LineTo(nX + nW, r.top + nW);
	dc.LineTo(nX, r.bottom);

	dc.MoveTo(nX2, r.top);
	dc.LineTo(nX2, r.bottom);
}

void CNavigationPanel::PaintEndBtn(void* pContext, const CRect& rect, CDC& dc) {
	CRect r = Helpers::InflateRect(rect, 0.3f);

	int nW = r.Height() / 2;
	dc.MoveTo(r.left, r.top+1);
	dc.LineTo(r.left + nW - 1, r.top + nW);
	dc.LineTo(r.left - 1, r.bottom);

	int nX2 = r.right - (int)(r.Width()*0.4f);

	dc.MoveTo(nX2, r.top);
	dc.LineTo(nX2, r.bottom);

	dc.MoveTo(r.right, r.top);
	dc.LineTo(r.right, r.bottom);
}

void CNavigationPanel::PaintDeleteBtn(void* pContext, const CRect& rect, CDC& dc) {
	CRect r = Helpers::InflateRect(rect, 0.3f);

	int size = r.right - r.left;
	dc.MoveTo(r.left + 1, r.top + 1);
	dc.LineTo(r.left + size, r.top + size);

	dc.MoveTo(r.left + 1, r.top + size - 1);
	dc.LineTo(r.left + size , r.top);
}

void CNavigationPanel::PaintZoomModeBtn(void* pContext, const CRect& rect, CDC& dc) {
	CRect r = Helpers::InflateRect(rect, 0.25f);

	int nW = (int)(r.Width() * 0.8f);
	if ((nW & 1) == 0) nW++;
	dc.Ellipse(r.left, r.top, r.left + nW, r.top + nW);

	int nZx = r.left + nW / 2;
	int nZy = r.top + nW / 2;
	int nWw = (int)(nW * 0.3f);
	dc.MoveTo(nZx - nWw + 1, nZy);
	dc.LineTo(nZx + nWw, nZy);
	dc.MoveTo(nZx, nZy - nWw + 1);
	dc.LineTo(nZx, nZy + nWw);

	int nO = (int)(nW * 0.35f);
	int nL = (int)(nW * 0.4f);
	int nLL = (int)(nW * 0.15f);
	int nSx = nZx + nO - nLL / 2;
	int nSy = nZy + nO + nLL / 2;
	dc.MoveTo(nSx, nSy);
	dc.LineTo(nSx + nL, nSy+ nL);
	dc.LineTo(nSx + nL + nLL, nSy + nL - nLL);
	dc.LineTo(nSx + nLL - 1, nSy - nLL - 1);
}

void CNavigationPanel::PaintZoomToFitBtn(void* pContext, const CRect& rect, CDC& dc) {
	CRect r = Helpers::InflateRect(rect, 0.25f);

	int nW = r.Width()/3;
	int nD = r.Width()/2 - 1;

	for (int i = 0; i < 4; i++) {
		int nX = (i < 2) ? r.left : r.right;
		int nXP = (i < 2) ? nW : -nW;
		int nXD = (i < 2) ? nD : -nD;
		int nXA = (i < 2) ? 1 : -1;
		int nY = (i & 1) ? r.bottom : r.top;
		int nYP = (i & 1) ? -nW : nW;
		int nYD = (i & 1) ? -nD : nD;
		dc.MoveTo(nX, nY+nYP);
		dc.LineTo(nX, nY);
		dc.LineTo(nX+nXP+nXA, nY);
		dc.MoveTo(nX, nY);
		dc.LineTo(nX+nXD, nY+nYD);
	}
}

void CNavigationPanel::PaintZoomTo1to1Btn(void* pContext, const CRect& rect, CDC& dc) {
	CFont font;
	font.CreatePointFont(min(rect.Width() * 3, 80), _T("Microsoft Sans Serif"), dc, true, false);
	HFONT oldFont = dc.SelectFont(font);
	dc.DrawText(_T("1:1"), 3, (LPRECT)&rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
	dc.SelectFont(oldFont);
}

void CNavigationPanel::PaintZoomFitToggleBtn(void* pContext, const CRect& rect, CDC& dc) {
	CNavigationPanel* pNavPanel = (CNavigationPanel*)pContext;
	if (pNavPanel->m_isCurrentImageFitToScreen(pNavPanel->m_pDecisionMethodParam)) {
		PaintZoomTo1to1Btn(pContext, rect, dc);
	} else {
		PaintZoomToFitBtn(pContext, rect, dc);
	}
}

void CNavigationPanel::PaintWindowModeBtn(void* pContext, const CRect& rect, CDC& dc) {
	CRect r = Helpers::InflateRect(rect, 0.25f);

	dc.SelectStockBrush(HOLLOW_BRUSH);
	dc.Rectangle(&r);
	int nTop = r.top + r.Height()/4;
	dc.MoveTo(r.left + 1, nTop);
	dc.LineTo(r.right, nTop);
}

void CNavigationPanel::PaintRotateCWBtn(void* pContext, const CRect& rect, CDC& dc) {
	CRect r = Helpers::InflateRect(rect, 0.3f);

	int nX = r.left + (int)(r.Width()*0.65f);

	dc.MoveTo(r.left-2, r.bottom);
	dc.LineTo(nX, r.bottom);
	dc.LineTo(nX, r.bottom - (int)(r.Height()*0.4f));
	dc.LineTo(r.left-2, r.bottom);

	dc.MoveTo(nX+2, r.bottom);
	dc.LineTo(r.right+2, r.bottom);
	dc.LineTo(nX+2, r.top);
	dc.LineTo(nX+2, r.bottom);
}

void CNavigationPanel::PaintRotateCCWBtn(void* pContext, const CRect& rect, CDC& dc) {
	CRect r = Helpers::InflateRect(rect, 0.3f);

	int nX = r.left + (int)(r.Width()*0.33f);

	dc.MoveTo(r.left-2, r.bottom);
	dc.LineTo(nX, r.bottom);
	dc.LineTo(nX, r.top);
	dc.LineTo(r.left-2, r.bottom);

	dc.MoveTo(nX+2, r.bottom);
	dc.LineTo(r.right+2, r.bottom);
	dc.LineTo(nX+2, r.bottom - (int)(r.Height()*0.4f));
	dc.LineTo(nX+2, r.bottom);
}

void CNavigationPanel::PaintFreeRotBtn(void* pContext, const CRect& rect, CDC& dc) {
	CRect r = Helpers::InflateRect(rect, 0.32f);
	r.OffsetRect(0, -1);

	int nArrowStart = (int)(r.Width()*0.6f);
	int nArrowLen = (int)(nArrowStart*0.68f);
	int nX = (r.right + r.left) >> 1;
	dc.Arc(r.left, r.top, r.right, r.bottom, nX, r.bottom, nX - 2, r.top);
	dc.MoveTo(nX, r.bottom - 1);
	dc.LineTo(nX - nArrowStart, r.bottom - 1);
	dc.MoveTo(nX - nArrowStart, r.bottom - 1);
	dc.LineTo(nX - nArrowStart + nArrowLen, r.bottom - 1 + nArrowLen);
	dc.MoveTo(nX - nArrowStart, r.bottom - 1);
	dc.LineTo(nX - nArrowStart + nArrowLen, r.bottom - 1 - nArrowLen);
}

void CNavigationPanel::PaintPerspectiveBtn(void* pContext, const CRect& rect, CDC& dc) {
	CRect r = Helpers::InflateRect(rect, 0.3f);

	int nW = (int)(r.Width() * 0.15f);
	int nW2 = (int)(r.Width() * 0.4f);
	dc.MoveTo(r.left + nW, r.bottom);
	dc.LineTo(r.right - nW, r.bottom);
	dc.LineTo(r.right - nW2, r.top);
	dc.LineTo(r.left + nW2, r.top);
	dc.LineTo(r.left + nW, r.bottom);
}

void CNavigationPanel::PaintKeepParamsBtn(void* pContext, const CRect& rect, CDC& dc) {
	CRect r = Helpers::InflateRect(rect, 0.25f);
	int nX = (int)(r.Width()*0.28f);
	int nY = r.bottom - (int)(r.Height()*0.25f);
	int nXM = (r.left + r.right) >> 1;

	dc.SelectStockBrush(HOLLOW_BRUSH);
	dc.Rectangle(r.left + nX, r.top, r.right - nX, nY);
	dc.MoveTo(r.left + nX, nY - nX);
	dc.LineTo(r.right - nX, nY - nX);
	dc.MoveTo(nXM, nY);
	dc.LineTo(nXM, r.bottom + 1);
}

void CNavigationPanel::PaintLandscapeModeBtn(void* pContext, const CRect& rect, CDC& dc) {
	CRect r = Helpers::InflateRect(rect, 0.3f);

	CPoint p1(r.left-1, r.bottom);
	dc.MoveTo(p1);
	CPoint p2(r.left + (int)(r.Width()*0.25f), r.top + (int)(r.Height()*0.1f));
	dc.LineTo(p2);
	CPoint p3(r.left + (int)(r.Width()*0.5f), r.top + (int)(r.Height()*0.8f));
	dc.LineTo(p3);
	dc.LineTo(r.left + (int)(r.Width()*0.75f), r.top + (int)(r.Height()*0.5f));
	dc.LineTo(r.right+1, r.bottom);
	dc.LineTo(r.left-1, r.bottom);

	int nRad = (int)(r.Width()*0.25f + 0.5f);
	dc.Ellipse(r.right - nRad - 1, r.top - nRad + 1, r.right + nRad - 1, r.top + nRad + 1);
}

void CNavigationPanel::PaintInfoBtn(void* pContext, const CRect& rect, CDC& dc) {
	CFont font;
	font.CreatePointFont(min(rect.Width() * 6, 160), _T("Times New Roman"), dc, false, true);
	HFONT oldFont = dc.SelectFont(font);
	dc.DrawText(_T("i"), 1, (LPRECT)&rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
	dc.SelectFont(oldFont);
}

static CString staticTooltip;

LPCTSTR CNavigationPanel::WindowModeTooltip(void* pContext) {
	CNavigationPanel* pNavPanel = (CNavigationPanel*)pContext;
	if (*(pNavPanel->m_pFullScreenMode)) {
		staticTooltip = pNavPanel->GetTooltip(pNavPanel->m_keyMap, _T("Window mode"), IDM_FULL_SCREEN_MODE);
	} else {
		staticTooltip = pNavPanel->GetTooltip(pNavPanel->m_keyMap, _T("Full screen mode"), IDM_FULL_SCREEN_MODE);
	}
	return staticTooltip;
}

LPCTSTR CNavigationPanel::ZoomFitToggleTooltip(void* pContext) {
	CNavigationPanel* pNavPanel = (CNavigationPanel*)pContext;
	if (pNavPanel->m_isCurrentImageFitToScreen(pNavPanel->m_pDecisionMethodParam)) {
		staticTooltip = pNavPanel->GetTooltip(pNavPanel->m_keyMap, _T("Actual size of image"), IDM_TOGGLE_FIT_TO_SCREEN_100_PERCENTS);
	} else {
		staticTooltip = pNavPanel->GetTooltip(pNavPanel->m_keyMap, _T("Fit image to screen"), IDM_TOGGLE_FIT_TO_SCREEN_100_PERCENTS);
	}
	return staticTooltip;
}