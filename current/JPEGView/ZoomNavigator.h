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
	CZoomNavigator();

	// Gets the visible rectangle in float coordinates (normalized to 0..1) relative to full image
	static CRectF GetVisibleRect(CSize sizeFull, CSize sizeClipped, CPoint offset);

	// Gets the position of the navigator rectangle (thumbnail image rect) în client coordinates.
	// The zoom navigator is placed relative to the image processing panel rectangle (panelRect)
	static CRect GetNavigatorRect(CJPEGImage* pImage, const CRect& panelRect);

	// Gets the maximal bounds of the navigator
	static CRect GetNavigatorBound(const CRect& panelRect);

	// Paints the zoom navigator
	void PaintZoomNavigator(CJPEGImage* pImage, const CRectF& visRect, const CRect& navigatorRect,
		const CPoint& mousePt,
		const CImageProcessingParams& processingParams,
		EProcessingFlags eFlags, double dRotation, CPaintDC& dc);

	// Paints the pan rectangle
	void PaintPanRectangle(CDC& dc, const CPoint& centerPt);

	// Rectangle of the visible image section in pixels
	CRect LastVisibleRect() { return m_lastSectionRect; }

	// Last dotted pan rectangle in pixels
	CPoint LastPanRectPoint() { return m_lastPanRectPoint; }

	// Last navigator rectangle in pixels
	CRect LastNavigatorRect() { return m_lastNavigatorRect; }

	// Prevents painting of the dotted pan rectangle
	void ClearLastPanRectPoint() { m_lastPanRectPoint = CPoint(-1, -1); }

private:
	CRect m_lastSectionRect;
	CRect m_lastNavigatorRect;
	CPoint m_lastPanRectPoint;
};
