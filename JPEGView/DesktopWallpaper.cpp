#include "StdAfx.h"
#include "DesktopWallpaper.h"
#include "JPEGImage.h"
#include "SaveImage.h"
#include "MultiMonitorSupport.h"

namespace SetDesktopWallpaper {

	// Sets a string value in the registry in the path HKEY_CURRENT_USER\Control Panel\Desktop
	static bool SetRegistryStringValue(LPCTSTR name, LPCTSTR stringValue) {
		HKEY key;
		if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Control Panel\\Desktop"), 0, KEY_READ | KEY_WRITE, &key) != ERROR_SUCCESS)
			return false;
		return RegSetValueEx(key, name, 0, REG_SZ, (const BYTE *)stringValue, ((int)_tcslen(stringValue) + 1) * sizeof(TCHAR)) == ERROR_SUCCESS;
	}

	void SetFileAsWallpaper(CJPEGImage& image, LPCTSTR fileName)
	{
		CRect largestMonitor = CMultiMonitorSupport::GetMonitorRect(-1);
		bool needsFill = image.InitOrigWidth() > largestMonitor.Width() || image.InitOrigHeight() > largestMonitor.Height();
		SetRegistryStringValue(_T("WallpaperStyle"), needsFill ? _T("1") : _T("0"));
		SetRegistryStringValue(_T("TileWallpaper"), _T("0"));

		void* parameter = (void*)fileName;
		::SystemParametersInfo(SPI_SETDESKWALLPAPER,
			0,
			parameter,
			SPIF_UPDATEINIFILE | SPIF_SENDWININICHANGE);
	}

	void SetProcessedImage(CJPEGImage& image)
	{
		SetRegistryStringValue(_T("WallpaperStyle"), _T("0"));
		SetRegistryStringValue(_T("TileWallpaper"), _T("0"));

		TCHAR tempPath[MAX_PATH];
		if (0 == ::GetTempPath(MAX_PATH, tempPath)) return;

		CString fileName = CString(tempPath) + _T("JPEGViewWallpaper.bmp");

		if (!CSaveImage::SaveImage(fileName, &image, false)) return;

		void* parameter = (void*)(LPCTSTR)fileName;
		::SystemParametersInfo(SPI_SETDESKWALLPAPER,
			0,
			parameter,
			SPIF_UPDATEINIFILE | SPIF_SENDWININICHANGE);
	}
}