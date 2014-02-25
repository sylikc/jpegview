#pragma once

#include "ProcessParams.h"
#include "ZoomNavigator.h"

class CMainDlg;
class CPanel;

// Implements functionality of the zoom navigator.
// This is not a CPanelController because the zoom navigator is not a CPanel (not painted to offscreen bitmap)
class CZoomNavigatorCtl
{
public:
	CZoomNavigatorCtl(CMainDlg* pMainDlg, CPanel* pImageProcPanel, CPanel* pNavigationPanel);

	bool IsVisible();
	bool IsActive();

	CRect PanelRect();

	// Returns if user is dragging with zoom navigator
	bool IsDragging() { return m_bDragging; }

	// Perform dragging with zoom navigator
	void StartDragging(int nX, int nY);
	void DoDragging(int nDeltaX, int nDeltaY);
	void EndDragging();

	bool OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY);
	bool OnMouseMove(int nOldX, int nOldY);
	// Paints zoom navigator with given DC (when currently visible). Needs all image processing parameters and flags, including rotation and
	// trapezoid (that can be NULL of course) to be able to generate the thumbnail image for the zoom navigator
	void OnPaint(CPaintDC& paintDC, CRectF visRectZoomNavigator, const CImageProcessingParams* pImageProcParams, 
		EProcessingFlags eProcessingFlags, double dRotationAngle, const CTrapezoid* pTrapezoid);

	void InvalidateZoomNavigatorRect(); // does nothing when zoom navigator is not active
	bool IsPointInZoomNavigatorThumbnail(const CPoint& pt); // returns always false when zoom navigator not visible

	// Gets the visible rectangle in float coordinates (normalized to 0..1) relative to full image
	static CRectF GetVisibleRect(CSize sizeFull, CSize sizeClipped, CPoint offset);

private:
	CMainDlg* m_pMainDlg;
	CPanel* m_pImageProcPanel;
	CPanel* m_pNavigationPanel;
	bool m_bDragging;
	CPoint m_capturedPosZoomNavSection;
	CZoomNavigator m_zoomNavigator;
};