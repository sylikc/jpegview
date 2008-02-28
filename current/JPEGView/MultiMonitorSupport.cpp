#include "StdAfx.h"
#include "MultiMonitorSupport.h"

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
