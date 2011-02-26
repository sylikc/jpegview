#include "StdAfx.h"
#include "GUIControls.h"
#include "EXIFDisplay.h"
#include "SettingsProvider.h"
#include "PaintMemDCMgr.h"

CPaintMemDCMgr::CPaintMemDCMgr(CPaintDC& paintDC) : m_paintDC(paintDC) {
	m_nNumElems = 0;
	for (int i = 0; i < MAX_REGIONS_CPaintMemDCMgr; i++) {
		m_managedRegions[i].MemoryDC = CDC();
	}
}

CPaintMemDCMgr::~CPaintMemDCMgr() {
	for (int i = 0; i < m_nNumElems; i++) {
		if (m_managedRegions[i].OffscreenBitmap != NULL) {
			::DeleteObject(m_managedRegions[i].OffscreenBitmap);
		}
	}
}

void CPaintMemDCMgr::ExcludeFromClippingRegion(CDC & paintDC, const std::list<CRect>& listExcludedRects) {
	std::list<CRect>::const_iterator iter;
	for (iter = listExcludedRects.begin( ); iter != listExcludedRects.end( ); iter++ ) {
		CRect rect = *iter;
		paintDC.ExcludeClipRect(&rect);
	}
}

void CPaintMemDCMgr::IncludeIntoClippingRegion(CDC & paintDC, const std::list<CRect>& listExcludedRects) {
	std::list<CRect>::const_iterator iter;
	for (iter = listExcludedRects.begin(); iter != listExcludedRects.end(); iter++) {
		CRect rect = *iter;
		CRgn rgn;
		rgn.CreateRectRgn(rect.left, rect.top, rect.right, rect.bottom);
		paintDC.SelectClipRgn(rgn, RGN_OR);
	}
}

HBITMAP CPaintMemDCMgr::PrepareRectForMemDCPainting(CDC & memDC, CDC & paintDC, const CRect& rect) {
	CBrush backBrush;
	backBrush.CreateSolidBrush(CSettingsProvider::This().ColorBackground());

	// Create a memory DC of correct size
	CBitmap memDCBitmap = CBitmap();
	memDC.CreateCompatibleDC(paintDC);
	memDCBitmap.CreateCompatibleBitmap(paintDC, rect.Width(), rect.Height());
	memDC.SelectBitmap(memDCBitmap);
	memDC.FillRect(CRect(0, 0, rect.Width(), rect.Height()), backBrush);
	return memDCBitmap.m_hBitmap;
}

// Blits the DIB data section to target DC using dimming (blending with a black bitmap)
void CPaintMemDCMgr::BitBltBlended(CDC & dc, CDC & paintDC, const CSize& dcSize, void* pDIBData, BITMAPINFO* pbmInfo, 
						  const CPoint& dibStart, const CSize& dibSize, float fDimFactor) {
	int nW = dcSize.cx;
	int nH = dcSize.cy;
	CDC memDCblack;
	memDCblack.CreateCompatibleDC(paintDC);
	CBitmap bitmapBlack;
	bitmapBlack.CreateCompatibleBitmap(paintDC, nW, nH);
	memDCblack.SelectBitmap(bitmapBlack);
	memDCblack.FillRect(CRect(0, 0, nW, nH), (HBRUSH)::GetStockObject(BLACK_BRUSH));
	
	dc.SetDIBitsToDevice(dibStart.x, dibStart.y, 
		dibSize.cx, dibSize.cy, 0, 0, 0, dibSize.cy, pDIBData, pbmInfo, DIB_RGB_COLORS);
	
	BLENDFUNCTION blendFunc;
	memset(&blendFunc, 0, sizeof(blendFunc));
	blendFunc.BlendOp = AC_SRC_OVER;
	blendFunc.SourceConstantAlpha = (unsigned char)(fDimFactor*255 + 0.5f);
	blendFunc.AlphaFormat = 0;
	dc.AlphaBlend(0, 0, nW, nH, memDCblack, 0, 0, nW, nH, blendFunc);
}

