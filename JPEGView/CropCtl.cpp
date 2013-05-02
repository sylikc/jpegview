#include "StdAfx.h"
#include "resource.h"
#include "MainDlg.h"
#include "JPEGImage.h"
#include "CropCtl.h"
#include "PanelMgr.h"
#include "NavigationPanelCtl.h"
#include "ZoomNavigatorCtl.h"
#include "TimerEventIDs.h"
#include "CropSizeDlg.h"
#include "HelpersGUI.h"
#include "NLS.h"
#include "FileList.h"
#include <math.h>

static const int AUTOSCROLL_TIMEOUT = 20; // autoscroll time in ms
static const int HANDLE_SIZE = 7; // Handle size in pixels
static const int HANDLE_HIT_TOLERANCE = 12;

static bool PointDifferenceSmall(const CPoint& p1, const CPoint& p2) {
	return abs(p1.x - p2.x) < 2 && abs(p1.y - p2.y) < 2;
}

CCropCtl::CCropCtl(CMainDlg* pMainDlg) {
	m_pMainDlg = pMainDlg;
	m_bCropping = false;
	m_bDoCropping = false;
	m_bDoTracking = false;
	m_bTrackingMode = false;
	m_bDontStartCropOnNextClick = false;
	m_dCropRectAspectRatio = 0;
	m_nHandleSize = HANDLE_SIZE;
	m_eHitHandle = HH_None;
	m_cropStart = CPoint(INT_MIN, INT_MIN);
	m_cropEnd = CPoint(INT_MIN, INT_MIN);
	m_cropMouse = CPoint(INT_MIN, INT_MIN);
	m_startTrackMousePos = CPoint(INT_MIN, INT_MIN);
}

void CCropCtl::OnPaint(CPaintDC& dc) {
	if (m_bCropping) {
		CPoint mousePos = m_pMainDlg->GetMousePos();
		if (m_eHitHandle != HH_None) {
			if (m_bDoTracking) {
				TrackCroppingRect(mousePos.x, mousePos.y, m_eHitHandle);
			}
		} else if (m_bDoCropping) {
			UpdateCroppingRect(mousePos.x, mousePos.y, NULL, true);
		}
		PaintCropRect(dc);
	}
}

bool CCropCtl::OnTimer(int nTimerId) {
	if (nTimerId == AUTOSCROLL_TIMER_EVENT_ID) {
		CPoint mousePos;
		::GetCursorPos(&mousePos);
		::ScreenToClient(m_pMainDlg->GetHWND(), &mousePos);
		const CRect& clientRect = m_pMainDlg->ClientRect();
		if (mousePos.x < clientRect.Width() - 1 && mousePos.x > 0 && mousePos.y < clientRect.Height() - 1 && mousePos.y > 0 ) {
			::KillTimer(m_pMainDlg->GetHWND(), AUTOSCROLL_TIMER_EVENT_ID);
		}
		const int PAN_DIST = 25;
		int nPanX = 0, nPanY = 0;
		if (mousePos.x <= 0) {
			nPanX = PAN_DIST;
		}
		if (mousePos.y <= 0) {
			nPanY = PAN_DIST;
		}
		if (mousePos.x >= clientRect.Width() - 1) {
			nPanX = -PAN_DIST;
		}
		if (mousePos.y >= clientRect.Height() - 1) {
			nPanY = -PAN_DIST;
		}
		if (IsCropping()) {
			m_pMainDlg->PerformPan(nPanX, nPanY, false);
		}
		return true;
	}
	return false;
}

void CCropCtl::StartCropping(int nX, int nY) {
	m_eHitHandle = HH_None;
	if (!m_pMainDlg->GetPanelMgr()->IsModalPanelShown()) {
		if (m_bDontStartCropOnNextClick) {
			m_eHitHandle = HitHandle(nX, nY);
			m_startTrackMousePos = ScreenToImage(nX, nY);
			if (m_eHitHandle != HH_None) {
				m_bCropping = true;
				SetMouseCursor(nX, nY);
				m_bDoTracking = true;
				m_bTrackingMode = true;
				InvalidateSceenCropRect();
			} else {
				AbortCropping();
			}
		} else {
			m_bCropping = true;
			float fX = (float)nX, fY = (float)nY;
			m_pMainDlg->ScreenToImage(fX, fY);
			m_cropStart = CPoint((int)fX, (int) fY);
			m_cropMouse = CPoint(nX, nY);
			m_cropEnd = CPoint(INT_MIN, INT_MIN);
			m_nHandleSize = (int)(HelpersGUI::ScreenScaling * HANDLE_SIZE + 0.5f);
			if ((m_nHandleSize & 1) == 0) m_nHandleSize++; // must be odd
		}
	}
}

