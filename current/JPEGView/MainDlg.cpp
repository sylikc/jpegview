// MainDlg.cpp : implementation of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"
#include <math.h>

#include "MainDlg.h"
#include "FileList.h"
#include "JPEGProvider.h"
#include "JPEGImage.h"
#include "SettingsProvider.h"
#include "BasicProcessing.h"
#include "MultiMonitorSupport.h"
#include "HistogramCorr.h"
#include "UserCommand.h"
#include "HelpDisplay.h"
#include "Clipboard.h"
#include "FileOpenDialog.h"
#include "GUIControls.h"
#include "ParameterDB.h"
#include "SaveImage.h"
#include "NLS.h"
#include "BatchCopyDlg.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// Constants
//////////////////////////////////////////////////////////////////////////////////////////////

static const double GAMMA_FACTOR = 1.02; // multiplicator for gamma value
static const double CONTRAST_INC = 0.03; // increment for contrast value
static const double SHARPEN_INC = 0.05; // increment for sharpen value
static const double LDC_INC = 0.1; // increment for LDC (lighten shadows and darken highlights)
static const int NUM_THREADS = 1; // number of readahead threads to use
static const int READ_AHEAD_BUFFERS = 2; // number of readahead buffers to use (NUM_THREADS+1 is a good choice)
static const int SLIDESHOW_TIMER_EVENT_ID = 1; // Slideshow timer ID
static const int ZOOM_TIMER_EVENT_ID = 2; // Zoom refinement timer ID
static const int ZOOM_TEXT_TIMER_EVENT_ID = 3; // Zoom label timer ID
static const int ZOOM_TIMEOUT = 200; // refinement done after this many milliseconds
static const int ZOOM_TEXT_TIMEOUT = 1000; // zoom label disappears after this many milliseconds

static const int DARKEN_HIGHLIGHTS = 0; // used in AdjustLDC() call
static const int BRIGHTEN_SHADOWS = 1; // used in AdjustLDC() call

static const int ZOOM_TEXT_RECT_WIDTH = 100; // zoom label width
static const int ZOOM_TEXT_RECT_HEIGHT = 25; // zoom label height
static const int ZOOM_TEXT_RECT_OFFSET = 40; // zoom label offset from right border
static const int PAN_STEP = 48; // number of pixels to pan if pan with cursor keys (SHIFT+up/down/left/right)

static const bool SHOW_TIMING_INFO = false; // Set to true for debugging

static const int NO_REQUEST = 1; // used in GotoImage() method
static const int NO_REMOVE_KEY_MSG = 2; // used in GotoImage() method

CMainDlg * CMainDlg::sm_instance = NULL;

//////////////////////////////////////////////////////////////////////////////////////////////
// Helpers
//////////////////////////////////////////////////////////////////////////////////////////////

// Gets default image processing parameters as set in INI file
static CImageProcessingParams GetDefaultProcessingParams() {
	CSettingsProvider& sp = CSettingsProvider::This();
	return CImageProcessingParams(
		sp.Contrast(),
		sp.Gamma(),
		sp.Sharpen(),
		0.0, 0.5,
		sp.BrightenShadows(),
		sp.DarkenHighlights(),
		sp.BrightenShadowsSteepness(),
		sp.CyanRed(),
		sp.MagentaGreen(),
		sp.YellowBlue());
}

// Gets hardcoded processing parameters, turning off all processing except sharpening according to INI
static CImageProcessingParams GetNoProcessingParams() {
	return CImageProcessingParams(0.0, 1.0, CSettingsProvider::This().Sharpen(), 0.0, 0.5, 0.5, 0.25, 0.5, 0.0, 0.0, 0.0); 
}

// Gets default image processing flags as set in INI file
static EProcessingFlags GetDefaultProcessingFlags() {
	CSettingsProvider& sp = CSettingsProvider::This();
	EProcessingFlags eProcFlags = PFLAG_None;
	eProcFlags = SetProcessingFlag(eProcFlags, PFLAG_HighQualityResampling, sp.HighQualityResampling());
	eProcFlags = SetProcessingFlag(eProcFlags, PFLAG_AutoContrast, sp.AutoContrastCorrection());
	eProcFlags = SetProcessingFlag(eProcFlags, PFLAG_LDC, sp.LocalDensityCorrection());
	return eProcFlags;
}

// Create processing flags from literal boolean values
static EProcessingFlags CreateProcessingFlags(bool bHQResampling, bool bAutoContrast, 
											  bool bAutoContrastSection, bool bLDC, bool bKeepParams) {
	EProcessingFlags eProcFlags = PFLAG_None;
	eProcFlags = SetProcessingFlag(eProcFlags, PFLAG_HighQualityResampling, bHQResampling);
	eProcFlags = SetProcessingFlag(eProcFlags, PFLAG_AutoContrast, bAutoContrast);
	eProcFlags = SetProcessingFlag(eProcFlags, PFLAG_AutoContrastSection, bAutoContrastSection);
	eProcFlags = SetProcessingFlag(eProcFlags, PFLAG_LDC, bLDC);
	eProcFlags = SetProcessingFlag(eProcFlags, PFLAG_KeepParams, bKeepParams);
	return eProcFlags;
}

