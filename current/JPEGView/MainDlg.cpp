// MainDlg.cpp : implementation of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"
#include <math.h>
#include <limits.h>

#include "MainDlg.h"
#include "FileList.h"
#include "JPEGProvider.h"
#include "JPEGImage.h"
#include "SettingsProvider.h"
#include "BasicProcessing.h"
#include "MultiMonitorSupport.h"
#include "HistogramCorr.h"
#include "UserCommand.h"
#include "Clipboard.h"
#include "FileOpenDialog.h"
#include "ParameterDB.h"
#include "SaveImage.h"
#include "NLS.h"
#include "HelpersGUI.h"
#include "TimerEventIDs.h"
#include "BatchCopyDlg.h"
#include "ResizeFilter.h"
#include "EXIFReader.h"
#include "EXIFHelpers.h"
#include "AboutDlg.h"
#include "CropSizeDlg.h"
#include "ProcessingThreadPool.h"
#include "PaintMemDCMgr.h"
#include "PanelMgr.h"
#include "ZoomNavigatorCtl.h"
#include "ImageProcPanelCtl.h"
#include "WndButtonPanelCtl.h"
#include "RotationPanelCtl.h"
#include "UnsharpMaskPanelCtl.h"
#include "EXIFDisplayCtl.h"
#include "NavigationPanelCtl.h"
#include "HelpDisplayCtl.h"
#include "NavigationPanel.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// Constants
//////////////////////////////////////////////////////////////////////////////////////////////

static const double GAMMA_FACTOR = 1.02; // multiplicator for gamma value
static const double CONTRAST_INC = 0.03; // increment for contrast value
static const double SHARPEN_INC = 0.05; // increment for sharpen value
static const double LDC_INC = 0.1; // increment for LDC (lighten shadows and darken highlights)
static const int NUM_THREADS = 1; // number of readahead threads to use
static const int READ_AHEAD_BUFFERS = 2; // number of readahead buffers to use (NUM_THREADS+1 is a good choice)
static const int ZOOM_TIMEOUT = 200; // refinement done after this many milliseconds
static const int ZOOM_TEXT_TIMEOUT = 1000; // zoom label disappears after this many milliseconds

static const int DARKEN_HIGHLIGHTS = 0; // used in AdjustLDC() call
static const int BRIGHTEN_SHADOWS = 1; // used in AdjustLDC() call

static const int ZOOM_TEXT_RECT_WIDTH = 70; // zoom label width
static const int ZOOM_TEXT_RECT_HEIGHT = 25; // zoom label height
static const int ZOOM_TEXT_RECT_OFFSET = 35; // zoom label offset from right border
static const int PAN_STEP = 48; // number of pixels to pan if pan with cursor keys (SHIFT+up/down/left/right)

static const bool SHOW_TIMING_INFO = false; // Set to true for debugging

static const int NO_REQUEST = 1; // used in GotoImage() method
static const int NO_REMOVE_KEY_MSG = 2; // used in GotoImage() method
static const int KEEP_PARAMETERS = 4; // used in GotoImage() method

//////////////////////////////////////////////////////////////////////////////////////////////
// Helpers
//////////////////////////////////////////////////////////////////////////////////////////////

