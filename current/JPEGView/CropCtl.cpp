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
#include "UserCommand.h"
#include <math.h>

static const int AUTOSCROLL_TIMEOUT = 20; // autoscroll time in ms

CCropCtl::CCropCtl(CMainDlg* pMainDlg) {
	m_pMainDlg = pMainDlg;
	m_bCropping = false;
	m_bDoCropping = false;
	m_bDontStartCropOnNextClick = false;
	m_bBlockPaintCropRect = false;
	m_dCropRectAspectRatio = 0;
	m_cropStart = CPoint(INT_MIN, INT_MIN);
	m_cropEnd = CPoint(INT_MIN, INT_MIN);
	m_cropMouse = CPoint(INT_MIN, INT_MIN);
}

void CCropCtl::OnPaint(CPaintDC& dc) {
	if (m_bDoCropping && m_bCropping && !m_bBlockPaintCropRect) {
		CPoint mousePos = m_pMainDlg->GetMousePos();
		ShowCroppingRect(mousePos.x, mousePos.y, NULL, false);
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
	if (!m_pMainDlg->GetPanelMgr()->IsModalPanelShown() && !m_bDontStartCropOnNextClick) {
		m_bCropping = true;
		float fX = (float)nX, fY = (float)nY;
		m_pMainDlg->ScreenToImage(fX, fY);
		m_cropStart = CPoint((int)fX, (int) fY);
		m_cropMouse = CPoint(nX, nY);
		m_cropEnd = CPoint(INT_MIN, INT_MIN);
	}
}

void CCropCtl::DoCropping(int nX, int nY) {
	ShowCroppingRect(nX, nY, NULL, true);
	const CRect& clientRect = m_pMainDlg->ClientRect();
	if (nX >= clientRect.Width() - 1 || nX <= 0 || nY >= clientRect.Height() - 1 || nY <= 0 ) {
		::SetTimer(m_pMainDlg->GetHWND(), AUTOSCROLL_TIMER_EVENT_ID, AUTOSCROLL_TIMEOUT, NULL);
	}
}

void CCropCtl::EndCropping() {
	if (m_pMainDlg->GetCurrentImage() == NULL || m_cropEnd == CPoint(INT_MIN, INT_MIN) || m_cropMouse == m_pMainDlg->GetMousePos()) {
		m_bCropping = false;
		m_bDoCropping = false;
		DeleteSceenCropRect();
		m_pMainDlg->GetNavPanelCtl()->HideNavPanelTemporary();
		m_pMainDlg->GetZoomNavigatorCtl()->InvalidateZoomNavigatorRect();
		return;
	}

	// Display the crop menu
	HMENU hMenu = ::LoadMenu(_Module.m_hInst, _T("CropMenu"));
	int nMenuCmd = 0;
	if (hMenu != NULL) {
		HMENU hMenuTrackPopup = ::GetSubMenu(hMenu, 0);
		if (m_pMainDlg->GetCurrentImage()->GetImageFormat() != CJPEGImage::IF_JPEG) {
			::DeleteMenu(hMenuTrackPopup, IDM_LOSSLESS_CROP_SEL, MF_BYCOMMAND);
		}
		HelpersGUI::TranslateMenuStrings(hMenuTrackPopup);

		CPoint posMouse = m_pMainDlg->GetMousePos();
		m_pMainDlg->ClientToScreen(&posMouse);

		HMENU hMenuCropMode = ::GetSubMenu(hMenuTrackPopup, SUBMENU_POS_CROPMODE);
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

		// Hack: When context menu was canceled by clicking outside of menu, the main window gets a mouse click event and a mouse move event.
		// This would start another crop what is not desired.
		if (nMenuCmd == 0) {
			m_bDontStartCropOnNextClick = true;
		}
	}

	if (m_bCropping && nMenuCmd != IDM_COPY_SEL && nMenuCmd != IDM_CROP_SEL) {
		DeleteSceenCropRect();
	}
	m_bCropping = false;
	m_bDoCropping = false;
	m_pMainDlg->GetNavPanelCtl()->HideNavPanelTemporary();
	m_pMainDlg->GetZoomNavigatorCtl()->InvalidateZoomNavigatorRect();
}

void CCropCtl::ShowCroppingRect(int nX, int nY, HDC hPaintDC, bool bShow) {
	if (bShow) {
		DeleteSceenCropRect();
	}
	float fX = (float)nX, fY = (float)nY;
	m_pMainDlg->ScreenToImage(fX, fY);

	CPoint newCropEnd = CPoint((int)fX, (int) fY);
	if (m_dCropRectAspectRatio > 0) {
		// fixed ratio crop mode
		int w = abs(m_cropStart.x - newCropEnd.x);
		int h = abs(m_cropStart.y - newCropEnd.y);
		double dRatio = (h < w) ? 1.0/m_dCropRectAspectRatio : m_dCropRectAspectRatio;
		int newH = (int)(w * dRatio + 0.5);
		int newW = (int)(h * (1.0/dRatio) + 0.5);
		if (w > h) {
			if (m_cropStart.y > newCropEnd.y) {
				newCropEnd = CPoint(newCropEnd.x, m_cropStart.y - newH);
			} else {
				newCropEnd = CPoint(newCropEnd.x, m_cropStart.y + newH);
			}
		} else {
			if (m_cropStart.x > newCropEnd.x) {
				newCropEnd = CPoint(m_cropStart.x - newW, newCropEnd.y);
			} else {
				newCropEnd = CPoint(m_cropStart.x + newW, newCropEnd.y);
			}
		}
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
	m_cropEnd = newCropEnd;
	if (bShow) {
		PaintCropRect(hPaintDC);
	}
}

void CCropCtl::PaintCropRect(HDC hPaintDC) {
	CRect cropRect = GetScreenCropRect();
	if (!cropRect.IsRectEmpty()) {
		HDC hDC = (hPaintDC == NULL) ? m_pMainDlg->GetDC() : hPaintDC;
		HPEN hPen = ::CreatePen(PS_DOT, 1, RGB(255, 255, 255));
		HGDIOBJ hOldPen = ::SelectObject(hDC, hPen);
		::SelectObject(hDC, ::GetStockObject(HOLLOW_BRUSH));
		::SetBkMode(hDC, OPAQUE);
		::SetBkColor(hDC, 0);
		::Rectangle(hDC, cropRect.left, cropRect.top, cropRect.right, cropRect.bottom);

		::SelectObject(hDC, hOldPen);
		::DeleteObject(hPen);
		if (hPaintDC == NULL) {
			m_pMainDlg->ReleaseDC(hDC);
		}
	}
}

void CCropCtl::DeleteSceenCropRect() {
	CRect cropRect = GetScreenCropRect();
	if (!cropRect.IsRectEmpty()) {
		m_pMainDlg->InvalidateRect(&CRect(cropRect.left-1, cropRect.top, cropRect.left+1, cropRect.bottom), FALSE);
		m_pMainDlg->InvalidateRect(&CRect(cropRect.right-1, cropRect.top, cropRect.right+1, cropRect.bottom), FALSE);
		m_pMainDlg->InvalidateRect(&CRect(cropRect.left, cropRect.top-1, cropRect.right, cropRect.top+1), FALSE);
		m_pMainDlg->InvalidateRect(&CRect(cropRect.left, cropRect.bottom-1, cropRect.right, cropRect.bottom+1), FALSE);
		bool bOldFlag = m_bBlockPaintCropRect;
		m_bBlockPaintCropRect = true;
		m_pMainDlg->UpdateWindow();
		m_bBlockPaintCropRect = bOldFlag;
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
	// coordinates must be multiples of 8 for lossless crop
	cropRect.left = cropRect.left & ~7;
	cropRect.right = (cropRect.right + 7) & ~7;
	cropRect.top = cropRect.top & ~7;
	cropRect.bottom = (cropRect.bottom + 7) & ~7;

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
		CString sCmd(_T("KeyCode: 0  Cmd: 'jpegtran -crop %w%x%h%+%x%+%y% -copy all -perfect %filename% \"%outfilename%\"' Flags: 'WaitForTerminate NoWindow ReloadFileList'"));
		sCmd.Replace(_T("%outfilename%"), fileDlg.m_szFileName);
		CUserCommand cmdCrop(sCmd);
		if (cmdCrop.Execute(m_pMainDlg->GetHWND(), m_pMainDlg->CurrentFileName(false), cropRect)) {
			if (_tcscmp(m_pMainDlg->CurrentFileName(false), fileDlg.m_szFileName) == 0) {
				m_pMainDlg->ExecuteCommand(IDM_RELOAD);
			}
		}
	}
}