// Initialize boolean processing values from flag bitfield
static void InitFromProcessingFlags(EProcessingFlags eFlags, bool& bHQResampling, bool& bAutoContrast, 
									bool& bAutoContrastSection, bool& bLDC) {
	bHQResampling = GetProcessingFlag(eFlags, PFLAG_HighQualityResampling);
	bAutoContrast = GetProcessingFlag(eFlags, PFLAG_AutoContrast);
	bAutoContrastSection = GetProcessingFlag(eFlags, PFLAG_AutoContrastSection);
	bLDC = GetProcessingFlag(eFlags, PFLAG_LDC);
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Public
//////////////////////////////////////////////////////////////////////////////////////////////

CMainDlg::CMainDlg() {
	CSettingsProvider& sp = CSettingsProvider::This();

	// Read the string table for the current thread language if any is present
	TCHAR buff[16];
	LCID threadLocale = ::GetThreadLocale();
	::GetLocaleInfo(threadLocale, LOCALE_SISO639LANGNAME, (LPTSTR) &buff, sizeof(buff));
	CNLS::ReadStringTable(CString(sp.GetEXEPath()) + _T("strings_") + buff + _T(".txt"));

	sm_instance = this;
	m_pImageProcParams = new CImageProcessingParams(GetDefaultProcessingParams());
	InitFromProcessingFlags(GetDefaultProcessingFlags(), m_bHQResampling, m_bAutoContrast, m_bAutoContrastSection, m_bLDC);

	// Initialize second parameter set using hard coded values, turning off all corrections except sharpening
	m_pImageProcParams2 = new CImageProcessingParams(GetNoProcessingParams()); 
	m_eProcessingFlags2 = PFLAG_HighQualityResampling;

	m_pImageProcParamsKept = new CImageProcessingParams();
	m_eProcessingFlagsKept = PFLAG_HighQualityResampling;
	m_nRotationKept = 0;
	m_dZoomKept = -1;
	m_offsetKept = CPoint(0, 0);
	m_bCurrentImageInParamDB = false;
	m_bCurrentImageIsSpecialProcessing = false;
	m_dCurrentInitialLightenShadows = -1;

	m_bShowFileName = sp.ShowFileName();
	m_bKeepParams = sp.KeepParams();
	m_eAutoZoomMode = sp.AutoZoomMode();

	CHistogramCorr::SetContrastCorrectionStrength((float)sp.AutoContrastAmount());
	CHistogramCorr::SetBrightnessCorrectionStrength((float)sp.AutoBrightnessAmount());

	m_pFileList = NULL;
	m_pJPEGProvider = NULL;
	m_pCurrentImage = NULL;

	m_eProcFlagsBeforeMovie = PFLAG_None;
	m_nRotation = 0;
	m_bUserZoom = false;
	m_bUserPan = false;
	m_bMovieMode = false;
	m_bProcFlagsTouched = false;
	m_bInTrackPopupMenu = false;
	m_dZoom = -1.0;
	m_dZoomMult = -1.0;
	m_bDragging = false;
	m_nOffsetX = 0;
	m_nOffsetY = 0;
	m_nCapturedX = m_nCapturedY = 0;
	m_nMouseX = m_nMouseY = 0;
	m_bShowHelp = false;
	m_bLockPaint = true;
	m_nCurrentTimeout = 0;
	m_startMouse.x = m_startMouse.y = -1;
	m_nCursorCnt = 0;
	m_virtualImageSize = CSize(-1, -1);
	m_bInZooming = false;
	m_bTemporaryLowQ = false;
	m_bShowZoomFactor = false;
	m_bSearchSubDirsOnEnter = false;
	m_bSpanVirtualDesktop = false;
	m_bShowIPTools = false;
	m_storedWindowPlacement.length = sizeof(WINDOWPLACEMENT);
	m_pSliderMgr = NULL;
}

CMainDlg::~CMainDlg() {
	delete m_pFileList;
	delete m_pJPEGProvider;
	delete m_pImageProcParams;
	delete m_pImageProcParamsKept;
	delete m_pSliderMgr;
}

LRESULT CMainDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{	
	this->SetWindowText(_T("JPEGView")); // only relevant for toolbar

	// get the scaling of the screen (DPI) compared to 96 DPI (design value)
	CPaintDC dc(this->m_hWnd);
	m_fScaling = ::GetDeviceCaps(dc, LOGPIXELSX)/96.0f;
	Helpers::ScreenScaling = m_fScaling;

	// Configure the image processing area at bottom of screen
	m_pSliderMgr = new CSliderMgr(this->m_hWnd);
	m_pSliderMgr->AddSlider(CNLS::GetString(_T("Contrast")), &(m_pImageProcParams->Contrast), NULL, -0.5, 0.5, false, false);
	m_pSliderMgr->AddSlider(CNLS::GetString(_T("Brightness")), &(m_pImageProcParams->Gamma), NULL, 0.5, 2.0, true, true);
	m_pSliderMgr->AddSlider(CNLS::GetString(_T("Sharpen")), &(m_pImageProcParams->Sharpen), NULL, 0.0, 0.5, false, false);
	m_pSliderMgr->AddSlider(CNLS::GetString(_T("C - R")), &(m_pImageProcParams->CyanRed), NULL, -1.0, 1.0, false, false);
	m_pSliderMgr->AddSlider(CNLS::GetString(_T("M - G")), &(m_pImageProcParams->MagentaGreen), NULL, -1.0, 1.0, false, false);
	m_pSliderMgr->AddSlider(CNLS::GetString(_T("Y - B")), &(m_pImageProcParams->YellowBlue), NULL, -1.0, 1.0, false, false);
	m_pSliderMgr->AddSlider(CNLS::GetString(_T("Lighten Shadows")), &(m_pImageProcParams->LightenShadows), &m_bLDC, 0.0, 1.0, false, false);
	m_pSliderMgr->AddSlider(CNLS::GetString(_T("Darken Highlights")), &(m_pImageProcParams->DarkenHighlights), &m_bLDC, 0.0, 1.0, false, false);
	m_pSliderMgr->AddSlider(CNLS::GetString(_T("Deep Shadows")), &(m_pImageProcParams->LightenShadowSteepness), &m_bLDC, 0.0, 1.0, false, false);
	m_pSliderMgr->AddSlider(CNLS::GetString(_T("Color Correction")), &(m_pImageProcParams->ColorCorrectionFactor), &m_bAutoContrast, -0.5, 0.5, false, false);
	m_pSliderMgr->AddSlider(CNLS::GetString(_T("Contrast Correction")), &(m_pImageProcParams->ContrastCorrectionFactor), &m_bAutoContrast, 0.0, 1.0, false, false);
	m_txtParamDB = m_pSliderMgr->AddText(CNLS::GetString(_T("Parameter DB:")), false, NULL);
	m_btnSaveToDB = m_pSliderMgr->AddButton(CNLS::GetString(_T("Save to")), &OnSaveToDB);
	m_btnRemoveFromDB = m_pSliderMgr->AddButton(CNLS::GetString(_T("Remove from")), &OnRemoveFromDB);
	m_txtRename = m_pSliderMgr->AddText(CNLS::GetString(_T("Rename:")), false, NULL);
	m_txtFileName = m_pSliderMgr->AddText(NULL, true, &OnRenameFile);

	// place window on monitor as requested in INI file
	int nMonitor = CSettingsProvider::This().DisplayMonitor();
	m_monitorRect = CMultiMonitorSupport::GetMonitorRect(nMonitor);
	if (CMultiMonitorSupport::IsMultiMonitorSystem()) {
		m_monitorRect.top -= 1;
		m_monitorRect.left -= 1;
		m_monitorRect.bottom += 1;
		m_monitorRect.right += 1;
		SetWindowPos(HWND_TOP, &m_monitorRect, SWP_NOZORDER);
	} else {
		CRect wndRect(-1, -1, ::GetSystemMetrics(SM_CXSCREEN) + 1, ::GetSystemMetrics(SM_CYSCREEN) + 1);
		//CRect wndRect(0, 0, 1024, 768);
		this->MoveWindow(&wndRect);
		m_monitorRect = wndRect;
	}
	this->GetClientRect(&m_clientRect);

	// set icons (for toolbar)
	HICON hIcon = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
	SetIcon(hIcon, TRUE);
	HICON hIconSmall = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	SetIcon(hIconSmall, FALSE);

	// turn off mouse coursor
	m_nCursorCnt = ::ShowCursor(FALSE);

	// intitialize navigation with startup file (and folder)
	m_pFileList = new CFileList(m_sStartupFile, CSettingsProvider::This().Sorting());
	m_pFileList->SetNavigationMode(CSettingsProvider::This().Navigation());

	// create JPEG provider and request first image
	m_pJPEGProvider = new CJPEGProvider(this->m_hWnd, NUM_THREADS, READ_AHEAD_BUFFERS);	
	m_pCurrentImage = m_pJPEGProvider->RequestJPEG(m_pFileList, CJPEGProvider::FORWARD,
		m_pFileList->Current(), CreateProcessParams());
	AfterNewImageLoaded(true); // synchronize to per image parameters

	m_bLockPaint = false;
	this->Invalidate(FALSE);

	// If no file was passed on command line, show the 'Open file' dialog
	if (m_sStartupFile.GetLength() == 0) {
		if (!OpenFile(true)) {
			EndDialog(0);
		}
	}

	return TRUE;
}

LRESULT CMainDlg::OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if (m_bLockPaint) {
		return 0;
	}
	CPaintDC dc(m_hWnd);

	this->GetClientRect(&m_clientRect);

	if (m_pCurrentImage == NULL) {
		dc.FillRect(&m_clientRect, (HBRUSH)::GetStockObject(BLACK_BRUSH));
	} else {
		// do this as very first - may changes size of image
		m_pCurrentImage->VerifyRotation(m_nRotation);

		// Find out zoom multiplier if not yet known
		if (m_dZoomMult < 0.0) {
			CSize fittedSize = Helpers::GetImageRect(m_pCurrentImage->OrigWidth(), m_pCurrentImage->OrigHeight(), 
				m_clientRect.Width(), m_clientRect.Height(), true, false, false);
			double dZoomToFit = (double)fittedSize.cx/m_pCurrentImage->OrigWidth();
			// Zoom multiplier (zoom step) should be around 1.1 but reach the value 1.0 after an integral number
			// of zooming steps
			int n = Helpers::RoundToInt(log(1/dZoomToFit)/log(1.1));
			m_dZoomMult = (n == 0) ? 1.1 : exp(log(1/dZoomToFit)/n);
		}

		// find out the size of the bitmap to request
		CSize newSize;
		if (m_dZoom < 0.0) {
			// zoom not set, interpret as 'fit to screen'
			// ---------------------------------------------
			newSize = Helpers::GetImageRect(m_pCurrentImage->OrigWidth(), m_pCurrentImage->OrigHeight(), 
				m_clientRect.Width(), m_clientRect.Height(), m_eAutoZoomMode);
			m_dZoom = (double)newSize.cx/m_pCurrentImage->OrigWidth();
		} else {
			// zoom set, use this value for the new size
			// ---------------------------------------------
			newSize = CSize((int)(m_pCurrentImage->OrigWidth() * m_dZoom + 0.5), (int)(m_pCurrentImage->OrigHeight() * m_dZoom + 0.5));
		}
		newSize.cx = max(1, min(65535, newSize.cx));
		newSize.cy = max(1, min(65535, newSize.cy));
		LimitOffsets(m_clientRect, newSize);
		m_virtualImageSize = newSize;

		// Clip to client rectangle and request the DIB
		CSize clippedSize(min(m_clientRect.Width(), newSize.cx), min(m_clientRect.Height(), newSize.cy));
		CPoint offsets(m_nOffsetX, m_nOffsetY);
		CPoint offsetsInImage = m_pCurrentImage->ConvertOffset(newSize, clippedSize, offsets);
		int nDimRegion = m_bShowIPTools ? m_pSliderMgr->SliderAreaHeight() - (m_clientRect.Height() - clippedSize.cy)/2 : 0;
		m_pCurrentImage->SetDimBitmapRegion(max(0, nDimRegion));
		void* pDIBData = m_pCurrentImage->GetDIB(newSize, clippedSize, offsetsInImage, 
			    *m_pImageProcParams, 
				CreateProcessingFlags(m_bHQResampling && !m_bTemporaryLowQ, 
				m_bAutoContrast, m_bAutoContrastSection, m_bLDC, false));

		// Paint the DIB
		if (pDIBData != NULL) {
			BITMAPINFO bmInfo;
			memset(&bmInfo, 0, sizeof(BITMAPINFO));
			bmInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmInfo.bmiHeader.biWidth = clippedSize.cx;
			bmInfo.bmiHeader.biHeight = m_pCurrentImage->GetFlagFlipped() ? clippedSize.cy : -clippedSize.cy;
			bmInfo.bmiHeader.biPlanes = 1;
			bmInfo.bmiHeader.biBitCount = 32;
			bmInfo.bmiHeader.biCompression = BI_RGB;
			int xDest = (m_clientRect.Width() - clippedSize.cx) / 2;
			int yDest = (m_clientRect.Height() - clippedSize.cy) / 2;
			dc.SetDIBitsToDevice(xDest, yDest, clippedSize.cx, clippedSize.cy, 0, 0, 0, clippedSize.cy, pDIBData, 
				&bmInfo, DIB_RGB_COLORS);
			// remaining client area is painted black
			if (clippedSize.cx < m_clientRect.Width()) {
				CRect r(0, 0, xDest, m_clientRect.Height());
				dc.FillRect(&r, (HBRUSH)::GetStockObject(BLACK_BRUSH));
				CRect rr(xDest+clippedSize.cx, 0, m_clientRect.Width(), m_clientRect.Height());
				dc.FillRect(&rr, (HBRUSH)::GetStockObject(BLACK_BRUSH));
			}
			if (clippedSize.cy < m_clientRect.Height()) {
				CRect r(0, 0, m_clientRect.Width(), yDest);
				dc.FillRect(&r, (HBRUSH)::GetStockObject(BLACK_BRUSH));
				CRect rr(0, yDest+clippedSize.cy, m_clientRect.Width(), m_clientRect.Height());
				dc.FillRect(&rr, (HBRUSH)::GetStockObject(BLACK_BRUSH));
			}
		}
	}

	dc.SelectStockFont(SYSTEM_FONT);
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(RGB(0, 255, 0));
	dc.SetBkColor(RGB(0, 0, 0));

	// Display file name if enabled
	if (m_bShowFileName && m_pFileList->Current() != NULL) {
		CString sFileName;
		sFileName.Format(_T("[%d/%d]  %s"), m_pFileList->CurrentIndex() + 1, m_pFileList->Size(), m_pFileList->Current());
		DrawTextBordered(dc, sFileName, CRect(Scale(2), Scale(2), m_clientRect.right, Scale(30)), DT_LEFT); 
	}

	// Display errors and warnings
	if (m_sStartupFile.IsEmpty()) {
		CRect rectText(0, m_clientRect.Height()/2 - Scale(40), m_clientRect.Width(), m_clientRect.Height());
		dc.DrawText(CString(CNLS::GetString(_T("No file or folder name was provided as command line parameter!"))) + _T('\n') +
			CNLS::GetString(_T("To use JPEGView, associate JPG, JPEG and BMP files with JPEGView.")) + _T('\n') +
			CNLS::GetString(_T("Press ESC to exit...")),
			-1, &rectText, DT_CENTER | DT_WORDBREAK);
	} else if (m_pCurrentImage == NULL) {
		const int BUF_LEN = 512;
		TCHAR buff[BUF_LEN];
		CRect rectText(0, m_clientRect.Height()/2 - Scale(40), m_clientRect.Width(), m_clientRect.Height());
		LPCTSTR sCurrentFile = m_pFileList->Current();
		if (sCurrentFile != NULL) {
			_stprintf_s(buff, BUF_LEN, CNLS::GetString(_T("The file '%s' could not be read!")), sCurrentFile);
			dc.DrawText(buff, -1, &rectText, DT_CENTER | DT_WORDBREAK);
		} else {
			if (m_pFileList->IsSlideShowList()) {
				_stprintf_s(buff, BUF_LEN, CNLS::GetString(_T("The file '%s' does not contain a list of file names!")),  
					m_sStartupFile);
			} else {
				_stprintf_s(buff, BUF_LEN, CString(CNLS::GetString(_T("The directory '%s' does not contain any image files!")))  + _T('\n') +
					CNLS::GetString(_T("Press any key to search the subdirectories for image files.")), m_pFileList->CurrentDirectory());
				m_bSearchSubDirsOnEnter = true;
			}
			dc.DrawText(buff, -1, &rectText, DT_CENTER | DT_WORDBREAK);
		}
	}

	// Show current zoom factor
	if (m_bInZooming || m_bShowZoomFactor) {
		TCHAR buff[32];
		_stprintf_s(buff, 32, _T("%d %%"), int(m_dZoom*100 + 0.5));
		CRect textRect(m_clientRect.right - Scale(ZOOM_TEXT_RECT_WIDTH + ZOOM_TEXT_RECT_OFFSET), 
			m_clientRect.bottom - Scale(ZOOM_TEXT_RECT_HEIGHT + ZOOM_TEXT_RECT_OFFSET), 
			m_clientRect.right - Scale(ZOOM_TEXT_RECT_OFFSET), m_clientRect.bottom - Scale(ZOOM_TEXT_RECT_OFFSET));
		DrawTextBordered(dc, buff, textRect, DT_RIGHT);
	}

	// Show timing info if requested
	if (SHOW_TIMING_INFO && m_pCurrentImage != NULL) {
		TCHAR buff[128];
		_stprintf_s(buff, 128, _T("Last operation: %d ms, Last resize: %s"), m_pCurrentImage->LastOpTickCount(), CBasicProcessing::TimingInfo());
		dc.SetBkMode(OPAQUE);
		dc.TextOut(5, 5, buff);
		dc.SetBkMode(TRANSPARENT);
	}

	// Show info about motiv detection
#ifdef _DEBUG
	if (m_pCurrentImage != NULL) {
		dc.SetBkMode(OPAQUE);
		TCHAR sNS[32];
		_stprintf_s(sNS, 32, _T("Nightshot: %.2f"), m_pCurrentImage->IsNightShot());
		dc.TextOut(5, 35, sNS);

		_stprintf_s(sNS, 32, _T("Sunset: %.2f"), m_pCurrentImage->IsSunset());
		dc.TextOut(120, 35, sNS);
		dc.SetBkMode(TRANSPARENT);
	}
#endif

	// Paint the image processing area at bottom of screen
	if (m_bShowIPTools) {
		m_pSliderMgr->OnPaint(dc);
	}

	// Display the help screen
	if (m_bShowHelp) {
		CHelpDisplay helpDisplay(dc);
		helpDisplay.AddTitle(CNLS::GetString(_T("JPEGView Help")));
		if (m_pFileList->Current() != NULL && m_pCurrentImage != NULL) {
			LPCTSTR sCurrent = m_pFileList->Current();
			LPCTSTR sTitle = m_pFileList->CurrentFileTitle();
			double fMPix = double(m_pCurrentImage->OrigWidth() * m_pCurrentImage->OrigHeight())/(1000000);
			TCHAR buff[256];
			_stprintf_s(buff, 256, _T("%s (%d x %d   %.1f MPixel)"), sTitle, m_pCurrentImage->OrigWidth(), m_pCurrentImage->OrigHeight(), fMPix);
			helpDisplay.AddLine(CNLS::GetString(_T("Current image")), buff);
		}
		helpDisplay.AddLine(_T("Esc"), CNLS::GetString(_T("Close help text display / Close JPEGView")));
		helpDisplay.AddLine(_T("F1"), CNLS::GetString(_T("Show/hide this help text")));
		helpDisplay.AddLineInfo(_T("F2"), m_bShowFileName, CNLS::GetString(_T("Show/hide file name")));
		helpDisplay.AddLineInfo(_T("F3"), m_bHQResampling, CNLS::GetString(_T("Enable/disable high quality resampling")));
		helpDisplay.AddLineInfo(_T("F4"), m_bKeepParams, CNLS::GetString(_T("Enable/disable keeping of geometry related (zoom/pan/rotation)")));
		helpDisplay.AddLineInfo(_T(""),  LPCTSTR(NULL), CNLS::GetString(_T("and image processing (brightness/contrast/sharpen) parameters between images")));
		helpDisplay.AddLineInfo(_T("F5"), m_bAutoContrast, CNLS::GetString(_T("Enable/disable automatic contrast correction (histogram equalization)")));
		helpDisplay.AddLineInfo(_T("Shift+F5"), m_bAutoContrastSection, CNLS::GetString(_T("Apply auto contrast correction using only visible section of image")));
		helpDisplay.AddLineInfo(_T("F6"), m_bLDC, CNLS::GetString(_T("Enable/disable automatic density correction (local brightness correction)")));
		TCHAR buffLS[16]; _stprintf_s(buffLS, 16, _T("%.2f"), m_pImageProcParams->LightenShadows);
		helpDisplay.AddLineInfo(_T("Ctrl/Alt+F6"), buffLS, CNLS::GetString(_T("Increase/decrease lightening of shadows (LDC must be on)")));
		TCHAR buffDH[16]; _stprintf_s(buffDH, 16, _T("%.2f"), m_pImageProcParams->DarkenHighlights);
		helpDisplay.AddLineInfo(_T("C/A+Shift+F6"), buffDH, CNLS::GetString(_T("Increase/decrease darkening of highlights (LDC must be on)")));
		helpDisplay.AddLineInfo(_T("F7"), m_pFileList->GetNavigationMode() == Helpers::NM_LoopDirectory, CNLS::GetString(_T("Loop through files in current folder")));
		helpDisplay.AddLineInfo(_T("F8"), m_pFileList->GetNavigationMode() == Helpers::NM_LoopSubDirectories, CNLS::GetString(_T("Loop through files in current directory and all subfolders")));
		helpDisplay.AddLineInfo(_T("F9"), m_pFileList->GetNavigationMode() == Helpers::NM_LoopSameDirectoryLevel, CNLS::GetString(_T("Loop through files in current directory and all sibling folders (folders on same level)")));
		helpDisplay.AddLineInfo(_T("F12"), m_bSpanVirtualDesktop, CNLS::GetString(_T("Maximize/restore to/from virtual desktop (only for multi-monitor systems)")));
		helpDisplay.AddLine(_T("Ctrl+C/Ctrl+X"), CNLS::GetString(_T("Copy screen to clipboard/ Copy processed full size image to clipboard")));
		helpDisplay.AddLine(_T("Ctrl+O"), CNLS::GetString(_T("Open new image or slideshow file")));
		helpDisplay.AddLine(_T("Ctrl+S"), CNLS::GetString(_T("Save processed image to JPEG file (original size)")));
		helpDisplay.AddLine(_T("s/d"), CNLS::GetString(_T("Save (s)/ delete (d) image processing parameters in/from parameter DB")));
		helpDisplay.AddLineInfo(_T("c/m/n"), 
			(m_pFileList->GetSorting() == Helpers::FS_LastModTime) ? _T("m") :
			(m_pFileList->GetSorting() == Helpers::FS_FileName) ? _T("n") : _T("c"), 
			CNLS::GetString(_T("Sort images by creation date, resp. modification date, resp. file name")));
		helpDisplay.AddLine(CNLS::GetString(_T("PgUp or Left")), CNLS::GetString(_T("Goto previous image")));
		helpDisplay.AddLine(CNLS::GetString(_T("PgDn or Right")), CNLS::GetString(_T("Goto next image")));
		helpDisplay.AddLine(_T("Home/End"), CNLS::GetString(_T("Goto first/last image of current folder (using sort order as defined)")));
		TCHAR buff[16]; _stprintf_s(buff, 16, _T("%.2f"), m_pImageProcParams->Gamma);
		helpDisplay.AddLineInfo(_T("Shift +/-"), buff, CNLS::GetString(_T("Increase/decrease brightness")));
		TCHAR buff1[16]; _stprintf_s(buff1, 16, _T("%.2f"), m_pImageProcParams->Contrast);
		helpDisplay.AddLineInfo(_T("Ctrl +/-"), buff1, CNLS::GetString(_T("Increase/decrease contrast")));
		TCHAR buff2[16]; _stprintf_s(buff2, 16, _T("%.2f"), m_pImageProcParams->Sharpen);
		helpDisplay.AddLineInfo(_T("Alt +/-"), buff2, CNLS::GetString(_T("Increase/decrease sharpness")));
		TCHAR buff3[16]; _stprintf_s(buff3, 16, _T("%.2f"), m_pImageProcParams->ColorCorrectionFactor);
		helpDisplay.AddLineInfo(_T("Alt+Ctrl +/-"), buff3, CNLS::GetString(_T("Increase/decrease auto color cast correction amount")));
		TCHAR buff4[16]; _stprintf_s(buff4, 16, _T("%.2f"), m_pImageProcParams->ContrastCorrectionFactor);
		helpDisplay.AddLineInfo(_T("Ctrl+Shift +/-"), buff4, CNLS::GetString(_T("Increase/decrease auto contrast correction amount")));
		helpDisplay.AddLine(_T("1 .. 9"), CNLS::GetString(_T("Slide show with timeout of n seconds (ESC to stop)")));
		helpDisplay.AddLineInfo(_T("Ctrl[+Shift] 1 .. 9"),  LPCTSTR(NULL), CNLS::GetString(_T("Set timeout to n/10 sec, respectively n/100 sec (Ctrl+Shift)")));
		helpDisplay.AddLine(CNLS::GetString(_T("up/down")), CNLS::GetString(_T("Rotate image and fit to screen")));
		helpDisplay.AddLine(_T("Return"), CNLS::GetString(_T("Fit image to screen")));
		helpDisplay.AddLine(_T("Space"), CNLS::GetString(_T("Zoom 1:1 (100 %)")));
		TCHAR buff5[16]; _stprintf_s(buff5, 16, _T("%.0f %%"), m_dZoom*100);
		helpDisplay.AddLineInfo(_T("+/-"), buff5, CNLS::GetString(_T("Zoom in/Zoom out (also Ctrl+up/down)")));
		helpDisplay.AddLine(CNLS::GetString(_T("Mouse wheel")), CNLS::GetString(_T("Zoom in/out image")));
		helpDisplay.AddLine(CNLS::GetString(_T("Left mouse & drag")), CNLS::GetString(_T("Pan image")));
		helpDisplay.AddLine(CNLS::GetString(_T("Fwd mouse button")), CNLS::GetString(_T("Next image")));
		helpDisplay.AddLine(CNLS::GetString(_T("Back mouse button")), CNLS::GetString(_T("Previous image")));
		std::list<CUserCommand*>::iterator iter;
		std::list<CUserCommand*> & userCmdList = CSettingsProvider::This().UserCommandList();
		for (iter = userCmdList.begin( ); iter != userCmdList.end( ); iter++ ) {
			if (!(*iter)->HelpText().IsEmpty()) {
				helpDisplay.AddLine((*iter)->HelpKey(), CNLS::GetString((*iter)->HelpText()));
			}
		}
		helpDisplay.Show(CRect(CPoint(0, 0), CSize(m_monitorRect.Width(), m_monitorRect.Height())));
	}

	return 0;
}

