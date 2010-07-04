#pragma once

// forward declarations
class CPanelMgr;
class CEXIFDisplay;

#define MAX_REGIONS_CPaintMemDCMgr 5

// Helps painting a panel or other region into a memory DC and then blitting it to the screen.
// This eliminates flickering.
class CPaintMemDCMgr {
public:
	CPaintMemDCMgr(CPaintDC& paintDC);
	~CPaintMemDCMgr();

	// Prepares a memory DC of given size by creating the backing store bitmap and clearing it
	static HBITMAP PrepareRectForMemDCPainting(CDC & memDC, CDC & paintDC, const CRect& rect);

	// Blits the DIB data section to target DC using dimming (blending with a black bitmap)
	static void BitBltBlended(CDC & dc, CDC & paintDC, const CSize& dcSize, void* pDIBData, BITMAPINFO* pbmInfo, 
						  const CPoint& dibStart, const CSize& dibSize, float fDimFactor);

	// Blits the DIB data section to target DC using blending with painted version of given panel
	static void CPaintMemDCMgr::BitBltBlended(CDC & dc, CDC & paintDC, const CSize& dcSize, void* pDIBData, BITMAPINFO* pbmInfo, 
						  const CPoint& dibStart, const CSize& dibSize, CPanelMgr& panel, const CPoint& offsetPanel,
						  float fBlendFactor);

	CRect CreatePanelRegion(CPanelMgr* pPanel, float fDimFactor, bool bBlendPanel);
	CRect CreateEXIFDisplayRegion(CEXIFDisplay* pEXIFDisplay, float fDimFactor, int nIPALeft, bool bShowFileName);

	void BlitImageToMemDC(void* pDIBData, BITMAPINFO* pBitmapInfo, CPoint destination, float fBlendFactor);

	void PaintMemDCToScreen();
private:

	enum EObjectType { Panel, EXIFDisplay };

	struct CManagedRegion {
	public:
		void* DisplayObject;
		EObjectType ObjectType;
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