bool CCropCtl::DoCropping(int nX, int nY) {
	if (m_bTrackingMode) {
		SetMouseCursor(nX, nY);
	}
	if (m_bDoTracking) {
		TrackCroppingRect(nX, nY, m_eHitHandle);
	} else if (m_eHitHandle == HH_None) {
		UpdateCroppingRect(nX, nY, NULL, true);
	}
	if (m_bDoCropping || m_bDoTracking) {
		const CRect& clientRect = m_pMainDlg->ClientRect();
		if (nX >= clientRect.Width() - 1 || nX <= 0 || nY >= clientRect.Height() - 1 || nY <= 0 ) {
			::SetTimer(m_pMainDlg->GetHWND(), AUTOSCROLL_TIMER_EVENT_ID, AUTOSCROLL_TIMEOUT, NULL);
		}
	}
    return m_bTrackingMode;
}

void CCropCtl::EndCropping() {
	if (m_eHitHandle != HH_None) {
		m_bDontStartCropOnNextClick = true;
		m_bDoTracking = false;
		NormalizeCroppingRect();
		return;
	}
	bool bCropRectSmall = PointDifferenceSmall(m_cropMouse, m_pMainDlg->GetMousePos());
	if (m_pMainDlg->GetCurrentImage() == NULL || m_cropEnd == CPoint(INT_MIN, INT_MIN) || bCropRectSmall) {
		m_bCropping = false;
		m_bDoCropping = false;
		InvalidateSceenCropRect();
		if (bCropRectSmall && !m_pMainDlg->GetNavPanelCtl()->IsVisible()) {
			m_pMainDlg->MouseOn();
			m_pMainDlg->GetNavPanelCtl()->ShowNavPanelTemporary();
		} else {
			m_pMainDlg->GetNavPanelCtl()->HideNavPanelTemporary();
		}
		m_pMainDlg->GetZoomNavigatorCtl()->InvalidateZoomNavigatorRect();
		return;
	}

	// Display the crop menu
	ShowCropContextMenu();
}

void CCropCtl::AbortCropping() {
	m_bCropping = false;
	m_bDoCropping = false;
	m_bDoTracking = false;
	m_bTrackingMode = false;
	m_bDontStartCropOnNextClick = false;
	InvalidateSceenCropRect();
	m_cropEnd = CPoint(INT_MIN, INT_MIN);
	::SetCursor(::LoadCursor(NULL, MAKEINTRESOURCE(IDC_ARROW)));
	if (!m_pMainDlg->IsPlayingAnimation()) m_pMainDlg->GetNavPanelCtl()->HideNavPanelTemporary();
	m_pMainDlg->GetZoomNavigatorCtl()->InvalidateZoomNavigatorRect();
}