LRESULT CMainDlg::OnAnotherInstanceStarted(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	// Another instance has been started, terminate this one
	if (lParam == KEY_MAGIC) {
		this->EndDialog(0);
	}
	return 0;
}

LRESULT CMainDlg::OnLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	this->SetCapture();
	// If the image processing area does not eat up the event, start dragging
	if (!m_bShowIPTools || !m_pSliderMgr->OnMouseLButton(MouseEvent_BtnDown, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam))) {
		StartDragging(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
	}
	return 0;
}

LRESULT CMainDlg::OnMButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	this->SetCapture();
	StartDragging(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
	return 0;
}

LRESULT CMainDlg::OnLButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	if (!m_bShowIPTools || m_bDragging || !m_pSliderMgr->OnMouseLButton(MouseEvent_BtnUp, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam))) {
		EndDragging();
	}
	::ReleaseCapture();
	return 0;
}

LRESULT CMainDlg::OnMButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	EndDragging();
	::ReleaseCapture();
	return 0;
}

LRESULT CMainDlg::OnXButtonDown(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) {
		GotoImage(POS_Next);
	} else {
		GotoImage(POS_Previous);
	}
	return 0;
}

LRESULT CMainDlg::OnMouseMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	// Turn mouse pointer on when mouse has moved some distance
	m_nMouseX = GET_X_LPARAM(lParam);
	m_nMouseY = GET_Y_LPARAM(lParam);
	if (!m_bDragging) {
		if (m_startMouse.x == -1 && m_startMouse.y == -1) {
			m_startMouse.x = m_nMouseX;
			m_startMouse.y = m_nMouseY;
		} else {
			if (abs(m_nMouseX - m_startMouse.x) + abs(m_nMouseY - m_startMouse.y) > 50) {
				MouseOn();
			}
		}
	}
	// Do dragging if needed or turn on/off image processing controls if in lower area of screen
	if (m_bDragging) {
		DoDragging(m_nMouseX, m_nMouseY);
	} else if (!m_bShowIPTools) {
		if (m_nMouseY > m_clientRect.bottom - m_pSliderMgr->SliderAreaHeight()) {
			m_bShowIPTools = true;
			this->Invalidate(FALSE);
		}
	} else {
		if (!m_pSliderMgr->OnMouseMove(m_nMouseX, m_nMouseY)) {
			if (m_nMouseY < m_clientRect.bottom - (m_pSliderMgr->SliderAreaHeight()*10 >> 3)) {
				m_bShowIPTools = false;
				this->Invalidate(FALSE);
			}
		}
	}
	return 0;
}

LRESULT CMainDlg::OnMouseWheel(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	if (m_dZoom > 0) {
		int nDelta = GET_WHEEL_DELTA_WPARAM(wParam);
		PerformZoom(double(nDelta)/WHEEL_DELTA, true);
	}
	return 0;
}

