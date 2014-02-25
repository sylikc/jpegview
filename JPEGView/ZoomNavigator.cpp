#include "StdAfx.h"
#include "ZoomNavigator.h"
#include "JPEGImage.h"
#include "Helpers.h"
#include "HelpersGUI.h"
#include <math.h>

// in 96 DPI
static const int cnThumbWL = 320; // maximal thumb size
static const int cnThumbHL = 240;

static const int cnThumbW = 200; // normal thumb size
static const int cnThumbH = 150;

static const int cnThumbWS = 133; // minimal thumb size
static const int cnThumbHS = 100;

static const int cnLimit1 = 800; // width of window where normal thumb size is reached
static const int cnLimit2 = 1400; // width of window where maximal thumb size is reached

CZoomNavigator::CZoomNavigator() {
	m_lastSectionRect = CRect(0, 0, 0, 0);
	m_lastNavigatorRect = CRect(0, 0, 0, 0);
	m_lastPanRectPoint = CPoint(-1, -1);
}

CRectF CZoomNavigator::GetVisibleRect(CSize sizeFull, CSize sizeClipped, CPoint offset) {
	float fLeft = (float)offset.x/sizeFull.cx;
	float fTop = (float)offset.y/sizeFull.cy;
	return CRectF(fLeft, fTop, fLeft + (float)sizeClipped.cx/sizeFull.cx, fTop + (float)sizeClipped.cy/sizeFull.cy);
}

CRect CZoomNavigator::GetNavigatorBound(const CRect& panelRect) {
	CSize thumbSize = GetThumbSize(panelRect.Width());
	int nLeft = panelRect.right - thumbSize.cx;
	int nTop = panelRect.top - thumbSize.cy;
	return CRect(nLeft, nTop, nLeft + thumbSize.cx, nTop + thumbSize.cy);
}

CRect CZoomNavigator::GetNavigatorRect(CJPEGImage* pImage, const CRect& panelRect, const CRect& navigatorRect) {
	CSize thumbSize = GetThumbSize(panelRect.Width());
	double dZoom;
	CSize sizeThumbOriginal = Helpers::GetImageRect(pImage->OrigWidth(), pImage->OrigHeight(), thumbSize.cy, thumbSize.cy, 
		Helpers::ZM_FitToScreenNoZoom, dZoom);
	int xDest = panelRect.right - thumbSize.cx - 1 + (thumbSize.cx - sizeThumbOriginal.cx) / 2;
	int yDest = panelRect.top - thumbSize.cy - 1 + (thumbSize.cy - sizeThumbOriginal.cy) / 2;
	if (panelRect.Width() < HelpersGUI::ScaleToScreen(cnLimit1)) {
		yDest += panelRect.Height() / 3;
	}
	int yBottom = yDest + max(1, sizeThumbOriginal.cy);
	if (yBottom >= navigatorRect.top && xDest < navigatorRect.right) {
		// collides with navigator rectangle
		yDest -= (yBottom - navigatorRect.top + 1);
	}
	return CRect(xDest, yDest, xDest + max(1, sizeThumbOriginal.cx), yDest + max(1, sizeThumbOriginal.cy));
}

CSize CZoomNavigator::GetThumbSize(int width) {
	const float fAspectRatio = 0.66666666f;
	float factor = (float)(cnThumbW - cnThumbWL) / (cnLimit1 - cnLimit2);
	float offset = cnThumbWL - factor * cnLimit2;
	int thumbWidth = (int)(factor * width + offset + 0.5f);
	thumbWidth = min(cnThumbWL, max(cnThumbWS, thumbWidth));
	int thumbHeight = (int)(thumbWidth * fAspectRatio + 0.5f);
	return CSize(HelpersGUI::ScaleToScreen(thumbWidth), HelpersGUI::ScaleToScreen(thumbHeight));
}