int CCropCtl::ShowCropContextMenu() {
	// Display the crop menu
	HMENU hMenu = ::LoadMenu(_Module.m_hInst, _T("CropMenu"));
	int nMenuCmd = 0;
	if (hMenu != NULL) {
		HMENU hMenuTrackPopup = ::GetSubMenu(hMenu, 0);
		int nSubMenuPosCropMode = SUBMENU_POS_CROPMODE;
		if (m_pMainDlg->GetCurrentImage()->GetImageFormat() != IF_JPEG) {
			::DeleteMenu(hMenuTrackPopup, IDM_LOSSLESS_CROP_SEL, MF_BYCOMMAND);
			nSubMenuPosCropMode--;
		}
		HelpersGUI::TranslateMenuStrings(hMenuTrackPopup);

		CPoint posMouse = m_pMainDlg->GetMousePos();
		m_pMainDlg->ClientToScreen(&posMouse);

		HMENU hMenuCropMode = ::GetSubMenu(hMenuTrackPopup, nSubMenuPosCropMode);
		if (abs(m_dCropRectAspectRatio - 1.25) < 0.001) {
			::CheckMenuItem(hMenuCropMode,  IDM_CROPMODE_5_4, MF_CHECKED);
		} else if (abs(m_dCropRectAspectRatio - 1.3333) < 0.001) {
			::CheckMenuItem(hMenuCropMode,  IDM_CROPMODE_4_3, MF_CHECKED);
		} else if (abs(m_dCropRectAspectRatio - 1.5) < 0.001) {
			::CheckMenuItem(hMenuCropMode,  IDM_CROPMODE_3_2, MF_CHECKED);
		} else if (abs(m_dCropRectAspectRatio - 1.7777) < 0.001) {
			::CheckMenuItem(hMenuCropMode,  IDM_CROPMODE_16_9, MF_CHECKED);
		} else if (abs(m_dCropRectAspectRatio - 1.6) < 0.001) {
			::CheckMenuItem(hMenuCropMode,  IDM_CROPMODE_16_10, MF_CHECKED);
		} else if (m_dCropRectAspectRatio < 0) {
			::CheckMenuItem(hMenuCropMode,  IDM_CROPMODE_FIXED_SIZE, MF_CHECKED);
		} else {
			::CheckMenuItem(hMenuCropMode,  IDM_CROPMODE_FREE, MF_CHECKED);
		}
		nMenuCmd = m_pMainDlg->TrackPopupMenu(posMouse, hMenuTrackPopup);

		if (m_bCropping) {
			m_pMainDlg->ExecuteCommand(nMenuCmd);
		}

		::DestroyMenu(hMenu);
	}

	InvalidateSceenCropRect();

	// Hack: When context menu was canceled by clicking outside of menu, the main window gets a mouse click event and a mouse move event.
	// This would start another crop what is not desired.
	if (nMenuCmd == 0) {
		m_bDontStartCropOnNextClick = true;
	} else if (nMenuCmd >= IDM_CROPMODE_FREE && nMenuCmd <= IDM_CROPMODE_16_10) {
		m_eHitHandle = HH_Border;
		float fX = (float)((nMenuCmd == IDM_CROPMODE_FIXED_SIZE) ? m_cropStart.x : m_cropEnd.x);
		float fY = (float)((nMenuCmd == IDM_CROPMODE_FIXED_SIZE) ? m_cropStart.y : m_cropEnd.y);
		m_pMainDlg->ImageToScreen(fX, fY);
		UpdateCroppingRect((int)(fX + 0.5f), (int)(fY + 0.5f), NULL, false);
		m_pMainDlg->Invalidate(FALSE);
		m_bDontStartCropOnNextClick = true;
		m_bDoCropping = false;
		m_bTrackingMode = true;
		return nMenuCmd;
	} else {
		m_cropEnd = CPoint(INT_MIN, INT_MIN);
		AbortCropping();
	}

	return nMenuCmd;
}

void CCropCtl::TrackCroppingRect(int nX, int nY, Handle eHandle) {
	CPoint trackPosInImage = ScreenToImage(nX, nY);
	int nDeltaX = trackPosInImage.x - m_startTrackMousePos.x;
	int nDeltaY = trackPosInImage.y - m_startTrackMousePos.y;

	if (nDeltaX == 0 && nDeltaY == 0) return;

	InvalidateSceenCropRect();

	switch (eHandle) {
		case HH_Border:
			m_cropStart = CPoint(m_cropStart.x + nDeltaX, m_cropStart.y + nDeltaY);
			m_cropEnd = CPoint(m_cropEnd.x + nDeltaX, m_cropEnd.y + nDeltaY);
			break;
		case HH_Right:
			m_cropEnd = PreserveAspectRatio(m_cropStart, CPoint(m_cropEnd.x + nDeltaX, m_cropEnd.y), true);
			break;
		case HH_Left:
			m_cropStart = PreserveAspectRatio(CPoint(m_cropStart.x + nDeltaX, m_cropStart.y), m_cropEnd, false);
			break;
		case HH_Top:
			m_cropStart = PreserveAspectRatio(CPoint(m_cropStart.x, m_cropStart.y + nDeltaY), m_cropEnd, false);
			break;
		case HH_Bottom:
			m_cropEnd = PreserveAspectRatio(m_cropStart, CPoint(m_cropEnd.x, m_cropEnd.y + nDeltaY), true);
			break;
		case HH_BottomRight:
			m_cropEnd = PreserveAspectRatio(m_cropStart, CPoint(m_cropEnd.x + nDeltaX, m_cropEnd.y + nDeltaY), true);
			break;
		case HH_TopRight:
			m_cropStart = PreserveAspectRatio(CPoint(m_cropStart.x, m_cropStart.y + nDeltaY), m_cropEnd, false);
			m_cropEnd = PreserveAspectRatio(m_cropStart, CPoint(m_cropEnd.x + nDeltaX, m_cropEnd.y), true);
			break;
		case HH_TopLeft:
			m_cropStart = PreserveAspectRatio(CPoint(m_cropStart.x + nDeltaX, m_cropStart.y + nDeltaY), m_cropEnd, false);
			break;
		case HH_BottomLeft:
			m_cropStart = PreserveAspectRatio(CPoint(m_cropStart.x + nDeltaX, m_cropStart.y), m_cropEnd, false);
			m_cropEnd = PreserveAspectRatio(m_cropStart, CPoint(m_cropEnd.x, m_cropEnd.y + nDeltaY), true);
			break;
	}

	InvalidateSceenCropRect();
	m_startTrackMousePos = trackPosInImage;
} 