LRESULT CMainDlg::OnKeyDown(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	bool bCtrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
	bool bShift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
	bool bAlt = (::GetKeyState(VK_MENU) & 0x8000) != 0;
	if (wParam == VK_ESCAPE) {
		if (m_bShowHelp) {
			m_bShowHelp = false;
			this->Invalidate(FALSE);
		} else if (m_bMovieMode) {
			StopMovieMode(); // stop any running movie/slideshow
		} else {
			this->EndDialog(0);
		}
	} else if (wParam == VK_F1) {
		m_bShowHelp = !m_bShowHelp;
		this->Invalidate(FALSE);
	} else if (!bCtrl && m_bSearchSubDirsOnEnter) {
		// search in subfolders if initially provider directory has no images
		m_bSearchSubDirsOnEnter = false;
		m_pFileList->SetNavigationMode(Helpers::NM_LoopSubDirectories);
		GotoImage(POS_Next);
	} else if (wParam == VK_PAGE_DOWN || (wParam == VK_RIGHT && !bCtrl && !bShift)) {
		GotoImage(POS_Next);
	} else if (wParam == VK_PAGE_UP || (wParam == VK_LEFT && !bCtrl && !bShift)) {
		GotoImage(POS_Previous);
	} else if (wParam == VK_HOME) {
		GotoImage(POS_First);
	} else if (wParam == VK_END) {
		GotoImage(POS_Last);
	} else if (wParam == VK_PLUS) {
		if (bCtrl && bShift && m_bAutoContrast) {
			m_pImageProcParams->ContrastCorrectionFactor = min(1.0, m_pImageProcParams->ContrastCorrectionFactor + 0.05);
			this->Invalidate(FALSE);
		} else if (bCtrl && bAlt && m_bAutoContrast) {
			m_pImageProcParams->ColorCorrectionFactor = min(0.5, m_pImageProcParams->ColorCorrectionFactor + 0.05);
			this->Invalidate(FALSE);
		} else if (bCtrl) {
			AdjustContrast(CONTRAST_INC);
		} else if (bShift) {
			AdjustGamma(1.0/GAMMA_FACTOR);
		} else {
			PerformZoom(1, true);
		}
	} else if (wParam == VK_MINUS) {
		if (bCtrl && bShift && m_bAutoContrast) {
			m_pImageProcParams->ContrastCorrectionFactor = max(0.0, m_pImageProcParams->ContrastCorrectionFactor - 0.05);
			this->Invalidate(FALSE);
		} else if (bCtrl && bAlt && m_bAutoContrast) {
			m_pImageProcParams->ColorCorrectionFactor = max(-0.5, m_pImageProcParams->ColorCorrectionFactor - 0.05);
			this->Invalidate(FALSE);
		} else if (bCtrl) {
			AdjustContrast(-CONTRAST_INC);
		} else if (bShift) {
			AdjustGamma(GAMMA_FACTOR);
		} else {
			PerformZoom(-1, true);
		}
	} else if (wParam == VK_F2) {
		ExecuteCommand(IDM_SHOW_FILENAME);
	} else if (wParam == VK_F3) {
		m_bHQResampling = !m_bHQResampling;
		this->Invalidate(FALSE);
	} else if (wParam == VK_F4) {
		ExecuteCommand(IDM_KEEP_PARAMETERS);
	} else if (wParam == VK_F5) {
		if (bShift) {
			m_bAutoContrastSection = !m_bAutoContrastSection;
			m_bAutoContrast = true;
			this->Invalidate(FALSE);
		} else {
			ExecuteCommand(IDM_AUTO_CORRECTION);
		}
	} else if (wParam == VK_F6) {
		if (bShift && bCtrl) {
			AdjustLDC(DARKEN_HIGHLIGHTS, LDC_INC);
		} else if (bCtrl) {
			AdjustLDC(BRIGHTEN_SHADOWS, LDC_INC);
		} else {
			ExecuteCommand(IDM_LDC);
		}
	} else if (wParam == VK_F7) {
		ExecuteCommand(IDM_LOOP_FOLDER);
	} else if (wParam == VK_F8) {
		ExecuteCommand(IDM_LOOP_RECURSIVELY);
	} else if (wParam == VK_F9) {
		ExecuteCommand(IDM_LOOP_SIBLINGS);
	} else if (wParam == VK_F12) {
		ExecuteCommand(IDM_SPAN_SCREENS);
	} else if (wParam >= '1' && wParam <= '9' && (!bShift || bCtrl)) {
		// Start the slideshow
		int nValue = wParam - '1' + 1;
		if (bCtrl && bShift) {
			nValue *= 10; // 1/100 seconds
		} else if (bCtrl) {
			nValue *= 100; // 1/10 seconds
		} else {
			nValue *= 1000; // seconds
		}
		StartMovieMode(1000.0/nValue);
	} else if (wParam == VK_SPACE) {
		if (fabs(m_dZoom - 1) < 0.01) {
			ResetZoomToFitScreen(false);
		} else {
			ResetZoomTo100Percents();
		}
	} else if (wParam == VK_RETURN) {
		ResetZoomToFitScreen(bCtrl);
	} else if ((wParam == VK_DOWN || wParam == VK_UP) && !bCtrl && !bShift) {
		ExecuteCommand((wParam == VK_DOWN) ? IDM_ROTATE_90 : IDM_ROTATE_270);
	} else if ((wParam == VK_DOWN || wParam == VK_UP) && bCtrl) {
		PerformZoom((wParam == VK_DOWN) ? -1 : +1, true);
	} else if (bCtrl && wParam == 'A') {
		ExchangeProcessingParams();
	} else if (wParam == 'C' || wParam == 'M' || wParam == 'N') {
		if (bCtrl && wParam == 'C') {
			ExecuteCommand(IDM_COPY);
		} else {
			if (!bCtrl) {
				ExecuteCommand((wParam == 'C') ? IDM_SORT_CREATION_DATE : (wParam == 'M') ? IDM_SORT_MOD_DATE : IDM_SORT_NAME);
			}
		}
	} else if (wParam == 'S') {
		if (!bCtrl) {
			ExecuteCommand(IDM_SAVE_PARAM_DB);
		} else {
			ExecuteCommand(IDM_SAVE);
		}
	} else if (wParam == 'D') {
		if (!bCtrl) {
			ExecuteCommand(IDM_CLEAR_PARAM_DB);
		}
	} else if (wParam == 'O') {
		if (bCtrl) {
			OpenFile(false);
		}
	} else if (wParam == 'X') {
		if (bCtrl) {
			ExecuteCommand(IDM_COPY_FULL);
		}
	} else if ((wParam == VK_DOWN || wParam == VK_UP || wParam == VK_RIGHT || wParam == VK_LEFT) && bShift) {
		if (wParam == VK_DOWN) {
			PerformPan(0, -PAN_STEP);
		} else if (wParam == VK_UP) {
			PerformPan(0, PAN_STEP);
		} else if (wParam == VK_RIGHT) {
			PerformPan(-PAN_STEP, 0);
		} else {
			PerformPan(PAN_STEP, 0);
		}
	} else {
		// look if any of the user commands wants to handle this key
		if (m_pFileList->Current() != NULL) {
			HandleUserCommands((uint32)wParam);
		}
	}

	return 1;
}

LRESULT CMainDlg::OnSysKeyDown(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	bool bShift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
	if (wParam == VK_PLUS) {
		AdjustSharpen(SHARPEN_INC);
	} else if (wParam == VK_MINUS) {
		AdjustSharpen(-SHARPEN_INC);
	} else if (wParam == VK_F4) {
		this->EndDialog(0);
	} else if (wParam == VK_F6) {
		if (bShift) {
			AdjustLDC(DARKEN_HIGHLIGHTS, -LDC_INC);
		} else {
			AdjustLDC(BRIGHTEN_SHADOWS, -LDC_INC);
		}
	}
	return 1;
}

LRESULT CMainDlg::OnGetDlgCode(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	// need to request key messages, else the dialog proc eats them all up
	return DLGC_WANTALLKEYS;
}

LRESULT CMainDlg::OnJPEGLoaded(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	// route to JPEG provider
	m_pJPEGProvider->OnJPEGLoaded((int)lParam);
	return 0;
}

LRESULT CMainDlg::OnTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	if (wParam == SLIDESHOW_TIMER_EVENT_ID && m_nCurrentTimeout > 0) {
		// Remove all timer messages for slideshow events that accumulated in the queue
		MSG msg;
		while (::PeekMessage(&msg, this->m_hWnd, WM_TIMER, WM_TIMER, PM_REMOVE)) {
			if (msg.wParam != SLIDESHOW_TIMER_EVENT_ID) {
				BOOL bNotUsed;
				OnTimer(WM_TIMER, msg.wParam, msg.lParam, bNotUsed);
			}
		}
		// Goto next image if no other messages to process are pending
		if (!::PeekMessage(&msg, this->m_hWnd, 0, 0, PM_NOREMOVE)) {
			GotoImage(POS_Next, NO_REMOVE_KEY_MSG);
		}
	} else if (wParam == ZOOM_TIMER_EVENT_ID) {
		::KillTimer(this->m_hWnd, ZOOM_TIMER_EVENT_ID);
		if (m_bTemporaryLowQ || m_bInZooming) {
			::SetTimer(this->m_hWnd, ZOOM_TEXT_TIMER_EVENT_ID, ZOOM_TEXT_TIMEOUT, NULL);
			if (m_bInZooming) m_bShowZoomFactor = true;
			m_bTemporaryLowQ = false;
			m_bInZooming = false;
			if (m_bHQResampling && m_pCurrentImage != NULL) {
				this->Invalidate(FALSE);
			}
		}
	} else if (wParam == ZOOM_TEXT_TIMER_EVENT_ID) {
		m_bShowZoomFactor = false;
		::KillTimer(this->m_hWnd, ZOOM_TEXT_TIMER_EVENT_ID);
		this->InvalidateRect(CRect(m_clientRect.right - Scale(ZOOM_TEXT_RECT_WIDTH + ZOOM_TEXT_RECT_OFFSET), 
			m_clientRect.bottom - Scale(ZOOM_TEXT_RECT_HEIGHT + ZOOM_TEXT_RECT_OFFSET), 
			m_clientRect.right - Scale(ZOOM_TEXT_RECT_OFFSET), m_clientRect.bottom - Scale(ZOOM_TEXT_RECT_OFFSET)), FALSE);
	}
	return 0;
}

// translate menu
static void TranslateMenu(HMENU hMenu) {
	int nMenuItemCount = ::GetMenuItemCount(hMenu);
	for (int i = 0; i < nMenuItemCount; i++) {
		const int TEXT_BUFF_LEN = 128;
		TCHAR menuText[TEXT_BUFF_LEN];
		::GetMenuString(hMenu, i, (LPTSTR)&menuText, TEXT_BUFF_LEN, MF_BYPOSITION);
		menuText[TEXT_BUFF_LEN-1] = 0;
		TCHAR* pTab = _tcschr(menuText, _T('\t'));
		if (pTab != NULL) {
			*pTab = 0;
			pTab++;
		}
		CString sNewMenuText = CNLS::GetString(menuText);
		if (sNewMenuText != menuText) {
			if (pTab != NULL) {
				sNewMenuText += _T('\t');
				sNewMenuText += pTab;
			}
			MENUITEMINFO menuInfo;
			memset(&menuInfo , 0, sizeof(MENUITEMINFO));
			menuInfo.cbSize = sizeof(MENUITEMINFO);
			menuInfo.fMask = MIIM_STRING;
			menuInfo.dwTypeData = (LPTSTR)(LPCTSTR)sNewMenuText;
			::SetMenuItemInfo(hMenu, i, TRUE, &menuInfo);
		}

		HMENU hSubMenu = ::GetSubMenu(hMenu, i);
		if (hSubMenu != NULL) {
			TranslateMenu(hSubMenu);
		}
	}
}

