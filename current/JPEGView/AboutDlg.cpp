#include "StdAfx.h"
#include "AboutDlg.h"
#include "NLS.h"
#include "SettingsProvider.h"
#include "Helpers.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// Helpers
//////////////////////////////////////////////////////////////////////////////////////////////

static LPCTSTR GetSIMDModeString() {
	Helpers::CPUType cpuType = CSettingsProvider::This().AlgorithmImplementation();
	if (cpuType == Helpers::CPU_MMX) {
		return _T("64 bit MMX");
	} else if (cpuType == Helpers::CPU_SSE) {
		return _T("128 bit SSE2");
	} else if (cpuType == Helpers::CPU_AVX2) {
		return _T("256 bit AVX2");
	}
	else {
		return _T("Generic CPU");
	}
}

static CString GetReadmeFileName() {
	// Check if there is a localized version of the readme.html file
	CString sReadmeFileName = CNLS::GetLocalizedFileName(_T(""), _T("readme"), _T("html"), CSettingsProvider::This().Language());
	if (::GetFileAttributes(CString(CSettingsProvider::This().GetEXEPath()) + sReadmeFileName) == INVALID_FILE_ATTRIBUTES) {
		sReadmeFileName = _T("readme.html");
	}
	return sReadmeFileName;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// CAboutDlg
//////////////////////////////////////////////////////////////////////////////////////////////

CAboutDlg::CAboutDlg(void) {
}

CAboutDlg::~CAboutDlg(void) {
}

LRESULT CAboutDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	CenterWindow(GetParent());

	HICON hIconSmall = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR | LR_SHARED);
	SetIcon(hIconSmall, FALSE);

#ifdef _DEBUG
#ifdef _WIN64
	LPCTSTR sTitle = _T("About JPEGView... (Debug version, 64 bit)");
#else
	LPCTSTR sTitle = _T("About JPEGView... (Debug version, 32 bit)");
#endif
#else
#ifdef _WIN64
	LPCTSTR sTitle = CNLS::GetString(_T("About JPEGView (64 bit version)..."));
#else
	LPCTSTR sTitle = CNLS::GetString(_T("About JPEGView..."));
#endif
#endif
	this->SetWindowText(sTitle);

	m_lblVersion.Attach(GetDlgItem(IDC_JPEGVIEW));
	m_lblSIMD.Attach(GetDlgItem(IDC_SIMDMODE));
	m_lblNumCores.Attach(GetDlgItem(IDC_NUMCORES));
	m_richEdit.Attach(GetDlgItem(IDC_LICENSE));
	m_btnClose.Attach(GetDlgItem(IDC_CLOSE));
	m_lblIcon.Attach(GetDlgItem(IDC_ICONJPEGVIEW));

	m_lblVersion.SetWindowText(CString(_T("JPEGView ")) + JPEGVIEW_VERSION);

	m_lblSIMD.SetWindowText(CString(CNLS::GetString(_T("SIMD mode used:"))) + _T(" ") + GetSIMDModeString());
	TCHAR sNumCores[16];
	_sntprintf_s(sNumCores, 16, 16, _T("%d"), CSettingsProvider::This().NumberOfCoresToUse());
	m_lblNumCores.SetWindowText(CString(CNLS::GetString(_T("Number of CPU cores used:"))) + _T(" ") + sNumCores);
	m_btnClose.SetWindowText(CNLS::GetString(_T("Close")));

	m_richEdit.SetBackgroundColor(::GetSysColor(COLOR_3DFACE));
	m_richEdit.SetAutoURLDetect(TRUE);
	m_richEdit.SetWindowText(CString(CNLS::GetString(_T("Licensed under the GNU general public license (GPL), see readme file for details:"))) + 
		_T("\nfile://") + GetReadmeFileName() + _T("\n") + 
		CNLS::GetString(_T("Project home page:")) + 
		_T(" \nhttp://jpegview.sourceforge.net\n"));
	m_richEdit.SetEventMask(ENM_LINK);

	HICON hIconLarge = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME),
		IMAGE_ICON, 64, 64, LR_DEFAULTCOLOR | LR_SHARED);
	m_lblIcon.SetIcon(hIconLarge);

	return TRUE;
}

LRESULT CAboutDlg::OnCloseDialog(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	EndDialog(IDCANCEL);
	return 0;
}

LRESULT CAboutDlg::OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	EndDialog(IDCANCEL);
	return 0;
}

LRESULT CAboutDlg::OnLinkClicked(WPARAM wParam, LPNMHDR lpnmhdr, BOOL& bHandled) {
	ENLINK* pLink = (ENLINK*) lpnmhdr;
	if (pLink->msg == WM_LBUTTONUP) {
		int nLen = pLink->chrg.cpMax - pLink->chrg.cpMin;
		TCHAR* pTextLink = new TCHAR[nLen + 1];
		m_richEdit.GetTextRange(pLink->chrg.cpMin, pLink->chrg.cpMax, pTextLink);
		CString sReadmeFileName = GetReadmeFileName();
		if (_tcsstr(pTextLink, sReadmeFileName) != NULL) {
			::ShellExecute(m_hWnd, _T("open"), CString(CSettingsProvider::This().GetEXEPath()) + _T("\\") + sReadmeFileName, 
				NULL, CSettingsProvider::This().GetEXEPath(), SW_SHOW);
		} else {
			::ShellExecute(m_hWnd, _T("open"), pTextLink, NULL, NULL, SW_SHOW);
		}
		delete[] pTextLink;
	}
	return 0;
}