void CCropCtl::UpdateCroppingRect(int nX, int nY, HDC hPaintDC, bool bShow) {
	float fX = (float)nX, fY = (float)nY;
	m_pMainDlg->ScreenToImage(fX, fY);

	CPoint newCropEnd = CPoint((int)fX, (int) fY);
	if (m_dCropRectAspectRatio > 0) {
		newCropEnd = PreserveAspectRatio(m_cropStart, newCropEnd, true);
	} else if (m_dCropRectAspectRatio < 0) {
		// fixed size crop mode
		CSize cropSize = CCropSizeDlg::GetCropSize();
		m_cropStart = CPoint((int)fX, (int) fY);
		if (CCropSizeDlg::UseScreenPixels() && m_pMainDlg->GetCurrentImage() != NULL) {
			double dZoom = m_pMainDlg->GetZoom();
			if (dZoom < 0.0) {
				dZoom = (double)m_pMainDlg->VirtualImageSize().cx/m_pMainDlg->GetCurrentImage()->OrigWidth();
			}
			newCropEnd = CPoint(m_cropStart.x + (int)(cropSize.cx/dZoom + 0.5) - 1, m_cropStart.y + (int)(cropSize.cy/dZoom + 0.5) - 1);
		} else {
			newCropEnd = CPoint((int)fX + cropSize.cx - 1, (int)fY + cropSize.cy - 1);
		}
	}

	if (m_bCropping) {
		m_bDoCropping = true;
	}
	if (bShow && m_cropEnd == CPoint(INT_MIN, INT_MIN)) {
		m_pMainDlg->InvalidateRect(m_pMainDlg->GetNavPanelCtl()->PanelRect(), FALSE);
	}
	if (m_cropEnd != newCropEnd) {
		if (bShow) InvalidateSceenCropRect();
		m_cropEnd = newCropEnd;
		if (bShow) InvalidateSceenCropRect();
	}
}

static void PaintHandle(HDC hDC, int x, int y, int nSize) {
	int nOffset = nSize >> 1;
	::SelectObject(hDC, ::GetStockObject(NULL_PEN));
	::SelectObject(hDC, ::GetStockObject(WHITE_BRUSH));
	::Rectangle(hDC, x - nOffset, y - nOffset, x - nOffset + nSize, y - nOffset + nSize);
	::SelectObject(hDC, ::GetStockObject(BLACK_BRUSH));
	::Rectangle(hDC, x - nOffset + 1, y - nOffset + 1, x - nOffset + nSize - 1, y - nOffset + nSize - 1);
}