LRESULT CMainDlg::OnContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	int nX = GET_X_LPARAM(lParam);
    int nY = GET_Y_LPARAM(lParam);

	MouseOn();

	HMENU hMenu = ::LoadMenu(_Module.m_hInst, _T("PopupMenu"));
	if (hMenu == NULL) return 1;

	HMENU hMenuTrackPopup = ::GetSubMenu(hMenu, 0);
	TranslateMenu(hMenuTrackPopup);
	
	if (m_bShowFileName) ::CheckMenuItem(hMenuTrackPopup, IDM_SHOW_FILENAME, MF_CHECKED);
	if (m_bAutoContrast) ::CheckMenuItem(hMenuTrackPopup, IDM_AUTO_CORRECTION, MF_CHECKED);
	if (m_bLDC) ::CheckMenuItem(hMenuTrackPopup, IDM_LDC, MF_CHECKED);
	if (m_bKeepParams) ::CheckMenuItem(hMenuTrackPopup, IDM_KEEP_PARAMETERS, MF_CHECKED);
	HMENU hMenuNavigation = ::GetSubMenu(hMenuTrackPopup, SUBMENU_POS_NAVIGATION);
	::CheckMenuItem(hMenuNavigation,  m_pFileList->GetNavigationMode()*10 + IDM_LOOP_FOLDER, MF_CHECKED);
	HMENU hMenuOrdering = ::GetSubMenu(hMenuTrackPopup, SUBMENU_POS_DISPLAY_ORDER);
	::CheckMenuItem(hMenuOrdering,  m_pFileList->GetSorting()*10 + IDM_SORT_MOD_DATE, MF_CHECKED);
	HMENU hMenuMovie = ::GetSubMenu(hMenuTrackPopup, SUBMENU_POS_MOVIE);
	if (!m_bMovieMode) ::EnableMenuItem(hMenuMovie, IDM_STOP_MOVIE, MF_BYCOMMAND | MF_GRAYED);
	HMENU hMenuZoom = ::GetSubMenu(hMenuTrackPopup, SUBMENU_POS_ZOOM);
	if (m_bSpanVirtualDesktop) ::CheckMenuItem(hMenuZoom,  IDM_SPAN_SCREENS, MF_CHECKED);
	HMENU hMenuAutoZoomMode = ::GetSubMenu(hMenuTrackPopup, SUBMENU_POS_AUTOZOOMMODE);
	::CheckMenuItem(hMenuAutoZoomMode,  m_eAutoZoomMode*10 + IDM_AUTO_ZOOM_FIT_NO_ZOOM, MF_CHECKED);

	if (m_pCurrentImage == NULL) {
		::EnableMenuItem(hMenuTrackPopup, IDM_SAVE, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_COPY, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_COPY_FULL, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_ROTATE_90, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_ROTATE_270, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_SAVE_PARAM_DB, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_CLEAR_PARAM_DB, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, SUBMENU_POS_ZOOM, MF_BYPOSITION  | MF_GRAYED);
	} else {
		if (m_bKeepParams || CParameterDB::This().FindEntry(m_pCurrentImage->GetPixelHash()) == NULL)
			::EnableMenuItem(hMenuTrackPopup, IDM_CLEAR_PARAM_DB, MF_BYCOMMAND | MF_GRAYED);
		if (m_bKeepParams)
			::EnableMenuItem(hMenuTrackPopup, IDM_SAVE_PARAM_DB, MF_BYCOMMAND | MF_GRAYED);
	}
	if (m_bMovieMode) {
		::EnableMenuItem(hMenuTrackPopup, IDM_SAVE, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_BATCH_COPY, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_SAVE_PARAMETERS, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_SAVE_PARAM_DB, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_CLEAR_PARAM_DB, MF_BYCOMMAND | MF_GRAYED);
	} else {
		// Delete the 'Stop movie' menu entry if no movie is playing
		::DeleteMenu(hMenuTrackPopup, 0, MF_BYPOSITION);
		::DeleteMenu(hMenuTrackPopup, 0, MF_BYPOSITION);
	}

	m_bInTrackPopupMenu = true;
	int nMenuCmd = ::TrackPopupMenu(hMenuTrackPopup, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
		nX, nY, 0, this->m_hWnd, NULL);
	m_bInTrackPopupMenu = false;

	ExecuteCommand(nMenuCmd);

	::DestroyMenu(hMenu);
	return 1;
}

// Make the text edit control for renaming image colored black/white
LRESULT CMainDlg::OnCtlColorEdit(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	HDC hDC = (HDC) wParam;
	::SetTextColor(hDC, RGB(255, 255, 255));
	::SetBkColor(hDC, RGB(0, 0, 0));
	return (LRESULT)::GetStockObject(BLACK_BRUSH);
}

LRESULT CMainDlg::OnEraseBackground(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	// prevent erasing background
	return 0;
}

LRESULT CMainDlg::OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	// NOP
	return 0;
}

LRESULT CMainDlg::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	EndDialog(wID);
	return 0;
}

// Handlers for image processing UI controls

void CMainDlg::OnSaveToDB(CButtonCtrl & sender) {
	sm_instance->ExecuteCommand(IDM_SAVE_PARAM_DB);
}

void CMainDlg::OnRemoveFromDB(CButtonCtrl & sender) {
	sm_instance->ExecuteCommand(IDM_CLEAR_PARAM_DB);
}

bool CMainDlg::OnRenameFile(CTextCtrl & sender, LPCTSTR sChangedText) {
	return sm_instance->RenameCurrentFile(sChangedText);
}

///////////////////////////////////////////////////////////////////////////////////
// Private
///////////////////////////////////////////////////////////////////////////////////

void CMainDlg::ExecuteCommand(int nCommand) {
	switch (nCommand) {
		case IDM_OPEN:
			OpenFile(false);
			break;
		case IDM_SAVE:
			if (m_pCurrentImage != NULL) {
				SaveImage();
			}
			break;
		case IDM_COPY:
			if (m_pCurrentImage != NULL) {
				CClipboard::CopyImageToClipboard(this->m_hWnd, m_pCurrentImage);
			}
			break;
		case IDM_COPY_FULL:
			if (m_pCurrentImage != NULL) {
				CClipboard::CopyFullImageToClipboard(this->m_hWnd, m_pCurrentImage, *m_pImageProcParams, 
					CreateProcessingFlags(m_bHQResampling, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, false));
				this->Invalidate(FALSE);
			}
			break;
		case IDM_BATCH_COPY:
			if (m_pCurrentImage != NULL) {
				BatchCopy();
			}
			break;
		case IDM_SHOW_FILENAME:
			m_bShowFileName = !m_bShowFileName;
			this->Invalidate(FALSE);
			break;
		case IDM_NEXT:
			GotoImage(POS_Next);
			break;
		case IDM_PREV:
			GotoImage(POS_Previous);
			break;
		case IDM_LOOP_FOLDER:
		case IDM_LOOP_RECURSIVELY:
		case IDM_LOOP_SIBLINGS:
			m_pFileList->SetNavigationMode(
				(nCommand == IDM_LOOP_FOLDER) ? Helpers::NM_LoopDirectory :
				(nCommand == IDM_LOOP_RECURSIVELY) ? Helpers::NM_LoopSubDirectories : 
				Helpers::NM_LoopSameDirectoryLevel);
			if (m_bShowHelp) {
				this->Invalidate(FALSE);
			}
			break;
		case IDM_SORT_MOD_DATE:
		case IDM_SORT_CREATION_DATE:
		case IDM_SORT_NAME:
			m_pFileList->SetSorting((nCommand == IDM_SORT_CREATION_DATE) ? Helpers::FS_CreationTime : 
				(nCommand == IDM_SORT_MOD_DATE) ? Helpers::FS_LastModTime : Helpers::FS_FileName);
			if (m_bShowHelp) {
				this->Invalidate(FALSE);
			}
			break;
		case IDM_STOP_MOVIE:
			StopMovieMode();
			break;
		case IDM_MOVIE_5_FPS:
		case IDM_MOVIE_10_FPS:
		case IDM_MOVIE_25_FPS:
		case IDM_MOVIE_30_FPS:
		case IDM_MOVIE_50_FPS:
		case IDM_MOVIE_100_FPS:
			StartMovieMode(nCommand - IDM_MOVIE_START_FPS);
			break;
		case IDM_SAVE_PARAM_DB:
			if (m_pCurrentImage != NULL && !m_bMovieMode && !m_bKeepParams) {
				CParameterDBEntry newEntry;
				EProcessingFlags procFlags = CreateProcessingFlags(m_bHQResampling, m_bAutoContrast, false, m_bLDC, false);
				newEntry.InitFromProcessParams(*m_pImageProcParams, procFlags, m_nRotation);
				if (m_bUserZoom || m_bUserPan) {
					newEntry.InitGeometricParams(CSize(m_pCurrentImage->OrigWidth(), m_pCurrentImage->OrigHeight()),
						m_dZoom, CPoint(m_nOffsetX, m_nOffsetY), CSize(m_clientRect.Width(), m_clientRect.Height()));
				}
				newEntry.SetHash(m_pCurrentImage->GetPixelHash());
				if (CParameterDB::This().AddEntry(newEntry)) {
					// these parameters need to be updated when image is reused from cache
					m_pCurrentImage->SetInitialParameters(*m_pImageProcParams, procFlags, m_nRotation, m_dZoom, CPoint(m_nOffsetX, m_nOffsetY));
					m_pCurrentImage->SetIsInParamDB(true);
					ShowHideSaveDBButtons();
				}
			}
			break;
		case IDM_CLEAR_PARAM_DB:
			if (m_pCurrentImage != NULL && !m_bMovieMode && !m_bKeepParams) {
				if (CParameterDB::This().DeleteEntry(m_pCurrentImage->GetPixelHash())) {
					// restore initial parameters and realize the parameters
					EProcessingFlags procFlags = GetDefaultProcessingFlags();
					m_pCurrentImage->RestoreInitialParameters(m_pFileList->Current(), 
						GetDefaultProcessingParams(), procFlags, 0, -1, CPoint(0, 0), CSize(0, 0));
					*m_pImageProcParams = GetDefaultProcessingParams();
					// Sunset and night shot detection may has changed this
					m_pImageProcParams->LightenShadows = m_pCurrentImage->GetInitialProcessParams().LightenShadows;
					InitFromProcessingFlags(procFlags, m_bHQResampling, m_bAutoContrast, m_bAutoContrastSection, m_bLDC);
					m_nRotation = 0;
					m_dZoom = -1;
					m_pCurrentImage->SetIsInParamDB(false);
					ShowHideSaveDBButtons();
					this->Invalidate(FALSE);
				}
			}
			break;
		case IDM_ROTATE_90:
		case IDM_ROTATE_270:
			if (m_pCurrentImage != NULL) {
				uint32 nRotationDelta = (nCommand == IDM_ROTATE_90) ? 90 : 270;
				m_nRotation = (m_nRotation + nRotationDelta) % 360;
				m_pCurrentImage->Rotate(nRotationDelta);
				m_dZoom = -1;
				m_bUserZoom = false;
				m_bUserPan = false;
				this->Invalidate(FALSE);
			}
			break;
		case IDM_AUTO_CORRECTION:
			m_bAutoContrastSection = false;
			m_bAutoContrast = !m_bAutoContrast;
			this->Invalidate(FALSE);
			break;
		case IDM_LDC:
			m_bLDC = !m_bLDC;
			this->Invalidate(FALSE);
			break;
		case IDM_KEEP_PARAMETERS:
			m_bKeepParams = !m_bKeepParams;
			if (m_bKeepParams) {
				*m_pImageProcParamsKept = *m_pImageProcParams;
				m_eProcessingFlagsKept = CreateProcessingFlags(m_bHQResampling, m_bAutoContrast, false, m_bLDC, m_bKeepParams);
				m_nRotationKept = m_nRotation;
				m_dZoomKept = m_bUserZoom ? m_dZoom : -1;
				m_offsetKept = m_bUserPan ? CPoint(m_nOffsetX, m_nOffsetY) : CPoint(0, 0);
			}
			ShowHideSaveDBButtons();
			if (m_bShowHelp || m_bShowIPTools) {
				this->Invalidate(FALSE);
			}
			break;
		case IDM_SAVE_PARAMETERS:
			SaveParameters();
			break;
		case IDM_FIT_TO_SCREEN:
			ResetZoomToFitScreen(false);
			break;
		case IDM_FILL_WITH_CROP:
			ResetZoomToFitScreen(true);
			break;
		case IDM_SPAN_SCREENS:
			if (CMultiMonitorSupport::IsMultiMonitorSystem()) {
				m_dZoom = -1.0;
				this->Invalidate(FALSE);
				if (m_bSpanVirtualDesktop) {
					this->SetWindowPlacement(&m_storedWindowPlacement);
				} else {
					this->GetWindowPlacement(&m_storedWindowPlacement);
					CRect rectAllScreens = CMultiMonitorSupport::GetVirtualDesktop();
					rectAllScreens.top -= 1;
					rectAllScreens.left -= 1;
					rectAllScreens.bottom += 1;
					rectAllScreens.right += 1;
					this->SetWindowPos(HWND_TOP, &rectAllScreens, SWP_NOZORDER);
				}
				m_bSpanVirtualDesktop = !m_bSpanVirtualDesktop;
				this->GetClientRect(&m_clientRect);
			}
			break;
		case IDM_ZOOM_400:
			PerformZoom(4.0, false);
			break;
		case IDM_ZOOM_200:
			PerformZoom(2.0, false);
			break;
		case IDM_ZOOM_100:
			ResetZoomTo100Percents();
			break;
		case IDM_ZOOM_50:
			PerformZoom(0.5, false);
			break;
		case IDM_ZOOM_25:
			PerformZoom(0.25, false);
			break;
		case IDM_AUTO_ZOOM_FIT_NO_ZOOM:
		case IDM_AUTO_ZOOM_FILL_NO_ZOOM:
		case IDM_AUTO_ZOOM_FIT:
		case IDM_AUTO_ZOOM_FILL: 
			m_eAutoZoomMode = (Helpers::EAutoZoomMode)((nCommand - IDM_AUTO_ZOOM_FIT_NO_ZOOM)/10);
			m_dZoom = -1.0;
			this->Invalidate(FALSE);
			break;
		case IDM_EXIT:
			this->EndDialog(0);
			break;
	}
}

