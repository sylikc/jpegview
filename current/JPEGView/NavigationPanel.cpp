#include "StdAfx.h"
#include "NavigationPanel.h"
#include "ImageProcessingPanel.h"
#include "Helpers.h"
#include "NLS.h"

#define NAV_PANEL_HEIGHT 32
#define NAV_PANEL_BORDER 6
#define NAV_PANEL_GAP 5

/////////////////////////////////////////////////////////////////////////////////////////////
// CNavigationPanel
/////////////////////////////////////////////////////////////////////////////////////////////

CNavigationPanel::CNavigationPanel(HWND hWnd, INotifiyMouseCapture* pNotifyMouseCapture, CPanel* pImageProcPanel, bool* pFullScreenMode,
		DecisionMethod* isCurrentImageFitToScreen, void* pDecisionMethodParam) : CPanel(hWnd, pNotifyMouseCapture) {
	m_pImageProcPanel = pImageProcPanel;
	m_clientRect = CRect(0, 0, 0, 0);
	SetScaledWidth(1.0f);
	m_isCurrentImageFitToScreen = isCurrentImageFitToScreen;
	m_pDecisionMethodParam = pDecisionMethodParam;

	AddUserPaintButton(ID_btnHome, CNLS::GetString(_T("Show first image in folder")), &PaintHomeBtn);
	AddUserPaintButton(ID_btnPrev, CNLS::GetString(_T("Show previous image")), &PaintPrevBtn);
	AddGap(ID_gap1, 4);
	AddUserPaintButton(ID_btnNext, CNLS::GetString(_T("Show next image")), &PaintNextBtn);
	AddUserPaintButton(ID_btnEnd, CNLS::GetString(_T("Show last image in folder")), &PaintEndBtn);
	AddGap(ID_gap2, 8);
	AddUserPaintButton(ID_btnFitToScreen, &ZoomFitToggleTooltip, &PaintZoomFitToggleBtn, NULL, this);
	AddUserPaintButton(ID_btnWindowMode, &WindowModeTooltip, &PaintWindowModeBtn, NULL, pFullScreenMode);
	AddGap(ID_gap3, 8);
	AddUserPaintButton(ID_btnRotateCW, CNLS::GetString(_T("Rotate image 90 deg clockwise")), &PaintRotateCWBtn);
	AddUserPaintButton(ID_btnRotateCCW, CNLS::GetString(_T("Rotate image 90 deg counter-clockwise")), &PaintRotateCCWBtn);
	AddUserPaintButton(ID_btnRotateFree, CNLS::GetString(_T("Rotate image by user-defined angle")), &PaintFreeRotBtn);
	AddGap(ID_gap4, 8);
	AddUserPaintButton(ID_btnKeepParams, CNLS::GetString(_T("Keep processing parameters between images")), &PaintKeepParamsBtn);
	AddUserPaintButton(ID_btnLandscapeMode, CNLS::GetString(_T("Landscape picture enhancement mode")), &PaintLandscapeModeBtn);
	AddGap(ID_gap5, 16);
	AddUserPaintButton(ID_btnShowInfo, CNLS::GetString(_T("Display image (EXIF) information")), &PaintInfoBtn);

	m_nOptimalWidth = PanelRect().Width();
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
					nTotalGap += (int)(pGapCtrl->GapWidth()*m_fDPIScale);
				}
			}
		}
		int nButtonSize = m_nHeight - m_nBorder;
		m_nWidth = m_nBorder * 2 + (nNumButtons - 1) * m_nGap + nNumButtons * nButtonSize + nTotalGap;
	}

	CRect sliderRect = m_pImageProcPanel->PanelRect();
	m_clientRect = CRect(CPoint((sliderRect.right + sliderRect.left - m_nWidth)/2, 
		((sliderRect.Width() < 800) ? (sliderRect.bottom - 26) : sliderRect.top) - m_nHeight),
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
				nStartX += (int)(pGap->GapWidth()*m_fDPIScale);
			}
		}
	}
}

void CNavigationPanel::SetScaledWidth(float fScale) {
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

	dc.MoveTo(r.right, r.top+1);
	dc.LineTo(nX + 3, (r.bottom + r.top)/2);
	dc.LineTo(r.right+1, r.bottom);
}

void CNavigationPanel::PaintPrevBtn(void* pContext, const CRect& rect, CDC& dc) {
	CRect r = Helpers::InflateRect(rect, 0.3f);

	int nX = r.left + (int)(r.Width()*0.2f);

	dc.MoveTo(nX, r.top);
	dc.LineTo(nX, r.bottom);

	int nX2 = nX + r.Height()/2 + 2;
	dc.MoveTo(nX2, r.top+1);
	dc.LineTo(nX + 3, (r.bottom + r.top)/2);
	dc.LineTo(nX2+1, r.bottom);
}

void CNavigationPanel::PaintNextBtn(void* pContext, const CRect& rect, CDC& dc) {
	CRect r = Helpers::InflateRect(rect, 0.3f);

	int nX = r.left + (int)(r.Width()*0.2f);
	int nX2 = nX + r.Height()/2;

	dc.MoveTo(nX+1, r.top+1);
	dc.LineTo(nX2, (r.bottom + r.top)/2);
	dc.LineTo(nX, r.bottom);

	dc.MoveTo(nX2 + 3, r.top);
	dc.LineTo(nX2 + 3, r.bottom);
}

void CNavigationPanel::PaintEndBtn(void* pContext, const CRect& rect, CDC& dc) {
	CRect r = Helpers::InflateRect(rect, 0.3f);

	int nX = r.left + r.Height()/2;

	dc.MoveTo(r.left+1, r.top+1);
	dc.LineTo(nX, (r.bottom + r.top)/2);
	dc.LineTo(r.left, r.bottom);

	dc.MoveTo(nX + 3, r.top);
	dc.LineTo(nX + 3, r.bottom);

	int nX2 = nX + 3 + (int)(r.Width()*0.4f);

	dc.MoveTo(nX2, r.top);
	dc.LineTo(nX2, r.bottom);
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
	//dc.Arc(p2.x - nRad, p2.y - nRad, p2.x + nRad, p2.y + nRad, p3.x, p3.y, p1.x, p1.y);
}

void CNavigationPanel::PaintInfoBtn(void* pContext, const CRect& rect, CDC& dc) {
	CFont font;
	font.CreatePointFont(min(rect.Width() * 6, 160), _T("Times New Roman"), dc, false, true);
	HFONT oldFont = dc.SelectFont(font);
	dc.DrawText(_T("i"), 1, (LPRECT)&rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
	dc.SelectFont(oldFont);
}

LPCTSTR CNavigationPanel::WindowModeTooltip(void* pContext) {
	if (*((bool*)pContext)) {
		return CNLS::GetString(_T("Window mode"));
	} else {
		return CNLS::GetString(_T("Full screen mode"));
	}
}

LPCTSTR CNavigationPanel::ZoomFitToggleTooltip(void* pContext) {
	CNavigationPanel* pNavPanel = (CNavigationPanel*)pContext;
	if (pNavPanel->m_isCurrentImageFitToScreen(pNavPanel->m_pDecisionMethodParam)) {
		return CNLS::GetString(_T("Actual size of image"));
	} else {
		return CNLS::GetString(_T("Fit image to screen"));
	}
}