void CCropCtl::PaintCropRect(HDC hPaintDC) {
	CRect cropRect = GetScreenCropRect();
	if (!cropRect.IsRectEmpty()) {
		HDC hDC = (hPaintDC == NULL) ? m_pMainDlg->GetDC() : hPaintDC;
		::SelectObject(hDC, ::GetStockObject(HOLLOW_BRUSH));
		::SelectObject(hDC, ::GetStockObject(BLACK_PEN));
		::Rectangle(hDC, cropRect.left, cropRect.top, cropRect.right, cropRect.bottom);
		HPEN hPen = ::CreatePen(PS_DOT, 1, RGB(255, 255, 255));
		HGDIOBJ hOldPen = ::SelectObject(hDC, hPen);
		::SetBkMode(hDC, TRANSPARENT);
		::Rectangle(hDC, cropRect.left, cropRect.top, cropRect.right, cropRect.bottom);

		if (m_dCropRectAspectRatio >= 0) {
			if (cropRect.Width() > m_nHandleSize * 2 && cropRect.Height() > m_nHandleSize * 2) {
				PaintHandle(hDC, cropRect.left, cropRect.top, m_nHandleSize);
				PaintHandle(hDC, cropRect.left, cropRect.bottom, m_nHandleSize);
				PaintHandle(hDC, cropRect.right, cropRect.top, m_nHandleSize);
				PaintHandle(hDC, cropRect.right, cropRect.bottom, m_nHandleSize);
				if (cropRect.Width() > m_nHandleSize * 10 && cropRect.Height() > m_nHandleSize * 10) {
					int nMiddleX = cropRect.CenterPoint().x;
					int nMiddleY = cropRect.CenterPoint().y;
					PaintHandle(hDC, nMiddleX, cropRect.top, m_nHandleSize);
					PaintHandle(hDC, nMiddleX, cropRect.bottom, m_nHandleSize);
					PaintHandle(hDC, cropRect.left, nMiddleY, m_nHandleSize);
					PaintHandle(hDC, cropRect.right, nMiddleY, m_nHandleSize);
				}
			}
		}

		::SelectObject(hDC, hOldPen);
		::DeleteObject(hPen);
		if (hPaintDC == NULL) {
			m_pMainDlg->ReleaseDC(hDC);
		}
	}
}

void CCropCtl::InvalidateSceenCropRect() {
	CRect cropRect = GetScreenCropRect();
	if (!cropRect.IsRectEmpty()) {
		int nHandleOffset = m_nHandleSize / 2 + 1;
		m_pMainDlg->InvalidateRect(&CRect(cropRect.left-nHandleOffset, cropRect.top-nHandleOffset, cropRect.left+nHandleOffset, cropRect.bottom+nHandleOffset), FALSE);
		m_pMainDlg->InvalidateRect(&CRect(cropRect.right-nHandleOffset, cropRect.top-nHandleOffset, cropRect.right+nHandleOffset, cropRect.bottom+nHandleOffset), FALSE);
		m_pMainDlg->InvalidateRect(&CRect(cropRect.left, cropRect.top-nHandleOffset, cropRect.right, cropRect.top+nHandleOffset), FALSE);
		m_pMainDlg->InvalidateRect(&CRect(cropRect.left, cropRect.bottom-nHandleOffset, cropRect.right, cropRect.bottom+nHandleOffset), FALSE);
	}
}

CRect CCropCtl::GetScreenCropRect() {
	if (m_cropEnd != CPoint(INT_MIN, INT_MIN) && m_pMainDlg->GetCurrentImage() != NULL) {
		CPoint cropStart(min(m_cropStart.x, m_cropEnd.x), min(m_cropStart.y, m_cropEnd.y));
		CPoint cropEnd(max(m_cropStart.x, m_cropEnd.x), max(m_cropStart.y, m_cropEnd.y));

		float fXStart = (float)cropStart.x;
		float fYStart = (float)cropStart.y;
		m_pMainDlg->ImageToScreen(fXStart, fYStart);
		float fXEnd = cropEnd.x + 0.999f;
		float fYEnd = cropEnd.y + 0.999f;
		m_pMainDlg->ImageToScreen(fXEnd, fYEnd);
		return CRect((int)fXStart, (int)fYStart, (int)fXEnd, (int)fYEnd);

	} else {
		return CRect(0, 0, 0, 0);
	}
}

CRect CCropCtl::GetImageCropRect() {
	CJPEGImage* pCurrentImage = m_pMainDlg->GetCurrentImage();
	if (pCurrentImage == NULL) {
		return CRect(0, 0, 0, 0);
	} else {
		return CRect(max(0, min(m_cropStart.x, m_cropEnd.x)), max(0, min(m_cropStart.y, m_cropEnd.y)),
			min(pCurrentImage->OrigWidth(), max(m_cropStart.x, m_cropEnd.x) + 1), min(pCurrentImage->OrigHeight(), max(m_cropStart.y, m_cropEnd.y) + 1));
	}
}