bool CMainDlg::OpenFile(bool bAtStartup) {
	StopMovieMode();
	MouseOn();
	CFileOpenDialog dlgOpen(this->m_hWnd, m_pFileList->Current(), CFileList::GetSupportedFileEndings());
	if (IDOK == dlgOpen.DoModal(this->m_hWnd)) {
		// recreate file list based on image opened
		Helpers::ESorting eOldSorting = m_pFileList->GetSorting();
		delete m_pFileList;
		m_sStartupFile = dlgOpen.m_szFileName;
		m_pFileList = new CFileList(m_sStartupFile, eOldSorting);
		// free current image and all read ahead images
		InitParametersForNewImage();
		m_pJPEGProvider->NotifyNotUsed(m_pCurrentImage);
		m_pJPEGProvider->ClearAllRequests();
		m_pCurrentImage = m_pJPEGProvider->RequestJPEG(m_pFileList, CJPEGProvider::FORWARD,
			m_pFileList->Current(), CreateProcessParams());
		AfterNewImageLoaded(true);
		m_startMouse.x = m_startMouse.y = -1;
		m_bSearchSubDirsOnEnter = false;
		m_sSaveDirectory = _T("");
		MouseOff();
		this->Invalidate(FALSE);
		return true;
	}
	return false;
}

bool CMainDlg::SaveImage() {
	if (m_bMovieMode) {
		return false;
	}

	MouseOn();

	CString sCurrentFile;
	if (m_sSaveDirectory.GetLength() == 0) {
		sCurrentFile = m_pFileList->Current();
	} else {
		sCurrentFile = m_sSaveDirectory + m_pFileList->CurrentFileTitle();
	}
	int nIndexPoint = sCurrentFile.ReverseFind(_T('.'));
	if (nIndexPoint > 0) {
		sCurrentFile.Insert(nIndexPoint, _T("_proc"));
	}
	if (sCurrentFile.Right(4).CompareNoCase(_T(".jpg")) != 0 && sCurrentFile.Right(5).CompareNoCase(_T(".jpeg")) != 0) {
		int nIdxPoint = sCurrentFile.ReverseFind(_T('.'));
		sCurrentFile = sCurrentFile.Left(nIdxPoint + 1) + _T("jpg");
	}
	CFileDialog fileDlg(FALSE, _T("jpg"), sCurrentFile, 
			OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_NOREADONLYRETURN | OFN_OVERWRITEPROMPT,
			Helpers::CReplacePipe(CString(CNLS::GetString(_T("Images"))) + _T(" (*.jpg;*.jpeg)|*.jpg;*.jpeg|") +
			CNLS::GetString(_T("All Files")) + _T("|*.*|")), m_hWnd);
	if (IDOK == fileDlg.DoModal(m_hWnd)) {
		m_sSaveDirectory = fileDlg.m_szFileName;
		m_sSaveDirectory = m_sSaveDirectory.Left(m_sSaveDirectory.ReverseFind(_T('\\')) + 1);
		if (CSaveImage::SaveImage(fileDlg.m_szFileName, m_pCurrentImage, *m_pImageProcParams, 
			CreateProcessingFlags(m_bHQResampling, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, false))) {
			m_pFileList->Reload(); // maybe image is stored to current directory - needs reload
			return true;
		} else {
			::MessageBox(m_hWnd, CNLS::GetString(_T("Error saving file")), 
				CNLS::GetString(_T("Error writing JPEG file to disk!")), MB_ICONSTOP | MB_OK); 
		}
	}
	return false;
}

void CMainDlg::BatchCopy() {
	if (m_bMovieMode) {
		return;
	}
	MouseOn();

	CBatchCopyDlg dlgBatchCopy(*m_pFileList);
	dlgBatchCopy.DoModal();
	this->Invalidate(FALSE);
}


void CMainDlg::HandleUserCommands(uint32 virtualKeyCode) {
	// iterate over user command list
	std::list<CUserCommand*>::iterator iter;
	std::list<CUserCommand*> & userCmdList = CSettingsProvider::This().UserCommandList();
	for (iter = userCmdList.begin( ); iter != userCmdList.end( ); iter++ ) {
		if ((*iter)->GetKeyCode() == virtualKeyCode) {
			MouseOn();
			if ((*iter)->Execute(this->m_hWnd, m_pFileList->Current())) {
				bool bReloadCurrent = false;
				// First go to next, then reload (else we get into troubles when the current image was deleted or moved)
				if ((*iter)->MoveToNextAfterCommand()) {
					// don't send a request if we reload all, it would be canceled anyway later
					GotoImage(POS_Next, (*iter)->NeedsReloadAll() ? NO_REQUEST : 0);
				}
				if ((*iter)->NeedsReloadAll()) {
					m_pFileList->Reload();
					m_pJPEGProvider->NotifyNotUsed(m_pCurrentImage);
					m_pJPEGProvider->ClearAllRequests();
					m_pCurrentImage = NULL;
					bReloadCurrent = true;
				} else {
					if ((*iter)->NeedsReloadFileList()) {
						m_pFileList->Reload();
						bReloadCurrent = m_pFileList->Current() == NULL; // needs "reload" in this case
					}
					if ((*iter)->NeedsReloadCurrent() && !(*iter)->MoveToNextAfterCommand()) {
						m_pJPEGProvider->NotifyNotUsed(m_pCurrentImage);
						m_pJPEGProvider->ClearRequest(m_pCurrentImage);
						m_pCurrentImage = NULL;
						bReloadCurrent = true;
					}
				}
				if (bReloadCurrent) {
					// Keep parameters when reloading
					CProcessParams procParams = CreateProcessParams();
					procParams.ProcFlags = SetProcessingFlag(procParams.ProcFlags, PFLAG_KeepParams, true);
					m_pCurrentImage = m_pJPEGProvider->RequestJPEG(m_pFileList, CJPEGProvider::FORWARD, 
						m_pFileList->Current(), procParams);
					AfterNewImageLoaded(false);
					if (!m_bUserZoom) {
						m_dZoom = -1;
					}
					this->Invalidate(FALSE);
				}
			}
			break;
		}
	}
}

void CMainDlg::StartDragging(int nX, int nY) {
	m_startMouse.x = m_startMouse.y = -1;
	m_bDragging = true;
	m_nCapturedX = nX;
	m_nCapturedY = nY;
}

void CMainDlg::DoDragging(int nX, int nY) {
	if (m_bDragging && m_pCurrentImage != NULL) {
		int nXDelta = m_nMouseX - m_nCapturedX;
		int nYDelta = m_nMouseY - m_nCapturedY;
		if (PerformPan(nXDelta, nYDelta)) {
			m_nCapturedX = m_nMouseX;
			m_nCapturedY = m_nMouseY;
			return;
		}
	}
}

void CMainDlg::EndDragging() {
	m_bDragging = false;
}

void CMainDlg::GotoImage(EImagePosition ePos) {
	GotoImage(ePos, 0);
}

void CMainDlg::GotoImage(EImagePosition ePos, int nFlags) {
	// Timer handling for slideshows
	if (ePos == POS_Next) {
		if (m_nCurrentTimeout > 0) {
			StartTimer(m_nCurrentTimeout);
		}
	} else {
		StopMovieMode();
	}

	MouseOff();

	InitParametersForNewImage();
	m_pJPEGProvider->NotifyNotUsed(m_pCurrentImage);
	m_pCurrentImage = NULL;
	
	CJPEGProvider::EReadAheadDirection eDirection = CJPEGProvider::FORWARD;
	switch (ePos) {
		case POS_First:
			m_pFileList->First();
			break;
		case POS_Last:
			m_pFileList->Last();
			break;
		case POS_Next:
			m_pFileList = m_pFileList->Next();
			break;
		case POS_Previous:
			m_pFileList = m_pFileList->Prev();
			eDirection = CJPEGProvider::BACKWARD;
			break;
	}

	// do not perform a new image request if flagged
	if (nFlags & NO_REQUEST) {
		return;
	}

	m_pCurrentImage = m_pJPEGProvider->RequestJPEG(m_pFileList, eDirection,  
		m_pFileList->Current(), CreateProcessParams());
	AfterNewImageLoaded(true);

	this->Invalidate(FALSE);
	// this will force to wait until really redrawn, preventing to process images but do not show them
	this->UpdateWindow();

	// remove key messages accumulated so far
	if (!(nFlags & NO_REMOVE_KEY_MSG)) {
		MSG msg;
		while (::PeekMessage(&msg, this->m_hWnd, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE));
	}
}

void CMainDlg::AdjustLDC(int nMode, double dInc) {
	if (nMode == BRIGHTEN_SHADOWS) {
		m_pImageProcParams->LightenShadows = min(1.0, max(0.0, m_pImageProcParams->LightenShadows + dInc));
	} else {
		m_pImageProcParams->DarkenHighlights = min(1.0, max(0.0, m_pImageProcParams->DarkenHighlights + dInc));
	}
	this->Invalidate(FALSE);
}

void CMainDlg::AdjustGamma(double dFactor) {
	m_pImageProcParams->Gamma *= dFactor;
	m_pImageProcParams->Gamma = min(2.0, max(0.5, m_pImageProcParams->Gamma));
	this->Invalidate(FALSE);
}

void CMainDlg::AdjustContrast(double dInc) {
	m_pImageProcParams->Contrast += dInc;
	m_pImageProcParams->Contrast = min(0.5, max(-0.5, m_pImageProcParams->Contrast));
	this->Invalidate(FALSE);
}

void CMainDlg::AdjustSharpen(double dInc) {
	m_pImageProcParams->Sharpen += dInc;
	m_pImageProcParams->Sharpen = min(0.5, max(0.0, m_pImageProcParams->Sharpen));
	this->Invalidate(FALSE);
}

void CMainDlg::PerformZoom(double dValue, bool bExponent) {
	double dOldZoom = m_dZoom;
	m_bUserZoom = true;
	if (bExponent) {
		m_dZoom = m_dZoom * pow(m_dZoomMult, dValue);
	} else {
		m_dZoom = dValue;
	}
	m_dZoom = max(Helpers::ZoomMin, min(Helpers::ZoomMax, m_dZoom));

	if (m_pCurrentImage == NULL) {
		return;
	}

	if (abs(m_dZoom - 1.0) < 0.01) {
		m_dZoom = 1.0;
	}
	if ((dOldZoom - 1.0)*(m_dZoom - 1.0) <= 0 && m_bInZooming) {
		// make a stop at 100 %
		m_dZoom = 1.0;
	} 

	// Never create images more than 65535 pixels wide or high - the basic processing cannot handle it
	int nOldXSize = (int)(m_pCurrentImage->OrigWidth() * dOldZoom + 0.5);
	int nOldYSize = (int)(m_pCurrentImage->OrigHeight() * dOldZoom + 0.5);
	int nNewXSize = (int)(m_pCurrentImage->OrigWidth() * m_dZoom + 0.5);
	int nNewYSize = (int)(m_pCurrentImage->OrigHeight() * m_dZoom + 0.5);
	if (nNewXSize > 65535 || nNewYSize > 65535) {
		double dFac = 65535.0/max(nNewXSize, nNewYSize);
		m_dZoom = m_dZoom*dFac;
		nNewXSize = (int)(m_pCurrentImage->OrigWidth() * m_dZoom + 0.5);
		nNewYSize = (int)(m_pCurrentImage->OrigHeight() * m_dZoom + 0.5);
	}

	if (m_nCursorCnt >= 0) {
		// cursor visible - zoom to mouse
		int nOldX = nOldXSize/2 - m_clientRect.Width()/2 + m_nMouseX - m_nOffsetX;
		int nOldY = nOldYSize/2 - m_clientRect.Height()/2 + m_nMouseY - m_nOffsetY;
		double dFac = m_dZoom/dOldZoom;
		m_nOffsetX = Helpers::RoundToInt(nNewXSize/2 - m_clientRect.Width()/2 + m_nMouseX - nOldX*dFac);
		m_nOffsetY = Helpers::RoundToInt(nNewYSize/2 - m_clientRect.Height()/2 + m_nMouseY - nOldY*dFac);
	} else {
		// cursor not visible - zoom to center
		m_nOffsetX = (int) (m_nOffsetX*m_dZoom/dOldZoom);
		m_nOffsetY = (int) (m_nOffsetY*m_dZoom/dOldZoom);
	}

	m_bInZooming = true;
	StartLowQTimer(ZOOM_TIMEOUT);
	if (fabs(dOldZoom - m_dZoom) > 0.01) {
		this->Invalidate(FALSE);
	}
}