// Gets default image processing parameters as set in INI file
static CImageProcessingParams GetDefaultProcessingParams() {
	CSettingsProvider& sp = CSettingsProvider::This();
	return CImageProcessingParams(
		sp.Contrast(),
		sp.Gamma(),
		sp.Saturation(),
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
	return CImageProcessingParams(0.0, 1.0, 1.0, CSettingsProvider::This().Sharpen(), 0.0, 0.5, 0.5, 0.25, 0.5, 0.0, 0.0, 0.0); 
}

// Gets default image processing flags as set in INI file
static EProcessingFlags GetDefaultProcessingFlags(bool bLandscapeMode) {
	CSettingsProvider& sp = CSettingsProvider::This();
	EProcessingFlags eProcFlags = PFLAG_None;
	eProcFlags = SetProcessingFlag(eProcFlags, PFLAG_HighQualityResampling, sp.HighQualityResampling());
	eProcFlags = SetProcessingFlag(eProcFlags, PFLAG_AutoContrast, sp.AutoContrastCorrection());
	eProcFlags = SetProcessingFlag(eProcFlags, PFLAG_LDC, sp.LocalDensityCorrection());
	eProcFlags = SetProcessingFlag(eProcFlags, PFLAG_LandscapeMode, bLandscapeMode);
	return eProcFlags;
}

// Create processing flags from literal boolean values
static EProcessingFlags CreateProcessingFlags(bool bHQResampling, bool bAutoContrast, 
											  bool bAutoContrastSection, bool bLDC, bool bKeepParams,
											  bool bLandscapeMode) {
	EProcessingFlags eProcFlags = PFLAG_None;
	eProcFlags = SetProcessingFlag(eProcFlags, PFLAG_HighQualityResampling, bHQResampling);
	eProcFlags = SetProcessingFlag(eProcFlags, PFLAG_AutoContrast, bAutoContrast);
	eProcFlags = SetProcessingFlag(eProcFlags, PFLAG_AutoContrastSection, bAutoContrastSection);
	eProcFlags = SetProcessingFlag(eProcFlags, PFLAG_LDC, bLDC);
	eProcFlags = SetProcessingFlag(eProcFlags, PFLAG_KeepParams, bKeepParams);
	eProcFlags = SetProcessingFlag(eProcFlags, PFLAG_LandscapeMode, bLandscapeMode);
	return eProcFlags;
}

// Initialize boolean processing values from flag bitfield
static void InitFromProcessingFlags(EProcessingFlags eFlags, bool& bHQResampling, bool& bAutoContrast, 
									bool& bAutoContrastSection, bool& bLDC, bool& bLandscapeMode) {
	bHQResampling = GetProcessingFlag(eFlags, PFLAG_HighQualityResampling);
	bAutoContrast = GetProcessingFlag(eFlags, PFLAG_AutoContrast);
	bAutoContrastSection = GetProcessingFlag(eFlags, PFLAG_AutoContrastSection);
	bLDC = GetProcessingFlag(eFlags, PFLAG_LDC);
	bLandscapeMode = GetProcessingFlag(eFlags, PFLAG_LandscapeMode);
}

// Sets the parameters according to the landscape enhancement mode
static CImageProcessingParams _SetLandscapeModeParams(bool bLandscapeMode, const CImageProcessingParams& params) {
	if (bLandscapeMode) {
		return CSettingsProvider::This().LandscapeModeParams(params);
	} else {
		return params;
	}
}

// Sets the processing flags according to the landscape enhancement mode
static EProcessingFlags _SetLandscapeModeFlags(EProcessingFlags eFlags) {
	if (GetProcessingFlag(eFlags, PFLAG_LandscapeMode)) {
		eFlags = SetProcessingFlag(eFlags, PFLAG_AutoContrast, true);
		eFlags = SetProcessingFlag(eFlags, PFLAG_LDC, true);
		return eFlags;
	} else {
		return eFlags;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Public
//////////////////////////////////////////////////////////////////////////////////////////////

CMainDlg::CMainDlg() {
	CSettingsProvider& sp = CSettingsProvider::This();

	CResizeFilterCache::This(); // Access before multiple threads are created

	// Read the string table for the requested language if one is present
	CNLS::ReadStringTable(CNLS::GetStringTableFileName(sp.Language()));

	m_bLandscapeMode = false;
	m_pImageProcParams = new CImageProcessingParams(GetDefaultProcessingParams());
	InitFromProcessingFlags(GetDefaultProcessingFlags(m_bLandscapeMode), m_bHQResampling, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, m_bLandscapeMode);

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
	m_bOutOfMemoryLastImage = false;
	m_nLastLoadError = HelpersGUI::FileLoad_Ok;

	m_eProcFlagsBeforeMovie = PFLAG_None;
	m_nRotation = 0;
	m_dZoomAtResizeStart = 1.0;
	m_bUserZoom = false;
	m_bUserPan = false;
	m_bMovieMode = false;
	m_bProcFlagsTouched = false;
	m_bInTrackPopupMenu = false;
	m_dZoom = -1.0;
	m_dZoomMult = -1.0;
	m_bDragging = false;
	m_bDoDragging = false;
	m_offsets = CPoint(0, 0);
	m_nCapturedX = m_nCapturedY = 0;
	m_nMouseX = m_nMouseY = 0;
	m_bShowHelp = false;
	m_bFullScreenMode = true;
	m_bLockPaint = true;
	m_nCurrentTimeout = 0;
	m_startMouse.x = m_startMouse.y = -1;
	m_virtualImageSize = CSize(-1, -1);
	m_bInZooming = false;
	m_bTemporaryLowQ = false;
	m_bShowZoomFactor = false;
	m_bSpanVirtualDesktop = false;
	m_bPanMouseCursorSet = false;
	m_storedWindowPlacement.length = sizeof(WINDOWPLACEMENT);
	memset(&m_storedWindowPlacement2, 0, sizeof(WINDOWPLACEMENT));
	m_storedWindowPlacement2.length = sizeof(WINDOWPLACEMENT);
	m_nMonitor = 0;
	m_monitorRect = CRect(0, 0, 0, 0);
	m_bMouseOn = false;
	m_fScaling = 1.0f;

	m_pPanelMgr = new CPanelMgr();
	m_pZoomNavigatorCtl = NULL;
	m_pEXIFDisplayCtl = NULL;
	m_pWndButtonPanelCtl = NULL;
	m_pRotationPanelCtl = NULL;
	m_pUnsharpMaskPanelCtl = NULL;
	m_pImageProcPanelCtl = NULL;
	m_pNavPanelCtl = NULL;
	m_pCropCtl = new CCropCtl(this);
}

CMainDlg::~CMainDlg() {
	delete m_pFileList;
	delete m_pJPEGProvider;
	delete m_pImageProcParams;
	delete m_pImageProcParamsKept;
	delete m_pZoomNavigatorCtl;
	delete m_pCropCtl;
	delete m_pPanelMgr; // this will delete all panel controllers and all panels
}

LRESULT CMainDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{	
	::SetWindowLong(m_hWnd, GWL_STYLE, WS_VISIBLE);

	UpdateWindowTitle();

	// get the scaling of the screen (DPI) compared to 96 DPI (design value)
	CPaintDC dc(this->m_hWnd);
	m_fScaling = ::GetDeviceCaps(dc, LOGPIXELSX)/96.0f;
	HelpersGUI::ScreenScaling = m_fScaling;

	// place window on monitor as requested in INI file
	m_nMonitor = CSettingsProvider::This().DisplayMonitor();
	m_monitorRect = CMultiMonitorSupport::GetMonitorRect(m_nMonitor);
	if (CSettingsProvider::This().ShowFullScreen()) {
		SetWindowPos(HWND_TOP, &m_monitorRect, SWP_NOZORDER);
	}
	this->GetClientRect(&m_clientRect);

	// Create image processing panel at bottom of screen
	m_pImageProcPanelCtl = new CImageProcPanelCtl(this, m_pImageProcParams, &m_bLDC, &m_bAutoContrast);
	m_pPanelMgr->AddPanelController(m_pImageProcPanelCtl);

	// Create EXIF display panel
	m_pEXIFDisplayCtl = new CEXIFDisplayCtl(this, m_pImageProcPanelCtl->GetPanel());
	m_pPanelMgr->AddPanelController(m_pEXIFDisplayCtl);

	// Create unsharp mask panel
	m_pUnsharpMaskPanelCtl = new CUnsharpMaskPanelCtl(this, m_pImageProcPanelCtl->GetPanel());
	m_pPanelMgr->AddPanelController(m_pUnsharpMaskPanelCtl);

	// Create rotation panel
	m_pRotationPanelCtl = new CRotationPanelCtl(this, m_pImageProcPanelCtl->GetPanel());
	m_pPanelMgr->AddPanelController(m_pRotationPanelCtl);

	// Create navigation panel
	m_pNavPanelCtl = new CNavigationPanelCtl(this, m_pImageProcPanelCtl->GetPanel(), &m_bFullScreenMode);
	m_pPanelMgr->AddPanelController(m_pNavPanelCtl);

	// Create window button panel (on top, right)
	m_pWndButtonPanelCtl = new CWndButtonPanelCtl(this, m_pImageProcPanelCtl->GetPanel());
	m_pPanelMgr->AddPanelController(m_pWndButtonPanelCtl);

	// Create zoom navigator
	m_pZoomNavigatorCtl = new CZoomNavigatorCtl(this, m_pImageProcPanelCtl->GetPanel());

	// set icons (for toolbar)
	HICON hIcon = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
	SetIcon(hIcon, TRUE);
	HICON hIconSmall = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	SetIcon(hIconSmall, FALSE);

	// turn on/off mouse coursor
	m_bMouseOn = !CSettingsProvider::This().ShowFullScreen();
	::ShowCursor(m_bMouseOn);

	// intitialize navigation with startup file (and folder)
	m_pFileList = new CFileList(m_sStartupFile, CSettingsProvider::This().Sorting());
	m_pFileList->SetNavigationMode(CSettingsProvider::This().Navigation());

	if (!CSettingsProvider::This().ShowFullScreen()) {
		ExecuteCommand(IDM_FULL_SCREEN_MODE_AFTER_STARTUP);
	}

	// create thread pool for processing requests on multiple CPU cores
	CProcessingThreadPool::This().CreateThreadPoolThreads();

	// create JPEG provider and request first image
	m_pJPEGProvider = new CJPEGProvider(this->m_hWnd, NUM_THREADS, READ_AHEAD_BUFFERS);	
	m_pCurrentImage = m_pJPEGProvider->RequestJPEG(m_pFileList, CJPEGProvider::FORWARD,
		m_pFileList->Current(), CreateProcessParams(), m_bOutOfMemoryLastImage);
	m_nLastLoadError = GetLoadErrorAfterOpenFile();
	AfterNewImageLoaded(true); // synchronize to per image parameters

	m_bLockPaint = false;

	this->Invalidate(FALSE);

	this->DragAcceptFiles();

	return TRUE;
}

LRESULT CMainDlg::OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	static bool s_bFirst = true;

	if (m_bLockPaint) {
		return 0;
	}

	// On first paint show 'Open File' dialog if no file passed on command line
	if (s_bFirst) {
		s_bFirst = false;
		if (m_sStartupFile.GetLength() == 0) {
			if (!OpenFile(true)) {
				EndDialog(0);
			}
		}
	}

	CPaintDC dc(m_hWnd);

	this->GetClientRect(&m_clientRect);
	CRect imageProcessingArea = m_pImageProcPanelCtl->PanelRect();
	CRectF visRectZoomNavigator(0.0f, 0.0f, 1.0f, 1.0f);
	CBrush backBrush;
	backBrush.CreateSolidBrush(CSettingsProvider::This().ColorBackground());

	// Exclude the help display from clipping area to reduce flickering
	CHelpDisplayCtl* pHelpDisplayCtl = NULL;
	std::list<CRect> excludedClippingRects;
	if (m_bShowHelp) {
		pHelpDisplayCtl = new CHelpDisplayCtl(this, dc, m_pImageProcParams);
		excludedClippingRects.push_back(pHelpDisplayCtl->PanelRect());
	}

	// Panels are handled over memory DCs to eliminate flickering
	CPaintMemDCMgr memDCMgr(dc);

	if (m_pCurrentImage == NULL) {
		m_pPanelMgr->OnPrePaint(dc);
		m_pPanelMgr->PrepareMemDCMgr(memDCMgr, excludedClippingRects);
		memDCMgr.ExcludeFromClippingRegion(dc, excludedClippingRects);
		dc.FillRect(&m_clientRect, backBrush);
	} else {
		// do this as very first - may changes size of image
		m_pCurrentImage->VerifyRotation(m_nRotation);

		// Find out zoom multiplier if not yet known
		if (m_dZoomMult < 0.0) {
			double dZoomToFit;
			CSize fittedSize = Helpers::GetImageRect(m_pCurrentImage->OrigWidth(), m_pCurrentImage->OrigHeight(), 
				m_clientRect.Width(), m_clientRect.Height(), true, false, false, dZoomToFit);
			// Zoom multiplier (zoom step) should be around 1.1 but reach the value 1.0 after an integral number
			// of zooming steps
			int n = Helpers::RoundToInt(log(1/dZoomToFit)/log(1.1));
			m_dZoomMult = (n == 0) ? 1.1 : exp(log(1/dZoomToFit)/n);
		}

		// find out the new vitual image size and the size of the bitmap to request
		CSize newSize = Helpers::GetVirtualImageSize(CSize(m_pCurrentImage->OrigWidth(), m_pCurrentImage->OrigHeight()),
			m_clientRect.Size(), m_eAutoZoomMode, m_dZoom);
		m_virtualImageSize = newSize;
		m_offsets = Helpers::LimitOffsets(m_offsets, m_clientRect.Size(), newSize);

		// Clip to client rectangle and request the DIB
		CSize clippedSize(min(m_clientRect.Width(), newSize.cx), min(m_clientRect.Height(), newSize.cy));
		CPoint offsetsInImage = m_pCurrentImage->ConvertOffset(newSize, clippedSize, m_offsets);

		void* pDIBData;
		if (m_pUnsharpMaskPanelCtl->IsVisible()) {
			pDIBData = m_pUnsharpMaskPanelCtl->GetUSMDIBForPreview(clippedSize, offsetsInImage, 
			    *m_pImageProcParams, CreateProcessingFlags(m_bHQResampling && !m_bTemporaryLowQ, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, false, m_bLandscapeMode));
		} else if (m_pRotationPanelCtl->IsVisible()) {
			pDIBData = m_pRotationPanelCtl->GetRotatedDIBForPreview(newSize, clippedSize, offsetsInImage, 
				*m_pImageProcParams, CreateProcessingFlags(false, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, false, m_bLandscapeMode));
		} else {
			pDIBData = m_pCurrentImage->GetDIB(newSize, clippedSize, offsetsInImage, 
				*m_pImageProcParams, 
				CreateProcessingFlags(m_bHQResampling && !m_bTemporaryLowQ, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, false, m_bLandscapeMode));
		}

		// Zoom navigator - check if visible and create exclusion rectangle
		if (m_pZoomNavigatorCtl->IsVisible()) {
			visRectZoomNavigator = m_pZoomNavigatorCtl->GetVisibleRect(newSize, clippedSize, offsetsInImage);
			excludedClippingRects.push_back(m_pZoomNavigatorCtl->PanelRect());
		}

		m_pPanelMgr->OnPrePaint(dc);
		m_pPanelMgr->PrepareMemDCMgr(memDCMgr, excludedClippingRects);
		memDCMgr.ExcludeFromClippingRegion(dc, excludedClippingRects);

		// Paint the DIB
		if (pDIBData != NULL) {
			BITMAPINFO bmInfo;
			CPoint ptDIBStart = HelpersGUI::DrawDIB32bppWithBlackBorders(dc, bmInfo, pDIBData, backBrush, m_clientRect, clippedSize);
			// The DIB is also blitted into the memory DCs of the panels
			memDCMgr.BlitImageToMemDC(pDIBData, &bmInfo, ptDIBStart, m_pNavPanelCtl->CurrentBlendingFactor());
		}
	}

	// Restore the old clipping region by adding the excluded rectangles again
	memDCMgr.IncludeIntoClippingRegion(dc, excludedClippingRects);

	// paint zoom navigator
	m_pZoomNavigatorCtl->OnPaint(dc, visRectZoomNavigator, m_pImageProcParams,
		CreateProcessingFlags(!m_pRotationPanelCtl->IsVisible(), m_bAutoContrast, false, m_bLDC, false, m_bLandscapeMode), 
		m_pRotationPanelCtl->IsVisible() ? m_pRotationPanelCtl->GetLQRotationAngle() : 0.0);

	dc.SelectStockFont(SYSTEM_FONT);
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(CSettingsProvider::This().ColorGUI());
	dc.SetBkColor(RGB(0, 0, 0));

	// Display file name if enabled
	if (m_bShowFileName) {
		CString sFileName;
		if (m_pCurrentImage != NULL && m_pCurrentImage->IsClipboardImage()) {
			sFileName = CurrentFileName(false);
		} else if (m_pFileList->Current() != NULL) {
			sFileName.Format(_T("[%d/%d]  %s"), m_pFileList->CurrentIndex() + 1, m_pFileList->Size(), m_pFileList->Current());
		}
		HelpersGUI::DrawTextBordered(dc, sFileName, CRect(Scale(2) + imageProcessingArea.left, 0, imageProcessingArea.right, Scale(30)), DT_LEFT); 
	}

	// Display errors and warnings
	if (m_sStartupFile.IsEmpty()) {
		CRect rectText(0, m_clientRect.Height()/2 - Scale(40), m_clientRect.Width(), m_clientRect.Height());
		dc.DrawText(CNLS::GetString(_T("Select file to display in 'File Open' dialog")), -1, &rectText, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
	} else if (m_pCurrentImage == NULL) {
		HelpersGUI::DrawImageLoadErrorText(dc, m_clientRect,
			(m_nLastLoadError == HelpersGUI::FileLoad_SlideShowListInvalid) ? m_sStartupFile :
			(m_nLastLoadError == HelpersGUI::FileLoad_NoFilesInDirectory) ? m_pFileList->CurrentDirectory() : CurrentFileName(false),
			m_nLastLoadError + (m_bOutOfMemoryLastImage ? HelpersGUI::FileLoad_OutOfMemory : 0));
	}

	// Now blit the memory DCs of the panels to the screen
	memDCMgr.PaintMemDCToScreen();

	// Show timing info if requested
	if (SHOW_TIMING_INFO && m_pCurrentImage != NULL) {
		TCHAR buff[256];
		_stprintf_s(buff, 256, _T("Loading: %.2f ms, Last op: %.2f ms, Last resize: %s, Last sharpen: %.2f ms"), m_pCurrentImage->GetLoadTickCount(), 
			m_pCurrentImage->LastOpTickCount(), CBasicProcessing::TimingInfo(), m_pCurrentImage->GetUnsharpMaskTickCount());
		dc.SetTextColor(RGB(255, 255, 255));
		dc.SetBkMode(OPAQUE);
		dc.TextOut(5, 5, buff);
		dc.SetBkMode(TRANSPARENT);
	}

	// Show current zoom factor
	if (m_bInZooming || m_bShowZoomFactor) {
		TCHAR buff[32];
		_stprintf_s(buff, 32, _T("%d %%"), int(m_dZoom*100 + 0.5));
		dc.SetTextColor(CSettingsProvider::This().ColorGUI());
		HelpersGUI::DrawTextBordered(dc, buff, GetZoomTextRect(imageProcessingArea), DT_RIGHT);
	}

	// Display the help screen
	if (pHelpDisplayCtl != NULL) {
		pHelpDisplayCtl->Show();
	}

	// let crop controller and panels paint its stuff
	m_pCropCtl->OnPaint(dc);
	m_pPanelMgr->OnPostPaint(dc);

	delete pHelpDisplayCtl;

	SetCursorForMoveSection();

	return 0;
}

LRESULT CMainDlg::OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	bool bKeepFitToScreen = fabs(m_dZoom - GetZoomFactorForFitToScreen(false, false)) < 0.01;
	this->GetClientRect(&m_clientRect);
	this->Invalidate(FALSE);
	if (m_clientRect.Width() < 800) {
		m_bShowHelp = false;
		m_pImageProcPanelCtl->SetVisible(false);
	}
	m_nMouseX = m_nMouseY = -1;

	// keep fit to screen
	if (bKeepFitToScreen) {
		if (fabs(m_dZoom - GetZoomFactorForFitToScreen(false, false)) >= 0.00001) {
			StartLowQTimer(ZOOM_TIMEOUT);
		}
		ResetZoomToFitScreen(false, false);
	}
	return 0;
}

LRESULT CMainDlg::OnGetMinMaxInfo(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	if (m_pJPEGProvider != NULL) {
		MINMAXINFO* pMinMaxInfo = (MINMAXINFO*) lParam;
		pMinMaxInfo->ptMinTrackSize = CPoint(400, 300);
		return 1;
	} else {
		return 0;
	}
}

LRESULT CMainDlg::OnAnotherInstanceStarted(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	// Another instance has been started, terminate this one
	if (lParam == KEY_MAGIC && m_bFullScreenMode) {
		this->EndDialog(0);
	}
	return 0;
}

LRESULT CMainDlg::OnLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	this->SetCapture();
	SetCursorForMoveSection();
	CPoint pointClicked(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
	bool bShowRotationPanel = m_pRotationPanelCtl->IsVisible();
	bool bEatenByPanel = m_pPanelMgr->OnMouseLButton(MouseEvent_BtnDown, pointClicked.x, pointClicked.y);

	if (!bEatenByPanel) {
		bool bCtrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
		bool bDraggingRequired = m_virtualImageSize.cx > m_clientRect.Width() || m_virtualImageSize.cy > m_clientRect.Height();
		if ((bCtrl || !bDraggingRequired) && !bShowRotationPanel) {
			m_pCropCtl->StartCropping(pointClicked.x, pointClicked.y);
		} else if (!m_pZoomNavigatorCtl->OnMouseLButton(MouseEvent_BtnDown, pointClicked.x, pointClicked.y) && !bShowRotationPanel) {
			StartDragging(pointClicked.x, pointClicked.y, false);
		}
	}

	m_pCropCtl->ResetStartCropOnNextClick();
	return 0;
}

LRESULT CMainDlg::OnNCLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	m_dZoomAtResizeStart = m_dZoom;
	bHandled = FALSE; // do not handle message, this would block correct processing by OS
	return 0;
}

LRESULT CMainDlg::OnMButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	this->SetCapture();
	StartDragging(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), false);
	return 0;
}