void CCropCtl::CropLossless() {
	CRect cropRect = GetImageCropRect();
	if (cropRect.IsRectEmpty()) {
		return;
	}
    CJPEGImage* pCurrentImage = m_pMainDlg->GetCurrentImage();
    pCurrentImage->TrimRectToMCUBlockSize(cropRect);

	CString sCurrentFile = m_pMainDlg->CurrentFileName(false);

	CFileDialog fileDlg(FALSE, _T(".jpg"), sCurrentFile, 
			OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_NOREADONLYRETURN | OFN_OVERWRITEPROMPT,
			Helpers::CReplacePipe(CString(_T("JPEG (*.jpg;*.jpeg)|*.jpg;*.jpeg|")) +
			CNLS::GetString(_T("All Files")) + _T("|*.*|")), m_pMainDlg->GetHWND());

	CString sDimension;
	sDimension.Format(_T(" (%d x %d)"), cropRect.Width(), cropRect.Height());
	CString sTitle = CNLS::GetString(_T("Save cropped image")) + sDimension;
	fileDlg.m_ofn.lpstrTitle = sTitle;

	if (IDOK == fileDlg.DoModal(m_pMainDlg->GetHWND())) {
        LPCTSTR sInputFileName = m_pMainDlg->CurrentFileName(false);
        LPCTSTR sCropFileName = fileDlg.m_szFileName;
        CJPEGLosslessTransform::EResult eResult = CJPEGLosslessTransform::PerformCrop(sInputFileName, sCropFileName, cropRect);
        if (eResult != CJPEGLosslessTransform::Success) {
            ::MessageBox(m_pMainDlg->GetHWND(), CString(CNLS::GetString(_T("Performing the lossless transformation failed!"))) + 
                + _T("\n") + CNLS::GetString(_T("Reason:")) + _T(" ") + HelpersGUI::LosslessTransformationResultToString(eResult), 
                CNLS::GetString(_T("Lossless JPEG transformations")), MB_OK | MB_ICONWARNING);
        } else {
            if (_tcscmp(sInputFileName, sCropFileName) == 0) {
                m_pMainDlg->ExecuteCommand(IDM_RELOAD);
            } else {
                m_pMainDlg->GetFileList()->Reload();
            }
        }

	}
}

static bool IsClose(int xFrom, int yFrom, int xTo, int yTo, int distance) {
	return (xFrom - xTo)*(xFrom - xTo) + (yFrom - yTo)*(yFrom - yTo) < distance*distance;
}

CCropCtl::Handle CCropCtl::HitHandle(int nX, int nY) {
	CRect cropRect = GetScreenCropRect();
	if (!cropRect.IsRectNull() &&
		nX > cropRect.left - HANDLE_HIT_TOLERANCE && nX < cropRect.right + HANDLE_HIT_TOLERANCE && 
		nY > cropRect.top - HANDLE_HIT_TOLERANCE && nY < cropRect.bottom + HANDLE_HIT_TOLERANCE &&
		(nX < cropRect.left + HANDLE_HIT_TOLERANCE || nX > cropRect.right - HANDLE_HIT_TOLERANCE ||
		nY < cropRect.top + HANDLE_HIT_TOLERANCE || nY > cropRect.bottom - HANDLE_HIT_TOLERANCE)) {
		if (m_dCropRectAspectRatio < 0) {
			return HH_Border;
		}
		int nWidth3 = cropRect.Width() / 3;
		int nHeight3 = cropRect.Height() / 3;
		if (nX < cropRect.left + nWidth3) {
			if (nY < cropRect.top + nHeight3) {
				return IsClose(nX, nY, cropRect.left, cropRect.top, HANDLE_HIT_TOLERANCE) ? HH_TopLeft : HH_Border;
			} else if (nY < cropRect.top + nHeight3*2) {
				return IsClose(nX, nY, cropRect.left, (cropRect.top + cropRect.bottom)/2, HANDLE_HIT_TOLERANCE) ? HH_Left : HH_Border;
			} else {
				return IsClose(nX, nY, cropRect.left, cropRect.bottom, HANDLE_HIT_TOLERANCE) ? HH_BottomLeft : HH_Border;
			}
		} else if (nX < cropRect.left + nWidth3*2) {
			if (nY < cropRect.top + nHeight3) {
				return IsClose(nX, nY, (cropRect.left + cropRect.right)/2, cropRect.top, HANDLE_HIT_TOLERANCE) ? HH_Top : HH_Border;
			} else {
				return IsClose(nX, nY, (cropRect.left + cropRect.right)/2, cropRect.bottom, HANDLE_HIT_TOLERANCE) ? HH_Bottom : HH_Border;
			}
		} else {
			if (nY < cropRect.top + nHeight3) {
				return IsClose(nX, nY, cropRect.right, cropRect.top, HANDLE_HIT_TOLERANCE) ? HH_TopRight : HH_Border;
			} else if (nY < cropRect.top + nHeight3*2) {
				return IsClose(nX, nY, cropRect.right, (cropRect.top + cropRect.bottom)/2, HANDLE_HIT_TOLERANCE) ? HH_Right : HH_Border;
			} else {
				return IsClose(nX, nY, cropRect.right, cropRect.bottom, HANDLE_HIT_TOLERANCE) ? HH_BottomRight : HH_Border;
			}
		}
	} else {
		return HH_None;
	}
}

