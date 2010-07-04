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
#include "HelpDisplay.h"
#include "Clipboard.h"
#include "FileOpenDialog.h"
#include "GUIControls.h"
#include "ParameterDB.h"
#include "SaveImage.h"
#include "NLS.h"
#include "BatchCopyDlg.h"
#include "ResizeFilter.h"
#include "EXIFReader.h"
#include "EXIFDisplay.h"
#include "AboutDlg.h"
#include "CropSizeDlg.h"
#include "ProcessingThreadPool.h"
#include "ZoomNavigator.h"
#include "PaintMemDCMgr.h"

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
static const int AUTOSCROLL_TIMER_EVENT_ID = 4; // when cropping, auto scroll timer ID
static const int NAVPANEL_TIMER_EVENT_ID = 5; // to show nav panel
static const int NAVPANEL_ANI_TIMER_EVENT_ID = 6; // animation timer for navigation panel
static const int NAVPANEL_START_ANI_TIMER_EVENT_ID = 7; // animation start timer for navigation panel
static const int IPPANEL_TIMER_EVENT_ID = 8; // to show image processing panel in window mode
static const int ZOOM_TIMEOUT = 200; // refinement done after this many milliseconds
static const int ZOOM_TEXT_TIMEOUT = 1000; // zoom label disappears after this many milliseconds
static const int AUTOSCROLL_TIMEOUT = 20; // autoscroll time in ms

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

// Dim factor (0..1) for image processing area, 1 means black, 0 means no dimming
static const float cfDimFactorIPA = 0.5f;
static const float cfDimFactorEXIF = 0.5f;
static const float cfDimFactorNavPanel = 0.5f;
static const float cfDimFactorWndButtons = 0.1f;
static const float cfDimFactorUSMPanel = 0.5f;

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

	// Read the string table for the current thread language if any is present
	LPCTSTR sLanguageCode = sp.Language();
	CString sNLSFile;
	TCHAR buff[16], buff2[16];
	if (_tcscmp(sLanguageCode, _T("auto")) == 0) {
		LCID threadLocale = ::GetThreadLocale();
		::GetLocaleInfo(threadLocale, LOCALE_SISO639LANGNAME, (LPTSTR) &buff, sizeof(buff));
		::GetLocaleInfo(threadLocale, LOCALE_SISO3166CTRYNAME, (LPTSTR) &buff2, sizeof(buff2));
		sLanguageCode = buff;
		sNLSFile = CString(sp.GetEXEPath()) + _T("strings_") + sLanguageCode + "-" + buff2 + _T(".txt");
		if (::GetFileAttributes(sNLSFile) == INVALID_FILE_ATTRIBUTES) {
			sNLSFile.Empty();
		}
	}
	if (sNLSFile.IsEmpty()) {
		sNLSFile = CString(sp.GetEXEPath()) + _T("strings_") + sLanguageCode + _T(".txt");
	}
	CNLS::ReadStringTable(sNLSFile);

	m_bLandscapeMode = false;
	sm_instance = this;
	m_pImageProcParams = new CImageProcessingParams(GetDefaultProcessingParams());
	InitFromProcessingFlags(GetDefaultProcessingFlags(m_bLandscapeMode), m_bHQResampling, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, m_bLandscapeMode);
	m_pUnsharpMaskParams = new CUnsharpMaskParams(CSettingsProvider::This().UnsharpMaskParams());

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
	m_bShowFileInfo = sp.ShowFileInfo();
	m_bShowNavPanel = sp.ShowNavPanel();
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
	m_bDoDragging = false;
	m_bDraggingWithZoomNavigator = false;
	m_bCropping = false;
	m_bDoCropping = false;
	m_bDontStartCropOnNextClick = false;
	m_bBlockPaintCropRect = false;
	m_nOffsetX = 0;
	m_nOffsetY = 0;
	m_nCapturedX = m_nCapturedY = 0;
	m_nMouseX = m_nMouseY = 0;
	m_bShowHelp = false;
	m_bFullScreenMode = true;
	m_bLockPaint = true;
	m_nCurrentTimeout = 0;
	m_startMouse.x = m_startMouse.y = -1;
	m_virtualImageSize = CSize(-1, -1);
	m_capturedPosZoomNavSection = CPoint(0, 0);
	m_bInZooming = false;
	m_bTemporaryLowQ = false;
	m_bShowZoomFactor = false;
	m_bSearchSubDirsOnEnter = false;
	m_bSpanVirtualDesktop = false;
	m_bShowIPTools = false;
	m_bShowWndButtonPanel = false;
	m_bShowUnsharpMaskPanel = false;
	m_bMouseInNavPanel = false;
	m_bPanMouseCursorSet = false;
	m_bInInitialOpenFile = false;
	m_dCropRatio = 0;
	m_dAlternateUSMAmount = 0;
	m_storedWindowPlacement.length = sizeof(WINDOWPLACEMENT);
	memset(&m_storedWindowPlacement2, 0, sizeof(WINDOWPLACEMENT));
	m_storedWindowPlacement2.length = sizeof(WINDOWPLACEMENT);
	m_pSliderMgr = NULL;
	m_pNavPanel = NULL;
	m_pWndButtonPanel = NULL;
	m_bPasteFromClipboardFailed = false;
	m_nMonitor = 0;

	m_cropStart = CPoint(INT_MIN, INT_MIN);
	m_cropEnd = CPoint(INT_MIN, INT_MIN);
	m_bInNavPanelAnimation = false;
	m_fCurrentBlendingFactorNavPanel = CSettingsProvider::This().BlendFactorNavPanel();
	m_nBlendInNavPanelCountdown = 0;
	m_pMemDCAnimation = NULL;
	m_hOffScreenBitmapAnimation = NULL;
}

CMainDlg::~CMainDlg() {
	delete m_pFileList;
	delete m_pJPEGProvider;
	delete m_pImageProcParams;
	delete m_pImageProcParamsKept;
	delete m_pUnsharpMaskParams;
	delete m_pSliderMgr;
	delete m_pNavPanel;
	delete m_pWndButtonPanel;
	delete m_pUnsharpMaskPanel;
	if (m_pMemDCAnimation != NULL) {
		delete m_pMemDCAnimation;
	}
	if (m_hOffScreenBitmapAnimation != NULL) {
		::DeleteObject(m_hOffScreenBitmapAnimation);
	}
	m_bInNavPanelAnimation = false;
}