LRESULT CMainDlg::OnLButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	if (m_bDragging) {
		EndDragging();
	} else if (m_pCropCtl->IsCropping()) {
		m_pCropCtl->EndCropping();
	} else {
		m_pPanelMgr->OnMouseLButton(MouseEvent_BtnUp, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
	}
	::ReleaseCapture();
	return 0;
}

LRESULT CMainDlg::OnLButtonDblClk(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	if (!m_bDragging && !m_pCropCtl->IsCropping()) {
		m_pPanelMgr->OnMouseLButton(MouseEvent_BtnDblClk, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
	}
	return 0;
}

LRESULT CMainDlg::OnMButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	EndDragging();
	::ReleaseCapture();
	return 0;
}

LRESULT CMainDlg::OnXButtonDown(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	if (!m_pPanelMgr->IsModalPanelShown()) {
		if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) {
			GotoImage(POS_Next);
		} else {
			GotoImage(POS_Previous);
		}
	}
	return 0;
}

LRESULT CMainDlg::OnMouseMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	// Turn mouse pointer on when mouse has moved some distance
	int nOldMouseY = m_nMouseY;
	int nOldMouseX = m_nMouseX;
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

	// Do dragging or cropping when needed, else pass event to panel manager and zoom navigator
	if (m_bDragging) {
		DoDragging();
	} else if (m_pCropCtl->IsCropping()) {
		m_pCropCtl->DoCropping(m_nMouseX, m_nMouseY);
	} else if (!m_pPanelMgr->OnMouseMove(m_nMouseX, m_nMouseY)) {
		m_pZoomNavigatorCtl->OnMouseMove(nOldMouseX, nOldMouseY);
	}
	return 0;
}

LRESULT CMainDlg::OnMouseWheel(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	bool bCtrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
	int nDelta = GET_WHEEL_DELTA_WPARAM(wParam);
	if (!bCtrl && CSettingsProvider::This().NavigateWithMouseWheel() && !m_pPanelMgr->IsModalPanelShown()) {
		if (nDelta < 0) {
			GotoImage(POS_Next);
		} else if (nDelta > 0) {
			GotoImage(POS_Previous);
		}
	} else if (m_dZoom > 0 && !m_pUnsharpMaskPanelCtl->IsVisible()) {
		PerformZoom(double(nDelta)/WHEEL_DELTA, true, m_bMouseOn);
	}
	return 0;
}

LRESULT CMainDlg::OnKeyDown(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	bool bCtrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
	bool bShift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
	bool bAlt = (::GetKeyState(VK_MENU) & 0x8000) != 0;
	if (m_pPanelMgr->OnKeyDown((unsigned int)wParam, bShift, bAlt, bCtrl)) {
		return 1; // a panel has handled the key
	}
	bool bHandled = false;
	if (wParam == VK_ESCAPE) {
		bHandled = true;
		if (m_bShowHelp) {
			m_bShowHelp = false;
			this->Invalidate(FALSE);
		} else if (m_bMovieMode) {
			StopMovieMode(); // stop any running movie/slideshow
		} else {
			this->EndDialog(0);
		}
	} else if (wParam == VK_F1) {
		bHandled = true;
		m_bShowHelp = !m_bShowHelp;
		if (m_clientRect.Width() < 800) {
			m_bShowHelp = false;
		}
		this->Invalidate(FALSE);
	} else if (!bCtrl && m_nLastLoadError == HelpersGUI::FileLoad_NoFilesInDirectory) {
		// search in subfolders if initially provider directory has no images
		bHandled = true;
		m_pFileList->SetNavigationMode(Helpers::NM_LoopSubDirectories);
		GotoImage(POS_Next);
	} else if (wParam == VK_PAGE_DOWN || (wParam == VK_RIGHT && !bCtrl && !bShift)) {
		bHandled = true;
		GotoImage(POS_Next);
	} else if (wParam == VK_PAGE_UP || (wParam == VK_LEFT && !bCtrl && !bShift)) {
		bHandled = true;
		GotoImage(POS_Previous);
	} else if ((wParam == VK_LEFT || wParam == VK_RIGHT) && bCtrl && m_pFileList->FileMarkedForToggle()) {
		bHandled = true;
		GotoImage(POS_Toggle);
	} else if (wParam == VK_HOME) {
		bHandled = true;
		GotoImage(POS_First);
	} else if (wParam == VK_END) {
		bHandled = true;
		GotoImage(POS_Last);
	} else if (wParam == VK_PLUS) {
		bHandled = true;
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
			PerformZoom(1, true, m_bMouseOn);
		}
	} else if (wParam == VK_MINUS) {
		bHandled = true;
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
			PerformZoom(-1, true, m_bMouseOn);
		}
	} else if (wParam == VK_F2) {
		bHandled = true;
		ExecuteCommand(bCtrl ? IDM_SHOW_FILENAME : IDM_SHOW_FILEINFO);
	} else if (wParam == VK_F3) {
		bHandled = true;
		m_bHQResampling = !m_bHQResampling;
		this->Invalidate(FALSE);
	} else if (wParam == VK_F4) {
		bHandled = true;
		ExecuteCommand(IDM_KEEP_PARAMETERS);
	} else if (wParam == VK_F5) {
		bHandled = true;
		if (bShift) {
			m_bAutoContrastSection = !m_bAutoContrastSection;
			m_bAutoContrast = true;
			this->Invalidate(FALSE);
		} else {
			ExecuteCommand(IDM_AUTO_CORRECTION);
		}
	} else if (wParam == VK_F6) {
		bHandled = true;
		if (bShift && bCtrl) {
			AdjustLDC(DARKEN_HIGHLIGHTS, LDC_INC);
		} else if (bCtrl) {
			AdjustLDC(BRIGHTEN_SHADOWS, LDC_INC);
		} else {
			ExecuteCommand(IDM_LDC);
		}
	} else if (wParam == VK_F7) {
		bHandled = true;
		ExecuteCommand(IDM_LOOP_FOLDER);
	} else if (wParam == VK_F8) {
		bHandled = true;
		ExecuteCommand(IDM_LOOP_RECURSIVELY);
	} else if (wParam == VK_F9) {
		bHandled = true;
		ExecuteCommand(IDM_LOOP_SIBLINGS);
	} else if (wParam == VK_F11) {
		bHandled = true;
		ExecuteCommand(IDM_SHOW_NAVPANEL);
	} else if (wParam == VK_F12) {
		bHandled = true;
		if (bCtrl) {
			ToggleMonitor();
		} else {
			ExecuteCommand(IDM_SPAN_SCREENS);
		}
	} else if (wParam >= '1' && wParam <= '9' && (!bShift || bCtrl)) {
		// Start the slideshow
		bHandled = true;
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
		bHandled = true;
		if (fabs(m_dZoom - 1) < 0.01) {
			ResetZoomToFitScreen(false, true);
		} else {
			ResetZoomTo100Percents(m_bMouseOn);
		}
	} else if (wParam == VK_RETURN) {
		bHandled = true;
		ResetZoomToFitScreen(bCtrl, true);
	} else if ((wParam == VK_DOWN || wParam == VK_UP) && !bCtrl && !bShift) {
		bHandled = true;
		ExecuteCommand((wParam == VK_DOWN) ? IDM_ROTATE_90 : IDM_ROTATE_270);
	} else if ((wParam == VK_DOWN || wParam == VK_UP) && bCtrl) {
		bHandled = true;
		PerformZoom((wParam == VK_DOWN) ? -1 : +1, true, m_bMouseOn);
	} else if (bCtrl && wParam == 'A') {
		bHandled = true;
		ExchangeProcessingParams();
	} else if (wParam == 'C' || wParam == 'M' || wParam == 'N') {
		if (bCtrl && wParam == 'C') {
			bHandled = true;
			ExecuteCommand(IDM_COPY);
		} else {
			if (!bCtrl) {
				bHandled = true;
				ExecuteCommand((wParam == 'C') ? IDM_SORT_CREATION_DATE : (wParam == 'M') ? IDM_SORT_MOD_DATE : IDM_SORT_NAME);
			} else if (bCtrl && wParam == 'M') {
				bHandled = true;
				if (bShift) {
					ExecuteCommand(IDM_TOUCH_IMAGE);
				} else if (m_pFileList->Current() != NULL && m_pCurrentImage != NULL && !m_pCurrentImage->IsClipboardImage()) {
					m_pFileList->MarkCurrentFile();
				}
			}
		}
	} else if (wParam == 'S') {
		bHandled = true;
		if (!bCtrl) {
			ExecuteCommand(IDM_SAVE_PARAM_DB);
		} else {
			if (!bShift && m_pCurrentImage != NULL && !m_pCurrentImage->IsClipboardImage() && CSettingsProvider::This().SaveWithoutPrompt()) {
				SaveImageNoPrompt(CurrentFileName(false), true);
			} else {
				ExecuteCommand(bShift ? IDM_SAVE_SCREEN : IDM_SAVE);
			}
		}
	} else if (wParam == 'D') {
		if (!bCtrl) {
			bHandled = true;
			ExecuteCommand(IDM_CLEAR_PARAM_DB);
		}
	} else if (wParam == 'E') {
		if (bCtrl && bShift) {
			bHandled = true;
			ExecuteCommand(IDM_TOUCH_IMAGE_EXIF);
		}
	} else if (wParam == 'O') {
		if (bCtrl) {
			bHandled = true;
			OpenFile(false);
		}
	}  else if (wParam == 'R') {
		if (bCtrl) {
			bHandled = true;
			ExecuteCommand(IDM_RELOAD);
		}
	} else if (wParam == 'X') {
		if (bCtrl) {
			bHandled = true;
			ExecuteCommand(IDM_COPY_FULL);
		}
	} else if (wParam == 'V') {
		if (bCtrl) {
			bHandled = true;
			ExecuteCommand(IDM_PASTE);
		}
	} else if (wParam == 'L') {
		if (bCtrl) {
			bHandled = true;
			ExecuteCommand(IDM_LANDSCAPE_MODE);
		}
	} else if (wParam == 'W') {
		if (bCtrl) {
			bHandled = true;
			ExecuteCommand(IDM_FULL_SCREEN_MODE);
		}
	} 
	else if ((wParam == VK_DOWN || wParam == VK_UP || wParam == VK_RIGHT || wParam == VK_LEFT) && bShift) {
		bHandled = true;
		if (wParam == VK_DOWN) {
			PerformPan(0, -PAN_STEP, false);
		} else if (wParam == VK_UP) {
			PerformPan(0, PAN_STEP, false);
		} else if (wParam == VK_RIGHT) {
			PerformPan(-PAN_STEP, 0, false);
		} else {
			PerformPan(PAN_STEP, 0, false);
		}
	}
	if (!bHandled) {
		// look if any of the user commands wants to handle this key
		if (m_pFileList->Current() != NULL) {
			HandleUserCommands((uint32)wParam);
		}
	}

	return 1;
}