bool CMainDlg::PerformPan(int dx, int dy) {
	if ((m_virtualImageSize.cx > 0 && m_virtualImageSize.cx > m_clientRect.Width()) ||
		(m_virtualImageSize.cy > 0 && m_virtualImageSize.cy > m_clientRect.Height())) {
		m_nOffsetX += dx;
		m_nOffsetY += dy;

		m_bUserPan = true;
		m_bInZooming = true; 
		StartLowQTimer(ZOOM_TIMEOUT);

		this->Invalidate(FALSE);
		return true;
	}
	return false;
}

void CMainDlg::SetStartupFile(LPCTSTR sStartupFile) { 
	m_sStartupFile = sStartupFile;
	if (m_sStartupFile.GetLength() == 0) {
		return;
	}

	// Extract the startup file or directory from the command line parameter string
	int nHyphen = m_sStartupFile.Find(_T('"'));
	if (nHyphen == -1) {
		// not enclosed with "", use part until first space is found
		int nSpace = m_sStartupFile.Find(_T(' '));
		if (nSpace > 0) {
			m_sStartupFile = m_sStartupFile.Left(nSpace);
		}
	} else {
		// enclosed in "", take first string enclosed in ""
		int nHyphenNext = m_sStartupFile.Find(_T('"'), nHyphen + 1);
		if (nHyphenNext > nHyphen) {
			m_sStartupFile = m_sStartupFile.Mid(nHyphen + 1, nHyphenNext - nHyphen - 1);
		}
	}
	// if it's a directory, add a trailing backslash
	if (m_sStartupFile.GetLength() > 0) {
		DWORD nAttributes = ::GetFileAttributes(m_sStartupFile);
		if (nAttributes != INVALID_FILE_ATTRIBUTES && (nAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
			if (m_sStartupFile[m_sStartupFile.GetLength() -1] != _T('\\')) {
				m_sStartupFile += _T('\\');
			}
		}
	}
}

void CMainDlg::LimitOffsets(const CRect & rect, const CSize & size) {
	int nMaxOffsetX = (size.cx - rect.Width())/2;
	nMaxOffsetX = max(0, nMaxOffsetX);
	int nMaxOffsetY = (size.cy - rect.Height())/2;
	nMaxOffsetY = max(0, nMaxOffsetY);
	m_nOffsetX = max(-nMaxOffsetX, min(+nMaxOffsetX, m_nOffsetX));
	m_nOffsetY = max(-nMaxOffsetY, min(+nMaxOffsetY, m_nOffsetY));
}

void CMainDlg::ResetZoomToFitScreen(bool bFillWithCrop) {
	if (m_pCurrentImage != NULL) {
		CSize newSize = Helpers::GetImageRect(m_pCurrentImage->OrigWidth(), m_pCurrentImage->OrigHeight(), 
			m_clientRect.Width(), m_clientRect.Height(), true, bFillWithCrop, false);
		double dOldZoom = m_dZoom;
		m_dZoom = (double)newSize.cx/m_pCurrentImage->OrigWidth();
		m_dZoom = max(Helpers::ZoomMin, min(Helpers::ZoomMax, m_dZoom));
		m_bUserZoom = m_dZoom > 1.0;
		m_bUserPan = false;
		if (fabs(dOldZoom - m_dZoom) > 0.01) {
			this->Invalidate(FALSE);
		}
	}
}

void CMainDlg::ResetZoomTo100Percents() {
	if (m_pCurrentImage != NULL && fabs(m_dZoom - 1) > 0.01) {
		double dOldZoom = m_dZoom;
		int nOffsetX, nOffsetY;
		if (m_nCursorCnt >= 0) {
			// mouse cursor visible, try to zoom image to mouse
			m_nOffsetX = nOffsetX = m_clientRect.Width()/2 - m_nMouseX;
			m_nOffsetY = nOffsetY = m_clientRect.Height()/2 - m_nMouseY;
		} else {
			nOffsetX = nOffsetY = 0;
		}

		m_bUserZoom = m_pCurrentImage->OrigWidth() > m_clientRect.Width() || m_pCurrentImage->OrigHeight() > m_clientRect.Height();
		m_bUserPan = m_bUserZoom;
		m_dZoom = 1.0;
		m_nOffsetX = (int) (m_nOffsetX*m_dZoom/dOldZoom) - nOffsetX;
		m_nOffsetY = (int) (m_nOffsetY*m_dZoom/dOldZoom) - nOffsetY;
		if (fabs(dOldZoom - m_dZoom) > 0.01) {
			this->Invalidate(FALSE);
		}
	}
}

CProcessParams CMainDlg::CreateProcessParams() {
	if (m_bKeepParams) {
		double dOldLightenShadows = m_pImageProcParamsKept->LightenShadows;
		*m_pImageProcParamsKept = *m_pImageProcParams;
		// when last image was detected as sunset or nightshot, do not take this value for next image
		if (m_bCurrentImageIsSpecialProcessing && m_pImageProcParams->LightenShadows == m_dCurrentInitialLightenShadows) {
			m_pImageProcParamsKept->LightenShadows = dOldLightenShadows;
		}
		m_eProcessingFlagsKept = CreateProcessingFlags(m_bHQResampling, m_bAutoContrast, false, m_bLDC, m_bKeepParams);
		m_nRotationKept = m_nRotation;
		m_dZoomKept = m_bUserZoom ? m_dZoom : -1;
		m_offsetKept = m_bUserPan ? CPoint(m_nOffsetX, m_nOffsetY) : CPoint(0, 0);

		return CProcessParams(m_clientRect.Width(), m_clientRect.Height(), m_nRotationKept, 
			m_dZoomKept,
			m_eAutoZoomMode,
			m_offsetKept,
			*m_pImageProcParamsKept, 
			m_eProcessingFlagsKept);
	} else {
		CSettingsProvider& sp = CSettingsProvider::This();
		return CProcessParams(m_clientRect.Width(), m_clientRect.Height(), 0,
			-1, m_eAutoZoomMode, CPoint(0, 0), 
			GetDefaultProcessingParams(),
			GetDefaultProcessingFlags());
	}
}

void CMainDlg::ResetParamsToDefault() {
	CSettingsProvider& sp = CSettingsProvider::This();
	m_nRotation = 0;
	m_dZoom = m_dZoomMult = -1.0;
	m_nOffsetX = m_nOffsetY = 0;
	m_bUserZoom = false;
	m_bUserPan = false;
	*m_pImageProcParams = GetDefaultProcessingParams();
	InitFromProcessingFlags(GetDefaultProcessingFlags(), m_bHQResampling, m_bAutoContrast, m_bAutoContrastSection, m_bLDC);
}

void CMainDlg::StartTimer(int nMilliSeconds) {
	m_nCurrentTimeout = nMilliSeconds;
	::SetTimer(this->m_hWnd, SLIDESHOW_TIMER_EVENT_ID, nMilliSeconds, NULL);
}

void CMainDlg::StopTimer(void) {
	if (m_nCurrentTimeout > 0) {
		m_nCurrentTimeout = 0;
		::KillTimer(this->m_hWnd, SLIDESHOW_TIMER_EVENT_ID);
	}
}

void CMainDlg::StartMovieMode(double dFPS) {
	// if more than this number of frames are requested per seconds, it is considered to be a movie
	const double cdFPSMovie = 4.9;

	// Save processing flags at the time movie mode starts
	if (!m_bMovieMode) {
		m_eProcFlagsBeforeMovie = CreateProcessingFlags(m_bHQResampling, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, m_bKeepParams);
	}
	// Keep parameters during movie mode
	if (!m_bKeepParams && dFPS > cdFPSMovie) {
		ExecuteCommand(IDM_KEEP_PARAMETERS);
	}
	// Turn off high quality resamping and auto corrections when requested to play many frames per second
	if (dFPS > cdFPSMovie) {
		m_bProcFlagsTouched = true;
		m_bHQResampling = false;
		m_bAutoContrastSection = false;
		m_bAutoContrast = false;
		m_bLDC = false;
	}
	m_bMovieMode = true;
	StartTimer(Helpers::RoundToInt(1000.0/dFPS));
	AfterNewImageLoaded(false);
}

void CMainDlg::StopMovieMode() {
	if (m_bMovieMode) {
		// undo changes done on processing parameters due to movie mode
		if (!GetProcessingFlag(m_eProcFlagsBeforeMovie, PFLAG_KeepParams)) {
			if (m_bKeepParams) {
				ExecuteCommand(IDM_KEEP_PARAMETERS);
			}
		}
		if (m_bProcFlagsTouched) {
			if (GetProcessingFlag(m_eProcFlagsBeforeMovie, PFLAG_AutoContrast)) {
				m_bAutoContrast = true;
			}
			if (GetProcessingFlag(m_eProcFlagsBeforeMovie, PFLAG_LDC)) {
				m_bLDC = true;
			}
			if (GetProcessingFlag(m_eProcFlagsBeforeMovie, PFLAG_HighQualityResampling)) {
				m_bHQResampling = true;
			}
		}
		m_bMovieMode = false;
		m_bProcFlagsTouched = false;
		StopTimer();
		AfterNewImageLoaded(false);
		this->Invalidate(FALSE);
	}
}

void CMainDlg::StartLowQTimer(int nTimeout) {
	m_bTemporaryLowQ = true;
	::KillTimer(this->m_hWnd, ZOOM_TIMER_EVENT_ID);
	::SetTimer(this->m_hWnd, ZOOM_TIMER_EVENT_ID, nTimeout, NULL);
}

void CMainDlg::MouseOff() {
	if (m_nCursorCnt >= 0 && m_nMouseY < m_clientRect.bottom - m_pSliderMgr->SliderAreaHeight() && !m_bInTrackPopupMenu) {
		m_nCursorCnt = ::ShowCursor(FALSE);
		m_startMouse.x = m_startMouse.y = -1;
	}
}

void CMainDlg::MouseOn() {
	if (m_nCursorCnt < 0) {
		m_nCursorCnt = ::ShowCursor(TRUE);
	}
}

void CMainDlg::InitParametersForNewImage() {
	if (!m_bKeepParams) {
		ResetParamsToDefault();
	} else if (!m_bUserZoom) {
		m_dZoom = -1;
	}

	m_bAutoContrastSection = false;
	*m_pImageProcParams2 = GetNoProcessingParams();
	m_eProcessingFlags2 = PFLAG_HighQualityResampling;

	m_bInZooming = m_bTemporaryLowQ = m_bShowZoomFactor = false;
}

void CMainDlg::DrawTextBordered(CPaintDC& dc, LPCTSTR sText, const CRect& rect, UINT nFormat) {
	static HFONT hFont = 0;
	if (hFont == 0) {
		// Use cleartype here, this improved readability
		hFont = ::CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, _T("Arial"));
	}

	HFONT hOldFont = dc.SelectFont(hFont);
	COLORREF oldColor = dc.SetTextColor(RGB(0, 0, 0));
	CRect textRect = rect;
	textRect.InflateRect(-1, -1);
	textRect.OffsetRect(-1, 0); dc.DrawText(sText, -1, &textRect, nFormat);
	textRect.OffsetRect(2, 0); dc.DrawText(sText, -1, &textRect, nFormat);
	textRect.OffsetRect(-1, -1); dc.DrawText(sText, -1, &textRect, nFormat);
	textRect.OffsetRect(0, 2); dc.DrawText(sText, -1, &textRect, nFormat);
	textRect.OffsetRect(0, -1);
	dc.SetTextColor(oldColor);
	dc.DrawText(sText, -1, &textRect, nFormat);
	dc.SelectFont(hOldFont);
}

