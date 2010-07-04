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

HBITMAP CPaintMemDCMgr::PrepareRectForMemDCPainting(CDC & memDC, CDC & paintDC, const CRect& rect) {
	paintDC.ExcludeClipRect(&rect);

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
						  const CPoint& dibStart, const CSize& dibSize, CPanelMgr& panel, const CPoint& offsetPanel,
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

CRect CPaintMemDCMgr::CreatePanelRegion(CPanelMgr* pPanel, float fDimFactor, bool bBlendPanel) {
	if (m_nNumElems == MAX_REGIONS_CPaintMemDCMgr) {
		return CRect(0, 0, 0, 0);
	}

	CRect displayRect = pPanel->PanelRect();
	m_managedRegions[m_nNumElems].DisplayRect = displayRect;
	m_managedRegions[m_nNumElems].DisplayObject = pPanel;
	m_managedRegions[m_nNumElems].ObjectType = Panel;
	m_managedRegions[m_nNumElems].DimFactor = fDimFactor;
	m_managedRegions[m_nNumElems].Blend = bBlendPanel;
	m_managedRegions[m_nNumElems].OffscreenBitmap = PrepareRectForMemDCPainting(m_managedRegions[m_nNumElems].MemoryDC, m_paintDC, displayRect);

	m_nNumElems++;
	return displayRect;
}

CRect CPaintMemDCMgr::CreateEXIFDisplayRegion(CEXIFDisplay* pEXIFDisplay, float fDimFactor, int nIPALeft, bool bShowFileName) {
	if (m_nNumElems == MAX_REGIONS_CPaintMemDCMgr) {
		return CRect(0, 0, 0, 0);
	}

	int nYStart = bShowFileName ? 32 : 0;
	CRect displayRect = CRect(nIPALeft, nYStart, nIPALeft + pEXIFDisplay->GetSize(m_paintDC).cx, pEXIFDisplay->GetSize(m_paintDC).cy + nYStart);
	m_managedRegions[m_nNumElems].DisplayRect = displayRect;
	m_managedRegions[m_nNumElems].DisplayObject = pEXIFDisplay;
	m_managedRegions[m_nNumElems].ObjectType = EXIFDisplay;
	m_managedRegions[m_nNumElems].DimFactor = fDimFactor;
	m_managedRegions[m_nNumElems].Blend = false;
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
				CPanelMgr* pPanel = (CPanelMgr*)(m_managedRegions[i].DisplayObject);
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
		// Paint panel or EXIF stuff into memory DC
		if (m_managedRegions[i].ObjectType == Panel && !m_managedRegions[i].Blend) {
			((CPanelMgr*)(m_managedRegions[i].DisplayObject))->OnPaint(m_managedRegions[i].MemoryDC, CPoint(-rect.left, -rect.top));
		}
		if (m_managedRegions[i].ObjectType == EXIFDisplay) {
			((CEXIFDisplay*)(m_managedRegions[i].DisplayObject))->Show(m_managedRegions[i].MemoryDC, 0, 0);
		}
		// Blit the memory DC to the screen
		m_paintDC.BitBlt(rect.left, rect.top, rect.Width(), rect.Height(), m_managedRegions[i].MemoryDC, 0, 0, SRCCOPY);
		// Paint tooltips
		if (m_managedRegions[i].ObjectType == Panel) {
			((CPanelMgr*)(m_managedRegions[i].DisplayObject))->GetTooltipMgr().OnPaint(m_paintDC);
		}
	}
}