LRESULT CMainDlg::OnSysKeyDown(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	if (m_pPanelMgr->IsModalPanelShown()) {
		return 1;
	}
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

LRESULT CMainDlg::OnDropFiles(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	HDROP hDrop = (HDROP) wParam;
	if (hDrop != NULL && !m_pPanelMgr->IsModalPanelShown()) {
		const int BUFF_SIZE = 512;
		TCHAR buff[BUFF_SIZE];
		if (::DragQueryFile(hDrop, 0, (LPTSTR) &buff, BUFF_SIZE - 1) > 0) {
			if (::GetFileAttributes(buff) & FILE_ATTRIBUTE_DIRECTORY) {
				_tcsncat_s(buff, BUFF_SIZE, _T("\\"), BUFF_SIZE);
			}
			OpenFile(buff);
		}
		::DragFinish(hDrop);
	}
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
		m_pZoomNavigatorCtl->InvalidateZoomNavigatorRect();
		CRect imageProcArea = m_pImageProcPanelCtl->PanelRect();
		this->InvalidateRect(GetZoomTextRect(imageProcArea), FALSE);
	} else {
		if (!m_pCropCtl->OnTimer((int)wParam)) {
			m_pPanelMgr->OnTimer((int)wParam);
		}
	}
	return 0;
}

LRESULT CMainDlg::OnContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	if (m_pPanelMgr->IsModalPanelShown()) {
		return 1;
	}
	int nX = GET_X_LPARAM(lParam);
    int nY = GET_Y_LPARAM(lParam);

	MouseOn();

	HMENU hMenu = ::LoadMenu(_Module.m_hInst, _T("PopupMenu"));
	if (hMenu == NULL) return 1;

	HMENU hMenuTrackPopup = ::GetSubMenu(hMenu, 0);
	HelpersGUI::TranslateMenuStrings(hMenuTrackPopup);
	
	if (m_pEXIFDisplayCtl->IsActive()) ::CheckMenuItem(hMenuTrackPopup, IDM_SHOW_FILEINFO, MF_CHECKED);
	if (m_bShowFileName) ::CheckMenuItem(hMenuTrackPopup, IDM_SHOW_FILENAME, MF_CHECKED);
	if (m_pNavPanelCtl->IsActive()) ::CheckMenuItem(hMenuTrackPopup, IDM_SHOW_NAVPANEL, MF_CHECKED);
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
	if (m_bFullScreenMode) ::CheckMenuItem(hMenuZoom,  IDM_FULL_SCREEN_MODE, MF_CHECKED);
	HMENU hMenuAutoZoomMode = ::GetSubMenu(hMenuTrackPopup, SUBMENU_POS_AUTOZOOMMODE);
	::CheckMenuItem(hMenuAutoZoomMode,  m_eAutoZoomMode*10 + IDM_AUTO_ZOOM_FIT_NO_ZOOM, MF_CHECKED);
	HMENU hMenuSettings = ::GetSubMenu(hMenuTrackPopup, SUBMENU_POS_SETTINGS);
	HMENU hMenuModDate = ::GetSubMenu(hMenuTrackPopup, SUBMENU_POS_MODDATE);

	if (CSettingsProvider::This().StoreToEXEPath()) ::EnableMenuItem(hMenuSettings, IDM_EDIT_USER_CONFIG, MF_BYCOMMAND | MF_GRAYED);
	if (CParameterDB::This().IsEmpty()) ::EnableMenuItem(hMenuSettings, IDM_BACKUP_PARAMDB, MF_BYCOMMAND | MF_GRAYED);

	::EnableMenuItem(hMenuMovie, IDM_SLIDESHOW_START, MF_BYCOMMAND | MF_GRAYED);
	::EnableMenuItem(hMenuMovie, IDM_MOVIE_START_FPS, MF_BYCOMMAND | MF_GRAYED);

	bool bCanPaste = ::IsClipboardFormatAvailable(CF_DIB);
	if (!bCanPaste) ::EnableMenuItem(hMenuTrackPopup, IDM_PASTE, MF_BYCOMMAND | MF_GRAYED);

	if (m_pCurrentImage == NULL) {
		::EnableMenuItem(hMenuTrackPopup, IDM_SAVE, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_RELOAD, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_COPY, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_COPY_FULL, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_ROTATE_90, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_ROTATE_270, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_SAVE_PARAM_DB, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_CLEAR_PARAM_DB, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, SUBMENU_POS_ZOOM, MF_BYPOSITION  | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, SUBMENU_POS_MODDATE, MF_BYPOSITION  | MF_GRAYED);
	} else {
		if (m_bKeepParams || m_pCurrentImage->IsClipboardImage() ||
			CParameterDB::This().FindEntry(m_pCurrentImage->GetPixelHash()) == NULL)
			::EnableMenuItem(hMenuTrackPopup, IDM_CLEAR_PARAM_DB, MF_BYCOMMAND | MF_GRAYED);
		if (m_bKeepParams || m_pCurrentImage->IsClipboardImage())
			::EnableMenuItem(hMenuTrackPopup, IDM_SAVE_PARAM_DB, MF_BYCOMMAND | MF_GRAYED);
		if (m_pCurrentImage->IsClipboardImage()) {
			::EnableMenuItem(hMenuModDate, IDM_TOUCH_IMAGE, MF_BYCOMMAND | MF_GRAYED);
			::EnableMenuItem(hMenuModDate, IDM_TOUCH_IMAGE_EXIF, MF_BYCOMMAND | MF_GRAYED);
		}
		if (m_pCurrentImage->GetEXIFReader() == NULL || !m_pCurrentImage->GetEXIFReader()->GetAcquisitionTimePresent()) {
			::EnableMenuItem(hMenuModDate, IDM_TOUCH_IMAGE_EXIF, MF_BYCOMMAND | MF_GRAYED);
		}
	}
	if (m_bMovieMode) {
		::EnableMenuItem(hMenuTrackPopup, IDM_SAVE, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_RELOAD, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_BATCH_COPY, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_SAVE_PARAMETERS, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_SAVE_PARAM_DB, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_CLEAR_PARAM_DB, MF_BYCOMMAND | MF_GRAYED);
	} else {
		// Delete the 'Stop movie' menu entry if no movie is playing
		::DeleteMenu(hMenuTrackPopup, 0, MF_BYPOSITION);
		::DeleteMenu(hMenuTrackPopup, 0, MF_BYPOSITION);
	}

	int nMenuCmd = TrackPopupMenu(CPoint(nX, nY), hMenuTrackPopup);
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

///////////////////////////////////////////////////////////////////////////////////
// Static helpers for being called from UI controls
///////////////////////////////////////////////////////////////////////////////////

void CMainDlg::OnExecuteCommand(void* pContext, int nParameter, CButtonCtrl & sender) {
	((CMainDlg*)pContext)->ExecuteCommand(nParameter);
}

