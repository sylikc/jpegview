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
#include "ParameterDB.h"
#include "SaveImage.h"
#include "NLS.h"
#include "HelpersGUI.h"
#include "TimerEventIDs.h"
#include "FileOpenDialog.h"
#include "BatchCopyDlg.h"
#include "FileExtensionsDlg.h"
#include "FileExtensionsRegistry.h"
#include "ManageOpenWithDlg.h"
#include "AboutDlg.h"
#include "CropSizeDlg.h"
#include "ResizeDlg.h"
#include "ResizeFilter.h"
#include "EXIFReader.h"
#include "EXIFHelpers.h"
#include "ProcessingThreadPool.h"
#include "PaintMemDCMgr.h"
#include "PanelMgr.h"
#include "ZoomNavigatorCtl.h"
#include "ImageProcPanelCtl.h"
#include "WndButtonPanelCtl.h"
#include "InfoButtonPanelCtl.h"
#include "RotationPanelCtl.h"
#include "TiltCorrectionPanelCtl.h"
#include "UnsharpMaskPanelCtl.h"
#include "EXIFDisplayCtl.h"
#include "NavigationPanelCtl.h"
#include "HelpDisplayCtl.h"
#include "NavigationPanel.h"
#include "KeyMap.h"
#include "JPEGLosslessTransform.h"
#include "DirectoryWatcher.h"
#include "DesktopWallpaper.h"
#include "PrintImage.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// Constants
//////////////////////////////////////////////////////////////////////////////////////////////

static const int MIN_WND_WIDTH = 320;
static const int MIN_WND_HEIGHT = 240;

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
static const int NO_UPDATE_WINDOW = 8; // used in GotoImage() method

static const CSize HUGE_SIZE = CSize(99999999, 99999999);

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

CMainDlg::CMainDlg(bool bForceFullScreen) {
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
	m_eAutoZoomModeWindowed = sp.AutoZoomMode();
	m_eAutoZoomModeFullscreen = sp.AutoZoomModeFullscreen();

	m_eTransitionEffect = sp.SlideShowTransitionEffect();
	m_nTransitionTime = sp.SlideShowEffectTimeMs();

	CHistogramCorr::SetContrastCorrectionStrength((float)sp.AutoContrastAmount());
	CHistogramCorr::SetBrightnessCorrectionStrength((float)sp.AutoBrightnessAmount());

	m_pFileList = NULL;
	m_pJPEGProvider = NULL;
	m_pCurrentImage = NULL;
	m_bOutOfMemoryLastImage = false;
	m_nLastLoadError = HelpersGUI::FileLoad_Ok;

	m_nAutoStartSlideShow = false;
	m_eForcedSorting = Helpers::FS_Undefined;

	m_eProcFlagsBeforeMovie = PFLAG_None;
	m_nRotation = 0;
	m_dZoomAtResizeStart = 1.0;
	m_bZoomMode = false;
	m_bZoomModeOnLeftMouse = false;
	m_bUserZoom = false;
	m_bUserPan = false;
	m_bMovieMode = false;
	m_bProcFlagsTouched = false;
	m_bInTrackPopupMenu = false;
	m_bResizeForNewImage = false;
	m_dZoom = -1.0;
	m_dZoomMult = -1.0;
	m_bDragging = false;
	m_bDoDragging = false;
	m_offsets = CPoint(0, 0);
	m_DIBOffsets = CPoint(0, 0);
	m_nCapturedX = m_nCapturedY = 0;
	m_nMouseX = m_nMouseY = 0;
	m_bShowHelp = false;
	m_bAutoFitWndToImage = sp.DefaultWndToImage();
	m_bFullScreenMode = bForceFullScreen || (sp.ShowFullScreen() && !sp.AutoFullScreen());
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
	m_nMonitor = 0;
	m_monitorRect = CRect(0, 0, 0, 0);
	m_windowRectOnClose = CRect(0, 0, 0, 0);
	m_bMouseOn = false;
	m_bKeepParametersBeforeAnimation = false;
	m_bIsAnimationPlaying = false;
	m_bUseLosslessWEBP = false;
	m_isBeforeFileSelected = true;
	m_dLastImageDisplayTime = 0.0;
	m_isUserFitToScreen = false;
	m_autoZoomFitToScreen = Helpers::ZM_FillScreen;

	m_pPanelMgr = new CPanelMgr();
	m_pZoomNavigatorCtl = NULL;
	m_pEXIFDisplayCtl = NULL;
	m_pWndButtonPanelCtl = NULL;
	m_pInfoButtonPanelCtl = NULL;
	m_pRotationPanelCtl = NULL;
	m_pTiltCorrectionPanelCtl = NULL;
	m_pUnsharpMaskPanelCtl = NULL;
	m_pImageProcPanelCtl = NULL;
	m_pNavPanelCtl = NULL;
	m_pCropCtl = new CCropCtl(this);
	m_pKeyMap = new CKeyMap(CString(CSettingsProvider::This().GetEXEPath()) + _T("KeyMap.txt"));
	m_pPrintImage = new CPrintImage(CSettingsProvider::This().PrintMargin(), CSettingsProvider::This().DefaultPrintWidth());
}

CMainDlg::~CMainDlg() {
	delete m_pDirectoryWatcher;
	delete m_pFileList;
	if (m_pJPEGProvider != NULL) delete m_pJPEGProvider;
	delete m_pImageProcParams;
	delete m_pImageProcParamsKept;
	delete m_pZoomNavigatorCtl;
	delete m_pCropCtl;
	delete m_pPanelMgr; // this will delete all panel controllers and all panels
	delete m_pKeyMap;
}

void CMainDlg::SetStartupInfo(LPCTSTR sStartupFile, int nAutostartSlideShow, Helpers::ESorting eSorting, Helpers::ETransitionEffect eEffect, 
	int nTransitionTime, bool bAutoExit, int nDisplayMonitor) { 
	m_sStartupFile = sStartupFile; m_nAutoStartSlideShow = nAutostartSlideShow; m_eForcedSorting = eSorting;
	m_bAutoExit = bAutoExit;
	if ((int)eEffect >= 0) m_eTransitionEffect = eEffect;
	if (nTransitionTime > 0) m_nTransitionTime = nTransitionTime;
	if (nDisplayMonitor >= 0) CSettingsProvider::This().SetMonitorOverride(nDisplayMonitor);
}

LRESULT CMainDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {	
	UpdateWindowTitle();

	// set the scaling of the screen (DPI) compared to 96 DPI (design value)
	CPaintDC dc(this->m_hWnd);
	HelpersGUI::ScreenScaling = ::GetDeviceCaps(dc, LOGPIXELSX)/96.0f;

	::SetClassLongPtr(m_hWnd, GCLP_HCURSOR, NULL);

	m_pDirectoryWatcher = new CDirectoryWatcher(m_hWnd);

	// determine the monitor rectangle and client rectangle
	CSettingsProvider& sp = CSettingsProvider::This();
	m_nMonitor = sp.DisplayMonitor();
	m_monitorRect = CMultiMonitorSupport::GetMonitorRect(m_nMonitor);
	// move to correct monitor - but no redraw as Windows does not like that this early
	::SetWindowPos(m_hWnd, HWND_TOP, m_monitorRect.left, m_monitorRect.top, m_monitorRect.Width(), m_monitorRect.Height(),
		SWP_NOZORDER | SWP_NOCOPYBITS | SWP_NOACTIVATE | SWP_NOREDRAW);
	m_clientRect = m_bFullScreenMode ? m_monitorRect : CMultiMonitorSupport::GetDefaultClientRectInWindowMode(sp.AutoFullScreen());

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

	// Create perspective correction panel
	m_pTiltCorrectionPanelCtl = new CTiltCorrectionPanelCtl(this, m_pImageProcPanelCtl->GetPanel());
	m_pPanelMgr->AddPanelController(m_pTiltCorrectionPanelCtl);

	// Create navigation panel
	m_pNavPanelCtl = new CNavigationPanelCtl(this, m_pImageProcPanelCtl->GetPanel(), &m_bFullScreenMode);
	m_pPanelMgr->AddPanelController(m_pNavPanelCtl);

	// Create window button panel (on top, right)
	m_pWndButtonPanelCtl = new CWndButtonPanelCtl(this, m_pImageProcPanelCtl->GetPanel());
	m_pPanelMgr->AddPanelController(m_pWndButtonPanelCtl);

	// Create info button panel (on top, left)
	m_pInfoButtonPanelCtl = new CInfoButtonPanelCtl(this, m_pImageProcPanelCtl->GetPanel());
	m_pPanelMgr->AddPanelController(m_pInfoButtonPanelCtl);

	// Create zoom navigator
	m_pZoomNavigatorCtl = new CZoomNavigatorCtl(this, m_pImageProcPanelCtl->GetPanel(), m_pNavPanelCtl->GetPanel());

	// set icons (for toolbar)
	HICON hIcon = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
	SetIcon(hIcon, TRUE);
	HICON hIconSmall = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	SetIcon(hIconSmall, FALSE);

	// turn on/off mouse coursor
	m_bMouseOn = !m_bFullScreenMode;
	::ShowCursor(m_bMouseOn);

	// intitialize list of files to show with startup file (and folder)
	m_pFileList = new CFileList(m_sStartupFile, *m_pDirectoryWatcher,
		(m_eForcedSorting == Helpers::FS_Undefined) ? sp.Sorting() : m_eForcedSorting, sp.IsSortedUpcounting(), sp.WrapAroundFolder(),
		0, m_eForcedSorting != Helpers::FS_Undefined);
	m_pFileList->SetNavigationMode(sp.Navigation());

	// create thread pool for processing requests on multiple CPU cores
	CProcessingThreadPool::This().CreateThreadPoolThreads();

	// create JPEG provider and request first image - do no processing yet if not in fullscreen mode (as we do not know the size yet)
	m_pJPEGProvider = new CJPEGProvider(m_hWnd, NUM_THREADS, READ_AHEAD_BUFFERS);	
	m_pCurrentImage = m_pJPEGProvider->RequestImage(m_pFileList, CJPEGProvider::FORWARD,
		m_pFileList->Current(), 0, CreateProcessParams(!m_bFullScreenMode), m_bOutOfMemoryLastImage);
	if (m_pCurrentImage != NULL && m_pCurrentImage->IsAnimation()) {
		StartAnimation();
	}
	m_nLastLoadError = GetLoadErrorAfterOpenFile();
	CheckIfApplyAutoFitWndToImage(true);
	AfterNewImageLoaded(true, true); // synchronize to per image parameters

	if (!m_bFullScreenMode) {
		// Window mode, set correct window size
		this->SetWindowLongW(GWL_STYLE, this->GetWindowLongW(GWL_STYLE) | WS_OVERLAPPEDWINDOW | WS_VISIBLE);
		if (!IsAdjustWindowToImage()) {
			CRect windowRect = CMultiMonitorSupport::GetDefaultWindowRect();
			this->SetWindowPos(HWND_TOP, windowRect.left, windowRect.top, windowRect.Width(), windowRect.Height(), SWP_NOZORDER | SWP_NOCOPYBITS);
		} else {
			AdjustWindowToImage(true);
		}
		if (sp.DefaultMaximized()) {
			this->ShowWindow(SW_MAXIMIZE);
		}
	} else {
		PrefetchDIB(m_monitorRect);
		SetWindowLong(GWL_STYLE, WS_VISIBLE);
		SetWindowPos(HWND_TOP, &m_monitorRect, SWP_NOZORDER);
		SetWindowPos(NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOCOPYBITS | SWP_FRAMECHANGED);
	}

	m_bLockPaint = false;
	m_isBeforeFileSelected = m_sStartupFile.IsEmpty();

	if (m_nAutoStartSlideShow > 0) {
		StartMovieMode(1.0 / m_nAutoStartSlideShow);
	}

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
		if (m_sStartupFile.IsEmpty()) {
			OpenFile(true, true);
			Invalidate(TRUE);
		}
	}

	CPaintDC dc(m_hWnd);
	double realizedZoom = 1.0;

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
		m_pCurrentImage->VerifyRotation(CRotationParams(m_pCurrentImage->GetRotationParams(), m_nRotation));

		// Find out zoom multiplier if not yet known
		if (m_dZoomMult < 0.0) {
			m_dZoomMult = GetZoomMultiplier(m_pCurrentImage, m_clientRect);
		}

		// find out the new vitual image size and the size of the bitmap to request
		CSize newSize = Helpers::GetVirtualImageSize(m_pCurrentImage->OrigSize(),
			m_clientRect.Size(), IsAdjustWindowToImage() ? Helpers::ZM_FitToScreenNoZoom : 
			   (m_isUserFitToScreen ? m_autoZoomFitToScreen : GetAutoZoomMode()), m_dZoom);
		m_virtualImageSize = newSize;
		realizedZoom = (double)newSize.cx / m_pCurrentImage->OrigSize().cx;
		CPoint unlimitedOffsets = m_offsets;
		m_offsets = Helpers::LimitOffsets(m_offsets, m_clientRect.Size(), newSize);
		m_DIBOffsets = m_bZoomMode ? (unlimitedOffsets - m_offsets) : CPoint(0, 0);

		// Clip to client rectangle and request the DIB
		CSize clippedSize(min(m_clientRect.Width(), newSize.cx), min(m_clientRect.Height(), newSize.cy));
		CPoint offsetsInImage = m_pCurrentImage->ConvertOffset(newSize, clippedSize, m_offsets);

		void* pDIBData;
		if (m_pUnsharpMaskPanelCtl->IsVisible()) {
			pDIBData = m_pUnsharpMaskPanelCtl->GetUSMDIBForPreview(clippedSize, offsetsInImage, 
				*m_pImageProcParams, CreateProcessingFlags(m_bHQResampling && !m_bTemporaryLowQ && !m_bZoomMode, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, false, m_bLandscapeMode));
		} else if (m_pRotationPanelCtl->IsVisible()) {
			pDIBData = m_pRotationPanelCtl->GetDIBForPreview(newSize, clippedSize, offsetsInImage, 
				*m_pImageProcParams, CreateProcessingFlags(false, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, false, m_bLandscapeMode));
		} else if (m_pTiltCorrectionPanelCtl->IsVisible()) {
			pDIBData = m_pTiltCorrectionPanelCtl->GetDIBForPreview(newSize, clippedSize, offsetsInImage, 
				*m_pImageProcParams, CreateProcessingFlags(false, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, false, m_bLandscapeMode));
		}  else {
			pDIBData = m_pCurrentImage->GetDIB(newSize, clippedSize, offsetsInImage, 
				*m_pImageProcParams, 
				CreateProcessingFlags(m_bHQResampling && !m_bTemporaryLowQ && !m_bZoomMode, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, false, m_bLandscapeMode));
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
			CPoint ptDIBStart = HelpersGUI::DrawDIB32bppWithBlackBorders(dc, bmInfo, pDIBData, backBrush, m_clientRect, clippedSize, m_DIBOffsets);
			// The DIB is also blitted into the memory DCs of the panels
			memDCMgr.BlitImageToMemDC(pDIBData, &bmInfo, ptDIBStart, m_pNavPanelCtl->CurrentBlendingFactor());
		}
		if (m_bZoomMode) m_offsets = unlimitedOffsets;
	}

	// Restore the old clipping region by adding the excluded rectangles again
	memDCMgr.IncludeIntoClippingRegion(dc, excludedClippingRects);

	// paint zoom navigator
	CTrapezoid trapezoid = m_pTiltCorrectionPanelCtl->IsVisible() ? m_pTiltCorrectionPanelCtl->GetCurrentTrapezoid(CZoomNavigator::GetNavigatorRect(m_pCurrentImage, m_pImageProcPanelCtl->PanelRect(), m_pNavPanelCtl->PanelRect()).Size()) : CTrapezoid();
	m_pZoomNavigatorCtl->OnPaint(dc, visRectZoomNavigator, m_pImageProcParams,
		CreateProcessingFlags(!m_pRotationPanelCtl->IsVisible() && !m_pTiltCorrectionPanelCtl->IsVisible(), m_bAutoContrast, false, m_bLDC, false, m_bLandscapeMode), 
		m_pRotationPanelCtl->IsVisible() ? m_pRotationPanelCtl->GetLQRotationAngle() : 0.0, 
		m_pTiltCorrectionPanelCtl->IsVisible() ? &trapezoid : NULL);

	// Display file name if enabled
	DisplayFileName(imageProcessingArea, dc, realizedZoom);

	// Display errors and warnings
	DisplayErrors(m_pCurrentImage, m_clientRect, dc);

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
		HelpersGUI::SelectDefaultFileNameFont(dc);
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