// Blits the DIB data section to target DC using blending with painted version of given panel
void CPaintMemDCMgr::BitBltBlended(CDC & dc, CDC & paintDC, const CSize& dcSize, void* pDIBData, BITMAPINFO* pbmInfo, 
						  const CPoint& dibStart, const CSize& dibSize, CPanel& panel, const CPoint& offsetPanel,
						  float fBlendFactor) {
	int nW = dcSize.cx;
	int nH = dcSize.cy;

	dc.SetDIBitsToDevice(dibStart.x, dibStart.y, 
		dibSize.cx, dibSize.cy, 0, 0, 0, dibSize.cy, pDIBData, pbmInfo, DIB_RGB_COLORS);

	CDC memDCPanel;
	memDCPanel.CreateCompatibleDC(paintDC);
	CBitmap bitmapPanel;
	bitmapPanel.CreateCompatibleBitmap(paintDC, nW, nH);
	memDCPanel.SelectBitmap(bitmapPanel);
	memDCPanel.BitBlt(0, 0, nW, nH, dc, 0, 0, SRCCOPY);
	panel.OnPaint(memDCPanel, offsetPanel);
	
	BLENDFUNCTION blendFunc;
	memset(&blendFunc, 0, sizeof(blendFunc));
	blendFunc.BlendOp = AC_SRC_OVER;
	blendFunc.SourceConstantAlpha = (unsigned char)(fBlendFactor*255 + 0.5f);
	blendFunc.AlphaFormat = 0;
	dc.AlphaBlend(0, 0, nW, nH, memDCPanel, 0, 0, nW, nH, blendFunc);
}

CRect CPaintMemDCMgr::CreatePanelRegion(CPanel* pPanel, float fDimFactor, bool bBlendPanel) {
	if (m_nNumElems == MAX_REGIONS_CPaintMemDCMgr) {
		return CRect(0, 0, 0, 0);
	}

	CRect displayRect = pPanel->PanelRect();
	m_managedRegions[m_nNumElems].DisplayRect = displayRect;
	m_managedRegions[m_nNumElems].DisplayRegion = pPanel;
	m_managedRegions[m_nNumElems].DimFactor = fDimFactor;
	m_managedRegions[m_nNumElems].Blend = bBlendPanel;
	m_managedRegions[m_nNumElems].OffscreenBitmap = PrepareRectForMemDCPainting(m_managedRegions[m_nNumElems].MemoryDC, m_paintDC, displayRect);

	m_nNumElems++;
	return displayRect;
}

void CPaintMemDCMgr::BlitImageToMemDC(void* pDIBData, BITMAPINFO* pBitmapInfo, CPoint destination, float fBlendFactor) {
	CSize bitmapSize(abs(pBitmapInfo->bmiHeader.biWidth), abs(pBitmapInfo->bmiHeader.biHeight));
	for (int i = 0; i < m_nNumElems; i++) {
		if (m_managedRegions[i].MemoryDC.m_hDC != NULL) {
			CRect rect = m_managedRegions[i].DisplayRect;
			if (m_managedRegions[i].Blend) {
				CPanel* pPanel = m_managedRegions[i].DisplayRegion;
				BitBltBlended(m_managedRegions[i].MemoryDC, m_paintDC, CSize(rect.Width(), rect.Height()), pDIBData, pBitmapInfo, 
						  CPoint(destination.x - rect.left, destination.y - rect.top), bitmapSize, 
						  *pPanel, CPoint(-rect.left, -rect.top), fBlendFactor);
			} else {
				BitBltBlended(m_managedRegions[i].MemoryDC, m_paintDC, CSize(rect.Width(), rect.Height()), pDIBData, pBitmapInfo, 
							  CPoint(destination.x - rect.left, destination.y - rect.top), bitmapSize, m_managedRegions[i].DimFactor);
			}
		}
	}
}

void CPaintMemDCMgr::PaintMemDCToScreen() {
	for (int i = 0; i < m_nNumElems; i++) {
		CRect rect = m_managedRegions[i].DisplayRect;
		// Paint panel into memory DC
		if (!m_managedRegions[i].Blend) {
			m_managedRegions[i].DisplayRegion->OnPaint(m_managedRegions[i].MemoryDC, CPoint(-rect.left, -rect.top));
		}
		// Blit the memory DC to the screen
		m_paintDC.BitBlt(rect.left, rect.top, rect.Width(), rect.Height(), m_managedRegions[i].MemoryDC, 0, 0, SRCCOPY);
		// Paint tooltips
		m_managedRegions[i].DisplayRegion->GetTooltipMgr().OnPaint(m_paintDC);
	}
}