void CMainDlg::ToggleWindowMode(CMainDlg* pMainDlg, CButtonCtrl & sender, bool bSetCursor) {
	pMainDlg->ExecuteCommand(IDM_FULL_SCREEN_MODE);

	pMainDlg->m_pNavPanelCtl->GetPanel()->RequestRepositioning();
	if (bSetCursor) {
		CRect rect = sender.GetPosition();
		CPoint ptWnd = rect.CenterPoint();
		::ClientToScreen(pMainDlg->m_hWnd, &ptWnd);
		::SetCursorPos(ptWnd.x, ptWnd.y);
	}
}

bool CMainDlg::IsCurrentImageFitToScreen(void* pContext) {
	CMainDlg* pThis = (CMainDlg*)pContext;
	return fabs(pThis->m_dZoom - pThis->GetZoomFactorForFitToScreen(false, true)) <= 0.01;
}

///////////////////////////////////////////////////////////////////////////////////
// Private
///////////////////////////////////////////////////////////////////////////////////

void CMainDlg::ExecuteCommand(int nCommand) {
	switch (nCommand) {
		case IDM_MINIMIZE:
			this->ShowWindow(SW_MINIMIZE);
			break;
		case IDM_OPEN:
			OpenFile(false);
			break;
		case IDM_SAVE:
		case IDM_SAVE_SCREEN:
			if (m_pCurrentImage != NULL) {
				SaveImage(nCommand == IDM_SAVE);
			}
			break;
		case IDM_RELOAD:
			GotoImage(POS_Current);
			break;
		case IDM_COPY:
			if (m_pCurrentImage != NULL) {
				CClipboard::CopyImageToClipboard(this->m_hWnd, m_pCurrentImage);
			}
			break;
		case IDM_COPY_FULL:
			if (m_pCurrentImage != NULL) {
				CClipboard::CopyFullImageToClipboard(this->m_hWnd, m_pCurrentImage, *m_pImageProcParams, 
					CreateProcessingFlags(m_bHQResampling, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, false, m_bLandscapeMode));
				this->Invalidate(FALSE);
			}
			break;
		case IDM_PASTE:
			if (::IsClipboardFormatAvailable(CF_DIB)) {
				GotoImage(POS_Clipboard);
			}
			break;
		case IDM_BATCH_COPY:
			if (m_pCurrentImage != NULL) {
				BatchCopy();
			}
			break;
		case IDM_SHOW_FILEINFO:
			m_pEXIFDisplayCtl->SetActive(!m_pEXIFDisplayCtl->IsActive());
			m_pNavPanelCtl->GetNavPanel()->GetBtnShowInfo()->SetActive(m_pEXIFDisplayCtl->IsActive());
			break;
		case IDM_SHOW_FILENAME:
			m_bShowFileName = !m_bShowFileName;
			this->Invalidate(FALSE);
			break;
		case IDM_SHOW_NAVPANEL:
			m_pNavPanelCtl->SetActive(!m_pNavPanelCtl->IsActive());
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
			if (m_bShowHelp || m_pEXIFDisplayCtl->IsActive() || m_bShowFileName) {
				this->Invalidate(FALSE);
			}
			break;
		case IDM_STOP_MOVIE:
			StopMovieMode();
			break;
		case IDM_SLIDESHOW_1:
		case IDM_SLIDESHOW_2:
		case IDM_SLIDESHOW_3:
		case IDM_SLIDESHOW_4:
		case IDM_SLIDESHOW_5:
		case IDM_SLIDESHOW_7:
		case IDM_SLIDESHOW_10:
		case IDM_SLIDESHOW_20:
			StartMovieMode(1.0/(nCommand - IDM_SLIDESHOW_START));
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
				EProcessingFlags procFlags = CreateProcessingFlags(m_bHQResampling, m_bAutoContrast, false, m_bLDC, false, m_bLandscapeMode);
				newEntry.InitFromProcessParams(*m_pImageProcParams, procFlags, m_nRotation);
				if ((m_bUserZoom || m_bUserPan) && !m_pCurrentImage->IsCropped()) {
					newEntry.InitGeometricParams(CSize(m_pCurrentImage->OrigWidth(), m_pCurrentImage->OrigHeight()),
						m_dZoom, m_offsets, m_clientRect.Size());
				}
				newEntry.SetHash(m_pCurrentImage->GetPixelHash());
				if (CParameterDB::This().AddEntry(newEntry)) {
					// these parameters need to be updated when image is reused from cache
					m_pCurrentImage->SetInitialParameters(*m_pImageProcParams, procFlags, m_nRotation, m_dZoom, m_offsets);
					m_pCurrentImage->SetIsInParamDB(true);
					m_pImageProcPanelCtl->ShowHideSaveDBButtons();
				}
			}
			break;
		case IDM_CLEAR_PARAM_DB:
			if (m_pCurrentImage != NULL && !m_bMovieMode && !m_bKeepParams) {
				if (CParameterDB::This().DeleteEntry(m_pCurrentImage->GetPixelHash())) {
					// restore initial parameters and realize the parameters
					EProcessingFlags procFlags = GetDefaultProcessingFlags(m_bLandscapeMode);
					m_pCurrentImage->RestoreInitialParameters(m_pFileList->Current(), 
						GetDefaultProcessingParams(), procFlags, 0, -1, CPoint(0, 0), CSize(0, 0));
					*m_pImageProcParams = GetDefaultProcessingParams();
					// Sunset and night shot detection may has changed this
					m_pImageProcParams->LightenShadows = m_pCurrentImage->GetInitialProcessParams().LightenShadows;
					InitFromProcessingFlags(procFlags, m_bHQResampling, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, m_bLandscapeMode);
					m_nRotation = m_pCurrentImage->GetInitialRotation();
					m_dZoom = -1;
					m_pCurrentImage->SetIsInParamDB(false);
					m_pImageProcPanelCtl->ShowHideSaveDBButtons();
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
		case IDM_LANDSCAPE_MODE:
			m_bLandscapeMode = !m_bLandscapeMode;
			m_pNavPanelCtl->GetNavPanel()->GetBtnLandscapeMode()->SetActive(m_bLandscapeMode);
			if (m_bLandscapeMode) {
				*m_pImageProcParams = _SetLandscapeModeParams(true, *m_pImageProcParams);
				if (m_pCurrentImage != NULL) {
					m_pImageProcParams->LightenShadows *= m_pCurrentImage->GetLightenShadowFactor();
				}
				m_bLDC = true;
				m_bAutoContrast = true;
			} else {
				EProcessingFlags eProcFlags = GetDefaultProcessingFlags(false);
				CImageProcessingParams ipa = GetDefaultProcessingParams();
				if (m_pCurrentImage != NULL) {
					m_pCurrentImage->GetFileParams(m_pFileList->Current(), eProcFlags, ipa);
				}
				m_bLDC = GetProcessingFlag(eProcFlags, PFLAG_LDC);
				m_bAutoContrast = GetProcessingFlag(eProcFlags, PFLAG_AutoContrast);
				*m_pImageProcParams = ipa;
			}
			this->Invalidate(FALSE);
			break;
		case IDM_KEEP_PARAMETERS:
			m_bKeepParams = !m_bKeepParams;
			m_pNavPanelCtl->GetNavPanel()->GetBtnKeepParams()->SetActive(m_bKeepParams);
			if (m_bKeepParams) {
				*m_pImageProcParamsKept = *m_pImageProcParams;
				m_eProcessingFlagsKept = CreateProcessingFlags(m_bHQResampling, m_bAutoContrast, false, m_bLDC, m_bKeepParams, m_bLandscapeMode);
				m_nRotationKept = m_nRotation;
				m_dZoomKept = m_bUserZoom ? m_dZoom : -1;
				m_offsetKept = m_bUserPan ? m_offsets : CPoint(0, 0);
			}
			m_pImageProcPanelCtl->ShowHideSaveDBButtons();
			if (m_bShowHelp) {
				this->Invalidate(FALSE);
			}
			break;
		case IDM_SAVE_PARAMETERS:
			SaveParameters();
			break;
		case IDM_FIT_TO_SCREEN:
			ResetZoomToFitScreen(false, true);
			break;
		case IDM_FILL_WITH_CROP:
			ResetZoomToFitScreen(true, true);
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
					this->SetWindowPos(HWND_TOP, &rectAllScreens, SWP_NOZORDER);
				}
				m_bSpanVirtualDesktop = !m_bSpanVirtualDesktop;
				this->GetClientRect(&m_clientRect);
			}
			break;
		case IDM_FULL_SCREEN_MODE:
		case IDM_FULL_SCREEN_MODE_AFTER_STARTUP:
			m_bFullScreenMode = !m_bFullScreenMode;
			if (!m_bFullScreenMode) {
				CRect windowRect;
				this->SetWindowLongW(GWL_STYLE, this->GetWindowLongW(GWL_STYLE) | WS_OVERLAPPEDWINDOW | WS_VISIBLE);
				if (::IsRectEmpty(&(m_storedWindowPlacement2.rcNormalPosition))) {
					// never set to window mode before, use default position
					windowRect = CSettingsProvider::This().DefaultWindowRect();
					CRect rectAllScreens = CMultiMonitorSupport::GetVirtualDesktop();
					if (windowRect.IsRectEmpty() || !rectAllScreens.IntersectRect(&rectAllScreens, &windowRect)) {
						int nDesiredWidth = m_monitorRect.Width()*2/3;
						windowRect = CRect(CPoint(m_monitorRect.left + m_monitorRect.Width()/10, m_monitorRect.top + m_monitorRect.Height()/10), CSize(nDesiredWidth, nDesiredWidth*3/4));
						AdjustWindowRectEx(&windowRect, GetStyle(), FALSE, GetExStyle());
					}
					this->SetWindowPos(HWND_TOP, windowRect.left, windowRect.top, windowRect.Width(), windowRect.Height(), SWP_NOZORDER | SWP_NOCOPYBITS);
					if (nCommand == IDM_FULL_SCREEN_MODE_AFTER_STARTUP && CSettingsProvider::This().DefaultMaximized()) {
						this->ShowWindow(SW_MAXIMIZE);
					}
				} else {
					m_storedWindowPlacement2.flags = 
					this->SetWindowPlacement(&m_storedWindowPlacement2);
				}
				this->MouseOn();
			} else {
				this->GetWindowPlacement(&m_storedWindowPlacement2);
				HMONITOR hMonitor = ::MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
				MONITORINFO monitorInfo;
				monitorInfo.cbSize = sizeof(MONITORINFO);
				if (::GetMonitorInfo(hMonitor, &monitorInfo)) {
					CRect monitorRect(&(monitorInfo.rcMonitor));
					this->SetWindowLongW(GWL_STYLE, WS_VISIBLE);
					this->SetWindowPos(HWND_TOP, monitorRect.left, monitorRect.top, monitorRect.Width(), monitorRect.Height(), SWP_NOZORDER | SWP_NOCOPYBITS);
				}
				this->MouseOn();
			}
			m_dZoom = -1;
			StartLowQTimer(ZOOM_TIMEOUT);
			this->SetWindowPos(NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOCOPYBITS | SWP_FRAMECHANGED);
			break;
		case IDM_ZOOM_400:
			PerformZoom(4.0, false, m_bMouseOn);
			break;
		case IDM_ZOOM_200:
			PerformZoom(2.0, false, m_bMouseOn);
			break;
		case IDM_ZOOM_100:
			ResetZoomTo100Percents(m_bMouseOn);
			break;
		case IDM_ZOOM_50:
			PerformZoom(0.5, false, m_bMouseOn);
			break;
		case IDM_ZOOM_25:
			PerformZoom(0.25, false, m_bMouseOn);
			break;
		case IDM_AUTO_ZOOM_FIT_NO_ZOOM:
		case IDM_AUTO_ZOOM_FILL_NO_ZOOM:
		case IDM_AUTO_ZOOM_FIT:
		case IDM_AUTO_ZOOM_FILL: 
			m_eAutoZoomMode = (Helpers::EAutoZoomMode)((nCommand - IDM_AUTO_ZOOM_FIT_NO_ZOOM)/10);
			m_dZoom = -1.0;
			this->Invalidate(FALSE);
			break;
		case IDM_EDIT_GLOBAL_CONFIG:
		case IDM_EDIT_USER_CONFIG:
			EditINIFile(nCommand == IDM_EDIT_GLOBAL_CONFIG);
			break;
		case IDM_BACKUP_PARAMDB:
			CParameterDB::This().BackupParamDB(m_hWnd);
			break;
		case IDM_RESTORE_PARAMDB:
			CParameterDB::This().RestoreParamDB(m_hWnd);
			break;
		case IDM_ABOUT:
			{
				MouseOn();
				HMODULE hMod = ::LoadLibrary(_T("RICHED32.DLL"));
				CAboutDlg dlgAbout;
				dlgAbout.DoModal();
				::FreeLibrary(hMod);
			}
			break;
		case IDM_EXIT:
			this->EndDialog(0);
			break;
		case IDM_ZOOM_SEL:
			ZoomToSelection();
			break;
		case IDM_CROP_SEL:
			if (m_pCurrentImage != NULL) {
				m_pCurrentImage->Crop(m_pCropCtl->GetImageCropRect());
				this->Invalidate(FALSE);
			}
			break;
		case IDM_LOSSLESS_CROP_SEL:
			m_pCropCtl->CropLossless();
			break;
		case IDM_COPY_SEL:
			if (m_pCurrentImage != NULL) {
				CClipboard::CopyFullImageToClipboard(this->m_hWnd, m_pCurrentImage, *m_pImageProcParams, 
					CreateProcessingFlags(m_bHQResampling, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, false, m_bLandscapeMode),
					m_pCropCtl->GetImageCropRect());
				this->Invalidate(FALSE);
			}
			break;
		case IDM_CROPMODE_FREE:
			m_pCropCtl->SetCropRectAR(0);
			break;
		case IDM_CROPMODE_FIXED_SIZE:
			{
				CCropSizeDlg dlgSetCropSize;
				dlgSetCropSize.DoModal();
			}
			m_pCropCtl->SetCropRectAR(-1);
			break;
		case IDM_CROPMODE_5_4:
			m_pCropCtl->SetCropRectAR(1.25);
			break;
		case IDM_CROPMODE_4_3:
			m_pCropCtl->SetCropRectAR(1.333333333333333333);
			break;
		case IDM_CROPMODE_3_2:
			m_pCropCtl->SetCropRectAR(1.5);
			break;
		case IDM_CROPMODE_16_9:
			m_pCropCtl->SetCropRectAR(1.777777777777777778);
			break;
		case IDM_CROPMODE_16_10:
			m_pCropCtl->SetCropRectAR(1.6);
			break;
		case IDM_TOUCH_IMAGE:
		case IDM_TOUCH_IMAGE_EXIF:
			if (m_pCurrentImage != NULL) {
				LPCTSTR strFileName = CurrentFileName(false);
				bool bOk;
				if (nCommand == IDM_TOUCH_IMAGE_EXIF) {
					bOk = EXIFHelpers::SetModificationDateToEXIF(strFileName, m_pCurrentImage);
				} else {
					SYSTEMTIME st;
					::GetSystemTime(&st);  // gets current time
					bOk = EXIFHelpers::SetModificationDate(strFileName, st);
				}
				if (bOk) {
					m_pFileList->ModificationTimeChanged();
					if (m_pEXIFDisplayCtl->IsActive()) {
						this->Invalidate(FALSE);
					}
				}
			}
			break;
		case IDM_TOUCH_IMAGE_EXIF_FOLDER:
			if (m_pCurrentImage != NULL && m_pFileList->CurrentDirectory() != NULL) {
				EXIFHelpers::EXIFResult result = EXIFHelpers::SetModificationDateToEXIFAllFiles(m_pFileList->CurrentDirectory());
				TCHAR buff1[128];
				_stprintf_s(buff1, 128, CNLS::GetString(_T("Number of JPEG files in folder: %d")), result.NumberOfSucceededFiles + result.NumberOfFailedFiles);
				TCHAR buff2[256];
				_stprintf_s(buff2, 256, CNLS::GetString(_T("EXIF date successfully set on %d images, failed on %d images")), result.NumberOfSucceededFiles, result.NumberOfFailedFiles);
				::MessageBox(m_hWnd, CString(buff1) + _T('\n') + buff2, _T("JPEGView"), MB_OK | MB_ICONINFORMATION);
				m_pFileList->Reload();
				if (m_pEXIFDisplayCtl->IsActive()) {
					this->Invalidate(FALSE);
				}
			}
			break;
	}
}