CPoint CCropCtl::ScreenToImage(int nX, int nY) {
	float fX = (float)nX, fY = (float)nY;
	m_pMainDlg->ScreenToImage(fX, fY);
	return CPoint((int)fX, (int)fY);
}

CPoint CCropCtl::PreserveAspectRatio(CPoint cropStart, CPoint cropEnd, bool bCalculateEnd) {
	if (m_dCropRectAspectRatio > 0) {
		CPoint newCropEnd;
		int w = abs(cropStart.x - cropEnd.x);
		int h = abs(cropStart.y - cropEnd.y);
		double dRatio = (h < w) ? 1.0/m_dCropRectAspectRatio : m_dCropRectAspectRatio;
		int newH = (int)(w * dRatio + 0.5);
		int newW = (int)(h * (1.0/dRatio) + 0.5);
		if (w > h) {
			if (cropStart.y > cropEnd.y) {
				newCropEnd = CPoint(cropEnd.x, cropStart.y - newH);
			} else {
				newCropEnd = CPoint(cropEnd.x, cropStart.y + newH);
			}
		} else {
			if (cropStart.x > cropEnd.x) {
				newCropEnd = CPoint(cropStart.x - newW, cropEnd.y);
			} else {
				newCropEnd = CPoint(cropStart.x + newW, cropEnd.y);
			}
		}
		if (bCalculateEnd) {
			return newCropEnd;
		} else {
			return CPoint(cropStart.x - (newCropEnd.x - cropEnd.x), cropStart.y - (newCropEnd.y - cropEnd.y));
		}
	} else {
		return bCalculateEnd ? cropEnd : cropStart;
	}
}

void CCropCtl::NormalizeCroppingRect() {
	if (m_cropStart.x > m_cropEnd.x) {
		LONG t = m_cropStart.x;
		m_cropStart.x = m_cropEnd.x;
		m_cropEnd.x = t;
	}
	if (m_cropStart.y > m_cropEnd.y) {
		LONG t = m_cropStart.y;
		m_cropStart.y = m_cropEnd.y;
		m_cropEnd.y = t;
	}
}

void CCropCtl::SetMouseCursor(int nX, int nY) {
	if (m_bDoTracking)	{
		return;
	}
	Handle handle = HitHandle(nX, nY);
	LPTSTR cursorId;
	switch (handle) {
		case HH_Border:
			cursorId = IDC_SIZEALL;
			break;
		case HH_TopLeft:
		case HH_BottomRight:
			cursorId = IDC_SIZENWSE;
			break;
		case HH_TopRight:
		case HH_BottomLeft:
			cursorId = IDC_SIZENESW;
			break;
		case HH_Left:
		case HH_Right:
			cursorId = IDC_SIZEWE;
			break;
		case HH_Top:
		case HH_Bottom:
			cursorId = IDC_SIZENS;
			break;
		default:
			cursorId = IDC_ARROW;
			break;
	}
	::SetCursor(::LoadCursor(NULL, cursorId));
}