void CMainDlg::PaintToDC(CDC& dc) {
	COLORREF backColor = CSettingsProvider::This().ColorBackground();
	if (backColor == 0)
		backColor = RGB(0, 0, 1); // these f**ing nVidia drivers have a bug when blending pure black
	CBrush backBrush;
	backBrush.CreateSolidBrush(backColor);
	double realizedZoom = 1.0;

	CJPEGImage* pCurrentImage = GetCurrentImage();
	if (pCurrentImage == NULL) {
		dc.FillRect(&m_clientRect, backBrush);
	} else {
		// do this as very first - may changes size of image
		pCurrentImage->VerifyRotation(CRotationParams(pCurrentImage->GetRotationParams(), GetRotation()));

		// find out the new virtual image size and the size of the bitmap to request
		double dZoom = GetZoom();
		CSize newSize = Helpers::GetVirtualImageSize(pCurrentImage->OrigSize(), m_clientRect.Size(), GetAutoZoomMode(), dZoom);
		CPoint offsets = Helpers::LimitOffsets(GetOffsets(), m_clientRect.Size(), newSize);
		realizedZoom = (double)newSize.cx / m_pCurrentImage->OrigSize().cx;

		// Clip to client rectangle and request the DIB
		CSize clippedSize(min(m_clientRect.Width(), newSize.cx), min(m_clientRect.Height(), newSize.cy));
		CPoint offsetsInImage = pCurrentImage->ConvertOffset(newSize, clippedSize, offsets);

		void* pDIBData = pCurrentImage->GetDIB(newSize, clippedSize, offsetsInImage, 
				*GetImageProcessingParams(), 
				CreateDefaultProcessingFlags());
		if (pDIBData != NULL) {
			BITMAPINFO bmInfo;
			CPoint ptDIBStart = HelpersGUI::DrawDIB32bppWithBlackBorders(dc, bmInfo, pDIBData, backBrush, m_clientRect, clippedSize, CPoint(0, 0));
		}

		CRect imageProcessingArea = m_pImageProcPanelCtl->PanelRect();

		if (m_pEXIFDisplayCtl->IsVisible()) {
			m_pEXIFDisplayCtl->OnPrePaintMainDlg(dc);
			BlendBlackRect(dc, *m_pEXIFDisplayCtl->GetPanel(), 0.5f); 
			m_pEXIFDisplayCtl->OnPaintPanel(dc, CPoint(0, 0));
		}

		DisplayFileName(imageProcessingArea, dc, realizedZoom);
		DisplayErrors(pCurrentImage, m_clientRect, dc);
	}
}

void CMainDlg::BlendBlackRect(CDC & targetDC, CPanel& panel, float fBlendFactor) {
	int nW = panel.PanelRect().Width(), nH = panel.PanelRect().Height();
	CDC memDCPanel;
	memDCPanel.CreateCompatibleDC(targetDC);
	CBitmap bitmapPanel;
	bitmapPanel.CreateCompatibleBitmap(targetDC, nW, nH);
	memDCPanel.SelectBitmap(bitmapPanel);
	memDCPanel.FillSolidRect(0, 0, nW, nH, RGB(0, 0, 1)); // nVidia workaround: blending pure black has a bug
	
	BLENDFUNCTION blendFunc;
	memset(&blendFunc, 0, sizeof(blendFunc));
	blendFunc.BlendOp = AC_SRC_OVER;
	blendFunc.SourceConstantAlpha = (unsigned char)(fBlendFactor*255 + 0.5f);
	blendFunc.AlphaFormat = 0;
	targetDC.AlphaBlend(panel.PanelRect().left, panel.PanelRect().top, nW, nH, memDCPanel, 0, 0, nW, nH, blendFunc);
}

void CMainDlg::DisplayErrors(CJPEGImage* pCurrentImage, const CRect& clientRect, CDC& dc) {
	dc.SetTextColor(CSettingsProvider::This().ColorGUI());
	if (m_sStartupFile.IsEmpty() && m_pCurrentImage == NULL) {
		CRect rectText(0, clientRect.Height()/2 - HelpersGUI::ScaleToScreen(40), clientRect.Width(), clientRect.Height());
		if (m_isBeforeFileSelected) {
			dc.DrawText(CNLS::GetString(_T("Select file to display in 'File Open' dialog")), -1, &rectText, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
		} else {
			dc.DrawText(CString(CNLS::GetString(_T("No image loaded!"))) + _T("\n\n") + CNLS::GetString(_T("Right mouse button: Context menu")) + _T("\nCtrl-V: ") +
				CNLS::GetString(_T("Paste from clipboard")) + _T("\nCtrl-O: ") + CNLS::GetString(_T("Open new image or slideshow file")) + _T("\n\n") +
				CNLS::GetString(_T("Press ESC to exit...")), -1, &rectText, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
	   }
	} else if (pCurrentImage == NULL) {
		HelpersGUI::DrawImageLoadErrorText(dc, clientRect,
			(m_nLastLoadError == HelpersGUI::FileLoad_SlideShowListInvalid) ? m_sStartupFile :
			(m_nLastLoadError == HelpersGUI::FileLoad_NoFilesInDirectory) ? m_pFileList->CurrentDirectory() : CurrentFileName(false),
			m_nLastLoadError + (m_bOutOfMemoryLastImage ? HelpersGUI::FileLoad_OutOfMemory : 0));
	}
}

void CMainDlg::DisplayFileName(const CRect& imageProcessingArea, CDC& dc, double realizedZoom) {
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(CSettingsProvider::This().ColorFileName());
	dc.SetBkColor(RGB(0, 0, 0));

	if (m_bShowFileName) {
		HelpersGUI::SelectDefaultFileNameFont(dc);
		CString sFileName = Helpers::GetFileInfoString(CSettingsProvider::This().FileNameFormat(), m_pCurrentImage, m_pFileList, realizedZoom);
		HelpersGUI::DrawTextBordered(dc, sFileName, CRect(HelpersGUI::ScaleToScreen(2) + imageProcessingArea.left, 0, imageProcessingArea.right, HelpersGUI::ScaleToScreen(30)), DT_LEFT); 
	}
}

LRESULT CMainDlg::OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	bool bKeepFitToScreen = !m_bResizeForNewImage && fabs(m_dZoom - GetZoomFactorForFitToScreen(false, false)) < 0.01;
	this->GetClientRect(&m_clientRect);
	this->Invalidate(FALSE);
	if (m_clientRect.Width() < HelpersGUI::ScaleToScreen(800)) {
		m_bShowHelp = false;
		if (m_pImageProcPanelCtl != NULL) m_pImageProcPanelCtl->SetVisible(false);
	}
	if (m_pNavPanelCtl != NULL) {
		m_pNavPanelCtl->AdjustMaximalWidth(m_clientRect.Width() - HelpersGUI::ScaleToScreen(16));
	}
	m_nMouseX = m_nMouseY = -1;

	// keep fit to screen
	if (bKeepFitToScreen) {
		if (fabs(m_dZoom - GetZoomFactorForFitToScreen(false, false)) >= 0.00001) {
			StartLowQTimer(ZOOM_TIMEOUT);
		}
		ResetZoomToFitScreen(false, false, false);
	}
	return 0;
}

LRESULT CMainDlg::OnGetMinMaxInfo(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	if (m_pJPEGProvider != NULL) {
		MINMAXINFO* pMinMaxInfo = (MINMAXINFO*) lParam;
		CSize minimalSize = CSettingsProvider::This().MinimalWindowSize();
		pMinMaxInfo->ptMinTrackSize = CPoint(max(0, minimalSize.cx), max(0, minimalSize.cy));
		return 1;
	} else {
		return 0;
	}
}

LRESULT CMainDlg::OnAnotherInstanceStarted(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled) {
	bHandled = FALSE;
	COPYDATASTRUCT* pData = (COPYDATASTRUCT*)lParam;
	if (pData != NULL && pData->dwData == KEY_MAGIC && pData->cbData > 0 && 
		((m_bFullScreenMode && CSettingsProvider::This().SingleFullScreenInstance()) || CSettingsProvider::This().SingleInstance())) {
		m_sStartupFile = CString((LPCTSTR)pData->lpData, pData->cbData / sizeof(TCHAR) - 1);
		::PostMessage(m_hWnd, WM_LOAD_FILE_ASYNCH, 0, KEY_MAGIC);
		bHandled = TRUE;
		return KEY_MAGIC;
	}
	return 0;
}

LRESULT CMainDlg::OnLoadFileAsynch(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled) {
	bHandled = lParam == KEY_MAGIC;
	if (lParam == KEY_MAGIC && ::IsWindowEnabled(m_hWnd)) {
		m_pPanelMgr->CancelModalPanel();
		GetNavPanelCtl()->HideNavPanelTemporary(true);
		StopMovieMode();
		StopAnimation();
		MouseOn();
		if (m_sStartupFile.IsEmpty()) {
			OpenFile(false, false);
		} else {
			OpenFile(m_sStartupFile, false);
		}
	}
	return 0;
}

LRESULT CMainDlg::OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled) {
	GetWindowRect(m_windowRectOnClose);
	bHandled = FALSE;
	return 0;
}

LRESULT CMainDlg::OnLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	this->SetCapture();
	CPoint pointClicked(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
	bool bEatenByPanel = m_pPanelMgr->OnMouseLButton(MouseEvent_BtnDown, pointClicked.x, pointClicked.y);

	if (!bEatenByPanel) {
		bool bCtrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
		bool bShift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;

		bool bDraggingRequired = m_virtualImageSize.cx > m_clientRect.Width() || m_virtualImageSize.cy > m_clientRect.Height();
		bool bHandleByCropping = m_pCropCtl->IsCropping() || m_pCropCtl->HitHandle(pointClicked.x, pointClicked.y) != CCropCtl::HH_None;
		bool bTransformPanelShown = m_pRotationPanelCtl->IsVisible() || m_pTiltCorrectionPanelCtl->IsVisible();
		if (bHandleByCropping || !m_pZoomNavigatorCtl->OnMouseLButton(MouseEvent_BtnDown, pointClicked.x, pointClicked.y)) {
			if (!m_bZoomModeOnLeftMouse && !bHandleByCropping && HandleMouseButtonByKeymap(VK_LBUTTON)) {
				return 0;
			}
			bool bZoomMode = (bShift || m_bZoomModeOnLeftMouse) && !bCtrl && !bTransformPanelShown && !bHandleByCropping && !m_pUnsharpMaskPanelCtl->IsVisible();
			if (bZoomMode) {
				m_bZoomMode = true;
				m_dStartZoom = m_dZoom;
				m_nCapturedX = m_nMouseX; m_nCapturedY = m_nMouseY;
			} else if ((bCtrl || !bDraggingRequired || bHandleByCropping) && !bTransformPanelShown) {
				m_pCropCtl->StartCropping(pointClicked.x, pointClicked.y);
			} else if (!bTransformPanelShown) {
				StartDragging(pointClicked.x, pointClicked.y, false);
			} 
		}
		SetCursorForMoveSection();
	}

	m_pCropCtl->ResetStartCropOnNextClick();
	return 0;
}

LRESULT CMainDlg::OnLButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	if (m_bZoomMode) {
		m_bZoomMode = false;
		AdjustWindowToImage(false);
		Invalidate(FALSE);
	} else if (m_bDragging) {
		EndDragging();
	} else if (m_pCropCtl->IsCropping()) {
		m_pCropCtl->EndCropping();
	} else {
		m_pPanelMgr->OnMouseLButton(MouseEvent_BtnUp, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
	}
	::ReleaseCapture();
	return 0;
}

LRESULT CMainDlg::OnNCLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	m_dZoomAtResizeStart = m_dZoom;
	bHandled = FALSE; // do not handle message, this would block correct processing by OS
	return 0;
}

