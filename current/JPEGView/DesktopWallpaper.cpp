#include "StdAfx.h"
#include "DesktopWallpaper.h"
#include "JPEGImage.h"
#include "SaveImage.h"
#include "MultiMonitorSupport.h"
#include "SettingsProvider.h"

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
		bool needsFitToScreen = image.InitOrigWidth() > largestMonitor.Width() || image.InitOrigHeight() > largestMonitor.Height();
		SetRegistryStringValue(_T("WallpaperStyle"), needsFitToScreen ? _T("6") : _T("0"));
		SetRegistryStringValue(_T("TileWallpaper"), _T("0"));

		void* parameter = (void*)fileName;
		::SystemParametersInfo(SPI_SETDESKWALLPAPER,
			0,
			parameter,
			SPIF_UPDATEINIFILE | SPIF_SENDWININICHANGE);
	}

	void SetProcessedImage(CJPEGImage& image)
	{
		bool bitmapMatchesDesktop = false;
		int windowsVersion = Helpers::GetWindowsVersion();
		LPCTSTR wallpaperStyle = _T("0");
		LPCTSTR tileWallpaper = _T("0");
		if (windowsVersion >= 601) {
			// Check if image spans all screens
			CRect allScreens = CMultiMonitorSupport::GetVirtualDesktop();
			bitmapMatchesDesktop = (allScreens.Size() == CSize(image.DIBWidth(), image.DIBHeight()));
			if (bitmapMatchesDesktop && windowsVersion >= 602) wallpaperStyle = _T("22"); // for Windows 8 ff
			if (bitmapMatchesDesktop && windowsVersion == 601) tileWallpaper = _T("1"); // for Windows 7
		}
		SetRegistryStringValue(_T("WallpaperStyle"), wallpaperStyle);
		SetRegistryStringValue(_T("TileWallpaper"), tileWallpaper);

        LPCTSTR path = CSettingsProvider::This().WallpaperPath();
        if (path[0] == 0) return;

        CString fileName = CString(path) + _T("\\JPEGViewWallpaper.bmp");

		if (!CSaveImage::SaveImage(fileName, &image, false)) return;

		void* parameter = (void*)(LPCTSTR)fileName;
		::SystemParametersInfo(SPI_SETDESKWALLPAPER,
			0,
			parameter,
			SPIF_UPDATEINIFILE | SPIF_SENDWININICHANGE);
	}
}