// JPEGView.cpp : main entry point for JPEGView.exe

#include "stdafx.h"
#include <locale.h>
#include <gdiplus.h>
#include "resource.h"
#include "MainDlg.h"
#include "SettingsProvider.h"

#ifdef DEBUG
#include <dbghelp.h>
#endif

// _CrtDumpMemoryLeaks

CAppModule _Module;

static HWND _HWNDOtherInstance = NULL;

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
	const int BUF_LEN = 255;
	TCHAR buff[BUF_LEN + 1];
	buff[BUF_LEN] = 0;
	::GetWindowText(hwnd, (LPTSTR)&buff, BUF_LEN);
	if (_tcsstr(buff, _T(" - JPEGView")) != NULL) {
		_HWNDOtherInstance = hwnd;
		return FALSE;
	}
	return TRUE;
}

static CString ParseCommandLineForStartupFile(LPCTSTR sCommandLine) { 
	CString sStartupFile = sCommandLine;
	if (sStartupFile.GetLength() == 0) {
		return CString(_T(""));
	}
	if (sCommandLine[0] == _T('/')) { // parameter
		return CString(_T(""));
	}

	// Extract the startup file or directory from the command line parameter string
	int nHyphen = sStartupFile.Find(_T('"'));
	if (nHyphen == -1) {
		// not enclosed with "", use part until first space is found
		int nSpace = sStartupFile.Find(_T(' '));
		if (nSpace > 0) {
			sStartupFile = sStartupFile.Left(nSpace);
		} // else we use the whole command line (as there is no space character in it)
	} else {
		// enclosed in "", take first string enclosed in ""
		int nHyphenNext = sStartupFile.Find(_T('"'), nHyphen + 1);
		if (nHyphenNext > nHyphen) {
			sStartupFile = sStartupFile.Mid(nHyphen + 1, nHyphenNext - nHyphen - 1);
		}
	}
	// if it's a directory, add a trailing backslash
	if (sStartupFile.GetLength() > 0) {
		DWORD nAttributes = ::GetFileAttributes(sStartupFile);
		if (nAttributes != INVALID_FILE_ATTRIBUTES && (nAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
			if (sStartupFile[sStartupFile.GetLength() -1] != _T('\\')) {
				sStartupFile += _T('\\');
			}
		}
	}

	if (sStartupFile.Find(_T('\\')) == -1)
		return _T(".\\") + sStartupFile;

	return sStartupFile;
}

static int ParseCommandLineForAutostart(LPCTSTR sCommandLine) {
	LPCTSTR sAutoStart = Helpers::stristr(sCommandLine, _T("/slideshow"));
	if (sAutoStart == NULL) {
		return 0;
	}
	int intervall = _ttoi(sAutoStart + _tcslen(_T("/slideshow")));
	return (intervall == 0) ? 5 : intervall;
}

static bool ParseCommandLineForFullScreen(LPCTSTR sCommandLine) {
	return Helpers::stristr(sCommandLine, _T("/fullscreen")) != NULL;
}

static bool ParseCommandLineForAutoExit(LPCTSTR sCommandLine) {
	return Helpers::stristr(sCommandLine, _T("/autoexit")) != NULL;
}

static int ParseCommandLineForDisplayMonitor(LPCTSTR sCommandLine) {
	LPCTSTR sMonitor = Helpers::stristr(sCommandLine, _T("/monitor"));
	if (sMonitor == NULL) {
		return -1;
	}
	return _ttoi(sMonitor + _tcslen(_T("/monitor")));
}

static Helpers::ESorting ParseCommandLineForSorting(LPCTSTR sCommandLine) {
	LPCTSTR sSorting = Helpers::stristr(sCommandLine, _T("/order"));
	if (sSorting == NULL) {
		return Helpers::FS_Undefined;
	}
	LPCTSTR sSortingMode = sSorting + _tcslen(_T("/order"));
	if (sSortingMode[0] != 0) {
		return 
			(_totupper(sSortingMode[1]) == _T('M')) ? Helpers::FS_LastModTime :
			(_totupper(sSortingMode[1]) == _T('C')) ? Helpers::FS_CreationTime :
			(_totupper(sSortingMode[1]) == _T('N')) ? Helpers::FS_FileName :
			(_totupper(sSortingMode[1]) == _T('Z')) ? Helpers::FS_Random :
			(_totupper(sSortingMode[1]) == _T('S')) ? Helpers::FS_FileSize : Helpers::FS_Undefined;
	}
	return Helpers::FS_Undefined; 
}

static Helpers::ETransitionEffect ParseCommandLineForSlideShowEffect(LPCTSTR sCommandLine) {
	LPCTSTR sEffect = Helpers::stristr(sCommandLine, _T("/effect"));
	if (sEffect == NULL) {
		return (Helpers::ETransitionEffect)(-1);
	}
	sEffect = sEffect + _tcslen(_T("/effect")) + 1;
	LPCTSTR posSpace = _tcschr(sEffect, _T(' '));
	CString effect = (posSpace == NULL) ? CString(sEffect) : CString(sEffect, (int)(posSpace - sEffect));
	return Helpers::ConvertTransitionEffectFromString(effect);
}

static int ParseCommandLineForTransitionTime(LPCTSTR sCommandLine) {
	LPCTSTR sTransitionTime = Helpers::stristr(sCommandLine, _T("/transitiontime"));
	if (sTransitionTime == NULL) {
		return 0;
	}
	return max(100, min(5000, _ttoi(sTransitionTime + _tcslen(_T("/transitiontime")))));
}

#ifdef DEBUG
static CRITICAL_SECTION s_lock;

static BOOL GenerateDump(EXCEPTION_POINTERS* pExceptionPointers)
{
	WCHAR* szAppName = L"JPEGView";
	DWORD dwBufferSize = MAX_PATH;
	HANDLE hDumpFile;
	SYSTEMTIME stLocalTime;
	MINIDUMP_EXCEPTION_INFORMATION ExpParam;

	GetLocalTime(&stLocalTime);

	CString fileName;
	fileName.Format(_T("%s\\%s-%04d%02d%02d-%02d%02d%02d-%ld-%ld.dmp"),
		Helpers::JPEGViewAppDataPath(), szAppName,
		stLocalTime.wYear, stLocalTime.wMonth, stLocalTime.wDay,
		stLocalTime.wHour, stLocalTime.wMinute, stLocalTime.wSecond,
		GetCurrentProcessId(), GetCurrentThreadId());

	hDumpFile = ::CreateFile(fileName, GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

	ExpParam.ThreadId = GetCurrentThreadId();
	ExpParam.ExceptionPointers = pExceptionPointers;
	ExpParam.ClientPointers = TRUE;

	return ::MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
		hDumpFile, MiniDumpWithDataSegs, &ExpParam, NULL, NULL);
}

static LONG WINAPI CrashHandler(EXCEPTION_POINTERS * pExceptionInfo)
{
	::EnterCriticalSection(&s_lock);
	if (GenerateDump(pExceptionInfo))
	{
		::MessageBox(NULL, CString(_T("Unhandled exception\nMinidump file written to:\n")) + Helpers::JPEGViewAppDataPath(), _T("Unhandled exception"), MB_OK);
	}
	else
	{
		::MessageBox(NULL, _T("Unhandled exception\nError writing minidump file"), _T("Error writing minidump"), MB_OK);
	}
	::LeaveCriticalSection(&s_lock);
	return EXCEPTION_EXECUTE_HANDLER;
}
#endif

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int /*nCmdShow*/)
{
	HRESULT hRes = ::CoInitialize(NULL);

#ifdef DEBUG
	::InitializeCriticalSection(&s_lock);

	::SetUnhandledExceptionFilter(CrashHandler);
#endif

	// use the locale from the operating system, however for numeric input/output always use 'C' locale
	// with the OS locale the INI file can not be read correctly (due to the different decimal point characters)
	_tsetlocale(LC_ALL, _T(""));
	_tsetlocale(LC_NUMERIC, _T("C"));

	srand(::GetTickCount());

	// this resolves ATL window thunking problem when Microsoft Layer for Unicode (MSLU) is used
	::DefWindowProc(NULL, 0, 0, 0L);

	AtlInitCommonControls(ICC_BAR_CLASSES);	// add flags to support other controls

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	CString sStartupFile = ParseCommandLineForStartupFile(lpstrCmdLine);
	int nAutostartSlideShow = (sStartupFile.GetLength() == 0) ? 0 : ParseCommandLineForAutostart(lpstrCmdLine);
	bool bForceFullScreen = ParseCommandLineForFullScreen(lpstrCmdLine);
	bool bAutoExit = ParseCommandLineForAutoExit(lpstrCmdLine);
	Helpers::ESorting eSorting = ParseCommandLineForSorting(lpstrCmdLine);
	Helpers::ETransitionEffect eTransitionEffect = ParseCommandLineForSlideShowEffect(lpstrCmdLine);
	int nTransitionTime = ParseCommandLineForTransitionTime(lpstrCmdLine);
	int nDisplayMonitor = ParseCommandLineForDisplayMonitor(lpstrCmdLine);

	// Searches for other instances and terminates them
	bool bFileLoadedByExistingInstance = false;
	HANDLE hMutex = ::CreateMutex(NULL, FALSE, _T("JPVMtX2869"));
	if (::GetLastError() == ERROR_ALREADY_EXISTS) {
		::EnumWindows((WNDENUMPROC)EnumWindowsProc, 0);
		if (_HWNDOtherInstance != NULL) {
			// Other instance found, send the filename to be loaded to this instance
			COPYDATASTRUCT copyData;
			memset(&copyData, 0, sizeof(COPYDATASTRUCT));
			copyData.dwData = KEY_MAGIC;
			copyData.cbData = (sStartupFile.GetLength() + 1) * sizeof(TCHAR);
			copyData.lpData = (LPVOID)(LPCTSTR)sStartupFile;
			ULONG_PTR result = 0;
			PDWORD_PTR resultPtr = &result;
			LRESULT res = ::SendMessageTimeout(_HWNDOtherInstance, WM_COPYDATA, 0, (LPARAM)&copyData, 0, 250, resultPtr);
			bFileLoadedByExistingInstance = *resultPtr == (ULONG_PTR)KEY_MAGIC;
		}
	}

	int nRet = 0;
	//Run application
	if (!bFileLoadedByExistingInstance) {
		// Initialize GDI+
		Gdiplus::GdiplusStartupInput gdiplusStartupInput;
		ULONG_PTR gdiplusToken;
		Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

		CMainDlg dlgMain(bForceFullScreen);

		dlgMain.SetStartupInfo(sStartupFile, nAutostartSlideShow, eSorting, eTransitionEffect, nTransitionTime, bAutoExit, nDisplayMonitor);

		try {
			nRet = (int)dlgMain.DoModal();
			if (CSettingsProvider::This().StickyWindowSize() && !dlgMain.IsFullScreenMode()) {
				CSettingsProvider::This().SaveStickyWindowRect(dlgMain.WindowRectOnClose());
			}
			::ShowCursor(TRUE);
		}
		catch (...) {
			::ShowCursor(TRUE);
		}

		// Shut down GDI+
		Gdiplus::GdiplusShutdown(gdiplusToken);
	}

	::CloseHandle(hMutex);

	_Module.Term();
	::CoUninitialize();

	return nRet;
}