void CZoomNavigator::PaintZoomNavigator(CJPEGImage* pImage, const CRectF& visRect, const CRect& navigatorRect,
										const CPoint& mousePt,
										const CImageProcessingParams& processingParams,
										EProcessingFlags eFlags, double dRotation, const CTrapezoid* pTrapezoid, CPaintDC& dc) {
	
	void* pDIBData = (fabs(dRotation) > 1e-3) ?
		pImage->GetThumbnailDIBRotated(navigatorRect.Size(), processingParams, eFlags, dRotation) :
		(pTrapezoid != NULL) ?
		pImage->GetThumbnailDIBTrapezoid(navigatorRect.Size(), processingParams, eFlags, *pTrapezoid) :
		pImage->GetThumbnailDIB(navigatorRect.Size(), processingParams, eFlags);

	// Paint the DIB
	if (pDIBData != NULL) {
		int xDest = navigatorRect.left;
		int yDest = navigatorRect.top;
		int xSize = navigatorRect.Width();
		int ySize = navigatorRect.Height();
		BITMAPINFO bmInfo;
		memset(&bmInfo, 0, sizeof(BITMAPINFO));
		bmInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmInfo.bmiHeader.biWidth = xSize;
		bmInfo.bmiHeader.biHeight = -ySize;
		bmInfo.bmiHeader.biPlanes = 1;
		bmInfo.bmiHeader.biBitCount = 32;
		bmInfo.bmiHeader.biCompression = BI_RGB;
		dc.SetDIBitsToDevice(xDest, yDest, xSize, ySize, 0, 0, 0, ySize, pDIBData, 
			&bmInfo, DIB_RGB_COLORS);
		dc.SelectStockBrush(HOLLOW_BRUSH);
		dc.SelectStockPen(WHITE_PEN);
		HelpersGUI::DrawRectangle(dc, CRect(xDest - 1, yDest - 1, xDest + xSize + 1, yDest + ySize + 1));

		CRect rectVisible(xDest + (int)(xSize*visRect.Left + 0.5f), yDest + (int)(ySize*visRect.Top + 0.5f),
			xDest + (int)(xSize*visRect.Right + 0.5f), yDest + (int)(ySize*visRect.Bottom + 0.5f));

		m_lastSectionRect = rectVisible;

		rectVisible.InflateRect(1, 1);
		dc.SelectStockPen(BLACK_PEN);
		dc.Rectangle(&rectVisible);
		rectVisible.InflateRect(-2, -2);
		dc.Rectangle(&rectVisible);
		rectVisible.InflateRect(1, 1);
		dc.SelectStockPen(WHITE_PEN);
		dc.Rectangle(&rectVisible);

		m_lastNavigatorRect = CRect(xDest, yDest, xDest + xSize, yDest + ySize);
		if (m_lastNavigatorRect.PtInRect(mousePt)) {
			PaintPanRectangle(dc, m_lastPanRectPoint);
		} else {
			m_lastPanRectPoint = CPoint(-1, -1);
		}
	}
}

void CZoomNavigator::PaintPanRectangle(CDC& dc, const CPoint& centerPt) {
	HBRUSH hOldBrush = dc.SelectStockBrush(HOLLOW_BRUSH);
	LOGBRUSH brush;
	brush.lbColor = RGB(255, 255, 255);
	brush.lbStyle = BS_SOLID;
	HPEN hPen = ::ExtCreatePen(PS_COSMETIC | PS_ALTERNATE, 1, &brush, 0, 0);
	HPEN hOldPen = dc.SelectPen(hPen);
	int nOldMode = dc.SetROP2(R2_XORPEN);
	dc.SetBkMode(OPAQUE);
	dc.SetBkColor(0);
    CPoint center = centerPt;
	if (centerPt.x == -1 && centerPt.y == -1) {
		center = m_lastPanRectPoint;
	}
	if (!(center.x == -1 && center.y == -1)) {
		int x1 = center.x - m_lastSectionRect.Width() / 2;
		int y1 = center.y - m_lastSectionRect.Height() / 2;
		if (x1 < m_lastNavigatorRect.left) {
			x1 = m_lastNavigatorRect.left;
		}
		if (x1 + m_lastSectionRect.Width() > m_lastNavigatorRect.right) {
			x1 = m_lastNavigatorRect.right - m_lastSectionRect.Width();
		}
		if (y1 < m_lastNavigatorRect.top) {
			y1 = m_lastNavigatorRect.top;
		}
		if (y1 + m_lastSectionRect.Height() > m_lastNavigatorRect.bottom) {
			y1 = m_lastNavigatorRect.bottom - m_lastSectionRect.Height();
		}
		dc.Rectangle(x1, y1, x1 + m_lastSectionRect.Width(), y1 + m_lastSectionRect.Height());
	}
	dc.SetROP2(nOldMode);
	dc.SelectBrush(hOldBrush);
	dc.SelectPen(hOldPen);
	::DeleteObject(hPen);
	m_lastPanRectPoint = centerPt;
}