bool CMainDlg::OpenFile(bool bFullScreen) {
	StopMovieMode();
	MouseOn();
	CFileOpenDialog dlgOpen(this->m_hWnd, m_pFileList->Current(), CFileList::GetSupportedFileEndings(), bFullScreen);
	if (IDOK == dlgOpen.DoModal(this->m_hWnd)) {
		OpenFile(dlgOpen.m_szFileName);
		return true;
	}
	return false;
}

void CMainDlg::OpenFile(LPCTSTR sFileName) {
	// recreate file list based on image opened
	Helpers::ESorting eOldSorting = m_pFileList->GetSorting();
	delete m_pFileList;
	m_sStartupFile = sFileName;
	m_pFileList = new CFileList(m_sStartupFile, eOldSorting);
	// free current image and all read ahead images
	InitParametersForNewImage();
	m_pJPEGProvider->NotifyNotUsed(m_pCurrentImage);
	m_pJPEGProvider->ClearAllRequests();
	m_pCurrentImage = m_pJPEGProvider->RequestJPEG(m_pFileList, CJPEGProvider::FORWARD,
		m_pFileList->Current(), CreateProcessParams(), m_bOutOfMemoryLastImage);
	m_nLastLoadError = GetLoadErrorAfterOpenFile();
	AfterNewImageLoaded(true);
	m_startMouse.x = m_startMouse.y = -1;
	m_sSaveDirectory = _T("");
	MouseOff();
	this->Invalidate(FALSE);
}

bool CMainDlg::SaveImage(bool bFullSize) {
	if (m_bMovieMode) {
		return false;
	}

	MouseOn();

	CString sCurrentFile;
	if (m_sSaveDirectory.GetLength() == 0) {
		sCurrentFile = CurrentFileName(false);
	} else {
		sCurrentFile = m_sSaveDirectory + CurrentFileName(true);
	}
	int nIndexPoint = sCurrentFile.ReverseFind(_T('.'));
	if (nIndexPoint > 0) {
		sCurrentFile = sCurrentFile.Left(nIndexPoint);
		sCurrentFile += _T("_proc");
	}

	CString sExtension = m_sSaveExtension;
	if (sExtension.IsEmpty()) {
		sExtension = CSettingsProvider::This().DefaultSaveFormat();
	}
	CFileDialog fileDlg(FALSE, sExtension, sCurrentFile, 
			OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_NOREADONLYRETURN | OFN_OVERWRITEPROMPT,
			Helpers::CReplacePipe(CString(_T("JPEG (*.jpg;*.jpeg)|*.jpg;*.jpeg|BMP (*.bmp)|*.bmp|PNG (*.png)|*.png|TIFF (*.tiff;*.tif)|*.tiff;*.tif|")) +
			CNLS::GetString(_T("All Files")) + _T("|*.*|")), m_hWnd);
	if (sExtension.CompareNoCase(_T("bmp")) == 0) {
		fileDlg.m_ofn.nFilterIndex = 2;
	} else if (sExtension.CompareNoCase(_T("png")) == 0) {
		fileDlg.m_ofn.nFilterIndex = 3;
	} else if (sExtension.CompareNoCase(_T("tiff")) == 0 || sExtension.CompareNoCase(_T("tif")) == 0) {
		fileDlg.m_ofn.nFilterIndex = 4;
	} 
	if (!bFullSize) {
		fileDlg.m_ofn.lpstrTitle = CNLS::GetString(_T("Save as (in screen size/resolution)"));
	}
	if (IDOK == fileDlg.DoModal(m_hWnd)) {
		m_sSaveDirectory = fileDlg.m_szFileName;
		m_sSaveExtension = m_sSaveDirectory.Right(m_sSaveDirectory.GetLength() - m_sSaveDirectory.ReverseFind(_T('.')) - 1);
		m_sSaveDirectory = m_sSaveDirectory.Left(m_sSaveDirectory.ReverseFind(_T('\\')) + 1);
		return SaveImageNoPrompt(fileDlg.m_szFileName, bFullSize);
	}
	return false;
}

bool CMainDlg::SaveImageNoPrompt(LPCTSTR sFileName, bool bFullSize) {
	if (m_bMovieMode) {
		return false;
	}

	MouseOn();

	HCURSOR hOldCursor = ::SetCursor(::LoadCursor(NULL, MAKEINTRESOURCE(IDC_WAIT)));	

	if (CSaveImage::SaveImage(sFileName, m_pCurrentImage, *m_pImageProcParams, 
		CreateProcessingFlags(m_bHQResampling, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, false, m_bLandscapeMode), bFullSize)) {
		m_pFileList->Reload(); // maybe image is stored to current directory - needs reload
		::SetCursor(hOldCursor);
		Invalidate();
		return true;
	} else {
		::SetCursor(hOldCursor);
		::MessageBox(m_hWnd, CNLS::GetString(_T("Error saving file")), 
			CNLS::GetString(_T("Error writing file to disk!")), MB_ICONSTOP | MB_OK);
		return false;
	}
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
	if (::GetFileAttributes(CurrentFileName(false)) == INVALID_FILE_ATTRIBUTES) {
		return; // file does not exist
	}

	// iterate over user command list
	std::list<CUserCommand*>::iterator iter;
	std::list<CUserCommand*> & userCmdList = CSettingsProvider::This().UserCommandList();
	for (iter = userCmdList.begin( ); iter != userCmdList.end( ); iter++ ) {
		if ((*iter)->GetKeyCode() == virtualKeyCode) {
			MouseOn();
			if ((*iter)->Execute(this->m_hWnd, CurrentFileName(false))) {
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
						bReloadCurrent = true;
					}
				}
				if (bReloadCurrent) {
					GotoImage(POS_Current);
				}
			}
			break;
		}
	}
}