LRESULT CMainDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{	
	::SetWindowLong(m_hWnd, GWL_STYLE, WS_VISIBLE);

	SetWindowTitle();

	// get the scaling of the screen (DPI) compared to 96 DPI (design value)
	CPaintDC dc(this->m_hWnd);
	m_fScaling = ::GetDeviceCaps(dc, LOGPIXELSX)/96.0f;
	Helpers::ScreenScaling = m_fScaling;

	// place window on monitor as requested in INI file
	m_nMonitor = CSettingsProvider::This().DisplayMonitor();
	m_monitorRect = CMultiMonitorSupport::GetMonitorRect(m_nMonitor);
	if (CSettingsProvider::This().ShowFullScreen()) {
		SetWindowPos(HWND_TOP, &m_monitorRect, SWP_NOZORDER);
	} else {
		ExecuteCommand(IDM_FULL_SCREEN_MODE_AFTER_STARTUP);
	}
	this->GetClientRect(&m_clientRect);

	// Configure the image processing area at bottom of screen
	m_pSliderMgr = new CSliderMgr(this->m_hWnd);
	m_pSliderMgr->AddSlider(CNLS::GetString(_T("Contrast")), &(m_pImageProcParams->Contrast), NULL, -0.5, 0.5, false, false);
	m_pSliderMgr->AddSlider(CNLS::GetString(_T("Brightness")), &(m_pImageProcParams->Gamma), NULL, 0.5, 2.0, true, true);
	m_pSliderMgr->AddSlider(CNLS::GetString(_T("Saturation")), &(m_pImageProcParams->Saturation), NULL, 0.0, 2.0, false, false);
	m_pSliderMgr->AddSlider(CNLS::GetString(_T("C - R")), &(m_pImageProcParams->CyanRed), NULL, -1.0, 1.0, false, false);
	m_pSliderMgr->AddSlider(CNLS::GetString(_T("M - G")), &(m_pImageProcParams->MagentaGreen), NULL, -1.0, 1.0, false, false);
	m_pSliderMgr->AddSlider(CNLS::GetString(_T("Y - B")), &(m_pImageProcParams->YellowBlue), NULL, -1.0, 1.0, false, false);
	m_pSliderMgr->AddSlider(CNLS::GetString(_T("Lighten Shadows")), &(m_pImageProcParams->LightenShadows), &m_bLDC, 0.0, 1.0, false, false);
	m_pSliderMgr->AddSlider(CNLS::GetString(_T("Darken Highlights")), &(m_pImageProcParams->DarkenHighlights), &m_bLDC, 0.0, 1.0, false, false);
	m_pSliderMgr->AddSlider(CNLS::GetString(_T("Deep Shadows")), &(m_pImageProcParams->LightenShadowSteepness), &m_bLDC, 0.0, 1.0, false, false);
	m_pSliderMgr->AddSlider(CNLS::GetString(_T("Color Correction")), &(m_pImageProcParams->ColorCorrectionFactor), &m_bAutoContrast, -0.5, 0.5, false, false);
	m_pSliderMgr->AddSlider(CNLS::GetString(_T("Contrast Correction")), &(m_pImageProcParams->ContrastCorrectionFactor), &m_bAutoContrast, 0.0, 1.0, false, false);
	m_pSliderMgr->AddSlider(CNLS::GetString(_T("Sharpen")), &(m_pImageProcParams->Sharpen), NULL, 0.0, 0.5, false, false);
	m_btnUnsharpMask = m_pSliderMgr->AddButton(CNLS::GetString(_T("Unsharp mask...")), &OnUnsharpMask);
	m_txtParamDB = m_pSliderMgr->AddText(CNLS::GetString(_T("Parameter DB:")), false, NULL);
	m_btnSaveToDB = m_pSliderMgr->AddButton(CNLS::GetString(_T("Save to")), &OnSaveToDB);
	m_btnRemoveFromDB = m_pSliderMgr->AddButton(CNLS::GetString(_T("Remove from")), &OnRemoveFromDB);
	m_txtRename = m_pSliderMgr->AddText(CNLS::GetString(_T("Rename:")), false, NULL);
	m_txtFileName = m_pSliderMgr->AddText(NULL, true, &OnRenameFile);
	m_txtAcqDate = m_pSliderMgr->AddText(NULL, false, NULL);
	m_txtAcqDate->SetRightAligned(true);

	// setup navigation panel
	m_pNavPanel = new CNavigationPanel(this->m_hWnd, m_pSliderMgr);
	m_pNavPanel->AddUserPaintButton(&(CNavigationPanel::PaintHomeBtn), &OnHome, CNLS::GetString(_T("Show first image in folder")));
	m_pNavPanel->AddUserPaintButton(&(CNavigationPanel::PaintPrevBtn), &OnPrev, CNLS::GetString(_T("Show previous image")));
	m_pNavPanel->AddGap(4);
	m_pNavPanel->AddUserPaintButton(&(CNavigationPanel::PaintNextBtn), &OnNext, CNLS::GetString(_T("Show next image")));
	m_pNavPanel->AddUserPaintButton(&(CNavigationPanel::PaintEndBtn), &OnEnd, CNLS::GetString(_T("Show last image in folder")));
	m_pNavPanel->AddGap(8);
	m_pNavPanel->AddUserPaintButton(&(CMainDlg::PaintZoomFitToggleBtn), &OnToggleZoomFit, &(CMainDlg::ZoomFitToggleTooltip));
	m_btnWindowMode = m_pNavPanel->AddUserPaintButton(&(CNavigationPanel::PaintWindowModeBtn), &OnToggleWindowMode, &(CMainDlg::WindowModeTooltip));
	m_pNavPanel->AddGap(8);
	m_pNavPanel->AddUserPaintButton(&(CNavigationPanel::PaintRotateCWBtn), &OnRotateCW, CNLS::GetString(_T("Rotate image 90 deg clockwise")));
	m_pNavPanel->AddUserPaintButton(&(CNavigationPanel::PaintRotateCCWBtn), &OnRotateCCW, CNLS::GetString(_T("Rotate image 90 deg counter-clockwise")));
	m_pNavPanel->AddGap(8);
	m_btnLandScape = m_pNavPanel->AddUserPaintButton(&(CNavigationPanel::PaintLandscapeModeBtn), &OnLandscapeMode, CNLS::GetString(_T("Landscape picture enhancement mode")));
	m_pNavPanel->AddGap(16);
	m_btnInfo = m_pNavPanel->AddUserPaintButton(&(CNavigationPanel::PaintInfoBtn), &OnShowInfo, CNLS::GetString(_T("Display image (EXIF) information")));

	// setup window button panel (on top, right)
	m_pWndButtonPanel = new CWndButtonPanel(this->m_hWnd, m_pSliderMgr);
	m_pWndButtonPanel->AddUserPaintButton(&(CWndButtonPanel::PaintMinimizeBtn), &OnMinimize, (LPCTSTR)NULL);
	m_pWndButtonPanel->AddUserPaintButton(&(CWndButtonPanel::PaintRestoreBtn), &OnRestore, (LPCTSTR)NULL);
	m_pWndButtonPanel->AddUserPaintButton(&(CWndButtonPanel::PaintCloseBtn), &OnClose, (LPCTSTR)NULL);

	// setup unsharp mask panel
	m_pUnsharpMaskPanel = new CUnsharpMaskPanel(this->m_hWnd, m_pSliderMgr);
	CTextCtrl* pTextUSM = m_pUnsharpMaskPanel->AddText(CNLS::GetString(_T("Apply Unsharp Mask")), false, NULL);
	pTextUSM->SetBold(true);
	m_pUnsharpMaskPanel->AddSlider(CNLS::GetString(_T("Radius")), &(m_pUnsharpMaskParams->Radius), NULL, 0.0, 5.0, false, false, 200);
	m_pUnsharpMaskPanel->AddSlider(CNLS::GetString(_T("Amount")), &(m_pUnsharpMaskParams->Amount), NULL, 0.0, 10.0, false, false, 200);
	m_pUnsharpMaskPanel->AddSlider(CNLS::GetString(_T("Threshold")), &(m_pUnsharpMaskParams->Threshold), NULL, 0, 20, false, false, 200);
	m_pUnsharpMaskPanel->AddButton(CNLS::GetString(_T("Cancel")), &OnCancelUnsharpMask);
    m_pUnsharpMaskPanel->AddButton(CNLS::GetString(_T("Apply")), &OnApplyUnsharpMask);

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

	// create thread pool for processing requests on multiple CPU cores
	CProcessingThreadPool::This().CreateThreadPoolThreads();

	// create JPEG provider and request first image
	m_pJPEGProvider = new CJPEGProvider(this->m_hWnd, NUM_THREADS, READ_AHEAD_BUFFERS);	
	m_pCurrentImage = m_pJPEGProvider->RequestJPEG(m_pFileList, CJPEGProvider::FORWARD,
		m_pFileList->Current(), CreateProcessParams());
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
			m_bInInitialOpenFile = true;
			if (!OpenFile(true)) {
				EndDialog(0);
			}
			m_bInInitialOpenFile = false;
		}
	}

	CPaintDC dc(m_hWnd);

	this->GetClientRect(&m_clientRect);
	CRect imageProcessingArea = m_pSliderMgr->PanelRect();
	bool bNavPanelVisible = IsNavPanelVisible();
	bool bShowZoomNavigator = false;
	bool bShowEXIFInfo = m_bShowFileInfo && m_pCurrentImage != NULL;
	CRectF visRectZoomNavigator(0.0f, 0.0f, 1.0f, 1.0f);
	CRect rectZoomNavigator(0, 0, 0, 0);
	CRect helpDisplayRect = (m_clientRect.Width() > m_monitorRect.Width()) ? m_monitorRect : m_clientRect;
	CBrush backBrush;
	backBrush.CreateSolidBrush(CSettingsProvider::This().ColorBackground());

	// Exclude the help display from clipping area to reduce flickering
	CHelpDisplay* pHelpDisplay = NULL;
	std::list<CRect> excludedClippingRects;
	if (m_bShowHelp) {
		pHelpDisplay = new CHelpDisplay(dc);
		GenerateHelpDisplay(*pHelpDisplay);
		CSize helpRectSize = pHelpDisplay->GetSize();
		CRect rectHelpDisplay = CRect(CPoint(helpDisplayRect.Width()/2 - helpRectSize.cx/2, 
			helpDisplayRect.Height()/2 - helpRectSize.cy/2), helpRectSize);
		dc.ExcludeClipRect(&rectHelpDisplay);
		excludedClippingRects.push_back(rectHelpDisplay);
	}

	// These panels and regions are handled over memory DCs to eliminate flickering
	CPaintMemDCMgr memDCMgr(dc);

	CEXIFDisplay* pEXIFDisplay = NULL;
	if (bShowEXIFInfo) {
		pEXIFDisplay =  new CEXIFDisplay();
		FillEXIFDataDisplay(pEXIFDisplay);
		excludedClippingRects.push_back(memDCMgr.CreateEXIFDisplayRegion(pEXIFDisplay, cfDimFactorEXIF, imageProcessingArea.left, m_bShowFileName));
	}
	if (m_bShowIPTools) {
		excludedClippingRects.push_back(memDCMgr.CreatePanelRegion(m_pSliderMgr, cfDimFactorIPA, false));
	}
	if (bNavPanelVisible) {
		excludedClippingRects.push_back(memDCMgr.CreatePanelRegion(m_pNavPanel, cfDimFactorNavPanel, !m_bMouseInNavPanel));
	}
	if (m_bShowWndButtonPanel) {
		excludedClippingRects.push_back(memDCMgr.CreatePanelRegion(m_pWndButtonPanel, cfDimFactorWndButtons, false));
	}
	if (m_bShowUnsharpMaskPanel) {
		excludedClippingRects.push_back(memDCMgr.CreatePanelRegion(m_pUnsharpMaskPanel, cfDimFactorUSMPanel, false));
	}

	if (m_pCurrentImage == NULL) {
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

		// find out the size of the bitmap to request
		CSize newSize;
		if (m_dZoom < 0.0) {
			// zoom not set, interpret as 'fit to screen'
			// ---------------------------------------------
			newSize = Helpers::GetImageRect(m_pCurrentImage->OrigWidth(), m_pCurrentImage->OrigHeight(), 
				m_clientRect.Width(), m_clientRect.Height(), m_eAutoZoomMode, m_dZoom);
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

		void* pDIBData;
		if (m_bShowUnsharpMaskPanel) {
			pDIBData = m_pCurrentImage->GetDIBUnsharpMasked(clippedSize, offsetsInImage, 
			    *m_pImageProcParams, 
				CreateProcessingFlags(m_bHQResampling && !m_bTemporaryLowQ, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, false, m_bLandscapeMode),
				*m_pUnsharpMaskParams);
		} else {
			pDIBData = m_pCurrentImage->GetDIB(newSize, clippedSize, offsetsInImage, 
			    *m_pImageProcParams, 
				CreateProcessingFlags(m_bHQResampling && !m_bTemporaryLowQ, 
				m_bAutoContrast, m_bAutoContrastSection, m_bLDC, false, m_bLandscapeMode));
		}

		// Zoom navigator - check if visible and create exclusion rectangle
		bShowZoomNavigator = IsZoomNavigatorCurrentlyShown();
		if (bShowZoomNavigator) {
			rectZoomNavigator = CZoomNavigator::GetNavigatorRect(m_pCurrentImage, imageProcessingArea);
			visRectZoomNavigator = CZoomNavigator::GetVisibleRect(newSize, clippedSize, offsetsInImage);
			CRect rectExpanded(rectZoomNavigator);
			rectExpanded.InflateRect(1, 1);
			dc.ExcludeClipRect(&rectExpanded);
			excludedClippingRects.push_back(rectExpanded);
		}

		// Paint the DIB
		if (pDIBData != NULL) {
			BITMAPINFO bmInfo;
			memset(&bmInfo, 0, sizeof(BITMAPINFO));
			bmInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmInfo.bmiHeader.biWidth = clippedSize.cx;
			bmInfo.bmiHeader.biHeight = -clippedSize.cy;
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
				dc.FillRect(&r, backBrush);
				CRect rr(xDest+clippedSize.cx, 0, m_clientRect.Width(), m_clientRect.Height());
				dc.FillRect(&rr, backBrush);
			}
			if (clippedSize.cy < m_clientRect.Height()) {
				CRect r(0, 0, m_clientRect.Width(), yDest);
				dc.FillRect(&r, backBrush);
				CRect rr(0, yDest+clippedSize.cy, m_clientRect.Width(), m_clientRect.Height());
				dc.FillRect(&rr, backBrush);
			}

			// The DIB is also blitted into the memory DCs of the panels
			memDCMgr.BlitImageToMemDC(pDIBData, &bmInfo, CPoint(xDest, yDest), m_fCurrentBlendingFactorNavPanel);
		}
	}

	// Restore the old clipping region if changed
	if (excludedClippingRects.size() > 0) {
		std::list<CRect>::iterator iter;
		for (iter = excludedClippingRects.begin(); iter != excludedClippingRects.end(); iter++) {
			CRect clippingRect = *iter;
			CRgn rgn;
			rgn.CreateRectRgn(clippingRect.left, clippingRect.top, clippingRect.right, clippingRect.bottom);
			dc.SelectClipRgn(rgn, RGN_OR);
		}
	}

	// paint zoom navigator
	if (bShowZoomNavigator && m_pCurrentImage != NULL) {
		CZoomNavigator::PaintZoomNavigator(m_pCurrentImage, visRectZoomNavigator, rectZoomNavigator,
			CPoint(m_nMouseX, m_nMouseY), *m_pImageProcParams,
			CreateProcessingFlags(true, m_bAutoContrast, false, m_bLDC, false, m_bLandscapeMode), dc);
	} else {
		CZoomNavigator::ClearLastPanRectPoint();
	}

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
		DrawTextBordered(dc, sFileName, CRect(Scale(2) + imageProcessingArea.left, 0, imageProcessingArea.right, Scale(30)), DT_LEFT); 
	}

	// Display errors and warnings
	if (m_sStartupFile.IsEmpty()) {
		CRect rectText(0, m_clientRect.Height()/2 - Scale(40), m_clientRect.Width(), m_clientRect.Height());
		CString sText = m_bInInitialOpenFile ? CNLS::GetString(_T("Select file to display in 'File Open' dialog")) :
			CString(CNLS::GetString(_T("No file or folder name was provided as command line parameter!"))) + _T('\n') +
			CNLS::GetString(_T("To use JPEGView, associate JPG, JPEG and BMP files with JPEGView.")) + _T('\n') +
			CNLS::GetString(_T("Press ESC to exit..."));
		dc.DrawText(sText, -1, &rectText, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
	} else if (m_pCurrentImage == NULL) {
		const int BUF_LEN = 512;
		TCHAR buff[BUF_LEN];
		CRect rectText(0, m_clientRect.Height()/2 - Scale(40), m_clientRect.Width(), m_clientRect.Height());
		LPCTSTR sCurrentFile = CurrentFileName(false);
		if (sCurrentFile != NULL) {
			if (m_bPasteFromClipboardFailed) {
				_tcsncpy(buff, CNLS::GetString(_T("Pasting image from clipboard failed!")), BUF_LEN);
			} else {
				_stprintf_s(buff, BUF_LEN, CNLS::GetString(_T("The file '%s' could not be read!")), sCurrentFile);
			}
			dc.DrawText(buff, -1, &rectText, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
		} else {
			if (m_pFileList->IsSlideShowList()) {
				_stprintf_s(buff, BUF_LEN, CNLS::GetString(_T("The file '%s' does not contain a list of file names!")),  
					m_sStartupFile);
			} else {
				_stprintf_s(buff, BUF_LEN, CString(CNLS::GetString(_T("The directory '%s' does not contain any image files!")))  + _T('\n') +
					CNLS::GetString(_T("Press any key to search the subdirectories for image files.")), m_pFileList->CurrentDirectory());
				m_bSearchSubDirsOnEnter = true;
			}
			dc.DrawText(buff, -1, &rectText, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
		}
	}

	// Show timing info if requested
	if (SHOW_TIMING_INFO && m_pCurrentImage != NULL) {
		TCHAR buff[256];
		_stprintf_s(buff, 256, _T("Loading: %.2f ms, Last op: %.2f ms, Last resize: %s, Last sharpen: %.2f ms"), m_pCurrentImage->GetLoadTickCount(), 
			m_pCurrentImage->LastOpTickCount(), CBasicProcessing::TimingInfo(), m_pCurrentImage->GetUnsharpMaskTickCount());
		dc.SetBkMode(OPAQUE);
		dc.TextOut(5, 5, buff);
		dc.SetBkMode(TRANSPARENT);
	}

	// Show info about nightshot/sunset detection
#ifdef _DEBUG
	if (m_pCurrentImage != NULL) {
		dc.SetBkMode(OPAQUE);
		TCHAR sNS[32];
		_stprintf_s(sNS, 32, _T("Nightshot: %.2f"), m_pCurrentImage->IsNightShot());
		dc.TextOut(500, 5, sNS);

		_stprintf_s(sNS, 32, _T("Sunset: %.2f"), m_pCurrentImage->IsSunset());
		dc.TextOut(620, 5, sNS);
		dc.SetBkMode(TRANSPARENT);
	}
#endif

	// Now blit the memory DCs of the panels to the screen
	memDCMgr.PaintMemDCToScreen();

	// Show current zoom factor
	if (m_bInZooming || m_bShowZoomFactor) {
		TCHAR buff[32];
		_stprintf_s(buff, 32, _T("%d %%"), int(m_dZoom*100 + 0.5));
		dc.SetTextColor(CSettingsProvider::This().ColorGUI());
		DrawTextBordered(dc, buff, GetZoomTextRect(imageProcessingArea), DT_RIGHT);
	}

	// Display the help screen
	if (pHelpDisplay != NULL) {
		pHelpDisplay->Show(CRect(CPoint(0, 0), CSize(helpDisplayRect.Width(), helpDisplayRect.Height())));
	}

	if (m_bDoCropping && m_bCropping && !m_bBlockPaintCropRect) {
		ShowCroppingRect(m_nMouseX, m_nMouseY, NULL, false);
		PaintCropRect(dc);
	}

	delete pHelpDisplay;
	delete pEXIFDisplay;

	SetCursorForMoveSection();

	return 0;
}

LRESULT CMainDlg::OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	this->GetClientRect(&m_clientRect);
	this->Invalidate(FALSE);
	if (m_clientRect.Width() < 800) {
		m_bShowHelp = false;
		if (m_bShowIPTools) {
			ShowHideIPTools(false);
		}
	}
	m_nMouseX = m_nMouseY = -1;
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
	bool bEatenByUnsharpMaskPanel = m_bShowUnsharpMaskPanel && m_pUnsharpMaskPanel->OnMouseLButton(MouseEvent_BtnDown, pointClicked.x, pointClicked.y);
	bool bEatenByIPA = m_bShowIPTools && m_pSliderMgr->OnMouseLButton(MouseEvent_BtnDown, pointClicked.x, pointClicked.y);
	bool bEatenByNavPanel = m_bShowNavPanel && !m_bShowIPTools && m_pNavPanel->OnMouseLButton(MouseEvent_BtnDown, pointClicked.x, pointClicked.y);
	bool bEatenByWndButtons = m_bShowWndButtonPanel && m_pWndButtonPanel->OnMouseLButton(MouseEvent_BtnDown, pointClicked.x, pointClicked.y);

	// If the panels do not eat up the event, start dragging
	if (!bEatenByUnsharpMaskPanel && !bEatenByIPA && !bEatenByNavPanel && !bEatenByWndButtons) {
		bool bCtrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
		bool bDraggingRequired = m_virtualImageSize.cx > m_clientRect.Width() || m_virtualImageSize.cy > m_clientRect.Height();
		if (bCtrl || !bDraggingRequired) {
			if (!m_bShowUnsharpMaskPanel && !m_bDontStartCropOnNextClick) {
				StartCropping(pointClicked.x, pointClicked.y);
			}
		} else {
			// Check if clicking in thumbnail image
			bool bClickInZoomNavigatorThumbnail = IsPointInZoomNavigatorThumbnail(pointClicked);
			if (bClickInZoomNavigatorThumbnail) {
				CPoint centerOfVisibleRect = CZoomNavigator::LastVisibleRect().CenterPoint();
				StartDragging(pointClicked.x + (centerOfVisibleRect.x - pointClicked.x), pointClicked.y + (centerOfVisibleRect.y - pointClicked.y), true);
				DoDragging();
				EndDragging();
				CZoomNavigator::ClearLastPanRectPoint();
				this->UpdateWindow(); // force repainting to update zoom navigator
			}
			StartDragging(pointClicked.x, pointClicked.y, bClickInZoomNavigatorThumbnail);
		}
	}
	bool bMouseInNavPanel = !m_bShowUnsharpMaskPanel && m_pNavPanel->PanelRect().PtInRect(pointClicked);
	if (bMouseInNavPanel && !m_bMouseInNavPanel) {
		m_bMouseInNavPanel = true;
		m_pNavPanel->GetTooltipMgr().EnableTooltips(true);
		this->InvalidateRect(m_pNavPanel->PanelRect(), FALSE);
	}

	m_bDontStartCropOnNextClick = false;
	return 0;
}

LRESULT CMainDlg::OnMButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	this->SetCapture();
	StartDragging(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), false);
	return 0;
}

