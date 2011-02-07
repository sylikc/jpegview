#include "StdAfx.h"
#include "ZoomNavigator.h"
#include "JPEGImage.h"
#include "Helpers.h"
#include <math.h>

static const int cnThumbWL = 320;
static const int cnThumbHL = 240;

static const int cnThumbW = 200;
static const int cnThumbH = 150;

static const int cnThumbWS = 150;
static const int cnThumbHS = 100;


CRect CZoomNavigator::sm_lastSectionRect(0, 0, 0, 0);
CRect CZoomNavigator::sm_lastNavigatorRect(0, 0, 0, 0);
CPoint CZoomNavigator::sm_lastPanRectPoint(-1, -1);

CRectF CZoomNavigator::GetVisibleRect(CSize sizeFull, CSize sizeClipped, CPoint offset) {
	float fLeft = (float)offset.x/sizeFull.cx;
	float fTop = (float)offset.y/sizeFull.cy;
	return CRectF(fLeft, fTop, fLeft + (float)sizeClipped.cx/sizeFull.cx, fTop + (float)sizeClipped.cy/sizeFull.cy);
}

CRect CZoomNavigator::GetNavigatorBound(const CRect& panelRect) {
	int nThumbW = (panelRect.Width() < 800) ? cnThumbWS : (panelRect.Width() < 1400) ? cnThumbW : cnThumbWL;
	int nThumbH = (panelRect.Width() < 800) ? cnThumbHS : (panelRect.Width() < 1400) ? cnThumbH : cnThumbHL;
	int nLeft = panelRect.right - nThumbW;
	int nTop = panelRect.top - nThumbH;
	return CRect(nLeft, nTop, nLeft + nThumbW, nTop + nThumbH);
}

CRect CZoomNavigator::GetNavigatorRect(CJPEGImage* pImage, const CRect& panelRect) {
	int nThumbW = (panelRect.Width() < 800) ? cnThumbWS : (panelRect.Width() < 1400) ? cnThumbW : cnThumbWL;
	int nThumbH = (panelRect.Width() < 800) ? cnThumbHS : (panelRect.Width() < 1400) ? cnThumbH : cnThumbHL;
	double dZoom;
	CSize sizeThumb = Helpers::GetImageRect(pImage->OrigWidth(), pImage->OrigHeight(), nThumbH, nThumbH, 
		Helpers::ZM_FitToScreenNoZoom, dZoom);
	int xDest = panelRect.right - nThumbW - 1 + (nThumbW - sizeThumb.cx) / 2;
	int yDest = panelRect.top - nThumbH - 1 + (nThumbH - sizeThumb.cy) / 2;
	if (panelRect.Width() < 800) {
		yDest += panelRect.Height() / 3;
	}
	return CRect(xDest, yDest, xDest + max(1, sizeThumb.cx), yDest + max(1, sizeThumb.cy));
}

void CZoomNavigator::PaintZoomNavigator(CJPEGImage* pImage, const CRectF& visRect, const CRect& navigatorRect,
										const CPoint& mousePt,
										const CImageProcessingParams& processingParams,
										EProcessingFlags eFlags, double dRotation, CPaintDC& dc) {
	
	void* pDIBData = (fabs(dRotation) > 1e-3) ?
		pImage->GetThumbnailDIBRotated(navigatorRect.Size(), processingParams, eFlags, dRotation) :
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
		dc.Rectangle(xDest - 1, yDest - 1, xDest + xSize + 1, yDest + ySize + 1);

		CRect rectVisible(xDest + (int)(xSize*visRect.Left + 0.5f), yDest + (int)(ySize*visRect.Top + 0.5f),
			xDest + (int)(xSize*visRect.Right + 0.5f), yDest + (int)(ySize*visRect.Bottom + 0.5f));

		sm_lastSectionRect = rectVisible;

		rectVisible.InflateRect(1, 1);
		dc.SelectStockPen(BLACK_PEN);
		dc.Rectangle(&rectVisible);
		rectVisible.InflateRect(-2, -2);
		dc.Rectangle(&rectVisible);
		rectVisible.InflateRect(1, 1);
		dc.SelectStockPen(WHITE_PEN);
		dc.Rectangle(&rectVisible);

		sm_lastNavigatorRect = CRect(xDest, yDest, xDest + xSize, yDest + ySize);
		if (sm_lastNavigatorRect.PtInRect(mousePt)) {
			PaintPanRectangle(dc, sm_lastPanRectPoint);
		} else {
			sm_lastPanRectPoint = CPoint(-1, -1);
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
		center = sm_lastPanRectPoint;
	}
	if (!(center.x == -1 && center.y == -1)) {
		int x1 = center.x - sm_lastSectionRect.Width() / 2;
		int y1 = center.y - sm_lastSectionRect.Height() / 2;
		if (x1 < sm_lastNavigatorRect.left) {
			x1 = sm_lastNavigatorRect.left;
		}
		if (x1 + sm_lastSectionRect.Width() > sm_lastNavigatorRect.right) {
			x1 = sm_lastNavigatorRect.right - sm_lastSectionRect.Width();
		}
		if (y1 < sm_lastNavigatorRect.top) {
			y1 = sm_lastNavigatorRect.top;
		}
		if (y1 + sm_lastSectionRect.Height() > sm_lastNavigatorRect.bottom) {
			y1 = sm_lastNavigatorRect.bottom - sm_lastSectionRect.Height();
		}
		dc.Rectangle(x1, y1, x1 + sm_lastSectionRect.Width(), y1 + sm_lastSectionRect.Height());
	}
	dc.SetROP2(nOldMode);
	dc.SelectBrush(hOldBrush);
	dc.SelectPen(hOldPen);
	::DeleteObject(hPen);
	sm_lastPanRectPoint = centerPt;
}