void CMainDlg::StartDragging(int nX, int nY, bool bDragWithZoomNavigator) {
	m_startMouse.x = m_startMouse.y = -1;
	m_bDragging = true;
	if (bDragWithZoomNavigator) {
		m_pZoomNavigatorCtl->StartDragging(nX, nY);
	}
	m_nCapturedX = nX;
	m_nCapturedY = nY;
	SetCursorForMoveSection();
	m_pNavPanelCtl->StartNavPanelAnimation(true, true);
}

void CMainDlg::DoDragging() {
	if (m_bDragging && m_pCurrentImage != NULL) {
		int nXDelta = m_nMouseX - m_nCapturedX;
		int nYDelta = m_nMouseY - m_nCapturedY;
		if (!m_bDoDragging && (nXDelta != 0 || nYDelta != 0)) {
			m_bDoDragging = true;
		}
		if (m_pZoomNavigatorCtl->IsDragging()) {
			m_pZoomNavigatorCtl->DoDragging(nXDelta, nYDelta);
		} else {
			if (PerformPan(nXDelta, nYDelta, false)) {
				m_nCapturedX = m_nMouseX;
				m_nCapturedY = m_nMouseY;
			}
		}
	}
}

void CMainDlg::EndDragging() {
	m_bDragging = false;
	m_bDoDragging = false;
	m_pZoomNavigatorCtl->EndDragging();
	if (m_pCurrentImage != NULL) {
		m_pCurrentImage->VerifyDIBPixelsCreated();
	}
	this->InvalidateRect(m_pNavPanelCtl->PanelRect(), FALSE);
	SetCursorForMoveSection();
}

void CMainDlg::GotoImage(EImagePosition ePos) {
	GotoImage(ePos, 0);
}

void CMainDlg::GotoImage(EImagePosition ePos, int nFlags) {
	// Timer handling for slideshows
	if (ePos == POS_Next) {
		if (m_nCurrentTimeout > 0) {
			StartSlideShowTimer(m_nCurrentTimeout);
		}
	} else {
		StopMovieMode();
	}

	if (ePos != POS_Current && ePos != POS_Clipboard) {
		MouseOff();
	}

	if (nFlags & KEEP_PARAMETERS) {
		if (!m_bUserZoom) {
			m_dZoom = -1;
		}
	} else {
		InitParametersForNewImage();
	}
	m_pJPEGProvider->NotifyNotUsed(m_pCurrentImage);
	if (ePos == POS_Current) {
		m_pJPEGProvider->ClearRequest(m_pCurrentImage);
	}
	m_pCurrentImage = NULL;
	m_pCropCtl->CancelCropping(); // cancel any running crop
	
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
		case POS_Toggle:
			m_pFileList->ToggleBetweenMarkedAndCurrentFile();
			eDirection = CJPEGProvider::TOGGLE;
			break;
		case POS_Current:
		case POS_Clipboard:
			break;
	}

	// do not perform a new image request if flagged
	if (nFlags & NO_REQUEST) {
		return;
	}

	CProcessParams procParams = CreateProcessParams();
	if (nFlags & KEEP_PARAMETERS) {
		procParams.ProcFlags = SetProcessingFlag(procParams.ProcFlags, PFLAG_KeepParams, true);
	}
	if (ePos == POS_Clipboard) {
		m_pCurrentImage = CClipboard::PasteImageFromClipboard(m_hWnd, procParams.ImageProcParams, procParams.ProcFlags);
		if (m_pCurrentImage != NULL) {
			m_pCurrentImage->SetFileDependentProcessParams(_T("_cbrd_"), &procParams);
			m_nLastLoadError = HelpersGUI::FileLoad_Ok;
		} else {
			m_nLastLoadError = HelpersGUI::FileLoad_PasteFromClipboardFailed;
		}
	} else {
		m_pCurrentImage = m_pJPEGProvider->RequestJPEG(m_pFileList, eDirection,  
			m_pFileList->Current(), procParams, m_bOutOfMemoryLastImage);
		m_nLastLoadError = (m_pCurrentImage == NULL) ? HelpersGUI::FileLoad_LoadError : HelpersGUI::FileLoad_Ok;
	}
	AfterNewImageLoaded((nFlags & KEEP_PARAMETERS) == 0);

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

void CMainDlg::PerformZoom(double dValue, bool bExponent, bool bZoomToMouse) {
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

	if (bZoomToMouse) {
		// zoom to mouse
		int nOldX = nOldXSize/2 - m_clientRect.Width()/2 + m_nMouseX - m_offsets.x;
		int nOldY = nOldYSize/2 - m_clientRect.Height()/2 + m_nMouseY - m_offsets.y;
		double dFac = m_dZoom/dOldZoom;
		m_offsets.x = Helpers::RoundToInt(nNewXSize/2 - m_clientRect.Width()/2 + m_nMouseX - nOldX*dFac);
		m_offsets.y = Helpers::RoundToInt(nNewYSize/2 - m_clientRect.Height()/2 + m_nMouseY - nOldY*dFac);
	} else {
		// zoom to center
		m_offsets.x = (int) (m_offsets.x*m_dZoom/dOldZoom);
		m_offsets.y = (int) (m_offsets.y*m_dZoom/dOldZoom);
	}

	m_bInZooming = true;
	StartLowQTimer(ZOOM_TIMEOUT);
	if (fabs(dOldZoom - m_dZoom) > 0.01) {
		this->Invalidate(FALSE);
	}
}

bool CMainDlg::PerformPan(int dx, int dy, bool bAbsolute) {
	if ((m_virtualImageSize.cx > 0 && m_virtualImageSize.cx > m_clientRect.Width()) ||
		(m_virtualImageSize.cy > 0 && m_virtualImageSize.cy > m_clientRect.Height())) {
		if (bAbsolute) {
			m_offsets = CPoint(dx, dy);
		} else {
			m_offsets = CPoint(m_offsets.x + dx, m_offsets.y + dy);
		}
		m_bUserPan = true;

		this->Invalidate(FALSE);
		return true;
	}
	return false;
}

void CMainDlg::ZoomToSelection() {
	CRect zoomRect(m_pCropCtl->GetImageCropRect());
	if (zoomRect.Width() > 0 && zoomRect.Height() > 0 && m_pCurrentImage != NULL) {
		float fZoom;
		CPoint offsets;
		Helpers::GetZoomParameters(fZoom, offsets, CSize(m_pCurrentImage->OrigWidth(), m_pCurrentImage->OrigHeight()), 
			m_clientRect.Size(), zoomRect);
		if (fZoom > 0) {
			PerformZoom(fZoom, false, m_bMouseOn);
			m_offsets = offsets;
			m_bUserPan = true;
		}
	}
}

double CMainDlg::GetZoomFactorForFitToScreen(bool bFillWithCrop, bool bAllowEnlarge) {
	if (m_pCurrentImage != NULL) {
		double dZoom;
		Helpers::GetImageRect(m_pCurrentImage->OrigWidth(), m_pCurrentImage->OrigHeight(), 
			m_clientRect.Width(), m_clientRect.Height(), true, bFillWithCrop, false, dZoom);
		double dZoomMax = bAllowEnlarge ? Helpers::ZoomMax : max(1.0, m_dZoomAtResizeStart);
		return max(Helpers::ZoomMin, min(dZoomMax, dZoom));
	} else {
		return 1.0;
	}
}

void CMainDlg::ResetZoomToFitScreen(bool bFillWithCrop, bool bAllowEnlarge) {
	if (m_pCurrentImage != NULL) {
		double dOldZoom = m_dZoom;
		m_dZoom = GetZoomFactorForFitToScreen(bFillWithCrop, bAllowEnlarge);
		m_bUserZoom = m_dZoom > 1.0;
		m_bUserPan = false;
		if (fabs(dOldZoom - m_dZoom) > 0.01) {
			this->Invalidate(FALSE);
		}
	}
}

void CMainDlg::ResetZoomTo100Percents(bool bZoomToMouse) {
	if (m_pCurrentImage != NULL && fabs(m_dZoom - 1) > 0.01) {
		PerformZoom(1.0, false, bZoomToMouse);
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
		m_eProcessingFlagsKept = CreateProcessingFlags(m_bHQResampling, m_bAutoContrast, false, m_bLDC, m_bKeepParams, m_bLandscapeMode);
		m_nRotationKept = m_nRotation;
		m_dZoomKept = m_bUserZoom ? m_dZoom : -1;
		m_offsetKept = m_bUserPan ? m_offsets : CPoint(0, 0);

		return CProcessParams(m_clientRect.Width(), m_clientRect.Height(), m_nRotationKept, 
			m_dZoomKept,
			m_eAutoZoomMode,
			m_offsetKept,
			_SetLandscapeModeParams(m_bLandscapeMode, *m_pImageProcParamsKept), 
			_SetLandscapeModeFlags(m_eProcessingFlagsKept));
	} else {
		CSettingsProvider& sp = CSettingsProvider::This();
		return CProcessParams(m_clientRect.Width(), m_clientRect.Height(), 0,
			-1, m_eAutoZoomMode, CPoint(0, 0), 
			_SetLandscapeModeParams(m_bLandscapeMode, GetDefaultProcessingParams()),
			_SetLandscapeModeFlags(GetDefaultProcessingFlags(m_bLandscapeMode)));
	}
}

void CMainDlg::ResetParamsToDefault() {
	CSettingsProvider& sp = CSettingsProvider::This();
	m_nRotation = 0;
	m_dZoom = m_dZoomMult = -1.0;
	m_offsets = CPoint(0, 0);
	m_bUserZoom = false;
	m_bUserPan = false;
	*m_pImageProcParams = GetDefaultProcessingParams();
	InitFromProcessingFlags(GetDefaultProcessingFlags(m_bLandscapeMode), m_bHQResampling, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, m_bLandscapeMode);
}

void CMainDlg::StartSlideShowTimer(int nMilliSeconds) {
	m_nCurrentTimeout = nMilliSeconds;
	::SetTimer(this->m_hWnd, SLIDESHOW_TIMER_EVENT_ID, nMilliSeconds, NULL);
	m_pNavPanelCtl->EndNavPanelAnimation();
}