LRESULT CMainDlg::OnLButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	if (m_bDragging || m_bCropping) {
		if (m_bDragging) {
			CZoomNavigator::ClearLastPanRectPoint();
			EndDragging();
		} else if (m_bCropping) {
			EndCropping();
		}
	} else {
		if (m_bShowIPTools) m_pSliderMgr->OnMouseLButton(MouseEvent_BtnUp, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		if (m_bShowNavPanel && !m_bShowIPTools) m_pNavPanel->OnMouseLButton(MouseEvent_BtnUp, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		if (m_bShowWndButtonPanel) m_pWndButtonPanel->OnMouseLButton(MouseEvent_BtnUp, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		if (m_bShowUnsharpMaskPanel) m_pUnsharpMaskPanel->OnMouseLButton(MouseEvent_BtnUp, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
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
	if (!m_bShowUnsharpMaskPanel) {
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

	// Start timer to fade out nav panel if no mouse movement
	if ((m_nMouseX != nOldMouseX || m_nMouseY != nOldMouseY) && !m_bPanMouseCursorSet) {
		::KillTimer(this->m_hWnd, NAVPANEL_START_ANI_TIMER_EVENT_ID);
		if (!m_bShowIPTools && !m_bShowUnsharpMaskPanel) {
			if (!m_bInNavPanelAnimation) {
				::SetTimer(this->m_hWnd, NAVPANEL_START_ANI_TIMER_EVENT_ID, 2000, NULL);
			} else {
				// Mouse moved - fade in navigation panel
				if (m_bMouseOn) {
					if (m_nBlendInNavPanelCountdown >= 5) {
						StartNavPanelAnimation(false, true);
						m_nBlendInNavPanelCountdown = 0;
					} else {
						m_nBlendInNavPanelCountdown++;
					}
				}
			}
		}
	}

	// Do dragging if needed or turn on/off image processing controls if in lower area of screen
	if (m_bDragging) {
		DoDragging();
	} else if (m_bCropping) {
		ShowCroppingRect(m_nMouseX, m_nMouseY, NULL, true);
		if (m_nMouseX >= m_clientRect.Width() - 1 || m_nMouseX <= 0 || m_nMouseY >= m_clientRect.Height() - 1 || m_nMouseY <= 0 ) {
			::SetTimer(this->m_hWnd, AUTOSCROLL_TIMER_EVENT_ID, AUTOSCROLL_TIMEOUT, NULL);
		}
	} else {
		if (m_clientRect.Width() >= 800 && !m_bShowUnsharpMaskPanel) { 
			if (!m_bShowIPTools) {
				int nIPAreaStart= m_clientRect.bottom - m_pSliderMgr->SliderAreaHeight();
				if (m_bShowNavPanel) {
					CRect rectNavPanel = m_pNavPanel->PanelRect();
					if (m_nMouseX > rectNavPanel.left && m_nMouseX < rectNavPanel.right) {
						nIPAreaStart += m_pSliderMgr->SliderAreaHeight()/2;
					}
				}
				if (!m_bFullScreenMode) {
					::KillTimer(m_hWnd, IPPANEL_TIMER_EVENT_ID);
				}
				if (m_nMouseY > nIPAreaStart) {
					if (!m_bFullScreenMode) {
						::SetTimer(m_hWnd, IPPANEL_TIMER_EVENT_ID, 50, NULL);
					} else if (nOldMouseY != 0 && !m_bShowIPTools && m_nMouseY > nIPAreaStart) {	
						ShowHideIPTools(true);
					}
				}
			} else {
				if (!m_pSliderMgr->OnMouseMove(m_nMouseX, m_nMouseY)) {
					if (m_nMouseY < m_pNavPanel->PanelRect().top) {
						ShowHideIPTools(false);
					}
				}
			}
		}

		if (m_bShowUnsharpMaskPanel) {
			m_bMouseInNavPanel = false;
			m_pUnsharpMaskPanel->OnMouseMove(m_nMouseX, m_nMouseY);
		} else {
			if (m_bShowNavPanel && !m_bShowIPTools) {
				bool bMouseInNavPanel = m_pNavPanel->PanelRect().PtInRect(CPoint(m_nMouseX, m_nMouseY));
				if (!bMouseInNavPanel && m_bMouseInNavPanel) {
					m_bMouseInNavPanel = false;
					m_pNavPanel->GetTooltipMgr().EnableTooltips(false);
					this->InvalidateRect(m_pNavPanel->PanelRect(), FALSE);
				} else if (bMouseInNavPanel && !m_bMouseInNavPanel) {
					StartNavPanelTimer(50);
				}
				m_pNavPanel->OnMouseMove(m_nMouseX, m_nMouseY);
			} else {
				m_pNavPanel->GetTooltipMgr().EnableTooltips(false);
				m_bMouseInNavPanel = false;
			}

			if (!m_bShowWndButtonPanel) {
				if (m_bFullScreenMode) {
					int nWndBtnAreaStart = m_pWndButtonPanel->ButtonPanelHeight();
					if (nOldMouseY != 0 && nOldMouseY > nWndBtnAreaStart && m_nMouseY <= nWndBtnAreaStart) {
						m_bShowWndButtonPanel = true;
						this->InvalidateRect(m_pWndButtonPanel->PanelRect(), FALSE);
					}
				}
			} else {
				if (!m_pWndButtonPanel->OnMouseMove(m_nMouseX, m_nMouseY)) {
					if (m_nMouseY > m_pWndButtonPanel->ButtonPanelHeight()*2) {
						m_bShowWndButtonPanel = false;
						this->InvalidateRect(m_pWndButtonPanel->PanelRect(), FALSE);
						m_pWndButtonPanel->GetTooltipMgr().RemoveActiveTooltip();
					}
				}
			}
		}

		CRect zoomHotArea = CZoomNavigator::GetNavigatorBound(m_pSliderMgr->PanelRect());
		BOOL bMouseInHotArea = zoomHotArea.PtInRect(CPoint(m_nMouseX, m_nMouseY));
		if (bMouseInHotArea && CSettingsProvider::This().ShowZoomNavigator()) {
			MouseOn();
			SetCursorForMoveSection();
		}
		if (bMouseInHotArea != zoomHotArea.PtInRect(CPoint(nOldMouseX, nOldMouseY))) {
			InvalidateZoomNavigatorRect();
		}
		if (m_nMouseX != nOldMouseX || m_nMouseY != nOldMouseY) {
			if (IsPointInZoomNavigatorThumbnail(CPoint(m_nMouseX, m_nMouseY))) {
				CDC dc(this->GetDC());
				CZoomNavigator::PaintPanRectangle(dc, CPoint(-1, -1));
				CZoomNavigator::PaintPanRectangle(dc, CPoint(m_nMouseX, m_nMouseY));
				StartNavPanelAnimation(true, true);
			} else if (CZoomNavigator::LastPanRectPoint() != CPoint(-1, -1)) {
				CZoomNavigator::PaintPanRectangle(CDC(this->GetDC()), CPoint(-1, -1));
			}
		}
	}
	return 0;
}

LRESULT CMainDlg::OnMouseWheel(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	bool bCtrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
	int nDelta = GET_WHEEL_DELTA_WPARAM(wParam);
	if (!bCtrl && CSettingsProvider::This().NavigateWithMouseWheel() && !m_bShowUnsharpMaskPanel) {
		if (nDelta < 0) {
			GotoImage(POS_Next);
		} else if (nDelta > 0) {
			GotoImage(POS_Previous);
		}
	} else if (m_dZoom > 0 && !m_bShowUnsharpMaskPanel) {
		PerformZoom(double(nDelta)/WHEEL_DELTA, true);
	}
	return 0;
}

LRESULT CMainDlg::OnKeyDown(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	bool bCtrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
	bool bShift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
	bool bAlt = (::GetKeyState(VK_MENU) & 0x8000) != 0;
	if (m_bShowUnsharpMaskPanel) {
		if (wParam == VK_ESCAPE) {
			TerminateUnsharpMaskPanel();
			return 1;
		}
		if (wParam == 'A' && bCtrl) {
			double dTemp = m_dAlternateUSMAmount;
			m_dAlternateUSMAmount = m_pUnsharpMaskParams->Amount;
			m_pUnsharpMaskParams->Amount = dTemp;
			Invalidate();
			return 1;
		}
		return 0; // no other keys are recogized in this mode
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
	} else if (!bCtrl && m_bSearchSubDirsOnEnter) {
		// search in subfolders if initially provider directory has no images
		m_bSearchSubDirsOnEnter = false;
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
			PerformZoom(1, true);
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
			PerformZoom(-1, true);
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
			ResetZoomToFitScreen(false);
		} else {
			ResetZoomTo100Percents();
		}
	} else if (wParam == VK_RETURN) {
		bHandled = true;
		ResetZoomToFitScreen(bCtrl);
	} else if ((wParam == VK_DOWN || wParam == VK_UP) && !bCtrl && !bShift) {
		bHandled = true;
		ExecuteCommand((wParam == VK_DOWN) ? IDM_ROTATE_90 : IDM_ROTATE_270);
	} else if ((wParam == VK_DOWN || wParam == VK_UP) && bCtrl) {
		bHandled = true;
		PerformZoom((wParam == VK_DOWN) ? -1 : +1, true);
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
	if (m_bShowUnsharpMaskPanel) {
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
	if (hDrop != NULL && !m_bShowUnsharpMaskPanel) {
		const int BUFF_SIZE = 512;
		TCHAR buff[BUFF_SIZE];
		if (::DragQueryFile(hDrop, 0, (LPTSTR) &buff, BUFF_SIZE - 1) > 0) {
			if (::GetFileAttributes(buff) & FILE_ATTRIBUTE_DIRECTORY) {
				_tcsncat(buff, _T("\\"), BUFF_SIZE);
			}
			OpenFile(buff);
		}
		::DragFinish(hDrop);
	}
	return 0;
}

LRESULT CMainDlg::OnTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	CPoint mousePos;
	::GetCursorPos(&mousePos);
	::ScreenToClient(m_hWnd, &mousePos);
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
	} else if (wParam == NAVPANEL_TIMER_EVENT_ID) {
		::KillTimer(this->m_hWnd, NAVPANEL_TIMER_EVENT_ID);
		if (!m_bShowIPTools) {
			bool bMouseInNavPanel = m_pNavPanel->PanelRect().PtInRect(CPoint(m_nMouseX, m_nMouseY));
			if (bMouseInNavPanel && !m_bMouseInNavPanel) {
				m_bMouseInNavPanel = true;
				m_pNavPanel->GetTooltipMgr().EnableTooltips(true);
				this->InvalidateRect(m_pNavPanel->PanelRect(), FALSE);
				EndNavPanelAnimation();
			}
		}
	} else if (wParam == NAVPANEL_START_ANI_TIMER_EVENT_ID) {
		::KillTimer(this->m_hWnd, NAVPANEL_START_ANI_TIMER_EVENT_ID);
		if (m_bShowNavPanel && !m_bMouseInNavPanel) {
			StartNavPanelAnimation(true, false);
		}
	} else if (wParam == NAVPANEL_ANI_TIMER_EVENT_ID) {
		DoNavPanelAnimation();
	} else if (wParam == IPPANEL_TIMER_EVENT_ID) {
		::KillTimer(m_hWnd, IPPANEL_TIMER_EVENT_ID);
		if (mousePos.y > m_clientRect.bottom - m_pSliderMgr->SliderAreaHeight() && mousePos.y < m_clientRect.Height() - 1) {
			ShowHideIPTools(true);
		}
	} else if (wParam == ZOOM_TEXT_TIMER_EVENT_ID) {
		m_bShowZoomFactor = false;
		::KillTimer(this->m_hWnd, ZOOM_TEXT_TIMER_EVENT_ID);
		InvalidateZoomNavigatorRect();
		CRect imageProcArea = m_pSliderMgr->PanelRect();
		this->InvalidateRect(GetZoomTextRect(imageProcArea), FALSE);
	} else if (wParam == AUTOSCROLL_TIMER_EVENT_ID) {
		if (mousePos.x < m_clientRect.Width() - 1 && mousePos.x > 0 && mousePos.y < m_clientRect.Height() - 1 && mousePos.y > 0 ) {
			::KillTimer(this->m_hWnd, AUTOSCROLL_TIMER_EVENT_ID);
		}
		const int PAN_DIST = 25;
		int nPanX = 0, nPanY = 0;
		if (mousePos.x <= 0) {
			nPanX = PAN_DIST;
		}
		if (mousePos.y <= 0) {
			nPanY = PAN_DIST;
		}
		if (mousePos.x >= m_clientRect.Width() - 1) {
			nPanX = -PAN_DIST;
		}
		if (mousePos.y >= m_clientRect.Height() - 1) {
			nPanY = -PAN_DIST;
		}
		if (m_bCropping) {
			this->PerformPan(nPanX, nPanY, false);
		}
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
	if (m_bShowUnsharpMaskPanel) {
		return 1;
	}
	int nX = GET_X_LPARAM(lParam);
    int nY = GET_Y_LPARAM(lParam);

	MouseOn();

	HMENU hMenu = ::LoadMenu(_Module.m_hInst, _T("PopupMenu"));
	if (hMenu == NULL) return 1;

	HMENU hMenuTrackPopup = ::GetSubMenu(hMenu, 0);
	TranslateMenu(hMenuTrackPopup);
	
	if (m_bShowFileInfo) ::CheckMenuItem(hMenuTrackPopup, IDM_SHOW_FILEINFO, MF_CHECKED);
	if (m_bShowFileName) ::CheckMenuItem(hMenuTrackPopup, IDM_SHOW_FILENAME, MF_CHECKED);
	if (m_bShowNavPanel) ::CheckMenuItem(hMenuTrackPopup, IDM_SHOW_NAVPANEL, MF_CHECKED);
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

void CMainDlg::OnUnsharpMask(CButtonCtrl & sender) {
	if (sm_instance->m_pCurrentImage != NULL) {
		sm_instance->StartUnsharpMaskPanel();
	}
}

void CMainDlg::OnCancelUnsharpMask(CButtonCtrl & sender) {
	sm_instance->TerminateUnsharpMaskPanel();
}

void CMainDlg::OnApplyUnsharpMask(CButtonCtrl & sender) {
	HCURSOR hOldCursor = ::SetCursor(::LoadCursor(NULL, MAKEINTRESOURCE(IDC_WAIT)));
	sm_instance->m_pCurrentImage->ApplyUnsharpMaskToOriginalPixels(*(sm_instance->m_pUnsharpMaskParams));
	::SetCursor(hOldCursor);
	sm_instance->TerminateUnsharpMaskPanel();
}

void CMainDlg::OnSaveToDB(CButtonCtrl & sender) {
	sm_instance->ExecuteCommand(IDM_SAVE_PARAM_DB);
}

void CMainDlg::OnRemoveFromDB(CButtonCtrl & sender) {
	sm_instance->ExecuteCommand(IDM_CLEAR_PARAM_DB);
}

bool CMainDlg::OnRenameFile(CTextCtrl & sender, LPCTSTR sChangedText) {
	return sm_instance->RenameCurrentFile(sChangedText);
}

// Handlers for the navigation panel UI controls

void CMainDlg::OnHome(CButtonCtrl & sender) {
	sm_instance->GotoImage(POS_First);
}

void CMainDlg::OnPrev(CButtonCtrl & sender) {
	sm_instance->ExecuteCommand(IDM_PREV);
}

void CMainDlg::OnNext(CButtonCtrl & sender) {
	sm_instance->ExecuteCommand(IDM_NEXT);
}

void CMainDlg::OnEnd(CButtonCtrl & sender) {
	sm_instance->GotoImage(POS_Last);
}

void CMainDlg::OnToggleZoomFit(CButtonCtrl & sender) {
	if (fabs(sm_instance->m_dZoom - 1) < 0.01) {
		sm_instance->ResetZoomToFitScreen(false);
	} else {
		sm_instance->ResetZoomTo100Percents();
	}
}

void CMainDlg::OnToggleWindowMode(CButtonCtrl & sender) {
	ToggleWindowMode(true);
}

void CMainDlg::ToggleWindowMode(bool bSetCursor) {
	sm_instance->ExecuteCommand(IDM_FULL_SCREEN_MODE);

	sm_instance->m_pNavPanel->RequestRepositioning();
	if (bSetCursor) {
		CRect rect = sm_instance->m_btnWindowMode->GetPosition();
		CPoint ptWnd = rect.CenterPoint();
		::ClientToScreen(sm_instance->m_hWnd, &ptWnd);
		::SetCursorPos(ptWnd.x, ptWnd.y);
	}
}

void CMainDlg::OnRotateCW(CButtonCtrl & sender) {
	sm_instance->ExecuteCommand(IDM_ROTATE_90);
}

void CMainDlg::OnRotateCCW(CButtonCtrl & sender) {
	sm_instance->ExecuteCommand(IDM_ROTATE_270);
}

void CMainDlg::OnShowInfo(CButtonCtrl & sender) {
	sm_instance->ExecuteCommand(IDM_SHOW_FILEINFO);
}

void CMainDlg::OnLandscapeMode(CButtonCtrl & sender) {
	sm_instance->ExecuteCommand(IDM_LANDSCAPE_MODE);
}

void CMainDlg::OnMinimize(CButtonCtrl & sender) {
	sm_instance->ExecuteCommand(IDM_MINIMIZE);
}

void CMainDlg::OnRestore(CButtonCtrl & sender) {
	ToggleWindowMode(false);
}

void CMainDlg::OnClose(CButtonCtrl & sender) {
	sm_instance->ExecuteCommand(IDM_EXIT);
}

void CMainDlg::PaintZoomFitToggleBtn(const CRect& rect, CDC& dc) {
	if (fabs(sm_instance->m_dZoom - 1) < 0.01) {
		CNavigationPanel::PaintZoomToFitBtn(rect, dc);
	} else {
		CNavigationPanel::PaintZoomTo1to1Btn(rect, dc);
	}
}

LPCTSTR CMainDlg::ZoomFitToggleTooltip() {
	if (fabs(sm_instance->m_dZoom - 1) < 0.01) {
		return CNLS::GetString(_T("Fit image to screen"));
	} else {
		return CNLS::GetString(_T("Actual size of image"));
	}
}

LPCTSTR CMainDlg::WindowModeTooltip() {
	if (sm_instance->m_bFullScreenMode) {
		return CNLS::GetString(_T("Window mode"));
	} else {
		return CNLS::GetString(_T("Full screen mode"));
	}
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
			m_bShowFileInfo = !m_bShowFileInfo;
			m_btnInfo->SetEnabled(m_bShowFileInfo);
			this->Invalidate(FALSE);
			break;
		case IDM_SHOW_FILENAME:
			m_bShowFileName = !m_bShowFileName;
			this->Invalidate(FALSE);
			break;
		case IDM_SHOW_NAVPANEL:
			m_bShowNavPanel = !m_bShowNavPanel;
			if (m_bShowNavPanel && !m_bShowIPTools) {
				EndNavPanelAnimation();
				::KillTimer(this->m_hWnd, NAVPANEL_START_ANI_TIMER_EVENT_ID);
				MouseOn();
			} else {
				m_pNavPanel->GetTooltipMgr().RemoveActiveTooltip();
			}
			if (m_bShowHelp) {
				this->Invalidate(FALSE);
			} else {
				this->InvalidateRect(m_pNavPanel->PanelRect(), FALSE);
			}
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
		case IDM_LANDSCAPE_MODE:
			m_bLandscapeMode = !m_bLandscapeMode;
			m_btnLandScape->SetEnabled(m_bLandscapeMode);
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
			if (m_bKeepParams) {
				*m_pImageProcParamsKept = *m_pImageProcParams;
				m_eProcessingFlagsKept = CreateProcessingFlags(m_bHQResampling, m_bAutoContrast, false, m_bLDC, m_bKeepParams, m_bLandscapeMode);
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
				m_bShowWndButtonPanel = false;
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
		case IDM_EDIT_GLOBAL_CONFIG:
		case IDM_EDIT_USER_CONFIG:
			EditINIFile(nCommand == IDM_EDIT_GLOBAL_CONFIG);
			break;
		case IDM_BACKUP_PARAMDB:
			BackupParamDB();
			break;
		case IDM_RESTORE_PARAMDB:
			RestoreParamDB();
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
				CRect cropRect(max(0, min(m_cropStart.x, m_cropEnd.x)), max(0, min(m_cropStart.y, m_cropEnd.y)),
					min(m_pCurrentImage->OrigWidth(), max(m_cropStart.x, m_cropEnd.x) + 1), min(m_pCurrentImage->OrigHeight(), max(m_cropStart.y, m_cropEnd.y) + 1));
				m_pCurrentImage->Crop(cropRect);
				this->Invalidate(FALSE);
			}
			break;
		case IDM_COPY_SEL:
			if (m_pCurrentImage != NULL) {
				CRect cropRect(min(m_cropStart.x, m_cropEnd.x), min(m_cropStart.y, m_cropEnd.y),
					max(m_cropStart.x, m_cropEnd.x) + 1, max(m_cropStart.y, m_cropEnd.y) + 1);

				CClipboard::CopyFullImageToClipboard(this->m_hWnd, m_pCurrentImage, *m_pImageProcParams, 
					CreateProcessingFlags(m_bHQResampling, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, false, m_bLandscapeMode),
					cropRect);
				this->Invalidate(FALSE);
			}
			break;
		case IDM_CROPMODE_FREE:
			m_dCropRatio = 0;
			break;
		case IDM_CROPMODE_FIXED_SIZE:
			{
				CCropSizeDlg dlgSetCropSize;
				dlgSetCropSize.DoModal();
			}
			m_dCropRatio = -1;
			break;
		case IDM_CROPMODE_5_4:
			m_dCropRatio = 1.25;
			break;
		case IDM_CROPMODE_4_3:
			m_dCropRatio = 1.333333333333333333;
			break;
		case IDM_CROPMODE_3_2:
			m_dCropRatio = 1.5;
			break;
		case IDM_CROPMODE_16_9:
			m_dCropRatio = 1.777777777777777778;
			break;
		case IDM_CROPMODE_16_10:
			m_dCropRatio = 1.6;
		break;
		case IDM_TOUCH_IMAGE:
		case IDM_TOUCH_IMAGE_EXIF:
			if (m_pCurrentImage != NULL) {
				if ((nCommand == IDM_TOUCH_IMAGE_EXIF) && (m_pCurrentImage->GetEXIFReader() == NULL || !m_pCurrentImage->GetEXIFReader()->GetAcquisitionTimePresent())) {
					break; // cannot use EXIF date in this case, do nothing
				}
				LPCTSTR strFileName = CurrentFileName(false);
				HANDLE hFile = ::CreateFile(strFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
				if (hFile != INVALID_HANDLE_VALUE) {
					FILETIME ft;
					SYSTEMTIME st;
					if (nCommand == IDM_TOUCH_IMAGE) {
						::GetSystemTime(&st);              // gets current time
					} else {
						// get date from EXIF
						st = m_pCurrentImage->GetEXIFReader()->GetAcquisitionTime();
					}
					::SystemTimeToFileTime(&st, &ft);  // converts to file time format
					::SetFileTime(hFile, NULL, NULL, &ft);
					::CloseHandle(hFile);
					m_pFileList->ModificationTimeChanged();
					if (m_bShowFileInfo) {
						this->Invalidate(FALSE);
					}
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
		m_pFileList->Current(), CreateProcessParams());
	AfterNewImageLoaded(true);
	m_startMouse.x = m_startMouse.y = -1;
	m_bSearchSubDirsOnEnter = false;
	m_bPasteFromClipboardFailed = false;
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
	m_bDraggingWithZoomNavigator = bDragWithZoomNavigator;
	if (m_bDraggingWithZoomNavigator) {
		m_capturedPosZoomNavSection = CZoomNavigator::LastVisibleRect().TopLeft();
	}
	m_nCapturedX = nX;
	m_nCapturedY = nY;
	SetCursorForMoveSection();
	StartNavPanelAnimation(true, true);
}

void CMainDlg::DoDragging() {
	if (m_bDragging && m_pCurrentImage != NULL) {
		int nXDelta = m_nMouseX - m_nCapturedX;
		int nYDelta = m_nMouseY - m_nCapturedY;
		if (!m_bDoDragging && (nXDelta != 0 || nYDelta != 0)) {
			m_bDoDragging = true;
		}
		if (m_bDraggingWithZoomNavigator) {
			int nNewX = m_capturedPosZoomNavSection.x + nXDelta;
			int nNewY = m_capturedPosZoomNavSection.y + nYDelta;
			CRect fullRect = CZoomNavigator::GetNavigatorRect(m_pCurrentImage, m_pSliderMgr->PanelRect());
			CPoint newOffsets = CJPEGImage::ConvertOffset(fullRect.Size(), CZoomNavigator::LastVisibleRect().Size(), CPoint(nNewX - fullRect.left, nNewY - fullRect.top));
			PerformPan((int)(newOffsets.x * (float)m_virtualImageSize.cx/fullRect.Width()), 
				(int)(newOffsets.y * (float)m_virtualImageSize.cy/fullRect.Height()), true);
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
	m_bDraggingWithZoomNavigator = false;
	if (m_pCurrentImage != NULL) {
		m_pCurrentImage->VerifyDIBPixelsCreated();
	}
	this->InvalidateRect(m_pNavPanel->PanelRect(), FALSE);
	InvalidateZoomNavigatorRect();
	SetCursorForMoveSection();
}


void CMainDlg::StartCropping(int nX, int nY) {
	m_bCropping = true;
	float fX = (float)nX, fY = (float)nY;
	ScreenToImage(fX, fY);
	m_cropStart = CPoint((int)fX, (int) fY);
	m_cropMouse = CPoint(nX, nY);
	m_cropEnd = CPoint(INT_MIN, INT_MIN);
}

void CMainDlg::ShowCroppingRect(int nX, int nY, HDC hPaintDC, bool bShow) {
	if (bShow) {
		DeleteCropRect();
	}
	float fX = (float)nX, fY = (float)nY;
	ScreenToImage(fX, fY);

	CPoint newCropEnd = CPoint((int)fX, (int) fY);
	if (m_dCropRatio > 0) {
		// fixed ratio crop mode
		int w = abs(m_cropStart.x - newCropEnd.x);
		int h = abs(m_cropStart.y - newCropEnd.y);
		double dRatio = (h < w) ? 1.0/m_dCropRatio : m_dCropRatio;
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
	} else if (m_dCropRatio < 0) {
		// fixed size crop mode
		CSize cropSize = CCropSizeDlg::GetCropSize();
		m_cropStart = CPoint((int)fX, (int) fY);
		if (CCropSizeDlg::UseScreenPixels() && m_pCurrentImage != NULL) {
			double dZoom = m_dZoom;
			if (dZoom < 0.0) {
				dZoom = (double)m_virtualImageSize.cx/m_pCurrentImage->OrigWidth();
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
		this->InvalidateRect(m_pNavPanel->PanelRect(), FALSE);
	}
	m_cropEnd = newCropEnd;
	if (bShow) {
		PaintCropRect(hPaintDC);
	}
}

void CMainDlg::PaintCropRect(HDC hPaintDC) {
	CRect cropRect = GetCropRect();
	if (!cropRect.IsRectEmpty()) {
		HDC hDC = (hPaintDC == NULL) ? this->GetDC() : hPaintDC;
		HPEN hPen = ::CreatePen(PS_DOT, 1, RGB(255, 255, 255));
		HGDIOBJ hOldPen = ::SelectObject(hDC, hPen);
		::SelectObject(hDC, ::GetStockObject(HOLLOW_BRUSH));
		::SetBkMode(hDC, OPAQUE);
		::SetBkColor(hDC, 0);
		::Rectangle(hDC, cropRect.left, cropRect.top, cropRect.right, cropRect.bottom);

		::SelectObject(hDC, hOldPen);
		::DeleteObject(hPen);
		if (hPaintDC == NULL) {
			this->ReleaseDC(hDC);
		}
	}
}

void CMainDlg::DeleteCropRect() {
	CRect cropRect = GetCropRect();
	if (!cropRect.IsRectEmpty()) {
		this->InvalidateRect(&CRect(cropRect.left-1, cropRect.top, cropRect.left+1, cropRect.bottom), FALSE);
		this->InvalidateRect(&CRect(cropRect.right-1, cropRect.top, cropRect.right+1, cropRect.bottom), FALSE);
		this->InvalidateRect(&CRect(cropRect.left, cropRect.top-1, cropRect.right, cropRect.top+1), FALSE);
		this->InvalidateRect(&CRect(cropRect.left, cropRect.bottom-1, cropRect.right, cropRect.bottom+1), FALSE);
		bool bOldFlag = m_bBlockPaintCropRect;
		m_bBlockPaintCropRect = true;
		this->UpdateWindow();
		m_bBlockPaintCropRect = bOldFlag;
	}
}

CRect CMainDlg::GetCropRect() {
	if (m_cropEnd != CPoint(INT_MIN, INT_MIN) && m_pCurrentImage != NULL) {
		CPoint cropStart(min(m_cropStart.x, m_cropEnd.x), min(m_cropStart.y, m_cropEnd.y));
		CPoint cropEnd(max(m_cropStart.x, m_cropEnd.x), max(m_cropStart.y, m_cropEnd.y));

		float fXStart = (float)cropStart.x;
		float fYStart = (float)cropStart.y;
		ImageToScreen(fXStart, fYStart);
		float fXEnd = cropEnd.x + 0.999f;
		float fYEnd = cropEnd.y + 0.999f;
		ImageToScreen(fXEnd, fYEnd);
		return CRect((int)fXStart, (int)fYStart, (int)fXEnd, (int)fYEnd);

	} else {
		return CRect(0, 0, 0, 0);
	}
}

void CMainDlg::EndCropping() {
	if (m_pCurrentImage == NULL || m_cropEnd == CPoint(INT_MIN, INT_MIN) || m_cropMouse == CPoint(m_nMouseX, m_nMouseY)) {
		m_bCropping = false;
		m_bDoCropping = false;
		DeleteCropRect();
		HideNavPanelTemporary();
		InvalidateZoomNavigatorRect();
		return;
	}

	// Display the crop menu
	HMENU hMenu = ::LoadMenu(_Module.m_hInst, _T("CropMenu"));
	int nMenuCmd = 0;
	if (hMenu != NULL) {
		HMENU hMenuTrackPopup = ::GetSubMenu(hMenu, 0);
		TranslateMenu(hMenuTrackPopup);

		CPoint posMouse(m_nMouseX, m_nMouseY);
		this->ClientToScreen(&posMouse);
		m_bInTrackPopupMenu = true;

		HMENU hMenuCropMode = ::GetSubMenu(hMenuTrackPopup, SUBMENU_POS_CROPMODE);
		if (abs(m_dCropRatio - 1.25) < 0.001) {
			::CheckMenuItem(hMenuCropMode,  IDM_CROPMODE_5_4, MF_CHECKED);
		} else if (abs(m_dCropRatio - 1.3333) < 0.001) {
			::CheckMenuItem(hMenuCropMode,  IDM_CROPMODE_4_3, MF_CHECKED);
		} else if (abs(m_dCropRatio - 1.5) < 0.001) {
			::CheckMenuItem(hMenuCropMode,  IDM_CROPMODE_3_2, MF_CHECKED);
		} else if (abs(m_dCropRatio - 1.7777) < 0.001) {
			::CheckMenuItem(hMenuCropMode,  IDM_CROPMODE_16_9, MF_CHECKED);
		} else if (abs(m_dCropRatio - 1.6) < 0.001) {
			::CheckMenuItem(hMenuCropMode,  IDM_CROPMODE_16_10, MF_CHECKED);
		} else if (m_dCropRatio < 0) {
			::CheckMenuItem(hMenuCropMode,  IDM_CROPMODE_FIXED_SIZE, MF_CHECKED);
		} else {
			::CheckMenuItem(hMenuCropMode,  IDM_CROPMODE_FREE, MF_CHECKED);
		}
		nMenuCmd = ::TrackPopupMenu(hMenuTrackPopup, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, 
			posMouse.x, posMouse.y, 0, this->m_hWnd, NULL);
		m_bInTrackPopupMenu = false;

		if (m_bCropping) {
			ExecuteCommand(nMenuCmd);
		}

		::DestroyMenu(hMenu);

		// Hack: When context menu was canceled by clicking outside of menu, the main window gets a mouse click event and a mouse move event.
		// This would start another crop what is not desired.
		if (nMenuCmd == 0) {
			m_bDontStartCropOnNextClick = true;
		}
	}

	if (m_bCropping && nMenuCmd != IDM_COPY_SEL && nMenuCmd != IDM_CROP_SEL) {
		DeleteCropRect();
	}
	m_bCropping = false;
	m_bDoCropping = false;
	HideNavPanelTemporary();
	InvalidateZoomNavigatorRect();
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
	m_bCropping = false; // cancel any running crop
	
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
		} else {
			m_bPasteFromClipboardFailed = true;
		}
	} else {
		m_pCurrentImage = m_pJPEGProvider->RequestJPEG(m_pFileList, eDirection,  
			m_pFileList->Current(), procParams);
		m_bPasteFromClipboardFailed = false;
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

	if (m_bMouseOn) {
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

bool CMainDlg::PerformPan(int dx, int dy, bool bAbsolute) {
	if ((m_virtualImageSize.cx > 0 && m_virtualImageSize.cx > m_clientRect.Width()) ||
		(m_virtualImageSize.cy > 0 && m_virtualImageSize.cy > m_clientRect.Height())) {
		if (bAbsolute) {
			m_nOffsetX = dx;
			m_nOffsetY = dy;
		} else {
			m_nOffsetX += dx;
			m_nOffsetY += dy;
		}
		m_bUserPan = true;

		this->Invalidate(FALSE);
		return true;
	}
	return false;
}

void CMainDlg::ZoomToSelection() {
	CRect zoomRect(max(0, min(m_cropStart.x, m_cropEnd.x)), max(0, min(m_cropStart.y, m_cropEnd.y)),
		min(m_pCurrentImage->OrigWidth(), max(m_cropStart.x, m_cropEnd.x)), min(m_pCurrentImage->OrigHeight(), max(m_cropStart.y, m_cropEnd.y)));
	if (zoomRect.Width() > 0 && zoomRect.Height() > 0 && m_pCurrentImage != NULL) {
		float fZoom;
		CPoint offsets;
		Helpers::GetZoomParameters(fZoom, offsets, CSize(m_pCurrentImage->OrigWidth(), m_pCurrentImage->OrigHeight()), 
			m_clientRect.Size(), zoomRect);
		if (fZoom > 0) {
			PerformZoom(fZoom, false);
			m_nOffsetX = offsets.x;
			m_nOffsetY = offsets.y;
			m_bUserPan = true;
		}
	}
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
		double dOldZoom = m_dZoom;
		CSize newSize = Helpers::GetImageRect(m_pCurrentImage->OrigWidth(), m_pCurrentImage->OrigHeight(), 
			m_clientRect.Width(), m_clientRect.Height(), true, bFillWithCrop, false, m_dZoom);
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
		PerformZoom(1.0, false);
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
		m_offsetKept = m_bUserPan ? CPoint(m_nOffsetX, m_nOffsetY) : CPoint(0, 0);

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
	m_nOffsetX = m_nOffsetY = 0;
	m_bUserZoom = false;
	m_bUserPan = false;
	*m_pImageProcParams = GetDefaultProcessingParams();
	InitFromProcessingFlags(GetDefaultProcessingFlags(m_bLandscapeMode), m_bHQResampling, m_bAutoContrast, m_bAutoContrastSection, m_bLDC, m_bLandscapeMode);
}

void CMainDlg::StartTimer(int nMilliSeconds) {
	m_nCurrentTimeout = nMilliSeconds;
	::SetTimer(this->m_hWnd, SLIDESHOW_TIMER_EVENT_ID, nMilliSeconds, NULL);
	EndNavPanelAnimation();
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
	StartTimer(Helpers::RoundToInt(1000.0/dFPS));
	AfterNewImageLoaded(false);
	this->InvalidateRect(m_pNavPanel->PanelRect(), FALSE);
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

void CMainDlg::StartNavPanelTimer(int nTimeout) {
	::KillTimer(this->m_hWnd, NAVPANEL_TIMER_EVENT_ID);
	::SetTimer(this->m_hWnd, NAVPANEL_TIMER_EVENT_ID, nTimeout, NULL);
}

void CMainDlg::MouseOff() {
	if (m_bMouseOn) {
		if (m_nMouseY < m_clientRect.bottom - m_pSliderMgr->SliderAreaHeight() && 
			!m_bInTrackPopupMenu && !m_pNavPanel->PanelRect().PtInRect(CPoint(m_nMouseX, m_nMouseY))) {
			if (m_bFullScreenMode) {
				while (::ShowCursor(FALSE) >= 0);
			}
			m_startMouse.x = m_startMouse.y = -1;
			m_bMouseOn = false;
		}
		this->InvalidateRect(m_pNavPanel->PanelRect(), FALSE);
	}
}

void CMainDlg::MouseOn() {
	if (!m_bMouseOn) {
		::ShowCursor(TRUE);
		m_bMouseOn = true;
		this->InvalidateRect(m_pNavPanel->PanelRect(), FALSE);
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
		// Use cleartype here, this improves the readability
		hFont = ::CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, _T("Arial"));
	}

	HFONT hOldFont = dc.SelectFont(hFont);
	COLORREF oldColor = dc.SetTextColor(RGB(0, 0, 0));
	CRect textRect = rect;
	textRect.InflateRect(-1, -1);
	textRect.OffsetRect(-1, 0); dc.DrawText(sText, -1, &textRect, nFormat | DT_NOPREFIX);
	textRect.OffsetRect(2, 0); dc.DrawText(sText, -1, &textRect, nFormat | DT_NOPREFIX);
	textRect.OffsetRect(-1, -1); dc.DrawText(sText, -1, &textRect, nFormat | DT_NOPREFIX);
	textRect.OffsetRect(0, 2); dc.DrawText(sText, -1, &textRect, nFormat | DT_NOPREFIX);
	textRect.OffsetRect(0, -1);
	dc.SetTextColor(oldColor);
	dc.DrawText(sText, -1, &textRect, nFormat | DT_NOPREFIX);
	dc.SelectFont(hOldFont);
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

void CMainDlg::ShowHideSaveDBButtons() {
	m_btnSaveToDB->SetShow(m_pCurrentImage != NULL && !m_pCurrentImage->IsClipboardImage() && !m_bMovieMode && !m_bKeepParams);
	m_btnRemoveFromDB->SetShow(m_pCurrentImage != NULL && !m_bMovieMode && !m_bKeepParams && 
		m_pCurrentImage->IsInParamDB());
}

void CMainDlg::AfterNewImageLoaded(bool bSynchronize) {
	SetWindowTitle();
	HideNavPanelTemporary();
	ShowHideSaveDBButtons();
	m_btnUnsharpMask->SetShow(m_pCurrentImage != NULL);
	if (m_pCurrentImage != NULL && !m_pCurrentImage->IsClipboardImage() && !m_bMovieMode) {
		m_txtParamDB->SetShow(true);
		m_txtRename->SetShow(true);
		LPCTSTR sCurrentFileTitle = m_pFileList->CurrentFileTitle();
		if (sCurrentFileTitle != NULL) {
			m_txtFileName->SetText(sCurrentFileTitle);
		} else {
			m_txtFileName->SetText(_T(""));
		}
		m_txtFileName->SetEditable(!m_pFileList->IsSlideShowList());
		CEXIFReader* pEXIF = m_pCurrentImage->GetEXIFReader();
		if (pEXIF != NULL && pEXIF->GetAcquisitionTime().wYear > 1600) {
			m_txtAcqDate->SetText(CString(_T("* ")) + Helpers::SystemTimeToString(pEXIF->GetAcquisitionTime()));
		} else {
			m_txtAcqDate->SetText(_T(""));
		}
	} else {
		m_txtAcqDate->SetText(_T(""));
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
	if (m_bShowFileInfo || m_bShowFileName || m_bShowHelp) {
		this->Invalidate(FALSE);
	}
	SetWindowTitle();
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

	const int BUFF_SIZE = 256;
	TCHAR buff[BUFF_SIZE];
	CString sText = CString(CNLS::GetString(_T("Do you really want to save the following parameters as default to the INI file"))) + _T('\n') +
					Helpers::JPEGViewAppDataPath() + _T("JPEGView.ini ?\n\n");
	_stprintf_s(buff, BUFF_SIZE, CString(CNLS::GetString(_T("Contrast"))) + _T(": %.2f\n"), m_pImageProcParams->Contrast);
	sText += buff;
	_stprintf_s(buff, BUFF_SIZE, CString(CNLS::GetString(_T("Gamma"))) + _T(": %.2f\n"), m_pImageProcParams->Gamma);
	sText += buff;
	_stprintf_s(buff, BUFF_SIZE, CString(CNLS::GetString(_T("Saturation"))) + _T(": %.2f\n"), m_pImageProcParams->Saturation);
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
		_stprintf_s(buff, BUFF_SIZE, CString(CNLS::GetString(_T("Lighten Shadows"))) + _T(": %.2f\n"), m_pImageProcParams->LightenShadows);
		sText += buff;
		_stprintf_s(buff, BUFF_SIZE, CString(CNLS::GetString(_T("Darken Highlights"))) + _T(": %.2f\n"), m_pImageProcParams->DarkenHighlights);
		sText += buff;
		_stprintf_s(buff, BUFF_SIZE, CString(CNLS::GetString(_T("Deep Shadows"))) + _T(": %.2f\n"), m_pImageProcParams->LightenShadowSteepness);
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
	sText += _T("\n");
	sText += CNLS::GetString(_T("Show navigation panel")); sText += _T(": ");
	sText += m_bShowNavPanel ? CNLS::GetString(_T("yes")) : CNLS::GetString(_T("no"));
	sText += _T("\n\n");
	sText += CNLS::GetString(_T("These values will override the values from the INI file located in the program folder of JPEGView!"));

	if (IDYES == this->MessageBox(sText, CNLS::GetString(_T("Confirm save default parameters")), MB_YESNO | MB_ICONQUESTION)) {
		CSettingsProvider::This().SaveSettings(*m_pImageProcParams, 
			CreateProcessingFlags(m_bHQResampling, m_bAutoContrast, 
			m_bAutoContrastSection, m_bLDC, m_bKeepParams, m_bLandscapeMode), m_pFileList->GetSorting(), m_eAutoZoomMode, m_bShowNavPanel);
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
		if (bFileTitle) {
			return m_pFileList->CurrentFileTitle();
		} else {
			return m_pFileList->Current();
		}
	} else {
		return NULL;
	}
}

void CMainDlg::FillEXIFDataDisplay(CEXIFDisplay* pEXIFDisplay) {
	CString sFileTitle;
	if (m_pCurrentImage->IsClipboardImage()) {
		sFileTitle = CurrentFileName(true);
	} else if (m_pFileList->Current() != NULL) {
		sFileTitle.Format(_T("[%d/%d]  %s"), m_pFileList->CurrentIndex() + 1, m_pFileList->Size(), CurrentFileName(true));
	}
	pEXIFDisplay->AddTitle(sFileTitle);
	pEXIFDisplay->AddLine(CNLS::GetString(_T("Image width:")), m_pCurrentImage->OrigWidth());
	pEXIFDisplay->AddLine(CNLS::GetString(_T("Image height:")), m_pCurrentImage->OrigHeight());
	if (!m_pCurrentImage->IsClipboardImage()) {
		CEXIFReader* pEXIFReader = m_pCurrentImage->GetEXIFReader();
		if (pEXIFReader != NULL) {
			if (pEXIFReader->GetAcquisitionTimePresent()) {
				pEXIFDisplay->AddLine(CNLS::GetString(_T("Acquisition date:")), pEXIFReader->GetAcquisitionTime());
			} else {
				const FILETIME* pFileTime = m_pFileList->CurrentModificationTime();
				if (pFileTime != NULL) {
					pEXIFDisplay->AddLine(CNLS::GetString(_T("Modification date:")), *pFileTime);
				}
			}
			if (pEXIFReader->GetCameraModelPresent()) {
				pEXIFDisplay->AddLine(CNLS::GetString(_T("Camera model:")), pEXIFReader->GetCameraModel());
			}
			if (pEXIFReader->GetExposureTimePresent()) {
				pEXIFDisplay->AddLine(CNLS::GetString(_T("Exposure time (s):")), pEXIFReader->GetExposureTime());
			}
			if (pEXIFReader->GetExposureBiasPresent()) {
				pEXIFDisplay->AddLine(CNLS::GetString(_T("Exposure bias (EV):")), pEXIFReader->GetExposureBias(), 2);
			}
			if (pEXIFReader->GetFlashFiredPresent()) {
				pEXIFDisplay->AddLine(CNLS::GetString(_T("Flash fired:")), pEXIFReader->GetFlashFired() ? CNLS::GetString(_T("yes")) : CNLS::GetString(_T("no")));
			}
			if (pEXIFReader->GetFocalLengthPresent()) {
				pEXIFDisplay->AddLine(CNLS::GetString(_T("Focal length (mm):")), pEXIFReader->GetFocalLength(), 1);
			}
			if (pEXIFReader->GetFNumberPresent()) {
				pEXIFDisplay->AddLine(CNLS::GetString(_T("F-Number:")), pEXIFReader->GetFNumber(), 1);
			}
			if (pEXIFReader->GetISOSpeedPresent()) {
				pEXIFDisplay->AddLine(CNLS::GetString(_T("ISO Speed:")), (int)pEXIFReader->GetISOSpeed());
			}
		} else {
			const FILETIME* pFileTime = m_pFileList->CurrentModificationTime();
			if (pFileTime != NULL) {
				pEXIFDisplay->AddLine(CNLS::GetString(_T("Modification date:")), *pFileTime);
			}
		}
	}
}

void CMainDlg::GenerateHelpDisplay(CHelpDisplay & helpDisplay) {
	helpDisplay.AddTitle(CNLS::GetString(_T("JPEGView Help")));
	LPCTSTR sTitle = CurrentFileName(true);
	if (sTitle != NULL && m_pCurrentImage != NULL) {
		double fMPix = double(m_pCurrentImage->OrigWidth() * m_pCurrentImage->OrigHeight())/(1000000);
		TCHAR buff[256];
		_stprintf_s(buff, 256, _T("%s (%d x %d   %.1f MPixel)"), sTitle, m_pCurrentImage->OrigWidth(), m_pCurrentImage->OrigHeight(), fMPix);
		helpDisplay.AddLine(CNLS::GetString(_T("Current image")), buff);
	}
	helpDisplay.AddLine(_T("Esc"), CNLS::GetString(_T("Close help text display / Close JPEGView")));
	helpDisplay.AddLine(_T("F1"), CNLS::GetString(_T("Show/hide this help text")));
	helpDisplay.AddLineInfo(_T("F2"), m_bShowFileInfo, CNLS::GetString(_T("Show/hide picture information (EXIF data)")));
	helpDisplay.AddLineInfo(_T("Ctrl+F2"), m_bShowFileName, CNLS::GetString(_T("Show/hide file name")));
	helpDisplay.AddLineInfo(_T("F11"), m_bShowNavPanel, CNLS::GetString(_T("Show/hide navigation panel")));
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
	helpDisplay.AddLineInfo(_T("Ctrl+L"), m_bLandscapeMode, CNLS::GetString(_T("Enable/disable landscape picture enhancement mode")));
	helpDisplay.AddLineInfo(_T("F7"), m_pFileList->GetNavigationMode() == Helpers::NM_LoopDirectory, CNLS::GetString(_T("Loop through files in current folder")));
	helpDisplay.AddLineInfo(_T("F8"), m_pFileList->GetNavigationMode() == Helpers::NM_LoopSubDirectories, CNLS::GetString(_T("Loop through files in current directory and all subfolders")));
	helpDisplay.AddLineInfo(_T("F9"), m_pFileList->GetNavigationMode() == Helpers::NM_LoopSameDirectoryLevel, CNLS::GetString(_T("Loop through files in current directory and all sibling folders (folders on same level)")));
	helpDisplay.AddLineInfo(_T("F12"), m_bSpanVirtualDesktop, CNLS::GetString(_T("Maximize/restore to/from virtual desktop (only for multi-monitor systems)")));
	helpDisplay.AddLineInfo(_T("Ctrl+F12"), LPCTSTR(NULL), CNLS::GetString(_T("Toggle between screens (only for multi-monitor systems)")));
	helpDisplay.AddLineInfo(_T("Ctrl+W"), m_bFullScreenMode, CNLS::GetString(_T("Enable/disable full screen mode")));
	helpDisplay.AddLine(_T("Ctrl+M"), CNLS::GetString(_T("Mark image for toggling. Use Ctrl+Left/Ctrl+Right to toggle between marked and current image")));
	helpDisplay.AddLine(_T("Ctrl+C/Ctrl+X"), CNLS::GetString(_T("Copy screen to clipboard/ Copy processed full size image to clipboard")));
	helpDisplay.AddLine(_T("Ctrl+O"), CNLS::GetString(_T("Open new image or slideshow file")));
	helpDisplay.AddLine(_T("Ctrl+S"), CNLS::GetString(_T("Save processed image to JPEG file (original size)")));
	helpDisplay.AddLine(_T("Ctrl+Shift+S"), CNLS::GetString(_T("Save processed image to JPEG file (screen size)")));
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
	helpDisplay.AddLine(CNLS::GetString(_T("Ctrl + L mouse")), CNLS::GetString(_T("Crop image")));
	helpDisplay.AddLine(CNLS::GetString(_T("Fwd mouse button")), CNLS::GetString(_T("Next image")));
	helpDisplay.AddLine(CNLS::GetString(_T("Back mouse button")), CNLS::GetString(_T("Previous image")));
	std::list<CUserCommand*>::iterator iter;
	std::list<CUserCommand*> & userCmdList = CSettingsProvider::This().UserCommandList();
	for (iter = userCmdList.begin( ); iter != userCmdList.end( ); iter++ ) {
		if (!(*iter)->HelpText().IsEmpty()) {
			helpDisplay.AddLine((*iter)->HelpKey(), CNLS::GetString((*iter)->HelpText()));
		}
	}
}

void CMainDlg::InvalidateZoomNavigatorRect() {
	bool bShowZoomNavigator = m_pCurrentImage != NULL && CSettingsProvider::This().ShowZoomNavigator();
	if (bShowZoomNavigator) {
		CRect rectZoomNavigator = CZoomNavigator::GetNavigatorRect(m_pCurrentImage, m_pSliderMgr->PanelRect());
		rectZoomNavigator.InflateRect(1, 1);
		this->InvalidateRect(rectZoomNavigator, FALSE);
	}
}

bool CMainDlg::IsZoomNavigatorCurrentlyShown() {
	bool bShowZoomNavigator = m_pCurrentImage != NULL && CSettingsProvider::This().ShowZoomNavigator();
	if (bShowZoomNavigator) {
		if (m_bDraggingWithZoomNavigator) {
			return true;
		}
		CSize clippedSize(min(m_clientRect.Width(), m_virtualImageSize.cx), min(m_clientRect.Height(), m_virtualImageSize.cy));
		bShowZoomNavigator = (m_virtualImageSize.cx > clippedSize.cx || m_virtualImageSize.cy > clippedSize.cy);
		if (bShowZoomNavigator) {
			CRect navBoundRect = CZoomNavigator::GetNavigatorBound(m_pSliderMgr->PanelRect());
			bShowZoomNavigator = m_bDoDragging || m_bInZooming || m_bShowZoomFactor || 
				(!m_bCropping && navBoundRect.PtInRect(CPoint(m_nMouseX, m_nMouseY)));
		}
	}
	return bShowZoomNavigator;
}

bool CMainDlg::IsPointInZoomNavigatorThumbnail(const CPoint& pt) {
	if (IsZoomNavigatorCurrentlyShown() && m_pCurrentImage != NULL) {
		CRect thumbnailRect = CZoomNavigator::GetNavigatorRect(m_pCurrentImage, m_pSliderMgr->PanelRect());
		return thumbnailRect.PtInRect(pt);
	}
	return false;
}

void CMainDlg::SetCursorForMoveSection() {
	if (IsPointInZoomNavigatorThumbnail(CPoint(m_nMouseX, m_nMouseY)) || m_bDragging) {
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
	if (m_bShowIPTools) {
		nStartX = max(nStartX, m_btnUnsharpMask->GetPosition().right);
	}
	int nEndX = nStartX + Scale(ZOOM_TEXT_RECT_WIDTH);
	if (m_bShowIPTools) {
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

void CMainDlg::BackupParamDB() {
	CFileDialog fileDlg(FALSE, _T(".db"), _T("ParamDBBackup.db"), 
			OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_NOREADONLYRETURN,
			Helpers::CReplacePipe(CString(_T("Param DB (*.db)|*.db|")) +
			CNLS::GetString(_T("All Files")) + _T("|*.*|")), m_hWnd);
	TCHAR buff[MAX_PATH];
	::SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, buff);
	fileDlg.m_ofn.lpstrInitialDir = buff;
	if (IDOK == fileDlg.DoModal(m_hWnd)) {
		if (!::CopyFile(CParameterDB::This().GetParamDBName(), fileDlg.m_szFileName, TRUE)) {
			LPTSTR lpMsgBuf = NULL;
			::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, ::GetLastError(),
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL);
			CString sMsg; sMsg.Format(CNLS::GetString(_T("Copying parameter DB to file '%s' failed. Reason: %s")), fileDlg.m_szFileName, lpMsgBuf);
			::MessageBox(m_hWnd, sMsg, _T("JPEGView"), MB_OK | MB_ICONERROR);
			::LocalFree(lpMsgBuf);
		}
	}
}

void CMainDlg::RestoreParamDB() {
	CFileDialog fileDlg(TRUE, _T(".db"), _T("ParamDBBackup.db"), 
		OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY,
		Helpers::CReplacePipe(CString(_T("Param DB (*.db)|*.db|")) +
		CNLS::GetString(_T("All Files")) + _T("|*.*|")), m_hWnd);
	TCHAR buff[MAX_PATH];
	::SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, buff);
	fileDlg.m_ofn.lpstrInitialDir = buff;
	fileDlg.m_ofn.lpstrTitle = CNLS::GetString(_T("Select parameter DB to restore"));
	if (IDOK == fileDlg.DoModal(m_hWnd)) {
		CParameterDB::This().MergeParamDB(fileDlg.m_szFileName);
	}
}

void CMainDlg::SetWindowTitle() {
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

bool CMainDlg::IsNavPanelVisible() {
	bool bMouseInNavPanel = m_bMouseInNavPanel && !m_bShowIPTools;
	return m_bShowNavPanel && !(m_fCurrentBlendingFactorNavPanel <= 0.0f && !bMouseInNavPanel) &&
		!m_bMovieMode && !m_bDoCropping && (m_bMouseOn || bMouseInNavPanel);
}

void CMainDlg::StartNavPanelAnimation(bool bFadeOut, bool bFast) {
	if (m_bShowUnsharpMaskPanel) return;
	if (!m_bInNavPanelAnimation) {
		m_bFadeOut = bFadeOut;
		if (!bFadeOut) {
			return; // already visible, do nothing
		}
		m_bInNavPanelAnimation = true;
		m_fCurrentBlendingFactorNavPanel = CSettingsProvider::This().BlendFactorNavPanel();
		::SetTimer(this->m_hWnd, NAVPANEL_ANI_TIMER_EVENT_ID, bFast ? 20 : 100, NULL);
	} else if (m_bFadeOut != bFadeOut) {
		m_bFadeOut = bFadeOut;
		::KillTimer(this->m_hWnd, NAVPANEL_ANI_TIMER_EVENT_ID);
		::SetTimer(this->m_hWnd, NAVPANEL_ANI_TIMER_EVENT_ID, bFast ? 20 : 100, NULL);
	}
}

void CMainDlg::DoNavPanelAnimation() {
	bool bDoAnimation = true;
	if (!m_bInNavPanelAnimation || m_pCurrentImage == NULL || m_bShowHelp) {
		bDoAnimation = false;
	} else {
		if (!IsNavPanelVisible()) {
			bDoAnimation = false;
		} else if ((m_bFadeOut && m_fCurrentBlendingFactorNavPanel <= 0) || (!m_bFadeOut && m_fCurrentBlendingFactorNavPanel >= CSettingsProvider::This().BlendFactorNavPanel())) {
			bDoAnimation = false;
		}
	}

	bool bTerminate = false;
	if (m_bFadeOut) {
		m_fCurrentBlendingFactorNavPanel = max(0.0f, m_fCurrentBlendingFactorNavPanel - 0.02f);
	} else {
		m_fCurrentBlendingFactorNavPanel = min(CSettingsProvider::This().BlendFactorNavPanel(), m_fCurrentBlendingFactorNavPanel + 0.06f);
		bTerminate = m_fCurrentBlendingFactorNavPanel >= CSettingsProvider::This().BlendFactorNavPanel();
	}

	if (bDoAnimation) {
		CRect rectNavPanel = m_pNavPanel->PanelRect();
		CDC screenDC = GetDC();
		void* pDIBData = m_pCurrentImage->DIBPixelsLastProcessed(false);
		if (pDIBData != NULL) {
			if (m_pMemDCAnimation == NULL) {
				m_pMemDCAnimation = new CDC();
				CDC screenDC = GetDC();
				m_hOffScreenBitmapAnimation = CPaintMemDCMgr::PrepareRectForMemDCPainting(*m_pMemDCAnimation, screenDC, m_pNavPanel->PanelRect());
			}

			CBrush backBrush;
			backBrush.CreateSolidBrush(CSettingsProvider::This().ColorBackground());
			m_pMemDCAnimation->FillRect(CRect(0, 0, rectNavPanel.Width(), rectNavPanel.Height()), backBrush);

			BITMAPINFO bmInfo;
			memset(&bmInfo, 0, sizeof(BITMAPINFO));
			bmInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmInfo.bmiHeader.biWidth = m_pCurrentImage->DIBWidth();
			bmInfo.bmiHeader.biHeight = -m_pCurrentImage->DIBHeight();
			bmInfo.bmiHeader.biPlanes = 1;
			bmInfo.bmiHeader.biBitCount = 32;
			bmInfo.bmiHeader.biCompression = BI_RGB;
			int xDest = (m_clientRect.Width() - m_pCurrentImage->DIBWidth()) / 2;
			int yDest = (m_clientRect.Height() - m_pCurrentImage->DIBHeight()) / 2;
			CPaintMemDCMgr::BitBltBlended(*m_pMemDCAnimation, screenDC, CSize(rectNavPanel.Width(), rectNavPanel.Height()), pDIBData, &bmInfo, 
								  CPoint(xDest - rectNavPanel.left, yDest - rectNavPanel.top), CSize(m_pCurrentImage->DIBWidth(), m_pCurrentImage->DIBHeight()), 
								  *m_pNavPanel, CPoint(-rectNavPanel.left, -rectNavPanel.top), m_fCurrentBlendingFactorNavPanel);
			screenDC.BitBlt(rectNavPanel.left, rectNavPanel.top, rectNavPanel.Width(), rectNavPanel.Height(), *m_pMemDCAnimation, 0, 0, SRCCOPY);
		}
	}
	if (bTerminate) {
		EndNavPanelAnimation();
	}
}

void CMainDlg::EndNavPanelAnimation() {
	if (m_bInNavPanelAnimation) {
		::KillTimer(this->m_hWnd, NAVPANEL_ANI_TIMER_EVENT_ID);
		m_bInNavPanelAnimation = false;
		m_fCurrentBlendingFactorNavPanel = CSettingsProvider::This().BlendFactorNavPanel();
		if (m_pMemDCAnimation != NULL) {
			delete m_pMemDCAnimation;
			m_pMemDCAnimation = NULL;
			::DeleteObject(m_hOffScreenBitmapAnimation);
			m_hOffScreenBitmapAnimation = NULL;
		}
		
	}
	::KillTimer(this->m_hWnd, NAVPANEL_START_ANI_TIMER_EVENT_ID);
	::SetTimer(this->m_hWnd, NAVPANEL_START_ANI_TIMER_EVENT_ID, 2000, NULL);
}

void CMainDlg::HideNavPanelTemporary() {
	if (!m_bMouseInNavPanel || m_bShowIPTools || m_bCropping) {
		m_bInNavPanelAnimation = true;
		m_fCurrentBlendingFactorNavPanel = 0.0;
		this->InvalidateRect(m_pNavPanel->PanelRect(), FALSE);
		m_bFadeOut = true;
	}
}

void CMainDlg::ShowHideIPTools(bool bShow) {
	m_bShowIPTools = bShow;
	this->InvalidateRect(m_pSliderMgr->PanelRect(), FALSE);
	this->InvalidateRect(m_pNavPanel->PanelRect(), FALSE);
	if (!bShow) {
		m_pSliderMgr->GetTooltipMgr().RemoveActiveTooltip();
	}
	if (bShow) {
		StartNavPanelAnimation(true, true);
	} else {
		StartNavPanelAnimation(false, true);
	}
}

void CMainDlg::TerminateUnsharpMaskPanel() {
	m_bShowUnsharpMaskPanel = false;
	m_bShowNavPanel = m_bOldShowNavPanel;
	m_pCurrentImage->FreeUnsharpMaskResources();
	Invalidate();
}

void CMainDlg::StartUnsharpMaskPanel() {
	bool bOldMouseOn = m_bMouseOn;
	m_bMouseOn = false;
	ResetZoomTo100Percents();
	m_bMouseOn = bOldMouseOn;
	m_bShowUnsharpMaskPanel = true;
	m_bShowHelp = false;
	m_bShowIPTools = false;
	m_bOldShowNavPanel = m_bShowNavPanel;
	m_bShowNavPanel = false;
	m_bShowWndButtonPanel = false;
	EndNavPanelAnimation();
	StopTimer();
	StopMovieMode();
	Invalidate();
}