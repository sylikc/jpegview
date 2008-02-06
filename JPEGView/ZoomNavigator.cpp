#include "StdAfx.h"
#include "ZoomNavigator.h"
#include "JPEGImage.h"
#include "Helpers.h"

static const int cnThumbW = 150;
static const int cnThumbH = 150;

CRect CZoomNavigator::sm_lastSectionRect(0, 0, 0, 0);

CRectF CZoomNavigator::GetVisibleRect(CSize sizeFull, CSize sizeClipped, CPoint offset) {
	float fLeft = (float)offset.x/sizeFull.cx;
	float fTop = (float)offset.y/sizeFull.cy;
	return CRectF(fLeft, fTop, fLeft + (float)sizeClipped.cx/sizeFull.cx, fTop + (float)sizeClipped.cy/sizeFull.cy);
}

CRect CZoomNavigator::GetNavigatorBound(const CRect& clientRect, int nBorderY) {
	int nLeft = clientRect.Width() - cnThumbW;
	int nTop = clientRect.Height() - nBorderY - cnThumbH;
	return CRect(nLeft, nTop, nLeft + cnThumbW, nTop + cnThumbH);
}

CRect CZoomNavigator::GetNavigatorRect(CJPEGImage* pImage, const CRect& clientRect, int nBorderY) {
	double dZoom;
	CSize sizeThumb = Helpers::GetImageRect(pImage->OrigWidth(), pImage->OrigHeight(), cnThumbW, cnThumbH, 
		Helpers::ZM_FitToScreenNoZoom, dZoom);
	int xDest = clientRect.Width() - cnThumbW - 1 + (cnThumbW - sizeThumb.cx) / 2;
	int yDest = clientRect.Height() - nBorderY - cnThumbH - 1 + (cnThumbH - sizeThumb.cy) / 2;
	return CRect(xDest, yDest, xDest + max(1, sizeThumb.cx), yDest + max(1, sizeThumb.cy));
}

void CZoomNavigator::PaintZoomNavigator(CJPEGImage* pImage, const CRectF& visRect, const CRect& navigatorRect,
										const CImageProcessingParams& processingParams,
										EProcessingFlags eFlags, CPaintDC& dc) {
	
	void* pDIBData = pImage->GetThumbnailDIB(navigatorRect.Size(), processingParams, eFlags);

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
	}
}
