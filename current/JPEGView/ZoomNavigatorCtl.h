#pragma once

#include "ProcessParams.h"
#include "ZoomNavigator.h"

class CMainDlg;
class CPanel;

// Implements functionality of the zoom navigator.
class CZoomNavigatorCtl
{
public:
	CZoomNavigatorCtl(CMainDlg* pMainDlg, CPanel* pImageProcPanel);

	bool IsVisible();
	bool IsActive();

	CRect PanelRect();

	bool IsDragging() { return m_bDragging; }

	void StartDragging(int nX, int nY);
	void DoDragging(int nDeltaX, int nDeltaY);
	void EndDragging();

	bool OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY);
	bool OnMouseMove(int nOldX, int nOldY);
	void OnPaint(CPaintDC& paintDC, CRectF visRectZoomNavigator, CImageProcessingParams* pImageProcParams, EProcessingFlags eProcessingFlags, double dRotationAngle);

	void InvalidateZoomNavigatorRect();
	bool IsPointInZoomNavigatorThumbnail(const CPoint& pt);

	CRectF GetVisibleRect(CSize sizeFull, CSize sizeClipped, CPoint offset);

private:
	CMainDlg* m_pMainDlg;
	CPanel* m_pImageProcPanel;
	bool m_bDragging;
	CPoint m_capturedPosZoomNavSection;
	CZoomNavigator m_zoomNavigator;
};