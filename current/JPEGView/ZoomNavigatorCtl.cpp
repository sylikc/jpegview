#include "StdAfx.h"
#include "resource.h"
#include "MainDlg.h"
#include "JPEGImage.h"
#include "ZoomNavigatorCtl.h"
#include "NavigationPanelCtl.h"
#include "SettingsProvider.h"
#include "Panel.h"

CZoomNavigatorCtl::CZoomNavigatorCtl(CMainDlg* pMainDlg, CPanel* pImageProcPanel) {
	m_pMainDlg = pMainDlg;
	m_pImageProcPanel = pImageProcPanel;
	m_bDragging = false;
	m_capturedPosZoomNavSection = CPoint(0, 0);
}

bool CZoomNavigatorCtl::IsVisible() {
	if (!IsActive()) {
		return false;
	} else {
		if (m_bDragging) {
			return true;
		}
		const CSize& virtualImageSize = m_pMainDlg->VirtualImageSize();
		CSize clippedSize(min(m_pMainDlg->ClientRect().Width(), virtualImageSize.cx), min(m_pMainDlg->ClientRect().Height(), virtualImageSize.cy));
		bool bShowZoomNavigator = (virtualImageSize.cx > clippedSize.cx || virtualImageSize.cy > clippedSize.cy);
		if (bShowZoomNavigator) {
			CRect navBoundRect = CZoomNavigator::GetNavigatorBound(m_pImageProcPanel->PanelRect());
			bShowZoomNavigator = m_pMainDlg->IsDoDragging() || m_pMainDlg->IsInZooming() || m_pMainDlg->IsShowZoomFactor() || 
				(!m_pMainDlg->IsCropping() && navBoundRect.PtInRect(m_pMainDlg->GetMousePos()));
		}
		return bShowZoomNavigator;
	}
}

bool CZoomNavigatorCtl::IsActive() {
	return m_pMainDlg->GetCurrentImage() != NULL && CSettingsProvider::This().ShowZoomNavigator();
}

CRect CZoomNavigatorCtl::PanelRect() {
	if (IsActive()) {
		CRect rect = CZoomNavigator::GetNavigatorRect(m_pMainDlg->GetCurrentImage(), m_pImageProcPanel->PanelRect());
		rect.InflateRect(1, 1);
		return rect;
	}
	return CRect(0, 0, 0, 0);
}

void CZoomNavigatorCtl::StartDragging(int nX, int nY) {
	m_bDragging = true;
	m_capturedPosZoomNavSection = m_zoomNavigator.LastVisibleRect().TopLeft();
}

void CZoomNavigatorCtl::DoDragging(int nDeltaX, int nDeltaY) {
	if (m_bDragging) {
		const CSize& virtualImageSize = m_pMainDlg->VirtualImageSize();
		int nNewX = m_capturedPosZoomNavSection.x + nDeltaX;
		int nNewY = m_capturedPosZoomNavSection.y + nDeltaY;
		CRect fullRect = CZoomNavigator::GetNavigatorRect(m_pMainDlg->GetCurrentImage(), m_pImageProcPanel->PanelRect());
		CPoint newOffsets = CJPEGImage::ConvertOffset(fullRect.Size(), m_zoomNavigator.LastVisibleRect().Size(), CPoint(nNewX - fullRect.left, nNewY - fullRect.top));
		m_pMainDlg->PerformPan((int)(newOffsets.x * (float)virtualImageSize.cx/fullRect.Width()), 
			(int)(newOffsets.y * (float)virtualImageSize.cy/fullRect.Height()), true);
	}
}

void CZoomNavigatorCtl::EndDragging() {
	m_bDragging = false;
	m_zoomNavigator.ClearLastPanRectPoint();
	InvalidateZoomNavigatorRect();
}