void CMainDlg::StopSlideShowTimer(void) {
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
		m_eProcFlagsBeforeMovie = CreateProcessingFlags(m_bHQResampling, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, m_bKeepParams, m_bLandscapeMode);
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
		m_bLandscapeMode = false;
	}
	m_bMovieMode = true;
	StartSlideShowTimer(Helpers::RoundToInt(1000.0/dFPS));
	AfterNewImageLoaded(false);
	this->InvalidateRect(m_pNavPanelCtl->PanelRect(), FALSE);
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
			if (GetProcessingFlag(m_eProcFlagsBeforeMovie, PFLAG_LandscapeMode)) {
				m_bLandscapeMode = true;
			}
		}
		m_bMovieMode = false;
		m_bProcFlagsTouched = false;
		StopSlideShowTimer();
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
	if (m_bMouseOn) {
		if (m_nMouseY < m_clientRect.bottom - m_pImageProcPanelCtl->PanelRect().Height() && 
			!m_bInTrackPopupMenu && !m_pNavPanelCtl->PanelRect().PtInRect(CPoint(m_nMouseX, m_nMouseY))) {
			if (m_bFullScreenMode) {
				while (::ShowCursor(FALSE) >= 0);
			}
			m_startMouse.x = m_startMouse.y = -1;
			m_bMouseOn = false;
		}
		this->InvalidateRect(m_pNavPanelCtl->PanelRect(), FALSE);
	}
}

void CMainDlg::MouseOn() {
	if (!m_bMouseOn) {
		::ShowCursor(TRUE);
		m_bMouseOn = true;
		if (m_pNavPanelCtl != NULL) { // can be called very early
		  this->InvalidateRect(m_pNavPanelCtl->PanelRect(), FALSE);
		}
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

void CMainDlg::ExchangeProcessingParams() {
	bool bOldAutoContrastSection = m_bAutoContrastSection;
	CImageProcessingParams tempParams = *m_pImageProcParams;
	EProcessingFlags tempFlags = CreateProcessingFlags(m_bHQResampling, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, false, m_bLandscapeMode);
	*m_pImageProcParams = *m_pImageProcParams2;
	InitFromProcessingFlags(m_eProcessingFlags2, m_bHQResampling, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, m_bLandscapeMode);
	*m_pImageProcParams2 = tempParams;
	m_eProcessingFlags2 = tempFlags;
	m_bAutoContrastSection = bOldAutoContrastSection; // this flag cannot be exchanged this way
	this->Invalidate(FALSE);
}

void CMainDlg::AfterNewImageLoaded(bool bSynchronize) {
	UpdateWindowTitle();
	m_pNavPanelCtl->HideNavPanelTemporary();
	m_pPanelMgr->AfterNewImageLoaded();
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
				m_offsets = m_pCurrentImage->GetInitialOffsets();

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
			if (m_bKeepParams) {
				if (m_pCurrentImage->IsRotationByEXIF()) {
					m_nRotation = m_pCurrentImage->GetInitialRotation();
				} else {
					m_nRotation = m_nRotationKept;
				}
			}
		}
	}
}

void CMainDlg::SaveParameters() {
	if (m_bMovieMode) {
		return;
	}

	EProcessingFlags eFlags = CreateProcessingFlags(m_bHQResampling, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, m_bKeepParams, m_bLandscapeMode);
	CString sText = HelpersGUI::GetINIFileSaveConfirmationText(*m_pImageProcParams, eFlags,
		m_pFileList->GetSorting(), m_eAutoZoomMode, m_pNavPanelCtl->IsActive());

	if (IDYES == this->MessageBox(sText, CNLS::GetString(_T("Confirm save default parameters")), MB_YESNO | MB_ICONQUESTION)) {
		CSettingsProvider::This().SaveSettings(*m_pImageProcParams, eFlags, m_pFileList->GetSorting(), m_eAutoZoomMode, m_pNavPanelCtl->IsActive());
	}
}

CRect CMainDlg::ScreenToDIB(const CSize& sizeDIB, const CRect& rect) {
	int nOffsetX = (sizeDIB.cx - m_clientRect.Width())/2;
	int nOffsetY = (sizeDIB.cy - m_clientRect.Height())/2;

	CRect rectDIB = CRect(rect.left + nOffsetX, rect.top + nOffsetY, rect.right + nOffsetX, rect.bottom + nOffsetY);
	
	CRect rectClipped;
	rectClipped.IntersectRect(rectDIB, CRect(0, 0, sizeDIB.cx, sizeDIB.cy));
	return rectClipped;
}

bool CMainDlg::ScreenToImage(float & fX, float & fY) {
	if (m_pCurrentImage == NULL) {
		return false;
	}
	int nOffsetX = (m_pCurrentImage->DIBWidth() - m_clientRect.Width())/2;
	int nOffsetY = (m_pCurrentImage->DIBHeight() - m_clientRect.Height())/2;

	fX += nOffsetX;
	fY += nOffsetY;

	m_pCurrentImage->DIBToOrig(fX, fY);

	return true;
}

bool CMainDlg::ImageToScreen(float & fX, float & fY) {
	if (m_pCurrentImage == NULL) {
		return false;
	}
	m_pCurrentImage->OrigToDIB(fX, fY);

	int nOffsetX = (m_pCurrentImage->DIBWidth() - m_clientRect.Width())/2;
	int nOffsetY = (m_pCurrentImage->DIBHeight() - m_clientRect.Height())/2;

	fX -= nOffsetX;
	fY -= nOffsetY;

	return true;
}

LPCTSTR CMainDlg::CurrentFileName(bool bFileTitle) {
	if (m_pCurrentImage != NULL && m_pCurrentImage->IsClipboardImage()) {
		return _T("Clipboard Image");
	}
	if (m_pFileList != NULL) {
		return bFileTitle ? m_pFileList->CurrentFileTitle() : m_pFileList->Current();
	} else {
		return NULL;
	}
}

void CMainDlg::SetCursorForMoveSection() {
	if (m_pZoomNavigatorCtl->IsPointInZoomNavigatorThumbnail(CPoint(m_nMouseX, m_nMouseY)) || m_bDragging) {
		::SetCursor(::LoadCursor(NULL, MAKEINTRESOURCE(IDC_SIZEALL)));
		m_bPanMouseCursorSet = true;
	} else if (m_bPanMouseCursorSet) {
		::SetCursor(::LoadCursor(NULL, MAKEINTRESOURCE(IDC_ARROW)));
		m_bPanMouseCursorSet = false;
	}
}

void CMainDlg::ToggleMonitor() {
	if (CMultiMonitorSupport::IsMultiMonitorSystem() && !m_bSpanVirtualDesktop) {
		int nMaxMonitorIdx = ::GetSystemMetrics(SM_CMONITORS);
		if (m_nMonitor == -1) {
			// index unknown, search
			for (int i = 0; i < nMaxMonitorIdx; i++) {
				if (m_monitorRect == CMultiMonitorSupport::GetMonitorRect(i)) {
					m_nMonitor = i;
					break;
				}
			}
		}
		m_dZoom = -1.0;
		this->Invalidate(FALSE);
		m_nMonitor = (m_nMonitor + 1) % nMaxMonitorIdx;
		m_monitorRect = CMultiMonitorSupport::GetMonitorRect(m_nMonitor);
		SetWindowPos(HWND_TOP, &m_monitorRect, SWP_NOZORDER);
		this->GetClientRect(&m_clientRect);
	}
}

CRect CMainDlg::GetZoomTextRect(CRect imageProcessingArea) {
	int nZoomTextRectBottomOffset = (m_clientRect.Width() < 800) ? 25 : ZOOM_TEXT_RECT_OFFSET;
	int nStartX = imageProcessingArea.right - Scale(ZOOM_TEXT_RECT_WIDTH + ZOOM_TEXT_RECT_OFFSET);
	if (m_pImageProcPanelCtl->IsVisible()) {
		nStartX = max(nStartX, m_pImageProcPanelCtl->GetUnsharpMaskButtonRect().right);
	}
	int nEndX = nStartX + Scale(ZOOM_TEXT_RECT_WIDTH);
	if (m_pImageProcPanelCtl->IsVisible()) {
		nEndX = min(nEndX, imageProcessingArea.right - 2);
	}
	return CRect(nStartX, 
		imageProcessingArea.bottom - Scale(ZOOM_TEXT_RECT_HEIGHT + nZoomTextRectBottomOffset), 
		nEndX, imageProcessingArea.bottom - Scale(nZoomTextRectBottomOffset));
}

void CMainDlg::EditINIFile(bool bGlobalINI) {
	LPCTSTR sINIFileName = bGlobalINI ? CSettingsProvider::This().GetGlobalINIFileName() : CSettingsProvider::This().GetUserINIFileName();
	if (!bGlobalINI) {
		if (::GetFileAttributes(sINIFileName) == INVALID_FILE_ATTRIBUTES) {
			// No user INI file, ask if global INI shall be copied
			if (IDYES == ::MessageBox(m_hWnd, CNLS::GetString(_T("No user INI file exits yet. Create user INI file from INI file template?")), _T("JPEGView"), MB_YESNO | MB_ICONQUESTION)) {
				::CreateDirectory(Helpers::JPEGViewAppDataPath(), NULL);
				::CopyFile(CString(CSettingsProvider::This().GetGlobalINIFileName()) + ".tpl", sINIFileName, TRUE);
			} else {
				return;
			}
		}
	}

	::ShellExecute(m_hWnd, _T("open"), _T("notepad.exe"), sINIFileName, NULL, SW_SHOW);
}

void CMainDlg::UpdateWindowTitle() {
	LPCTSTR sCurrentFileName = CurrentFileName(true);
	if (sCurrentFileName == NULL || m_pCurrentImage == NULL) {
		this->SetWindowText(_T("JPEGView"));
	} else {
		CString sWindowText = CString(_T("JPEGView - ")) + sCurrentFileName;
		CEXIFReader* pEXIF = m_pCurrentImage->GetEXIFReader();
		if (pEXIF != NULL && pEXIF->GetAcquisitionTime().wYear > 1600) {
			sWindowText += " - " + Helpers::SystemTimeToString(pEXIF->GetAcquisitionTime());
		}
		this->SetWindowText(sWindowText);
	}
}

bool CMainDlg::PrepareForModalPanel() {
	m_bShowHelp = false;
	m_pImageProcPanelCtl->SetVisible(false);
	bool bOldShowNavPanel = m_pNavPanelCtl->IsActive();
	m_pNavPanelCtl->SetActive(false);
	StopSlideShowTimer();
	StopMovieMode();
	return bOldShowNavPanel;
}

int CMainDlg::TrackPopupMenu(CPoint pos, HMENU hMenu) {
	m_bInTrackPopupMenu = true;
	int nMenuCmd = ::TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
		pos.x, pos.y, 0, this->m_hWnd, NULL);
	m_bInTrackPopupMenu = false;
	return nMenuCmd;
}

int CMainDlg::GetLoadErrorAfterOpenFile() {
	if (m_pCurrentImage == NULL) {
		if (CurrentFileName(false) == NULL) {
			if (m_pFileList->IsSlideShowList()) {
				return HelpersGUI::FileLoad_SlideShowListInvalid;
			}
			return HelpersGUI::FileLoad_NoFilesInDirectory;
		}
		return HelpersGUI::FileLoad_LoadError;
	}
	return HelpersGUI::FileLoad_Ok;
}
