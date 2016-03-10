#pragma once

// forward declarations
class CPanel;

#define MAX_REGIONS_CPaintMemDCMgr 16

// Helps painting a panel into a memory DC and then blitting it to the screen.
// This eliminates flickering.
class CPaintMemDCMgr {
public:
	CPaintMemDCMgr(CPaintDC& paintDC);
	~CPaintMemDCMgr();

	CPaintDC& GetPaintDC() { return m_paintDC; }

	// Excludes, respectively includes the given list of rectangles into the clipping region
	static void ExcludeFromClippingRegion(CDC & paintDC, const std::list<CRect>& listExcludedRects);
	static void IncludeIntoClippingRegion(CDC & paintDC, const std::list<CRect>& listExcludedRects);

	// Prepares a memory DC of given size by creating the backing store bitmap and clearing it
	static HBITMAP PrepareRectForMemDCPainting(CDC & memDC, CDC & paintDC, const CRect& rect);

	// Blits the DIB data section to target DC using dimming (blending with a black bitmap)
	static void BitBltBlended(CDC & dc, CDC & paintDC, const CSize& dcSize, void* pDIBData, BITMAPINFO* pbmInfo, 
						  const CPoint& dibStart, const CSize& dibSize, float fDimFactor);

	// Blits the DIB data section to target DC using blending with painted version of given panel
	static void CPaintMemDCMgr::BitBltBlended(CDC & dc, CDC & paintDC, const CSize& dcSize, void* pDIBData, BITMAPINFO* pbmInfo, 
						  const CPoint& dibStart, const CSize& dibSize, CPanel& panel, const CPoint& offsetPanel,
						  float fBlendFactor);

	CRect CreatePanelRegion(CPanel* pPanel, float fDimFactor, bool bBlendPanel);

	void BlitImageToMemDC(void* pDIBData, BITMAPINFO* pBitmapInfo, CPoint destination, float fBlendFactor);

	void PaintMemDCToScreen();
private:

	struct CManagedRegion {
	public:
		CPanel* DisplayRegion;
		float DimFactor;
		bool Blend;
		CDC MemoryDC;
		HBITMAP OffscreenBitmap;
		CRect DisplayRect;
	};

	CPaintDC& m_paintDC;
	int m_nNumElems;
	CManagedRegion m_managedRegions[MAX_REGIONS_CPaintMemDCMgr];
};