bool CZoomNavigatorCtl::OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY) {
	if (eMouseEvent == MouseEvent_BtnDown) {
		// Check if clicking in thumbnail image
		bool bClickInZoomNavigatorThumbnail = IsPointInZoomNavigatorThumbnail(CPoint(nX, nY));
		if (bClickInZoomNavigatorThumbnail) {
			CPoint centerOfVisibleRect = m_zoomNavigator.LastVisibleRect().CenterPoint();
			m_pMainDlg->StartDragging(nX + (centerOfVisibleRect.x - nX), nY + (centerOfVisibleRect.y - nY), true);
			m_pMainDlg->DoDragging();
			m_pMainDlg->EndDragging();
			m_zoomNavigator.ClearLastPanRectPoint();
			m_pMainDlg->UpdateWindow(); // force repainting to update zoom navigator
			m_pMainDlg->StartDragging(nX, nY, true);
			return true;
		}
	}
	return false;
}

bool CZoomNavigatorCtl::OnMouseMove(int nOldX, int nOldY) {
	CPoint mousePos = m_pMainDlg->GetMousePos();
	CRect zoomHotArea = CZoomNavigator::GetNavigatorBound(m_pImageProcPanel->PanelRect());
	BOOL bMouseInHotArea = zoomHotArea.PtInRect(mousePos);
	if (bMouseInHotArea && CSettingsProvider::This().ShowZoomNavigator()) {
		m_pMainDlg->MouseOn();
		m_pMainDlg->SetCursorForMoveSection();
	}
	if (bMouseInHotArea != zoomHotArea.PtInRect(CPoint(nOldX, nOldY))) {
		InvalidateZoomNavigatorRect();
	}
	if (mousePos.x != nOldX || mousePos.y != nOldY) {
		if (IsPointInZoomNavigatorThumbnail(mousePos)) {
			CDC dc(m_pMainDlg->GetDC());
			m_zoomNavigator.PaintPanRectangle(dc, CPoint(-1, -1));
			m_zoomNavigator.PaintPanRectangle(dc, mousePos);
			m_pMainDlg->GetNavPanelCtl()->StartNavPanelAnimation(true, true);
			return true;
		} else if (m_zoomNavigator.LastPanRectPoint() != CPoint(-1, -1)) {
			m_zoomNavigator.PaintPanRectangle(CDC(m_pMainDlg->GetDC()), CPoint(-1, -1));
		}
	}
	return false;
}

void CZoomNavigatorCtl::OnPaint(CPaintDC& paintDC, CRectF visRectZoomNavigator, const CImageProcessingParams* pImageProcParams, 
								EProcessingFlags eProcessingFlags, double dRotationAngle, const CTrapezoid* pTrapezoid) {
	if (IsVisible()) {
		m_zoomNavigator.PaintZoomNavigator(m_pMainDlg->GetCurrentImage(), visRectZoomNavigator,
			CZoomNavigator::GetNavigatorRect(m_pMainDlg->GetCurrentImage(), m_pImageProcPanel->PanelRect()),
			m_pMainDlg->GetMousePos(), *pImageProcParams, eProcessingFlags, 
			dRotationAngle, pTrapezoid, paintDC);
	} else {
		m_zoomNavigator.ClearLastPanRectPoint();
	}
}

void CZoomNavigatorCtl::InvalidateZoomNavigatorRect() {
	if (IsActive()) {
		m_pMainDlg->InvalidateRect(PanelRect(), FALSE);
	}
}

bool CZoomNavigatorCtl::IsPointInZoomNavigatorThumbnail(const CPoint& pt) {
	if (IsVisible()) {
		CRect rect = CZoomNavigator::GetNavigatorRect(m_pMainDlg->GetCurrentImage(), m_pImageProcPanel->PanelRect());
		return rect.PtInRect(pt);
	}
	return false;
}

CRectF CZoomNavigatorCtl::GetVisibleRect(CSize sizeFull, CSize sizeClipped, CPoint offset) {
	return CZoomNavigator::GetVisibleRect(sizeFull, sizeClipped, offset);
}