LRESULT CMainDlg::OnRButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled) {
	bHandled = HandleMouseButtonByKeymap(VK_RBUTTON);
	return 0;
}

LRESULT CMainDlg::OnRButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	bHandled = HandleMouseButtonByKeymap(VK_RBUTTON, false);
	return 0;
}

LRESULT CMainDlg::OnMButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	this->SetCapture();
	if (HandleMouseButtonByKeymap(VK_MBUTTON)) {
		return 0;
	}
	StartDragging(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), false);
	return 0;
}

LRESULT CMainDlg::OnMButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	EndDragging();
	::ReleaseCapture();
	return 0;
}

LRESULT CMainDlg::OnLButtonDblClk(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	if (!m_bDragging && !m_pCropCtl->IsCropping()) {
		if (m_pPanelMgr->OnMouseLButton(MouseEvent_BtnDblClk, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam))) {
			return 0;
		}
	}
	if (!m_pCropCtl->IsCropping() && m_pCurrentImage != NULL) {
		if (HandleMouseButtonByKeymap(VK_LBUTTONDBLCLK)) {
			return 0;
		}
		double dZoom = -1.0;
		CSize sizeAutoZoom = Helpers::GetVirtualImageSize(m_pCurrentImage->OrigSize(),
			m_clientRect.Size(), IsAdjustWindowToImage() ? Helpers::ZM_FitToScreenNoZoom : GetAutoZoomMode(), dZoom);
		if (sizeAutoZoom != m_virtualImageSize) {
			ExecuteCommand(GetAutoZoomMode() * 10 + IDM_AUTO_ZOOM_FIT_NO_ZOOM);
		} else {
			ResetZoomTo100Percents(true);
		}
	}
	return 0;
}

LRESULT CMainDlg::OnXButtonDown(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	if (!m_pPanelMgr->IsModalPanelShown()) {
		if (HandleMouseButtonByKeymap((GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2)) {
			return 0;
		}
		bool bExchangeXButtons = CSettingsProvider::This().ExchangeXButtons();
		if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) {
			GotoImage(bExchangeXButtons ? POS_Next : POS_Previous);
		} else {
			GotoImage(bExchangeXButtons ? POS_Previous : POS_Next);
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
	bool bMouseCursorSet = false;
	if (m_bZoomMode) {
		int nDX = m_nMouseX - m_nCapturedX;
		double dFactor = 1 + pow(nDX * nDX, 0.8) / 1500;
		if (nDX < 0) dFactor = 1.0/dFactor;
		PerformZoom(m_dStartZoom * dFactor, false, true, false);
	} else if (m_bDragging) {
		DoDragging();
	} else if (m_pCropCtl->IsCropping()) {
		bMouseCursorSet = m_pCropCtl->DoCropping(m_nMouseX, m_nMouseY);
	} else if (!m_pPanelMgr->OnMouseMove(m_nMouseX, m_nMouseY)) {
		m_pZoomNavigatorCtl->OnMouseMove(nOldMouseX, nOldMouseY);
	}
	if (!m_bPanMouseCursorSet && !bMouseCursorSet) {
		::SetCursor(::LoadCursor(NULL, MAKEINTRESOURCE(IDC_ARROW)));
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
		PerformZoom(double(nDelta)/WHEEL_DELTA, true, m_bMouseOn, true);
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
	if (wParam == VK_ESCAPE && m_bShowHelp) {
		bHandled = true;
		m_bShowHelp = false;
		this->Invalidate(FALSE);
	} else if (wParam == VK_ESCAPE && m_pCropCtl->IsCropping()) {
		bHandled = true;
		m_pCropCtl->AbortCropping();
	} else if (!bCtrl && wParam != VK_ESCAPE && m_nLastLoadError == HelpersGUI::FileLoad_NoFilesInDirectory && !m_sStartupFile.IsEmpty()) {
		// search in subfolders if initial directory has no images
		bHandled = true;
		m_pFileList->SetNavigationMode(Helpers::NM_LoopSubDirectories);
		GotoImage(POS_Next);
	} else if (wParam >= '1' && wParam <= '9' && (!bShift || bCtrl)) {
		// Start the slideshow
		bHandled = true;
		int nValue = (int)wParam - '1' + 1;
		if (bCtrl && bShift) {
			nValue *= 10; // 1/100 seconds
		} else if (bCtrl) {
			nValue *= 100; // 1/10 seconds
		} else {
			nValue *= 1000; // seconds
		}
		StartMovieMode(1000.0/nValue);
	} else if (wParam == VK_F1) {
		bHandled = true;
		ExecuteCommand(IDM_HELP);
	} else {
		int nCommand = m_pKeyMap->GetCommandIdForKey((int)wParam, bAlt, bCtrl, bShift);
		if (nCommand > 0) {
			bHandled = true;
			ExecuteCommand(nCommand);
		}
	}

	if (!bHandled) {
		// look if any of the user commands wants to handle this key
		if (m_pFileList->Current() != NULL) {
			HandleUserCommands(CKeyMap::GetCombinedKeyCode((int)wParam, bAlt, bCtrl, bShift));
		}
	}

	return 1;
}

LRESULT CMainDlg::OnSysKeyDown(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	if (m_pPanelMgr->IsModalPanelShown()) {
		return 1;
	}
	bool bShift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
	if (wParam == VK_F4) {
		CleanupAndTerminate();
	} else {
		int nCommand = m_pKeyMap->GetCommandIdForKey((int)wParam, true, false, bShift);
		if (nCommand > 0) {
			ExecuteCommand(nCommand);
		} else if (m_pFileList->Current() != NULL) {
			HandleUserCommands(CKeyMap::GetCombinedKeyCode((int)wParam, true, false, bShift));
		}
	}
	return 1;
}


LRESULT CMainDlg::OnGetDlgCode(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	// need to request key messages, else the dialog proc eats them all up
	return DLGC_WANTALLKEYS;
}

LRESULT CMainDlg::OnImageLoadCompleted(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	// route to JPEG provider
	m_pJPEGProvider->OnImageLoadCompleted((int)lParam);
	return 0;
}

LRESULT CMainDlg::OnDisplayedFileChangedOnDisk(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	if (CSettingsProvider::This().ReloadWhenDisplayedImageChanged() && m_pCurrentImage != NULL && !m_pCurrentImage->IsClipboardImage() &&
		m_pFileList != NULL && m_pFileList->CanOpenCurrentFileForReading()) {
		ExecuteCommand(IDM_RELOAD);
	}
	return 0;
}

LRESULT CMainDlg::OnActiveDirectoryFilelistChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	if (CSettingsProvider::This().ReloadWhenDisplayedImageChanged() && m_pFileList != NULL && m_pFileList->CurrentFileExists()) {
		m_pFileList->Reload(NULL, false);
		Invalidate(FALSE);
	}
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
			OpenFile(buff, false);
		}
		::DragFinish(hDrop);
	}
	return 0;
}

