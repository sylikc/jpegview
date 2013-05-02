#include "StdAfx.h"
#include "MultiMonitorSupport.h"
#include "SettingsProvider.h"

struct EnumMonitorParams {
	EnumMonitorParams(int nIndexMonitor) {
		IndexMonitor = nIndexMonitor;
		NumPixels = 0;
		Iterations = 0;
		rectMonitor = CRect(0, 0, 0, 0);
	}

	int Iterations;
	int IndexMonitor;
	int NumPixels;
	CRect rectMonitor;
};

static CRect defaultWindowRect = CRect(0, 0, 0, 0);

bool CMultiMonitorSupport::IsMultiMonitorSystem() {
	return ::GetSystemMetrics(SM_CMONITORS) > 1;
}

CRect CMultiMonitorSupport::GetVirtualDesktop() {
	return CRect(CPoint(::GetSystemMetrics(SM_XVIRTUALSCREEN), ::GetSystemMetrics(SM_YVIRTUALSCREEN)),
		CSize(::GetSystemMetrics(SM_CXVIRTUALSCREEN), ::GetSystemMetrics(SM_CYVIRTUALSCREEN)));
}

// Callback called during enumaration of monitors
static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	MONITORINFO monitorInfo;
	monitorInfo.cbSize = sizeof(MONITORINFO);
	EnumMonitorParams* pParams = (EnumMonitorParams*) dwData;
	if (pParams->IndexMonitor == -1) {
		// Use the monitor with largest number of pixels
		int nNumPixels = (lprcMonitor->right - lprcMonitor->left)*(lprcMonitor->bottom - lprcMonitor->top);
		if (nNumPixels > pParams->NumPixels) {
			pParams->NumPixels = nNumPixels;
			pParams->rectMonitor = CRect(lprcMonitor);
		} else if (nNumPixels == pParams->NumPixels) {
			// if same size take primary
			::GetMonitorInfo(hMonitor, &monitorInfo);
			if (monitorInfo.dwFlags & MONITORINFOF_PRIMARY) {
				pParams->rectMonitor = CRect(lprcMonitor);
			}
		}
	} else {
		::GetMonitorInfo(hMonitor, &monitorInfo);
		if (pParams->IndexMonitor == 0) {
			// take primary monitor
			if (monitorInfo.dwFlags & MONITORINFOF_PRIMARY) {
				pParams->rectMonitor = CRect(lprcMonitor);
				return FALSE;
			}
		} else {
			// take the i-th non primary monitor
			if (!(monitorInfo.dwFlags & MONITORINFOF_PRIMARY)) {
				pParams->Iterations += 1;
				if (pParams->IndexMonitor == pParams->Iterations) {
					pParams->rectMonitor = CRect(lprcMonitor);
					return FALSE;
				}
			}
		}
	}
	return TRUE;
}

CRect CMultiMonitorSupport::GetMonitorRect(int nIndex) {
	if (!CMultiMonitorSupport::IsMultiMonitorSystem()) {
		return CRect(0, 0, ::GetSystemMetrics(SM_CXSCREEN), ::GetSystemMetrics(SM_CYSCREEN));
	}
	EnumMonitorParams params(nIndex);
	::EnumDisplayMonitors(NULL, NULL, &MonitorEnumProc, (LPARAM) &params);
	if (params.rectMonitor.Width() == 0) {
		return CRect(0, 0, ::GetSystemMetrics(SM_CXSCREEN), ::GetSystemMetrics(SM_CYSCREEN));
	} else {
		return params.rectMonitor;
	}
}

CRect CMultiMonitorSupport::GetMonitorRect(HWND hWnd) {
	HMONITOR hMonitor = ::MonitorFromWindow(hWnd, MONITOR_DEFAULTTOPRIMARY);
	MONITORINFO monitorInfo;
	monitorInfo.cbSize = sizeof(MONITORINFO);
	::GetMonitorInfo(hMonitor, &monitorInfo);
	return CRect(monitorInfo.rcMonitor);
}

CRect CMultiMonitorSupport::GetWorkingRect(HWND hWnd) {
	HMONITOR hMonitor = ::MonitorFromWindow(hWnd, MONITOR_DEFAULTTOPRIMARY);
	MONITORINFO monitorInfo;
	monitorInfo.cbSize = sizeof(MONITORINFO);
	::GetMonitorInfo(hMonitor, &monitorInfo);
	return CRect(monitorInfo.rcWork);
}

CRect CMultiMonitorSupport::GetDefaultWindowRect() {
    CSettingsProvider& settings = CSettingsProvider::This();
	CRect windowRect = !defaultWindowRect.IsRectEmpty() ? defaultWindowRect : settings.StickyWindowSize() ? settings.StickyWindowRect() : settings.DefaultWindowRect();
	CRect rectAllScreens = CMultiMonitorSupport::GetVirtualDesktop();
	if (windowRect.IsRectEmpty() || !rectAllScreens.IntersectRect(&rectAllScreens, &windowRect)) {
		CRect monitorRect = CMultiMonitorSupport::GetMonitorRect(settings.DisplayMonitor());
		int nDesiredWidth = monitorRect.Width()*2/3;
		int nDesiredHeight = nDesiredWidth*3/4;
		nDesiredWidth += ::GetSystemMetrics(SM_CXSIZEFRAME) * 2;
		nDesiredHeight += ::GetSystemMetrics(SM_CYSIZEFRAME) * 2 + ::GetSystemMetrics(SM_CYCAPTION);
		windowRect = CRect(CPoint(monitorRect.left + (monitorRect.Width() - nDesiredWidth) / 2, monitorRect.top + (monitorRect.Height() - nDesiredHeight) / 2), CSize(nDesiredWidth, nDesiredHeight));
	}
	return windowRect;
}

void CMultiMonitorSupport::SetDefaultWindowRect(CRect rect) {
    defaultWindowRect = rect;
}

CRect CMultiMonitorSupport::GetDefaultClientRectInWindowMode(bool bAutoFitWndToImage) {
	if (bAutoFitWndToImage) {
		CRect monitorRect = CMultiMonitorSupport::GetMonitorRect(CSettingsProvider::This().DisplayMonitor());
		return CRect(0, 0, monitorRect.Width(), monitorRect.Height());
	}
	CRect wndRect = CMultiMonitorSupport::GetDefaultWindowRect();
	int nBorderWidth = ::GetSystemMetrics(SM_CXSIZEFRAME) * 2;
	int nBorderHeight = ::GetSystemMetrics(SM_CYSIZEFRAME) * 2 + ::GetSystemMetrics(SM_CYCAPTION);
	return CRect(0, 0, wndRect.Width() - nBorderWidth, wndRect.Height() - nBorderHeight);
}
