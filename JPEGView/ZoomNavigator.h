#pragma once

#include "ProcessParams.h"

class CJPEGImage;

struct CRectF {
	CRectF(float fLeft, float fTop, float fRight, float fBottom) {
		Left = fLeft;
		Right = fRight;
		Top = fTop;
		Bottom = fBottom;
	}

	float Left;
	float Right;
	float Top;
	float Bottom;
};

// Provides support for a small area containing a thumbnail image giving an overview of the currently
// displayed section of the image when zoomed in
class CZoomNavigator
{
public:
	// Gets the visible rectangle in float coordinates (normalized to 0..1) relative to full image
	static CRectF GetVisibleRect(CSize sizeFull, CSize sizeClipped, CPoint offset);

	// Gets the position of the navigator rectangle (thumbnail image rect), relative to the image processing
	// panel rectangle
	static CRect GetNavigatorRect(CJPEGImage* pImage, const CRect& panelRect);

	// Gets the maximal bounds of the navigator
	static CRect GetNavigatorBound(const CRect& panelRect);

	// Paints the zoom navigator
	static void PaintZoomNavigator(CJPEGImage* pImage, const CRectF& visRect, const CRect& navigatorRect,
		const CImageProcessingParams& processingParams,
		EProcessingFlags eFlags, CPaintDC& dc);

	// Rectangle of the visible image section
	static CRect LastImageSectionRect() { return sm_lastSectionRect; }

private:
	CZoomNavigator(void);
	~CZoomNavigator(void);

	static CRect sm_lastSectionRect;
};