LRESULT CMainDlg::OnTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	if (wParam == ANIMATION_TIMER_EVENT_ID || (wParam == SLIDESHOW_TIMER_EVENT_ID && m_nCurrentTimeout > 0)) {
		// Remove all timer messages for slideshow and animation events that accumulated in the queue
		MSG msg;
		while (::PeekMessage(&msg, this->m_hWnd, WM_TIMER, WM_TIMER, PM_REMOVE)) {
			if (msg.wParam != SLIDESHOW_TIMER_EVENT_ID && msg.wParam != ANIMATION_TIMER_EVENT_ID) {
				BOOL bNotUsed;
				OnTimer(WM_TIMER, msg.wParam, msg.lParam, bNotUsed);
			}
			if (msg.wParam == SLIDESHOW_TIMER_EVENT_ID && wParam == ANIMATION_TIMER_EVENT_ID) {
				// if there are queued slideshow timer events and we process an animation event, the slideshow event has preceedence
				wParam = SLIDESHOW_TIMER_EVENT_ID;
			}
		}
		// Goto next image if no other messages to process are pending
		if (!::PeekMessage(&msg, this->m_hWnd, 0, 0, PM_NOREMOVE)) {
			int nRealDisplayTimeMs = ::GetTickCount() - m_nLastSlideShowImageTickCount;
			if (m_nCurrentTimeout > 250 && wParam == SLIDESHOW_TIMER_EVENT_ID) {
				if (m_nCurrentTimeout - nRealDisplayTimeMs > 100) {
					// restart timer
					::Sleep(m_nCurrentTimeout - nRealDisplayTimeMs);
					::KillTimer(this->m_hWnd, SLIDESHOW_TIMER_EVENT_ID);
					::SetTimer(this->m_hWnd, SLIDESHOW_TIMER_EVENT_ID, m_nCurrentTimeout, NULL);
				}
			}
			GotoImage((wParam == ANIMATION_TIMER_EVENT_ID) ? POS_NextAnimation : POS_NextSlideShow, NO_REMOVE_KEY_MSG);
			if (wParam == SLIDESHOW_TIMER_EVENT_ID && UseSlideShowTransitionEffect()) {
				AnimateTransition();
			}
			if (wParam != ANIMATION_TIMER_EVENT_ID) {
				m_nLastSlideShowImageTickCount = ::GetTickCount();
			}
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

	if (m_pCropCtl->IsCropping()) {
		m_pCropCtl->ShowCropContextMenu();
		return 1;
	}

	HMENU hMenu = ::LoadMenu(_Module.m_hInst, _T("PopupMenu"));
	if (hMenu == NULL) return 1;

	HMENU hMenuTrackPopup = ::GetSubMenu(hMenu, 0);
	HelpersGUI::TranslateMenuStrings(hMenuTrackPopup, m_pKeyMap);
	
	if (m_pEXIFDisplayCtl->IsActive()) ::CheckMenuItem(hMenuTrackPopup, IDM_SHOW_FILEINFO, MF_CHECKED);
	if (m_bShowFileName) ::CheckMenuItem(hMenuTrackPopup, IDM_SHOW_FILENAME, MF_CHECKED);
	if (m_pNavPanelCtl->IsActive()) ::CheckMenuItem(hMenuTrackPopup, IDM_SHOW_NAVPANEL, MF_CHECKED);
	if (m_bAutoContrast) ::CheckMenuItem(hMenuTrackPopup, IDM_AUTO_CORRECTION, MF_CHECKED);
	if (m_bLDC) ::CheckMenuItem(hMenuTrackPopup, IDM_LDC, MF_CHECKED);
	if (m_bKeepParams) ::CheckMenuItem(hMenuTrackPopup, IDM_KEEP_PARAMETERS, MF_CHECKED);
	HMENU hMenuNavigation = ::GetSubMenu(hMenuTrackPopup, SUBMENU_POS_NAVIGATION);
	::CheckMenuItem(hMenuNavigation,  m_pFileList->GetNavigationMode()*10 + IDM_LOOP_FOLDER, MF_CHECKED);
	HMENU hMenuOrdering = ::GetSubMenu(hMenuTrackPopup, SUBMENU_POS_DISPLAY_ORDER);
	::CheckMenuItem(hMenuOrdering,  
		(m_pFileList->GetSorting() == Helpers::FS_LastModTime) ? IDM_SORT_MOD_DATE :
		(m_pFileList->GetSorting() == Helpers::FS_CreationTime) ? IDM_SORT_CREATION_DATE :
		(m_pFileList->GetSorting() == Helpers::FS_FileName) ? IDM_SORT_NAME :
		(m_pFileList->GetSorting() == Helpers::FS_Random) ? IDM_SORT_RANDOM : IDM_SORT_SIZE
		, MF_CHECKED);
	::CheckMenuItem(hMenuOrdering, m_pFileList->IsSortedUpcounting() ? IDM_SORT_UPCOUNTING : IDM_SORT_DOWNCOUNTING, MF_CHECKED);
	if (m_pFileList->GetSorting() == Helpers::FS_Random) {
		::EnableMenuItem(hMenuOrdering, IDM_SORT_UPCOUNTING, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuOrdering, IDM_SORT_DOWNCOUNTING, MF_BYCOMMAND | MF_GRAYED);
	}
	HMENU hMenuMovie = ::GetSubMenu(hMenuTrackPopup, SUBMENU_POS_MOVIE);
	if (!m_bMovieMode) ::EnableMenuItem(hMenuMovie, IDM_STOP_MOVIE, MF_BYCOMMAND | MF_GRAYED);
	HMENU hMenuZoom = ::GetSubMenu(hMenuTrackPopup, SUBMENU_POS_ZOOM);
	if (m_bSpanVirtualDesktop) ::CheckMenuItem(hMenuZoom,  IDM_SPAN_SCREENS, MF_CHECKED);
	if (m_bFullScreenMode) ::CheckMenuItem(hMenuZoom,  IDM_FULL_SCREEN_MODE, MF_CHECKED);
	if (IsAdjustWindowToImage() && IsImageExactlyFittingWindow()) ::CheckMenuItem(hMenuZoom, IDM_FIT_WINDOW_TO_IMAGE, MF_CHECKED);
	HMENU hMenuAutoZoomMode = ::GetSubMenu(hMenuTrackPopup, SUBMENU_POS_AUTOZOOMMODE);
	::CheckMenuItem(hMenuAutoZoomMode, GetAutoZoomMode() * 10 + IDM_AUTO_ZOOM_FIT_NO_ZOOM, MF_CHECKED);
	HMENU hMenuSettings = ::GetSubMenu(hMenuTrackPopup, SUBMENU_POS_SETTINGS);
	HMENU hMenuModDate = ::GetSubMenu(hMenuTrackPopup, SUBMENU_POS_MODDATE);
	HMENU hMenuUserCommands = ::GetSubMenu(hMenuTrackPopup, SUBMENU_POS_USER_COMMANDS);
	HMENU hMenuOpenWithCommands = ::GetSubMenu(hMenuTrackPopup, SUBMENU_POS_OPENWITH);
	HMENU hMenuWallpaper = ::GetSubMenu(hMenuTrackPopup, SUBMENU_POS_WALLPAPER);

	if (!HelpersGUI::CreateUserCommandsMenu(hMenuUserCommands)) {
		::DeleteMenu(hMenuTrackPopup, SUBMENU_POS_USER_COMMANDS + 1, MF_BYPOSITION);
		::DeleteMenu(hMenuTrackPopup, SUBMENU_POS_USER_COMMANDS, MF_BYPOSITION);
		::DeleteMenu(hMenuTrackPopup, SUBMENU_POS_USER_COMMANDS - 1, MF_BYPOSITION);
	}
	if (!m_bFullScreenMode) {
		// Transition effect and speed only available in full screen mode
		::DeleteMenu(hMenuMovie, 9, MF_BYPOSITION);
		::DeleteMenu(hMenuMovie, 9, MF_BYPOSITION);
	} else {
		::CheckMenuItem(hMenuMovie, m_eTransitionEffect + IDM_EFFECT_NONE, MF_CHECKED);
		int nIndex = (m_nTransitionTime < 180) ? 0 : (m_nTransitionTime < 375) ? 1 : (m_nTransitionTime < 750) ? 2 : (m_nTransitionTime < 1500) ? 3 : 4;
		::CheckMenuItem(hMenuMovie, nIndex + IDM_EFFECTTIME_VERY_FAST, MF_CHECKED);
	}

	if (CParameterDB::This().IsEmpty()) ::EnableMenuItem(hMenuSettings, IDM_BACKUP_PARAMDB, MF_BYCOMMAND | MF_GRAYED);
	if (m_bFullScreenMode) ::EnableMenuItem(hMenuZoom, IDM_FIT_WINDOW_TO_IMAGE, MF_BYCOMMAND | MF_GRAYED);
	if (!m_bFullScreenMode) ::EnableMenuItem(hMenuZoom, IDM_SPAN_SCREENS, MF_BYCOMMAND | MF_GRAYED);

	::EnableMenuItem(hMenuMovie, IDM_SLIDESHOW_START, MF_BYCOMMAND | MF_GRAYED);
	::EnableMenuItem(hMenuMovie, IDM_MOVIE_START_FPS, MF_BYCOMMAND | MF_GRAYED);

	if (!CSettingsProvider::This().AllowEditGlobalSettings()) {
		::DeleteMenu(hMenuSettings, 0, MF_BYPOSITION);
	}

	bool bCanPaste = ::IsClipboardFormatAvailable(CF_DIB);
	if (!bCanPaste) ::EnableMenuItem(hMenuTrackPopup, IDM_PASTE, MF_BYCOMMAND | MF_GRAYED);

	bool bCanDoLosslessJPEGTransform = (m_pCurrentImage != NULL) && m_pCurrentImage->GetImageFormat() == IF_JPEG && !m_pCurrentImage->IsDestructivlyProcessed();

	if (!bCanDoLosslessJPEGTransform) ::EnableMenuItem(hMenuTrackPopup, SUBMENU_POS_TRANSFORM_LOSSLESS, MF_BYPOSITION | MF_GRAYED);

	if (m_pCurrentImage == NULL) {
		::EnableMenuItem(hMenuTrackPopup, IDM_SAVE, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_RELOAD, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_PRINT, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_COPY, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_COPY_FULL, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_SAVE_PARAM_DB, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_CLEAR_PARAM_DB, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, SUBMENU_POS_ZOOM, MF_BYPOSITION  | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, SUBMENU_POS_MODDATE, MF_BYPOSITION  | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, SUBMENU_POS_TRANSFORM, MF_BYPOSITION  | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, SUBMENU_POS_WALLPAPER, MF_BYPOSITION | MF_GRAYED);
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
		int windowsVersion = Helpers::GetWindowsVersion();
		if (m_pCurrentImage->IsClipboardImage() || (windowsVersion < 600 && m_pCurrentImage->GetImageFormat() != IF_WindowsBMP) || 
			(windowsVersion < 602 && !(m_pCurrentImage->GetImageFormat() == IF_WindowsBMP || m_pCurrentImage->GetImageFormat() == IF_JPEG)) ||
			!m_pCurrentImage->IsGDIPlusFormat()) {
			::EnableMenuItem(hMenuWallpaper, IDM_SET_WALLPAPER_ORIG, MF_BYCOMMAND | MF_GRAYED);
		}
	}
	if (!HelpersGUI::CreateOpenWithCommandsMenu(hMenuOpenWithCommands) || m_pCurrentImage == NULL) {
		::DeleteMenu(hMenuTrackPopup, SUBMENU_POS_OPENWITH, MF_BYPOSITION);
	}
	if (m_bMovieMode) {
		::EnableMenuItem(hMenuTrackPopup, IDM_SAVE, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_RELOAD, MF_BYCOMMAND | MF_GRAYED);
		::EnableMenuItem(hMenuTrackPopup, IDM_PRINT, MF_BYCOMMAND | MF_GRAYED);
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

LRESULT CMainDlg::OnEraseBackground(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled) {
	// prevent erasing background
	bHandled = TRUE;
	return TRUE;
}

LRESULT CMainDlg::OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	// NOP
	return 0;
}

LRESULT CMainDlg::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CleanupAndTerminate();
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////
// Static helpers for being called from UI controls
///////////////////////////////////////////////////////////////////////////////////

void CMainDlg::OnExecuteCommand(void* pContext, int nParameter, CButtonCtrl & sender) {
	((CMainDlg*)pContext)->ExecuteCommand(nParameter);
}

bool CMainDlg::IsCurrentImageFitToScreen(void* pContext) {
	CMainDlg* pThis = (CMainDlg*)pContext;
	return fabs(pThis->m_dZoom - pThis->GetZoomFactorForFitToScreen(false, true)) <= 0.01;
}

///////////////////////////////////////////////////////////////////////////////////
// Private
///////////////////////////////////////////////////////////////////////////////////

void CMainDlg::ExecuteCommand(int nCommand) {
	CSettingsProvider& sp = CSettingsProvider::This();
	switch (nCommand) {
		case IDM_HELP:
			m_bShowHelp = !m_bShowHelp;
			if (m_clientRect.Width() < HelpersGUI::ScaleToScreen(800) || m_pCropCtl->IsCropping()) {
				m_bShowHelp = false;
			}
			this->Invalidate(FALSE);
			break;
		case IDM_TOGGLE:
			if (m_pFileList->FileMarkedForToggle()) {
				GotoImage(POS_Toggle);
			}
			break;
		case IDM_MARK_FOR_TOGGLE:
			if (m_pFileList->Current() != NULL && m_pCurrentImage != NULL && !m_pCurrentImage->IsClipboardImage()) {
				m_pFileList->MarkCurrentFile();
			}
			break;
		case IDM_MINIMIZE:
			this->ShowWindow(SW_MINIMIZE);
			break;
		case IDM_OPEN:
			OpenFile(false, false);
			break;
		case IDM_SAVE:
		case IDM_SAVE_SCREEN:
		case IDM_SAVE_ALLOW_NO_PROMPT:
			if (m_pCurrentImage != NULL) {
				if (nCommand == IDM_SAVE_ALLOW_NO_PROMPT && sp.SaveWithoutPrompt() && !m_pCurrentImage->IsClipboardImage()) {
					SaveImageNoPrompt(CurrentFileName(false), true);
				} else {
					SaveImage(nCommand != IDM_SAVE_SCREEN);
				}
			}
			break;
		case IDM_RELOAD:
			ReloadImage(false);
			break;
		case IDM_PRINT:
			if (m_pCurrentImage != NULL) {
				StopAnimation(); // stop any running animation
				if (m_pPrintImage->Print(this->m_hWnd, m_pCurrentImage, *m_pImageProcParams, CreateDefaultProcessingFlags(), m_pFileList->Current())) {
					this->Invalidate(FALSE);
				}
			}
			break;
		case IDM_COPY:
			if (m_pCurrentImage != NULL) {
				CClipboard::CopyImageToClipboard(this->m_hWnd, m_pCurrentImage, m_pFileList->Current());
			}
			break;
		case IDM_COPY_FULL:
			if (m_pCurrentImage != NULL) {
				CClipboard::CopyFullImageToClipboard(this->m_hWnd, m_pCurrentImage, *m_pImageProcParams, CreateDefaultProcessingFlags(), m_pFileList->Current());
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
		case IDM_RENAME:
			m_pImageProcPanelCtl->EnterRenameCurrentFile();
			break;
		case IDM_MOVE_TO_RECYCLE_BIN:
		case IDM_MOVE_TO_RECYCLE_BIN_CONFIRM:
		case IDM_MOVE_TO_RECYCLE_BIN_CONFIRM_PERMANENT_DELETE:
			MouseOn();
			if (m_pCurrentImage != NULL && m_pFileList != NULL && !m_pCurrentImage->IsClipboardImage() && sp.AllowFileDeletion()) {
				LPCTSTR currentFileName = CurrentFileName(false);
				bool noConfirmation = nCommand == IDM_MOVE_TO_RECYCLE_BIN ||
					(nCommand == IDM_MOVE_TO_RECYCLE_BIN_CONFIRM_PERMANENT_DELETE && CFileList::DriveHasRecycleBin(currentFileName));
				if (noConfirmation ||
					IDYES == ::MessageBox(m_hWnd, CString(CNLS::GetString(_T("Do you really want to delete the current image file on disk?"))) + _T("\n") + currentFileName, CNLS::GetString(_T("Confirm")), MB_YESNOCANCEL | MB_ICONWARNING)) {
					CFileList* fileListOfDeletedImage = m_pFileList;
					GotoImage(POS_AwayFromCurrent, NO_REQUEST);
					if (m_pFileList->DeleteFile(currentFileName)) {
						fileListOfDeletedImage->Reload(NULL, false);
						m_pFileList->DeleteHistory(true);
						Invalidate();
						GotoImage(POS_Current);
					} else {
						Invalidate();
						if (m_pFileList->Current() == NULL) GotoImage(POS_First);
						else GotoImage(POS_Previous);
					}
				}
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
		case IDM_FIRST:
			GotoImage(POS_First);
			break;
		case IDM_LAST:
			GotoImage(POS_Last);
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
		case IDM_SORT_RANDOM:
		case IDM_SORT_SIZE:
			m_pFileList->SetSorting(
				(nCommand == IDM_SORT_CREATION_DATE) ? Helpers::FS_CreationTime : 
				(nCommand == IDM_SORT_MOD_DATE) ? Helpers::FS_LastModTime : 
				(nCommand == IDM_SORT_RANDOM) ? Helpers::FS_Random : 
				(nCommand == IDM_SORT_SIZE) ? Helpers::FS_FileSize : Helpers::FS_FileName, m_pFileList->IsSortedUpcounting());
			if (m_bShowHelp || m_pEXIFDisplayCtl->IsActive() || m_bShowFileName) {
				this->Invalidate(FALSE);
			}
			break;
		case IDM_SORT_UPCOUNTING:
		case IDM_SORT_DOWNCOUNTING:
			m_pFileList->SetSorting(m_pFileList->GetSorting(), nCommand == IDM_SORT_UPCOUNTING);
			if (m_pEXIFDisplayCtl->IsActive() || m_bShowFileName) {
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
		case IDM_EFFECT_NONE:
		case IDM_EFFECT_BLEND:
		case IDM_EFFECT_SLIDE_RL:
		case IDM_EFFECT_SLIDE_LR:
		case IDM_EFFECT_SLIDE_TB:
		case IDM_EFFECT_SLIDE_BT:
		case IDM_EFFECT_ROLL_RL:
		case IDM_EFFECT_ROLL_LR:
		case IDM_EFFECT_ROLL_TB:
		case IDM_EFFECT_ROLL_BT:
		case IDM_EFFECT_SCROLL_RL:
		case IDM_EFFECT_SCROLL_LR:
		case IDM_EFFECT_SCROLL_TB:
		case IDM_EFFECT_SCROLL_BT:
			m_eTransitionEffect = (Helpers::ETransitionEffect)(nCommand - IDM_EFFECT_NONE);
			break;
		case IDM_EFFECTTIME_VERY_FAST:
		case IDM_EFFECTTIME_FAST:
		case IDM_EFFECTTIME_NORMAL:
		case IDM_EFFECTTIME_SLOW:
		case IDM_EFFECTTIME_VERY_SLOW:
			m_nTransitionTime = 125 * (1 << (nCommand - IDM_EFFECTTIME_VERY_FAST));
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
				newEntry.InitFromProcessParams(*m_pImageProcParams, procFlags, m_pCurrentImage->IsAnimation() ? CRotationParams(m_nRotation) : CRotationParams(m_pCurrentImage->GetRotationParams(), m_nRotation));
				if ((m_bUserZoom || m_bUserPan) && !m_pCurrentImage->IsCropped()) {
					newEntry.InitGeometricParams(m_pCurrentImage->OrigSize(), m_dZoom, m_offsets, 
						m_bAutoFitWndToImage ? CMultiMonitorSupport::GetMonitorRect(m_hWnd).Size() : m_clientRect.Size(), m_bAutoFitWndToImage);
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
						GetDefaultProcessingParams(), procFlags, 0, -1, CPoint(0, 0), CSize(0, 0), CSize(0, 0));
					*m_pImageProcParams = GetDefaultProcessingParams();
					// Sunset and night shot detection may has changed this
					m_pImageProcParams->LightenShadows = m_pCurrentImage->GetInitialProcessParams().LightenShadows;
					InitFromProcessingFlags(procFlags, m_bHQResampling, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, m_bLandscapeMode);
					m_nRotation = m_pCurrentImage->GetInitialRotation();
					m_dZoom = -1;
					m_pCurrentImage->SetIsInParamDB(false);
					m_pImageProcPanelCtl->ShowHideSaveDBButtons();
					if (fabs(m_pCurrentImage->GetRotationParams().FreeRotation) > 0.009) {
						ReloadImage(false); // free rotation cannot be restored, needs reload
					}
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
				if (!IsAdjustWindowToImage()) m_dZoom = -1;
				m_bUserZoom = false;
				m_bUserPan = false;
				AdjustWindowToImage(false);
				this->Invalidate(FALSE);
			}
			break;
		case IDM_ROTATE:
			m_pCropCtl->AbortCropping();
			GetRotationPanelCtl()->SetVisible(true);
			break;
		case IDM_CHANGESIZE:
			if (m_pCurrentImage != NULL) {
				MouseOn();
				CResizeDlg dlgResize(m_pCurrentImage->OrigSize());
				if (dlgResize.DoModal(m_hWnd) == IDOK) {
					HCURSOR hOldCursor = ::SetCursor(::LoadCursor(NULL, MAKEINTRESOURCE(IDC_WAIT)));
					m_pCurrentImage->ResizeOriginalPixels(dlgResize.GetFilter(), dlgResize.GetNewSize());
					::SetCursor(hOldCursor);
					m_pImageProcPanelCtl->ShowHideSaveDBButtons();
					this->Invalidate(FALSE);
				}
			}
			break;
		case IDM_PERSPECTIVE:
			m_pCropCtl->AbortCropping();
			GetTiltCorrectionPanelCtl()->SetVisible(true);
			break;
		case IDM_MIRROR_H:
		case IDM_MIRROR_V:
			if (m_pCurrentImage != NULL) {
				m_pCurrentImage->Mirror(nCommand == IDM_MIRROR_H);
				m_pImageProcPanelCtl->ShowHideSaveDBButtons();
				this->Invalidate(FALSE);
			}
			break;
		case IDM_ROTATE_90_LOSSLESS:
		case IDM_ROTATE_90_LOSSLESS_CONFIRM:
		case IDM_ROTATE_270_LOSSLESS:
		case IDM_ROTATE_270_LOSSLESS_CONFIRM:
		case IDM_ROTATE_180_LOSSLESS:
		case IDM_MIRROR_H_LOSSLESS:
		case IDM_MIRROR_V_LOSSLESS:
			if (m_pCurrentImage != NULL && m_pCurrentImage->GetImageFormat() == IF_JPEG) {
				bool bCanTransformWithoutTrim = m_pCurrentImage->CanUseLosslessJPEGTransformations();
				bool bAskIfToTrim = !(bCanTransformWithoutTrim ||sp.TrimWithoutPromptLosslessJPEG());
				bool bPerformTransformation = true;
				bool bTrim = false;
				MouseOn();
				if (bAskIfToTrim) {
					bTrim = IDYES == ::MessageBox(m_hWnd, CString(CNLS::GetString(_T("Image width and height must be dividable by the JPEG block size (8 or 16) for lossless transformations!"))) + _T("\n") +
						CNLS::GetString(_T("The transformation can be applied if the image is trimmed to the next matching size but this will remove some border pixels.")) + _T("\n") +
						CNLS::GetString(_T("Trim the image and apply transformation? Trimming cannot be undone!")) + _T("\n\n") +
						CNLS::GetString(_T("Note: Set the key 'TrimWithoutPromptLosslessJPEG=true' in the INI file to always trim without showing this message.")), 
						CNLS::GetString(_T("Lossless JPEG transformations")), MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON2);
				}
				if (!bAskIfToTrim || bTrim) {
					if (!bAskIfToTrim && (nCommand == IDM_ROTATE_90_LOSSLESS_CONFIRM || nCommand == IDM_ROTATE_270_LOSSLESS_CONFIRM)) {
						LPCTSTR sConfirmMsg = (nCommand == IDM_ROTATE_90_LOSSLESS_CONFIRM) ?
							_T("Rotate current file on disk lossless by 90 deg (W/H must be multiple of 16)") :
							_T("Rotate current file on disk lossless by 270 deg (W/H must be multiple of 16)");
						bPerformTransformation = IDYES == ::MessageBox(m_hWnd, 
							CNLS::GetString(sConfirmMsg), CNLS::GetString(_T("Confirm")), MB_YESNOCANCEL | MB_ICONWARNING);
					}
					if (bPerformTransformation) {
						CJPEGLosslessTransform::EResult eResult =
							CJPEGLosslessTransform::PerformTransformation(m_pFileList->Current(), m_pFileList->Current(), HelpersGUI::CommandIdToLosslessTransformation(nCommand), bTrim || sp.TrimWithoutPromptLosslessJPEG());
						if (eResult != CJPEGLosslessTransform::Success) {
							::MessageBox(m_hWnd, CString(CNLS::GetString(_T("Performing the lossless transformation failed!"))) + 
								+ _T("\n") + CNLS::GetString(_T("Reason:")) + _T(" ") + HelpersGUI::LosslessTransformationResultToString(eResult), 
								CNLS::GetString(_T("Lossless JPEG transformations")), MB_OK | MB_ICONWARNING);
						} else {
							ReloadImage(false); // reload current image
						}
					}
				}
			}
			break;
		case IDM_AUTO_CORRECTION:
			m_bAutoContrastSection = false;
			m_bAutoContrast = !m_bAutoContrast;
			this->Invalidate(FALSE);
			break;
		case IDM_AUTO_CORRECTION_SECTION:
			m_bAutoContrast = true;
			m_bAutoContrastSection = !m_bAutoContrastSection;
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
				m_dZoomKept = (m_bUserZoom || IsAdjustWindowToImage()) ? m_dZoom : -1;
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
		case IDM_FIT_TO_SCREEN_NO_ENLARGE:
			ResetZoomToFitScreen(false, nCommand == IDM_FIT_TO_SCREEN, true);
			break;
		case IDM_FILL_WITH_CROP:
			ResetZoomToFitScreen(true, true, true);
			break;
		case IDM_TOGGLE_FIT_TO_SCREEN_100_PERCENTS:
			if (fabs(m_dZoom - 1) < 0.01) {
				ResetZoomToFitScreen(false, true, true);
			} else {
				ResetZoomTo100Percents(m_bMouseOn);
			}
			break;
		case IDM_SPAN_SCREENS:
			if (CMultiMonitorSupport::IsMultiMonitorSystem() && m_bFullScreenMode) {
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
			m_bFullScreenMode = !m_bFullScreenMode;
			m_dZoomAtResizeStart = 1.0;
			if (!m_bFullScreenMode) {
				CRect windowRect;
				this->SetWindowLongW(GWL_STYLE, this->GetWindowLongW(GWL_STYLE) | WS_OVERLAPPEDWINDOW | WS_VISIBLE);
				HICON hIconSmall = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
					IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
				SetIcon(hIconSmall, FALSE);
				CRect defaultWindowRect = CMultiMonitorSupport::GetDefaultWindowRect();
				double dZoom = -1;
				windowRect = sp.ExplicitWindowRect() ? defaultWindowRect : Helpers::GetWindowRectMatchingImageSize(m_hWnd, CSize(MIN_WND_WIDTH, MIN_WND_HEIGHT), defaultWindowRect.Size(), dZoom, m_pCurrentImage, false, true);
				this->SetWindowPos(HWND_TOP, windowRect.left, windowRect.top, windowRect.Width(), windowRect.Height(), SWP_NOZORDER | SWP_NOCOPYBITS);
				this->MouseOn();
				m_bSpanVirtualDesktop = false;
			} else {
				if (!IsZoomed() && sp.ExplicitWindowRect()) {
					// Save the old window rect to be able to restore it
					CRect rect;
					GetWindowRect(&rect);
					CMultiMonitorSupport::SetDefaultWindowRect(rect);
				}
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
		case IDM_FIT_WINDOW_TO_IMAGE:
			// Note: If auto fit is on but the window size does not match the image size (due to manual window resizing), restore window to image
			if (!(m_bAutoFitWndToImage && !IsImageExactlyFittingWindow()))
				m_bAutoFitWndToImage = !m_bAutoFitWndToImage;
			AdjustWindowToImage(false);
			break;
		case IDM_ZOOM_400:
			PerformZoom(4.0, false, m_bMouseOn, true);
			break;
		case IDM_ZOOM_200:
			PerformZoom(2.0, false, m_bMouseOn, true);
			break;
		case IDM_ZOOM_100:
			ResetZoomTo100Percents(m_bMouseOn);
			break;
		case IDM_ZOOM_50:
			PerformZoom(0.5, false, m_bMouseOn, true);
			break;
		case IDM_ZOOM_25:
			PerformZoom(0.25, false, m_bMouseOn, true);
			break;
		case IDM_ZOOM_INC:
		case IDM_ZOOM_DEC:
			PerformZoom((nCommand == IDM_ZOOM_INC) ? 1 : -1, true, m_bMouseOn, true);
			break;
		case IDM_ZOOM_MODE:
			m_bZoomModeOnLeftMouse = !m_bZoomModeOnLeftMouse;
			m_pNavPanelCtl->GetNavPanel()->GetBtnZoomMode()->SetActive(m_bZoomModeOnLeftMouse);
			break;
		case IDM_AUTO_ZOOM_FIT_NO_ZOOM:
		case IDM_AUTO_ZOOM_FILL_NO_ZOOM:
		case IDM_AUTO_ZOOM_FIT:
		case IDM_AUTO_ZOOM_FILL:
			{
				Helpers::EAutoZoomMode eAutoZoomMode = (Helpers::EAutoZoomMode)((nCommand - IDM_AUTO_ZOOM_FIT_NO_ZOOM) / 10);
				if (m_eAutoZoomModeFullscreen == m_eAutoZoomModeWindowed)
					m_eAutoZoomModeFullscreen = m_eAutoZoomModeWindowed = eAutoZoomMode;
				else if (m_bFullScreenMode)
					m_eAutoZoomModeFullscreen = eAutoZoomMode;
				else
					m_eAutoZoomModeWindowed = eAutoZoomMode;
				m_dZoom = -1.0;
				m_offsets = CPoint(0, 0);
				this->Invalidate(FALSE);
				AdjustWindowToImage(false);
				break;
			}
		case IDM_EDIT_GLOBAL_CONFIG:
		case IDM_EDIT_USER_CONFIG:
			EditINIFile(nCommand == IDM_EDIT_GLOBAL_CONFIG);
			break;
		case IDM_MANAGE_OPEN_WITH_MENU:
			{
				CManageOpenWithDlg dlgManageOpenWithMenu;
				dlgManageOpenWithMenu.DoModal();
			}
			break;
		case IDM_SET_AS_DEFAULT_VIEWER:
			SetAsDefaultViewer();
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
			CleanupAndTerminate();
			break;
		case IDM_DEFAULT_ESC:
			if (m_bMovieMode) {
				if (m_bAutoExit)
					CleanupAndTerminate();
				else
					StopMovieMode(); // stop any running movie/slideshow
			} else if (m_bIsAnimationPlaying) {
				if (m_bAutoExit)
					CleanupAndTerminate();
				else
					StopAnimation(); // stop any running animation
			} else {
				CleanupAndTerminate();
			}
			break;
		case IDM_ZOOM_SEL:
			ZoomToSelection();
			break;
		case IDM_CROP_SEL:
			if (m_pCurrentImage != NULL) {
				m_pCurrentImage->Crop(m_pCropCtl->GetImageCropRect(false));
				m_pImageProcPanelCtl->ShowHideSaveDBButtons();
				this->Invalidate(FALSE);
				AdjustWindowToImage(false);
			}
			break;
		case IDM_LOSSLESS_CROP_SEL:
			m_pCropCtl->CropLossless();
			break;
		case IDM_COPY_SEL:
			if (m_pCurrentImage != NULL) {
				CClipboard::CopyFullImageToClipboard(this->m_hWnd, m_pCurrentImage, *m_pImageProcParams, 
					CreateDefaultProcessingFlags(),
					m_pCropCtl->GetImageCropRect(false), NULL);
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
		case IDM_CROPMODE_USER:
		{
			CSize userCrop = sp.UserCropAspectRatio();
			m_pCropCtl->SetCropRectAR(userCrop.cx / (double)userCrop.cy);
			break;
		}
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
				MouseOn();
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
		case IDM_CONTRAST_CORRECTION_INC:
		case IDM_CONTRAST_CORRECTION_DEC:
			if (m_bAutoContrast) {
				double dInc = (nCommand == IDM_CONTRAST_CORRECTION_INC) ? 0.05 : -0.05;
				m_pImageProcParams->ContrastCorrectionFactor = max(0.0, min(1.0, m_pImageProcParams->ContrastCorrectionFactor + dInc));
				this->Invalidate(FALSE);
			}
			break;
		case IDM_COLOR_CORRECTION_INC:
		case IDM_COLOR_CORRECTION_DEC:
			if (m_bAutoContrast) {
				double dInc = (nCommand == IDM_COLOR_CORRECTION_INC) ? 0.05 : -0.05;
				m_pImageProcParams->ColorCorrectionFactor = max(-0.5, min(0.5, m_pImageProcParams->ColorCorrectionFactor + dInc));
				this->Invalidate(FALSE);
			}
			break;
		case IDM_CONTRAST_INC:
		case IDM_CONTRAST_DEC:
			AdjustContrast((nCommand == IDM_CONTRAST_INC)? CONTRAST_INC : -CONTRAST_INC);
			break;
		case IDM_GAMMA_INC:
		case IDM_GAMMA_DEC:
			AdjustGamma((nCommand == IDM_GAMMA_INC)? 1.0/GAMMA_FACTOR : GAMMA_FACTOR);
			break;
		case IDM_LDC_SHADOWS_INC:	
		case IDM_LDC_SHADOWS_DEC:
		case IDM_LDC_HIGHLIGHTS_INC:
		case IDM_LDC_HIGHLIGHTS_DEC:
			AdjustLDC((nCommand == IDM_LDC_HIGHLIGHTS_INC || nCommand == IDM_LDC_HIGHLIGHTS_DEC) ? DARKEN_HIGHLIGHTS : BRIGHTEN_SHADOWS,
				(nCommand == IDM_LDC_SHADOWS_INC || nCommand == IDM_LDC_HIGHLIGHTS_INC) ? LDC_INC : -LDC_INC);
			break;
		case IDM_TOGGLE_RESAMPLING_QUALITY:
			m_bHQResampling = !m_bHQResampling;
			this->Invalidate(FALSE);
			break;
		case IDM_TOGGLE_MONITOR:
			ToggleMonitor();
			break;
		case IDM_EXCHANGE_PROC_PARAMS:
			ExchangeProcessingParams();
			break;
		case IDM_PAN_UP:
		case IDM_PAN_DOWN:
		case IDM_PAN_RIGHT:
		case IDM_PAN_LEFT:
			PerformPan((nCommand == IDM_PAN_LEFT) ? PAN_STEP : (nCommand == IDM_PAN_RIGHT) ? -PAN_STEP : 0,
				(nCommand == IDM_PAN_UP) ? PAN_STEP : (nCommand == IDM_PAN_DOWN) ? -PAN_STEP : 0, false);
			break;
		case IDM_SHARPEN_INC:
		case IDM_SHARPEN_DEC:
			AdjustSharpen((nCommand == IDM_SHARPEN_INC) ? SHARPEN_INC : -SHARPEN_INC);
			break;
		case IDM_CONTEXT_MENU:
			BOOL bNotUsed;
			OnContextMenu(0, 0, (m_nMouseX & 0xFFFF) | ((m_nMouseY & 0xFFFF) << 16), bNotUsed);
			break;
		case IDM_SET_WALLPAPER_ORIG:
			if (m_pFileList->Current() != NULL && m_pCurrentImage != NULL) {
				SetDesktopWallpaper::SetFileAsWallpaper(*m_pCurrentImage, m_pFileList->Current());
			}
			break;
		case IDM_SET_WALLPAPER_DISPLAY:
			if (m_pCurrentImage != NULL) {
				SetDesktopWallpaper::SetProcessedImageAsWallpaper(*m_pCurrentImage);
			}
			break;
	}
	if (nCommand >= IDM_FIRST_USER_CMD && nCommand <= IDM_LAST_USER_CMD) {
		ExecuteUserCommand(HelpersGUI::FindUserCommand(nCommand - IDM_FIRST_USER_CMD));
	}
	if (nCommand >= IDM_FIRST_OPENWITH_CMD && nCommand <= IDM_LAST_OPENWITH_CMD) {
		ExecuteUserCommand(HelpersGUI::FindOpenWithCommand(nCommand - IDM_FIRST_OPENWITH_CMD));
	}
}

bool CMainDlg::OpenFile(bool bFullScreen, bool bAfterStartup) {
	StopMovieMode();
	StopAnimation();
	MouseOn();
	CFileOpenDialog dlgOpen(this->m_hWnd, m_pFileList->Current(), CFileList::GetSupportedFileEndings(), bFullScreen);
	if (IDOK == dlgOpen.DoModal(this->m_hWnd)) {
		m_isBeforeFileSelected = false;
		OpenFile(dlgOpen.m_szFileName, bAfterStartup);
		return true;
	}
	m_isBeforeFileSelected = false;
	return false;
}

void CMainDlg::OpenFile(LPCTSTR sFileName, bool bAfterStartup) {
	// recreate file list based on image opened
	Helpers::ESorting eOldSorting = m_pFileList->GetSorting();
	bool oOldUpcounting = m_pFileList->IsSortedUpcounting();
	delete m_pFileList;
	m_sStartupFile = sFileName;
	m_pFileList = new CFileList(m_sStartupFile, *m_pDirectoryWatcher, eOldSorting, oOldUpcounting, CSettingsProvider::This().WrapAroundFolder());
	// free current image and all read ahead images
	InitParametersForNewImage();
	m_pJPEGProvider->NotifyNotUsed(m_pCurrentImage);
	m_pJPEGProvider->ClearAllRequests();
	m_pCurrentImage = m_pJPEGProvider->RequestImage(m_pFileList, CJPEGProvider::FORWARD,
		m_pFileList->Current(), 0, CreateProcessParams(!m_bFullScreenMode && (bAfterStartup || IsAdjustWindowToImage())), m_bOutOfMemoryLastImage);
	m_nLastLoadError = GetLoadErrorAfterOpenFile();
	if (bAfterStartup) CheckIfApplyAutoFitWndToImage(false);
	AfterNewImageLoaded(true, false);
	if (m_pCurrentImage != NULL && m_pCurrentImage->IsAnimation()) {
		StartAnimation();
	}
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
			Helpers::CReplacePipe(CString(_T("JPEG (*.jpg;*.jpeg)|*.jpg;*.jpeg|BMP (*.bmp)|*.bmp|PNG (*.png)|*.png|TIFF (*.tiff;*.tif)|*.tiff;*.tif|WEBP (*.webp)|*.webp|WEBP lossless (*.webp)|*.webp|")) +
			CNLS::GetString(_T("All Files")) + _T("|*.*|")), m_hWnd);
	if (sExtension.CompareNoCase(_T("bmp")) == 0) {
		fileDlg.m_ofn.nFilterIndex = 2;
	} else if (sExtension.CompareNoCase(_T("png")) == 0) {
		fileDlg.m_ofn.nFilterIndex = 3;
	} else if (sExtension.CompareNoCase(_T("tiff")) == 0 || sExtension.CompareNoCase(_T("tif")) == 0) {
		fileDlg.m_ofn.nFilterIndex = 4;
	} else if (sExtension.CompareNoCase(_T("webp")) == 0) {
		fileDlg.m_ofn.nFilterIndex = m_bUseLosslessWEBP ? 6 : 5;
	}
	if (!bFullSize) {
		fileDlg.m_ofn.lpstrTitle = CNLS::GetString(_T("Save as (in screen size/resolution)"));
	}
	if (IDOK == fileDlg.DoModal(m_hWnd)) {
		m_sSaveDirectory = fileDlg.m_szFileName;
		m_bUseLosslessWEBP = fileDlg.m_ofn.nFilterIndex == 6;
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
		CreateDefaultProcessingFlags(), bFullSize, m_bUseLosslessWEBP)) {
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

void CMainDlg::SetAsDefaultViewer() {
	if (m_bMovieMode) {
		return;
	}
	MouseOn();

	if (Helpers::GetWindowsVersion() >= 602) {
		// It is Windows 8 or later, launch the system provided assiciation setting dialog
		CFileExtensionsRegistrationWindows8 registry;
		if (registry.RegisterJPEGView()) {
			registry.LaunchApplicationAssociationDialog();
		} else {
			CString sError = CNLS::GetString(_T("Error while writing the following registry key:"));
			sError += _T("\n");
			sError += registry.GetLastFailedRegistryKey();
			::MessageBox(m_hWnd, sError, CNLS::GetString(_T("Error")), MB_OK | MB_ICONERROR);
		}
	} else {
		CFileExtensionsDlg dlgSetAsDefaultViewer;
		dlgSetAsDefaultViewer.DoModal();
	}
	Invalidate(FALSE);
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
			ExecuteUserCommand(*iter);
			return;
		}
	}
	// iterate over open with command list
	std::list<CUserCommand*> & openWithCmdList = CSettingsProvider::This().OpenWithCommandList();
	for (iter = openWithCmdList.begin( ); iter != openWithCmdList.end( ); iter++ ) {
		if ((*iter)->GetKeyCode() == virtualKeyCode) {
			ExecuteUserCommand(*iter);
			return;
		}
	}
}

void CMainDlg::ExecuteUserCommand(CUserCommand* pUserCommand) {
	MouseOn();
	LPCTSTR sCurrentFileName = CurrentFileName(false);
	if (pUserCommand != NULL && pUserCommand->CanExecute(this->m_hWnd, sCurrentFileName)) {
		bool bReloadCurrent = false;
		// First go to next, then execute the command and reload current file.
		// Otherwise we get into troubles when the current image was deleted or moved.
		CFileList* oldFileList = m_pFileList;
		if (pUserCommand->MoveToNextAfterCommand()) {
			// don't send a request if we reload all, it would be canceled anyway later
			GotoImage(POS_AwayFromCurrent, pUserCommand->NeedsReloadAll() ? NO_REQUEST : 0);
		}
		if (pUserCommand->Execute(this->m_hWnd, sCurrentFileName)) {
			if (pUserCommand->NeedsReloadAll() || pUserCommand->NeedsReloadFileList()) {
				m_pFileList->Reload(NULL, false);
				if (oldFileList != m_pFileList) {
					// If GotoImage(POS_AwayFromCurrent) changed the file list, the old file list also needs to be reloaded
					oldFileList->Reload(NULL, false);
				}
				m_pFileList->DeleteHistory(true);
			}
			if (pUserCommand->NeedsReloadAll()) {
				m_pJPEGProvider->NotifyNotUsed(m_pCurrentImage);
				m_pJPEGProvider->ClearAllRequests();
				m_pCurrentImage = NULL;
				bReloadCurrent = true;
			} else {
				if (pUserCommand->NeedsReloadFileList()) {
					bReloadCurrent = m_pFileList->Current() == NULL; // needs "reload" in this case
					Invalidate();
				}
				if (pUserCommand->NeedsReloadCurrent() && !pUserCommand->MoveToNextAfterCommand()) {
					bReloadCurrent = true;
				}
			}
			if (bReloadCurrent) {
				ReloadImage(false);
			}
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
	if (!m_bDoDragging && !m_pZoomNavigatorCtl->IsPointInZoomNavigatorThumbnail(CPoint(m_nMouseX, m_nMouseY))) {
		if (!GetNavPanelCtl()->IsVisible()) {
			MouseOn();
			GetNavPanelCtl()->ShowNavPanelTemporary();
		} else {
			GetNavPanelCtl()->HideNavPanelTemporary();
		}
	}
	m_bDragging = false;
	m_bDoDragging = false;
	m_pZoomNavigatorCtl->EndDragging();
	if (m_pCurrentImage != NULL) {
		m_pCurrentImage->VerifyDIBPixelsCreated();
	}
	if (m_DIBOffsets != CPoint(0, 0)) {
		Invalidate(FALSE);
	} else {
		this->InvalidateRect(m_pNavPanelCtl->PanelRect(), FALSE);
	}
	SetCursorForMoveSection();
}

void CMainDlg::GotoImage(EImagePosition ePos) {
	GotoImage(ePos, 0);
}

void CMainDlg::GotoImage(EImagePosition ePos, int nFlags) {
	// Timer handling for slideshows
	if (ePos == POS_Next || ePos == POS_NextSlideShow) {
		if (m_nCurrentTimeout > 0) {
			StartSlideShowTimer(m_nCurrentTimeout);
		}
		StopAnimation();
	} else if (ePos != POS_NextAnimation) {
		StopMovieMode();
		StopAnimation();
	}

	m_pCropCtl->CancelCropping(); // cancel any running crop

	int nFrameIndex = 0;
	bool bCheckIfSameImage = true;
	m_pFileList->SetCheckpoint();
	CFileList* pOldFileList = m_pFileList;
	int nOldFrameIndex = (m_pCurrentImage == NULL) ? 0 : m_pCurrentImage->FrameIndex();
	CJPEGProvider::EReadAheadDirection eDirection = CJPEGProvider::FORWARD;
	switch (ePos) {
		case POS_First:
			m_pFileList->First();
			break;
		case POS_Last:
			m_pFileList->Last();
			break;
		case POS_Next:
		case POS_NextAnimation:
			{
				bool bGotoNextImage = true;
				nFrameIndex = Helpers::GetFrameIndex(m_pCurrentImage, true, ePos == POS_NextAnimation,  bGotoNextImage);
				if (bGotoNextImage) m_pFileList = m_pFileList->Next();
				break;
			}
		case POS_NextSlideShow:
			m_pFileList = m_pFileList->Next();
			break;
		case POS_Previous:
			{
				bool bGotoPrevImage;
				nFrameIndex = Helpers::GetFrameIndex(m_pCurrentImage, false, false, bGotoPrevImage);
				if (bGotoPrevImage) m_pFileList = m_pFileList->Prev();
				eDirection = CJPEGProvider::BACKWARD;
				break;
			}
		case POS_Toggle:
			m_pFileList->ToggleBetweenMarkedAndCurrentFile();
			eDirection = CJPEGProvider::TOGGLE;
			break;
		case POS_Current:
		case POS_Clipboard:
			bCheckIfSameImage = false; // do something even when not moving iterator on filelist
			break;
		case POS_AwayFromCurrent:
			m_pFileList = m_pFileList->AwayFromCurrent();
			break;
	}

	if (bCheckIfSameImage && (m_pFileList == pOldFileList && nOldFrameIndex == nFrameIndex && !m_pFileList->ChangedSinceCheckpoint())) {
		if (m_bMovieMode && m_bAutoExit)
			CleanupAndTerminate();
		else
			return; // not placed on a new image, don't do anything
	}

	if (ePos != POS_Current && ePos != POS_NextAnimation && ePos != POS_Clipboard && ePos != POS_AwayFromCurrent) {
		MouseOff();
	}

	if (nFlags & KEEP_PARAMETERS) {
		if (!(m_bUserZoom || IsAdjustWindowToImage())) {
			m_dZoom = -1;
		}
	} else {
		InitParametersForNewImage();
	}
	m_pJPEGProvider->NotifyNotUsed(m_pCurrentImage);
	if (ePos == POS_Current || ePos == POS_AwayFromCurrent) {
		m_pJPEGProvider->ClearRequest(m_pCurrentImage, ePos == POS_AwayFromCurrent);
	}
	m_pCurrentImage = NULL;

	// do not perform a new image request if flagged
	if (nFlags & NO_REQUEST) {
		return;
	}

	CProcessParams procParams = CreateProcessParams(false);
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
		m_pCurrentImage = m_pJPEGProvider->RequestImage(m_pFileList, (ePos == POS_AwayFromCurrent) ? CJPEGProvider::NONE : eDirection,  
			m_pFileList->Current(), nFrameIndex, procParams, m_bOutOfMemoryLastImage);
		m_nLastLoadError = (m_pCurrentImage == NULL) ? ((m_pFileList->Current() == NULL) ? HelpersGUI::FileLoad_NoFilesInDirectory : HelpersGUI::FileLoad_LoadError) : HelpersGUI::FileLoad_Ok;
	}
	AfterNewImageLoaded((nFlags & KEEP_PARAMETERS) == 0, false);

	// if it is an animation (currently only animated GIF) start movie automatically
	if (m_pCurrentImage != NULL && m_pCurrentImage->IsAnimation()) {
		if (m_bIsAnimationPlaying) {
			AdjustAnimationFrameTime();
		} else {
			StartAnimation();
		}
	}

	// Sleep if a minimal display time is defined
	double currentTime = Helpers::GetExactTickCount();
	if (ePos == POS_Next || ePos == POS_Previous) {
		double imageTime = currentTime - m_dLastImageDisplayTime;
		double minimalTime = CSettingsProvider::This().MinimalDisplayTime();
		if (minimalTime > 0 && (imageTime < minimalTime)) ::Sleep((int)(minimalTime - imageTime));
	}
	m_dLastImageDisplayTime = Helpers::GetExactTickCount();

	if (((nFlags & NO_UPDATE_WINDOW) == 0) && !(ePos == POS_NextSlideShow && UseSlideShowTransitionEffect())) {
		this->Invalidate(FALSE);
		// this will force to wait until really redrawn, preventing to process images but do not show them
		this->UpdateWindow();
	}

	// remove key messages accumulated so far
	if (!(nFlags & NO_REMOVE_KEY_MSG)) {
		MSG msg;
		while (::PeekMessage(&msg, this->m_hWnd, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE));
	}
}

void CMainDlg::ReloadImage(bool keepParameters, bool updateWindow) {
	int flags = keepParameters ? KEEP_PARAMETERS : 0;
	GotoImage(CMainDlg::POS_Current, updateWindow ? flags : flags | NO_UPDATE_WINDOW);
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

void CMainDlg::PerformZoom(double dValue, bool bExponent, bool bZoomToMouse, bool bAdjustWindowToImage) {
	double dOldZoom = m_dZoom;
	m_bUserZoom = true;
	m_isUserFitToScreen = false;
	if (bExponent) {
		m_dZoom = m_dZoom * pow(m_dZoomMult, dValue);
	} else {
		m_dZoom = dValue;
	}

	if (m_pCurrentImage == NULL) {
		m_dZoom = max(Helpers::ZoomMin, min(Helpers::ZoomMax, m_dZoom));
		return;
	}

	double dZoomMin = max(0.0001, min(Helpers::ZoomMin, GetZoomFactorForFitToScreen(false, false) * 0.5));
	m_dZoom = max(dZoomMin, min(Helpers::ZoomMax, m_dZoom));

	if (abs(m_dZoom - 1.0) < 0.01) {
		m_dZoom = 1.0;
	}
	if ((dOldZoom - 1.0)*(m_dZoom - 1.0) <= 0 && m_bInZooming && !m_bZoomMode) {
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
		int nCenterX = m_bZoomMode ? m_nCapturedX : m_nMouseX;
		int nCenterY = m_bZoomMode ? m_nCapturedY : m_nMouseY;
		int nOldX = nOldXSize/2 - m_clientRect.Width()/2 + nCenterX - m_offsets.x;
		int nOldY = nOldYSize/2 - m_clientRect.Height()/2 + nCenterY - m_offsets.y;
		double dFac = m_dZoom/dOldZoom;
		m_offsets.x = Helpers::RoundToInt(nNewXSize/2 - m_clientRect.Width()/2 + nCenterX - nOldX*dFac);
		m_offsets.y = Helpers::RoundToInt(nNewYSize/2 - m_clientRect.Height()/2 + nCenterY - nOldY*dFac);
	} else {
		// zoom to center
		m_offsets.x = (int) (m_offsets.x*m_dZoom/dOldZoom);
		m_offsets.y = (int) (m_offsets.y*m_dZoom/dOldZoom);
	}

	m_bInZooming = true;
	StartLowQTimer(ZOOM_TIMEOUT);
	if (fabs(dOldZoom - m_dZoom) > 0.0001 || m_bZoomMode) {
		this->Invalidate(FALSE);
		if (bAdjustWindowToImage) {
			AdjustWindowToImage(false);
		}
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
	CRect zoomRect(m_pCropCtl->GetImageCropRect(false));
	if (zoomRect.Width() > 0 && zoomRect.Height() > 0 && m_pCurrentImage != NULL) {
		float fZoom;
		CPoint offsets;
		Helpers::GetZoomParameters(fZoom, offsets, m_pCurrentImage->OrigSize(), m_clientRect.Size(), zoomRect);
		if (fZoom > 0) {
			PerformZoom(fZoom, false, m_bMouseOn, false);
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
		return max(0.0001, min(dZoomMax, dZoom));
	} else {
		return 1.0;
	}
}

void CMainDlg::ResetZoomToFitScreen(bool bFillWithCrop, bool bAllowEnlarge, bool bAdjustWindowSize) {
	m_isUserFitToScreen = false;
	if (m_pCurrentImage != NULL) {
		if (bAdjustWindowSize && !m_bFullScreenMode && !IsZoomed() && m_bAutoFitWndToImage) {
			m_dZoom = bAllowEnlarge ? Helpers::ZoomMax : 1;
			CRect wndRect = Helpers::GetWindowRectMatchingImageSize(m_hWnd, CSize(MIN_WND_WIDTH, MIN_WND_HEIGHT), HUGE_SIZE, m_dZoom, m_pCurrentImage, false, true);
			if (m_dZoom <= 1) {
				m_dZoom = -1;
			}
			m_bResizeForNewImage = true;
			this->SetWindowPos(HWND_TOP, wndRect.left, wndRect.top, wndRect.Width(), wndRect.Height(), SWP_NOZORDER | SWP_NOCOPYBITS);
			this->GetClientRect(&m_clientRect);
			m_bResizeForNewImage = false;
			this->Invalidate(FALSE);
		} else {
			double dOldZoom = m_dZoom;
			m_dZoom = GetZoomFactorForFitToScreen(bFillWithCrop, bAllowEnlarge);
			if (bAllowEnlarge && !IsAdjustWindowToImage()) {
				m_isUserFitToScreen = true;
				m_dZoom = -1;
				m_autoZoomFitToScreen = bFillWithCrop ? Helpers::ZM_FillScreen : Helpers::ZM_FitToScreen;
			}
			m_bUserZoom = m_dZoom > 1.0;
			m_bUserPan = false;
			if (fabs(dOldZoom - m_dZoom) > 0.01) {
				this->Invalidate(FALSE);
			}
		}
	}
}

void CMainDlg::ResetZoomTo100Percents(bool bZoomToMouse) {
	if (m_pCurrentImage != NULL && fabs(m_dZoom - 1) > 0.01) {
		PerformZoom(1.0, false, bZoomToMouse, true);
	}
}

CProcessParams CMainDlg::CreateProcessParams(bool bNoProcessingAfterLoad) {
	int nClientWidth = m_clientRect.Width();
	int nClientHeight = m_clientRect.Height();
	if (nClientWidth == 0 && nClientHeight == 0) {
		CRect rect = CMultiMonitorSupport::GetMonitorRect(m_hWnd);
		nClientWidth = rect.Width();
		nClientHeight = rect.Height();
	}
	Helpers::EAutoZoomMode eAutoZoomMode = GetAutoZoomMode();
	if (IsAdjustWindowToImage() && !bNoProcessingAfterLoad) {
		CSize maxClientSize = Helpers::GetMaxClientSize(m_hWnd);
		nClientWidth = maxClientSize.cx;
		nClientHeight = maxClientSize.cy;
		eAutoZoomMode = Helpers::ZM_FitToScreenNoZoom;
	}
	if (m_bKeepParams) {
		double dOldLightenShadows = m_pImageProcParamsKept->LightenShadows;
		*m_pImageProcParamsKept = *m_pImageProcParams;
		// when last image was detected as sunset or nightshot, do not take this value for next image
		if (m_bCurrentImageIsSpecialProcessing && m_pImageProcParams->LightenShadows == m_dCurrentInitialLightenShadows) {
			m_pImageProcParamsKept->LightenShadows = dOldLightenShadows;
		}
		m_eProcessingFlagsKept = CreateProcessingFlags(m_bHQResampling, m_bAutoContrast, false, m_bLDC, m_bKeepParams, m_bLandscapeMode);
		m_nRotationKept = m_nRotation;
		m_dZoomKept = (m_bUserZoom || IsAdjustWindowToImage()) ? m_dZoom : -1;
		m_offsetKept = m_bUserPan ? m_offsets : CPoint(0, 0);
		if (m_isUserFitToScreen && !IsAdjustWindowToImage()) { eAutoZoomMode = m_autoZoomFitToScreen; }

		return CProcessParams(nClientWidth, nClientHeight,
			CMultiMonitorSupport::GetMonitorRect(m_hWnd).Size(),
			CRotationParams(m_nRotationKept), 
			m_dZoomKept,
			eAutoZoomMode,
			m_offsetKept,
			_SetLandscapeModeParams(m_bLandscapeMode, *m_pImageProcParamsKept), 
			SetProcessingFlag(_SetLandscapeModeFlags(m_eProcessingFlagsKept), PFLAG_NoProcessingAfterLoad, bNoProcessingAfterLoad));
	} else {
		m_isUserFitToScreen = false;
		CSettingsProvider& sp = CSettingsProvider::This();
		return CProcessParams(nClientWidth, nClientHeight, 
			CMultiMonitorSupport::GetMonitorRect(m_hWnd).Size(),
			CRotationParams(0), -1, eAutoZoomMode, CPoint(0, 0),
			_SetLandscapeModeParams(m_bLandscapeMode, GetDefaultProcessingParams()),
			SetProcessingFlag(_SetLandscapeModeFlags(GetDefaultProcessingFlags(m_bLandscapeMode)), PFLAG_NoProcessingAfterLoad, bNoProcessingAfterLoad));
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
	m_nLastSlideShowImageTickCount = ::GetTickCount();
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
		m_eProcFlagsBeforeMovie = CreateDefaultProcessingFlags(m_bKeepParams);
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
	AfterNewImageLoaded(false, false);
	Invalidate(FALSE);
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
		AfterNewImageLoaded(false, false);
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
	} else if (!(m_bUserZoom || IsAdjustWindowToImage())) {
		m_dZoom = -1;
	}

	m_bAutoContrastSection = false;
	*m_pImageProcParams2 = GetNoProcessingParams();
	m_eProcessingFlags2 = PFLAG_HighQualityResampling;

	if (!m_bIsAnimationPlaying) {
		m_bInZooming = m_bTemporaryLowQ = m_bShowZoomFactor = false;
	}
}

void CMainDlg::ExchangeProcessingParams() {
	bool bOldAutoContrastSection = m_bAutoContrastSection;
	CImageProcessingParams tempParams = *m_pImageProcParams;
	EProcessingFlags tempFlags = CreateDefaultProcessingFlags();
	*m_pImageProcParams = *m_pImageProcParams2;
	InitFromProcessingFlags(m_eProcessingFlags2, m_bHQResampling, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, m_bLandscapeMode);
	*m_pImageProcParams2 = tempParams;
	m_eProcessingFlags2 = tempFlags;
	m_bAutoContrastSection = bOldAutoContrastSection; // this flag cannot be exchanged this way
	this->Invalidate(FALSE);
}

void CMainDlg::AfterNewImageLoaded(bool bSynchronize, bool bAfterStartup) {
	UpdateWindowTitle();
	m_pDirectoryWatcher->SetCurrentFile(CurrentFileName(false));
	if (!m_bIsAnimationPlaying) m_pNavPanelCtl->HideNavPanelTemporary();
	m_pPanelMgr->AfterNewImageLoaded();
	m_pCropCtl->AbortCropping();
	m_pPrintImage->ClearOffsets();
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
		if (!bAfterStartup && !m_bIsAnimationPlaying) {
			AdjustWindowToImage(false);
		}
	}
}

bool CMainDlg::IsAdjustWindowToImage() {
	return !m_bFullScreenMode && !::IsZoomed(m_hWnd) && m_bAutoFitWndToImage;
}

bool CMainDlg::IsImageExactlyFittingWindow() {
	return (m_pCurrentImage != NULL) && m_pCurrentImage->DIBWidth() == m_clientRect.Width() && m_pCurrentImage->DIBHeight() == m_clientRect.Height();
}

void CMainDlg::AdjustWindowToImage(bool bAfterStartup) {
	if (IsAdjustWindowToImage() && (m_pCurrentImage != NULL || bAfterStartup)) {
		// window size shall be adjusted to image size (at least keep aspect ratio)
		double dZoom = m_dZoom;
		CRect windowRect = Helpers::GetWindowRectMatchingImageSize(m_hWnd, CSize(MIN_WND_WIDTH, MIN_WND_HEIGHT), HUGE_SIZE, dZoom, m_pCurrentImage, bAfterStartup, dZoom < 0);
		CRect defaultRect = CMultiMonitorSupport::GetDefaultWindowRect();
		if (bAfterStartup && CSettingsProvider::This().ExplicitWindowRect()) {
			windowRect = CRect(defaultRect.TopLeft(), windowRect.Size());
		}
		m_bResizeForNewImage = true;
		this->Invalidate(FALSE);
		this->SetWindowPos(HWND_TOP, windowRect.left, windowRect.top, windowRect.Width(), windowRect.Height(), SWP_NOZORDER | SWP_NOCOPYBITS);
		this->GetClientRect(&m_clientRect);
		m_bResizeForNewImage = false;
	}
}

void CMainDlg::CheckIfApplyAutoFitWndToImage(bool bInInitDialog) {
	if (CSettingsProvider::This().AutoFullScreen()) {
		if (!Helpers::CanDisplayImageWithoutResize(m_hWnd, m_pCurrentImage)) {
			if (!bInInitDialog) {
				ExecuteCommand(IDM_FULL_SCREEN_MODE);
			}
			m_bFullScreenMode = true;
			m_bTemporaryLowQ = false;
			MouseOff();
		} else {
			m_bAutoFitWndToImage = true;
		}
	}
}

void CMainDlg::SaveParameters() {
	if (m_bMovieMode) {
		return;
	}

	EProcessingFlags eFlags = CreateDefaultProcessingFlags(m_bKeepParams);
	CString sText = HelpersGUI::GetINIFileSaveConfirmationText(*m_pImageProcParams, eFlags,
		m_pFileList->GetSorting(), m_pFileList->IsSortedUpcounting(), GetAutoZoomMode(), m_pNavPanelCtl->IsActive());

	if (IDYES == this->MessageBox(sText, CNLS::GetString(_T("Confirm save default parameters")), MB_YESNO | MB_ICONQUESTION)) {
		CSettingsProvider::This().SaveSettings(*m_pImageProcParams, eFlags, m_pFileList->GetSorting(), m_pFileList->IsSortedUpcounting(),
			m_eAutoZoomModeWindowed, m_eAutoZoomModeFullscreen, m_pNavPanelCtl->IsActive());
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
	if (!m_pCropCtl->IsCropping()) {
		if (m_pZoomNavigatorCtl->IsPointInZoomNavigatorThumbnail(CPoint(m_nMouseX, m_nMouseY)) || m_bDragging) {
			::SetCursor(::LoadCursor(NULL, MAKEINTRESOURCE(IDC_SIZEALL)));
			m_bPanMouseCursorSet = true;
		} else if (m_bPanMouseCursorSet) {
			::SetCursor(::LoadCursor(NULL, MAKEINTRESOURCE(IDC_ARROW)));
			m_bPanMouseCursorSet = false;
		}
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
	int nZoomTextRectBottomOffset = (m_clientRect.Width() < HelpersGUI::ScaleToScreen(800)) ? 25 : ZOOM_TEXT_RECT_OFFSET;
	int nStartX = imageProcessingArea.right - HelpersGUI::ScaleToScreen(ZOOM_TEXT_RECT_WIDTH + ZOOM_TEXT_RECT_OFFSET);
	if (m_pImageProcPanelCtl->IsVisible()) {
		nStartX = max(nStartX, m_pImageProcPanelCtl->GetUnsharpMaskButtonRect().right);
	}
	int nEndX = nStartX + HelpersGUI::ScaleToScreen(ZOOM_TEXT_RECT_WIDTH);
	if (m_pImageProcPanelCtl->IsVisible()) {
		nEndX = min(nEndX, imageProcessingArea.right - 2);
	}
	return CRect(nStartX, 
		imageProcessingArea.bottom - HelpersGUI::ScaleToScreen(ZOOM_TEXT_RECT_HEIGHT + nZoomTextRectBottomOffset), 
		nEndX, imageProcessingArea.bottom - HelpersGUI::ScaleToScreen(nZoomTextRectBottomOffset));
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
		CString sWindowText =  sCurrentFileName;
		sWindowText += Helpers::GetMultiframeIndex(m_pCurrentImage);
		if (CSettingsProvider::This().ShowEXIFDateInTitle()) {
			CEXIFReader* pEXIF = m_pCurrentImage->GetEXIFReader();
			if (pEXIF != NULL && pEXIF->GetAcquisitionTime().wYear > 1600) {
				sWindowText += " - " + Helpers::SystemTimeToString(pEXIF->GetAcquisitionTime());
			}
		}
		sWindowText += _T(" - JPEGView");
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
	StopAnimation();
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

void CMainDlg::PrefetchDIB(const CRect& clientRect) {
	if (m_pCurrentImage == NULL) return;

	CSize newSize = Helpers::GetVirtualImageSize(m_pCurrentImage->OrigSize(), clientRect.Size(), GetAutoZoomMode(), m_dZoom);
	CSize clippedSize(min(clientRect.Width(), newSize.cx), min(clientRect.Height(), newSize.cy));
	CPoint offsetsInImage = m_pCurrentImage->ConvertOffset(newSize, clippedSize, Helpers::LimitOffsets(m_offsets, clientRect.Size(), newSize));
	m_pCurrentImage->GetDIB(newSize, clippedSize, offsetsInImage, *m_pImageProcParams, CreateDefaultProcessingFlags());
}

double CMainDlg::GetZoomMultiplier(CJPEGImage* pImage, const CRect& clientRect) {
	double dZoomToFit;
	CSize fittedSize = Helpers::GetImageRect(pImage->OrigWidth(), pImage->OrigHeight(), 
		clientRect.Width(), clientRect.Height(), true, false, false, dZoomToFit);
	// Zoom multiplier (zoom step) should be around 1.1 but reach the value 1.0 after an integral number
	// of zooming steps
	int n = Helpers::RoundToInt(log(1/dZoomToFit)/log(1.1));
	return (n == 0) ? 1.1 : exp(log(1/dZoomToFit)/n);
}

EProcessingFlags CMainDlg::CreateDefaultProcessingFlags(bool bKeepParams) {
	return CreateProcessingFlags(m_bHQResampling, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, bKeepParams, m_bLandscapeMode);
}

bool CMainDlg::HandleMouseButtonByKeymap(int nMouseButtonCode, bool bExecuteCommand) {
	if (m_pPanelMgr->IsModalPanelShown()) {
		return false;
	}
	bool bAlt = (::GetKeyState(VK_MENU) & 0x8000) != 0;
	bool bCtrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
	bool bShift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
	int nCommand = m_pKeyMap->GetCommandIdForKey(nMouseButtonCode, bAlt, bCtrl, bShift);
	if (nCommand > 0) {
		if (bExecuteCommand) {
			ExecuteCommand(nCommand);
		}
		return true;
	}
	return false;
}

bool CMainDlg::UseSlideShowTransitionEffect() {
	return m_bFullScreenMode && m_nCurrentTimeout >= 1000 && m_eTransitionEffect != Helpers::TE_None;
}

void CMainDlg::AnimateTransition() {

	int nFrameTimeMs = (m_eTransitionEffect >= Helpers::TE_RollLR || m_eTransitionEffect <= Helpers::TE_ScrollBT) ? 10 : 20;

	// paint to memory DC
	int nW = m_clientRect.Width(), nH = m_clientRect.Height();
	CDC paintDC(::GetDC(m_hWnd));
	CDC memDC;
	memDC.CreateCompatibleDC(paintDC);
	CBitmap memDCBitmap;
	memDCBitmap.CreateCompatibleBitmap(paintDC, nW, nH);
	memDC.SelectBitmap(memDCBitmap);
	PaintToDC(memDC);

	int nSteps = max(1, (m_nTransitionTime + 20) / nFrameTimeMs);

	BLENDFUNCTION blendFunc;
	memset(&blendFunc, 0, sizeof(blendFunc));
	blendFunc.BlendOp = AC_SRC_OVER;
	blendFunc.AlphaFormat = 0;
	float fAlphaStep = 255.0f / nSteps;

	DWORD lastTime = ::GetTickCount();
	for (int i = 0; i <= nSteps; i++) {
		switch (m_eTransitionEffect)
		{
		case Helpers::TE_Blend:
			{
				if (i == nSteps) {
					paintDC.BitBlt(0, 0, nW, nH, memDC, 0, 0, SRCCOPY);
				} else {
					float fFactor = (float)i / nSteps ;
					blendFunc.SourceConstantAlpha = min(255, (int)((fFactor * fFactor * i + 1) * fAlphaStep + 0.5f));
					paintDC.AlphaBlend(0, 0, nW, nH, memDC, 0, 0, nW, nH, blendFunc);
				}
				break;
			}
		case Helpers::TE_SlideLR:
		case Helpers::TE_SlideRL:
		case Helpers::TE_SlideBT:
		case Helpers::TE_SlideTB:
			{
				float fFactor = (float)(i + 1) / (nSteps + 1);
				int nFracHeight = (int)(nH * fFactor + 0.5f);
				int nFracWidth = (int)(nW * fFactor + 0.5f);
				int nStartX = (m_eTransitionEffect == Helpers::TE_SlideLR) ? nFracWidth - nW : (m_eTransitionEffect == Helpers::TE_SlideRL) ? nW - nFracWidth : 0;
				int nStartY = (m_eTransitionEffect == Helpers::TE_SlideTB) ? nFracHeight - nH : (m_eTransitionEffect == Helpers::TE_SlideBT) ? nH - nFracHeight : 0;
				paintDC.BitBlt(nStartX, nStartY, nW, nH, memDC, 0, 0, SRCCOPY);
				break;
			}
		case Helpers::TE_RollLR:
		case Helpers::TE_RollRL:
		case Helpers::TE_RollBT:
		case Helpers::TE_RollTB:
			{
				float fFactor = (float)(i + 1) / (nSteps + 1);
				int nFracHeight = (int)(nH * fFactor + 0.5f);
				int nClipH = (m_eTransitionEffect == Helpers::TE_RollLR || m_eTransitionEffect == Helpers::TE_RollRL) ? nH : 1 + nH / (nSteps + 1);
				int nFracWidth = (int)(nW * fFactor + 0.5f);
				int nClipW = (m_eTransitionEffect == Helpers::TE_RollTB || m_eTransitionEffect == Helpers::TE_RollBT) ? nW : 1 + nW / (nSteps + 1);
				int nClipX = (m_eTransitionEffect == Helpers::TE_RollLR) ? nFracWidth - nClipW : (m_eTransitionEffect == Helpers::TE_RollRL) ? nW - nFracWidth : 0;
				int nClipY = (m_eTransitionEffect == Helpers::TE_RollTB) ? nFracHeight - nClipH : (m_eTransitionEffect == Helpers::TE_RollBT) ? nH - nFracHeight : 0;
				CRgn region;
				region.CreateRectRgn(nClipX, nClipY, nClipX + nClipW, nClipY + nClipH);
				paintDC.SelectClipRgn(region);
				paintDC.BitBlt(0, 0, nW, nH, memDC, 0, 0, SRCCOPY);
				break;
			}
		case Helpers::TE_ScrollLR:
		case Helpers::TE_ScrollRL:
		case Helpers::TE_ScrollBT:
		case Helpers::TE_ScrollTB:
			{
				float fFactorLast = (float)i / (nSteps + 1);
				float fFactor = (float)(i + 1) / (nSteps + 1);
				int nScrollW, nScrollH;
				int nStartX, nStartY;
				int nStartClipX, nStartClipY, nEndClipX, nEndClipY;
				if (m_eTransitionEffect == Helpers::TE_ScrollLR) {
					nScrollW = (int)(nW * fFactor + 0.5f) - (int)(nW * fFactorLast + 0.5f);
					nScrollH = 0;
					nStartX = (int)(nW * fFactor + 0.5f) - nW;
					nStartY = 0;
					nStartClipX = nStartClipY = 0;
					nEndClipX = nScrollW; nEndClipY = nH;
				} else if (m_eTransitionEffect == Helpers::TE_ScrollRL) {
					nScrollW = - ((int)(nW * fFactor + 0.5f) - (int)(nW * fFactorLast + 0.5f));
					nScrollH = 0;
					nStartX = nW - (int)(nW * fFactor + 0.5f);
					nStartY = 0;
					nStartClipX = nW + nScrollW;
					nStartClipY = 0;
					nEndClipX = nW; nEndClipY = nH;
				} else if (m_eTransitionEffect == Helpers::TE_ScrollTB) {
					nScrollW = 0;
					nScrollH = (int)(nH * fFactor + 0.5f) - (int)(nH * fFactorLast + 0.5f);
					nStartX = 0;
					nStartY = (int)(nH * fFactor + 0.5f) - nH;
					nStartClipX = 0; nStartClipY = 0;
					nEndClipX = nW; nEndClipY = nScrollH;
				} else {
					nScrollW = 0;
					nScrollH = -((int)(nH * fFactor + 0.5f) - (int)(nH * fFactorLast + 0.5f));
					nStartX = 0;
					nStartY = nH - (int)(nH * fFactor + 0.5f);
					nStartClipX = 0; nStartClipY = nH + nScrollH;
					nEndClipX = nW; nEndClipY = nH;
				}

				paintDC.SelectClipRgn(NULL);
				paintDC.BitBlt(nScrollW, nScrollH, nW, nH, paintDC, 0, 0, SRCCOPY);
				
				CRgn region;
				region.CreateRectRgn(nStartClipX, nStartClipY, nEndClipX, nEndClipY);
				paintDC.SelectClipRgn(region);

				paintDC.BitBlt(nStartX, nStartY, nW, nH, memDC, 0, 0, SRCCOPY);
				break;
			}
		}
		DWORD time = ::GetTickCount();
		if (time - lastTime < nFrameTimeMs) {
			::Sleep(nFrameTimeMs - (time - lastTime));
		}
		lastTime = time;
		
		// terminate if a key is pressed or context menu shall be shown
		MSG msg;
		if (::PeekMessage(&msg, m_hWnd, WM_KEYFIRST, WM_KEYLAST, PM_NOREMOVE)) break;
		if (::PeekMessage(&msg, m_hWnd, WM_CONTEXTMENU, WM_CONTEXTMENU, PM_NOREMOVE)) break;
	}
}

void CMainDlg::CleanupAndTerminate() {
	StopMovieMode();
	StopAnimation();
	delete m_pJPEGProvider; // delete this early to properly shut down the loading threads
	m_pJPEGProvider = NULL;
	EndDialog(0);
}
	
void CMainDlg::StartAnimation() {
	if (m_bIsAnimationPlaying) {
		return;
	}
	m_eProcFlagsBeforeMovie = CreateDefaultProcessingFlags(m_bKeepParams);
	m_bKeepParametersBeforeAnimation = IsKeepParams();
	if (!m_bKeepParametersBeforeAnimation) {
		ExecuteCommand(IDM_KEEP_PARAMETERS);
	}
	m_bAutoContrastSection = false;
	m_bAutoContrast = false;
	m_bLDC = false;
	m_bLandscapeMode = false;
	m_bIsAnimationPlaying = true;
	::SetTimer(this->m_hWnd, ANIMATION_TIMER_EVENT_ID, max(10, m_pCurrentImage->FrameTimeMs()), NULL);
	m_pNavPanelCtl->EndNavPanelAnimation();
	m_nLastSlideShowImageTickCount = ::GetTickCount();
}

void CMainDlg::AdjustAnimationFrameTime() {
	// restart timer with new frame time
	::KillTimer(this->m_hWnd, ANIMATION_TIMER_EVENT_ID);
	::SetTimer(this->m_hWnd, ANIMATION_TIMER_EVENT_ID, max(10, m_pCurrentImage->FrameTimeMs()), NULL);
}

void CMainDlg::StopAnimation() {
	if (!m_bIsAnimationPlaying) {
		return;
	}
	if (IsKeepParams() != m_bKeepParametersBeforeAnimation) {
		ExecuteCommand(IDM_KEEP_PARAMETERS);
	}
	if (GetProcessingFlag(m_eProcFlagsBeforeMovie, PFLAG_AutoContrast)) {
		m_bAutoContrast = true;
	}
	if (GetProcessingFlag(m_eProcFlagsBeforeMovie, PFLAG_LDC)) {
		m_bLDC = true;
	}
	if (GetProcessingFlag(m_eProcFlagsBeforeMovie, PFLAG_LandscapeMode)) {
		m_bLandscapeMode = true;
	}
	::KillTimer(this->m_hWnd, ANIMATION_TIMER_EVENT_ID);
	m_bIsAnimationPlaying = false;
}