void CMainDlg::ExchangeProcessingParams() {
	bool bOldAutoContrastSection = m_bAutoContrastSection;
	CImageProcessingParams tempParams = *m_pImageProcParams;
	EProcessingFlags tempFlags = CreateProcessingFlags(m_bHQResampling, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, false);
	*m_pImageProcParams = *m_pImageProcParams2;
	InitFromProcessingFlags(m_eProcessingFlags2, m_bHQResampling, m_bAutoContrast, m_bAutoContrastSection, m_bLDC);
	*m_pImageProcParams2 = tempParams;
	m_eProcessingFlags2 = tempFlags;
	m_bAutoContrastSection = bOldAutoContrastSection; // this flag cannot be exchanged this way
	this->Invalidate(FALSE);
}

void CMainDlg::ShowHideSaveDBButtons() {
	m_btnSaveToDB->SetShow(m_pCurrentImage != NULL && !m_bMovieMode && !m_bKeepParams);
	m_btnRemoveFromDB->SetShow(m_pCurrentImage != NULL && !m_bMovieMode && !m_bKeepParams && 
		m_pCurrentImage->IsInParamDB());
}

void CMainDlg::AfterNewImageLoaded(bool bSynchronize) {
	ShowHideSaveDBButtons();
	if (m_pCurrentImage != NULL && !m_bMovieMode) {
		m_txtParamDB->SetShow(true);
		m_txtRename->SetShow(true);
		LPCTSTR sCurrentFileTitle = m_pFileList->CurrentFileTitle();
		if (sCurrentFileTitle != NULL) {
			m_txtFileName->SetText(sCurrentFileTitle);
		} else {
			m_txtFileName->SetText(_T(""));
		}
		m_txtFileName->SetEditable(!m_pFileList->IsSlideShowList());
	} else {
		m_txtFileName->SetText(_T(""));
		m_txtFileName->SetEditable(false);
		m_txtParamDB->SetShow(false);
		m_txtRename->SetShow(false);
	}
	if (bSynchronize) {
		// after loading an image, the per image processing parameters must be synchronized with
		// the current processing parameters
		bool bLastWasSpecialProcessing = m_bCurrentImageIsSpecialProcessing;
		bool bLastWasInParamDB = m_bCurrentImageInParamDB;
		m_bCurrentImageInParamDB = false;
		m_bCurrentImageIsSpecialProcessing = false;
		if (m_pCurrentImage != NULL) {
			m_dCurrentInitialLightenShadows = m_pCurrentImage->GetInitialProcessParams().LightenShadows;
			m_bCurrentImageInParamDB = m_pCurrentImage->IsInParamDB();
			m_bCurrentImageIsSpecialProcessing = m_pCurrentImage->GetLightenShadowFactor() != 1.0f;
			if (!m_bKeepParams) {
				m_bHQResampling = GetProcessingFlag(m_pCurrentImage->GetInitialProcessFlags(), PFLAG_HighQualityResampling);
				m_bAutoContrast = GetProcessingFlag(m_pCurrentImage->GetInitialProcessFlags(), PFLAG_AutoContrast);
				m_bLDC = GetProcessingFlag(m_pCurrentImage->GetInitialProcessFlags(), PFLAG_LDC);

				m_nRotation = m_pCurrentImage->GetInitialRotation();
				m_dZoom = m_pCurrentImage->GetInititialZoom();
				CPoint offsets = m_pCurrentImage->GetInitialOffsets();
				m_nOffsetX = offsets.x;
				m_nOffsetY = offsets.y;

				if (m_pCurrentImage->HasZoomStoredInParamDB()) {
					m_bUserZoom = m_bUserPan = true;
				}

				*m_pImageProcParams = m_pCurrentImage->GetInitialProcessParams();
			} else if (m_bCurrentImageIsSpecialProcessing && m_bKeepParams) {
				// set this factor, no matter if we keep parameters
				m_pImageProcParams->LightenShadows = m_pCurrentImage->GetInitialProcessParams().LightenShadows;
			} else if (bLastWasSpecialProcessing && m_bKeepParams) {
				// take kept value when last was special processing as the special processing value is not usable
				m_pImageProcParams->LightenShadows = m_pImageProcParamsKept->LightenShadows;
			}
		}
	}
}

bool CMainDlg::RenameCurrentFile(LPCTSTR sNewFileTitle) {
	LPCTSTR sCurrentFileName = m_pFileList->Current();
	if (sCurrentFileName == NULL) {
		return false;
	}

	DWORD nAttributes = ::GetFileAttributes(sCurrentFileName);
	if (nAttributes & FILE_ATTRIBUTE_READONLY) {
		::MessageBox(this->m_hWnd, CNLS::GetString(_T("The file is read-only!")), CNLS::GetString(_T("Can't rename file")), MB_OK | MB_ICONSTOP);
		return false;
	}
	if (_tcschr(sNewFileTitle, _T('\\')) != NULL) {
		::MessageBox(this->m_hWnd, CNLS::GetString(_T("New name contains backslash character!")), CNLS::GetString(_T("Can't rename file")), MB_OK | MB_ICONSTOP);
		return false;
	}

	CString sNewFileName = sNewFileTitle;
	if (_tcschr(sNewFileTitle, _T('.')) == NULL) {
		// no ending given, take the one from original file
		LPCTSTR sEndingOld = _tcsrchr(sCurrentFileName, _T('.'));
		if (sEndingOld != NULL) {
			sNewFileName += sEndingOld;
		}
	}
	LPCTSTR sPosOld = _tcsrchr(sCurrentFileName, _T('\\'));
	int nNumChrDir = (sPosOld == NULL) ? 0 : (sPosOld - sCurrentFileName);
	sNewFileName = CString(sCurrentFileName, nNumChrDir) + _T('\\') + sNewFileName;

	// Rename the file
	if (!::MoveFileEx(sCurrentFileName, sNewFileName, 0)) {
		CString sError(CNLS::GetString(_T("Renaming file failed!")));
		DWORD lastError = ::GetLastError();
		LPTSTR lpMsgBuf = NULL;
		::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, lastError,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL);
		sError += _T("\n");
		sError += CNLS::GetString(_T("Reason: "));
		sError += lpMsgBuf;
		LocalFree(lpMsgBuf);
		::MessageBox(this->m_hWnd, sError, CNLS::GetString(_T("Can't rename file")), MB_OK | MB_ICONSTOP);
		return false;
	}

	// Set new name
	LPCTSTR sNewFinalFileTitle = _tcsrchr(sNewFileName, _T('\\'));
	m_txtFileName->SetText((sNewFinalFileTitle == NULL) ? sNewFileName : sNewFinalFileTitle+1);

	// Tell that file was renamed to JPEG Provider and file list
	CString sCurFileName(sCurrentFileName); // copy, ptr will be replaced
	m_pFileList->FileHasRenamed(sCurFileName, sNewFileName);
	m_pJPEGProvider->FileHasRenamed(sCurFileName, sNewFileName);

	// Needs to update filename
	if (m_bShowFileName || m_bShowHelp) {
		this->Invalidate(FALSE);
	}
	return false;
}

static void AddFlagText(CString& sText, LPCTSTR sFlagText, bool bFlag) {
	sText += sFlagText;
	if (bFlag) {
		sText += CNLS::GetString(_T("on"));
	} else {
		sText += CNLS::GetString(_T("off"));
	}
	sText += _T('\n');
}

void CMainDlg::SaveParameters() {
	if (m_bMovieMode) {
		return;
	}

	const int BUFF_SIZE = 128;
	TCHAR buff[BUFF_SIZE];
	CString sText = CString(CNLS::GetString(_T("Do you really want to save the following parameters as default to the INI file"))) + _T('\n') +
					Helpers::JPEGViewAppDataPath() + _T("JPEGView.ini ?\n\n");
	_stprintf_s(buff, BUFF_SIZE, CString(CNLS::GetString(_T("Contrast"))) + _T(": %.2f\n"), m_pImageProcParams->Contrast);
	sText += buff;
	_stprintf_s(buff, BUFF_SIZE, CString(CNLS::GetString(_T("Gamma"))) + _T(": %.2f\n"), m_pImageProcParams->Gamma);
	sText += buff;
	_stprintf_s(buff, BUFF_SIZE, CString(CNLS::GetString(_T("Sharpen"))) + _T(": %.2f\n"), m_pImageProcParams->Sharpen);
	sText += buff;
	_stprintf_s(buff, BUFF_SIZE, CString(CNLS::GetString(_T("Cyan-Red"))) + _T(": %.2f\n"), m_pImageProcParams->CyanRed);
	sText += buff;
	_stprintf_s(buff, BUFF_SIZE, CString(CNLS::GetString(_T("Magenta-Green"))) + _T(": %.2f\n"), m_pImageProcParams->MagentaGreen);
	sText += buff;
	_stprintf_s(buff, BUFF_SIZE, CString(CNLS::GetString(_T("Yellow-Blue"))) + _T(": %.2f\n"), m_pImageProcParams->YellowBlue);
	sText += buff;
	if (m_bLDC) {
		_stprintf_s(buff, BUFF_SIZE, CString(CNLS::GetString(_T("Lighten shadows"))) + _T(": %.2f\n"), m_pImageProcParams->LightenShadows);
		sText += buff;
		_stprintf_s(buff, BUFF_SIZE, CString(CNLS::GetString(_T("Darken highlights"))) + _T(": %.2f\n"), m_pImageProcParams->DarkenHighlights);
		sText += buff;
		_stprintf_s(buff, BUFF_SIZE, CString(CNLS::GetString(_T("Deep shadows"))) + _T(": %.2f\n"), m_pImageProcParams->LightenShadowSteepness);
		sText += buff;
	}
	AddFlagText(sText, CNLS::GetString(_T("Auto contrast and color correction: ")), m_bAutoContrast);
	AddFlagText(sText, CNLS::GetString(_T("Local density correction: ")), m_bLDC);
	sText += CNLS::GetString(_T("Order files by: "));
	if (m_pFileList->GetSorting() == Helpers::FS_CreationTime) {
		sText += CNLS::GetString(_T("Creation date/time"));
	} else if (m_pFileList->GetSorting() == Helpers::FS_LastModTime) {
		sText += CNLS::GetString(_T("Last modification date/time"));
	} else {
		sText += CNLS::GetString(_T("File name"));
	}
	sText += _T("\n");
	sText += CNLS::GetString(_T("Auto zoom mode")); sText += _T(": ");
	if (m_eAutoZoomMode == Helpers::ZM_FillScreen) {
		sText += CNLS::GetString(_T("Fill with crop"));
	} else if (m_eAutoZoomMode == Helpers::ZM_FillScreenNoZoom) {
		sText += CNLS::GetString(_T("Fill with crop (no zoom)"));
	} else if (m_eAutoZoomMode == Helpers::ZM_FitToScreen) {
		sText += CNLS::GetString(_T("Fit to screen"));
	} else {
		sText += CNLS::GetString(_T("Fit to screen (no zoom)"));
	}
	sText += _T("\n\n");
	sText += CNLS::GetString(_T("These values will override the values from the INI file located in the program folder of JPEGView!"));

	if (IDYES == this->MessageBox(sText, CNLS::GetString(_T("Confirm save default parameters")), MB_YESNO | MB_ICONQUESTION)) {
		CSettingsProvider::This().SaveSettings(*m_pImageProcParams, 
			CreateProcessingFlags(m_bHQResampling, m_bAutoContrast, 
			m_bAutoContrastSection, m_bLDC, m_bKeepParams), m_pFileList->GetSorting(), m_eAutoZoomMode);
	}
}