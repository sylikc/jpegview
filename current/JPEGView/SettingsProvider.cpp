#include "StdAfx.h"
#include "SettingsProvider.h"
#include "NLS.h"
#include <float.h>
#include <shlobj.h>
#include <algorithm>

static const TCHAR* DEFAULT_INI_FILE_NAME = _T("JPEGView.ini");
static const TCHAR* SECTION_NAME = _T("JPEGView");

static float COLOR_CORR_DEFAULT_FACTORS[6] = {0.2f, 0.1f, 0.3f, 0.3f, 0.3f, 0.15f};

CSettingsProvider* CSettingsProvider::sm_instance;

static CString ParseCommandLineForIniName(LPCTSTR sCommandLine) {
	if (sCommandLine == NULL) {
		return CString();
	}
	LPCTSTR iniFile = Helpers::stristr(sCommandLine, _T("/ini"));
	if (iniFile == NULL) {
		return CString();
	}
	iniFile = iniFile + _tcslen(_T("/ini")) + 1;
	LPCTSTR posSpace = _tcschr(iniFile, _T(' '));
	CString iniFileName = (posSpace == NULL) ? CString(iniFile) : CString(iniFile, (int)(posSpace - iniFile));
	return iniFileName;
}

CSettingsProvider& CSettingsProvider::This() {
	if (sm_instance == NULL) {
		sm_instance = new CSettingsProvider();
	}
	return *sm_instance;
}

float* CSettingsProvider::ColorCorrectionAmounts() { 
	return (m_fColorCorrections[0] < 0) ? COLOR_CORR_DEFAULT_FACTORS : m_fColorCorrections;
}

CSettingsProvider::CSettingsProvider(void) {
	// parse command line to find the process startup path
	LPTSTR pCmdLine = ::GetCommandLine();
	LPTSTR pStart = pCmdLine;
	bool bQuot = false;
	if (pCmdLine[0] == _T('"')) {
		pStart++;
		bQuot = true;
	}
	LPTSTR pEnd = pStart;
	int nLastIdx = -1;
	int nIdx = 0;
	while (((bQuot && *pEnd != _T('"')) || (!bQuot && *pEnd != _T(' '))) && *pEnd != 0) {
		if (*pEnd == _T('\\')) {
			nLastIdx = nIdx;
		}
		pEnd++;
		nIdx++;
	}

	if (nIdx > 0 && nLastIdx > 0) {
		m_sEXEPath = CString(pStart, nLastIdx+1);
	} else {
		m_sEXEPath = _T(".\\");
	}

	// Check if a custom INI file name is specified on the command line
	m_sIniFileTitle = DEFAULT_INI_FILE_NAME;
	CString customIniFileName = ParseCommandLineForIniName(pEnd);
	if (!customIniFileName.IsEmpty() && customIniFileName.FindOneOf(_T("/\\")) == -1)
	{
		if (customIniFileName.GetLength() < 4 || customIniFileName.Right(4) != _T(".ini"))
			customIniFileName = customIniFileName + _T(".ini");
		m_sIniFileTitle = customIniFileName;
	}

	// Global INI file (at EXE location)
	m_sIniNameGlobal = m_sEXEPath + m_sIniFileTitle; // use current dir as startup path
	if (!customIniFileName.IsEmpty() && ::GetFileAttributes(m_sIniNameGlobal) == INVALID_FILE_ATTRIBUTES)
		m_sIniNameGlobal = m_sEXEPath + DEFAULT_INI_FILE_NAME;

	// Read if the user INI file shall be used
	m_bUserINIExists = false;
	m_pIniGlobalSectionBuffer = NULL;
	m_pIniUserSectionBuffer = NULL;
	m_pGlobalKeys = NULL;
	m_pUserKeys = NULL;
	m_sIniNameUser = m_sIniNameGlobal;
	m_bStoreToEXEPath = GetBool(_T("StoreToEXEPath"), false);

	if (!m_bStoreToEXEPath) {
		// User INI file
		m_sIniNameUser = CString(Helpers::JPEGViewAppDataPath()) + m_sIniFileTitle;
		m_bUserINIExists = (::GetFileAttributes(m_sIniNameUser) != INVALID_FILE_ATTRIBUTES);
	} else {
		Helpers::SetJPEGViewAppDataPath(m_sEXEPath);
	}

	// Get "My documents" path
	CString sMyDocumentsFolder;
	LPTSTR lpFolderBuffer = sMyDocumentsFolder.GetBuffer(MAX_PATH);
	::SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, lpFolderBuffer);
	sMyDocumentsFolder.ReleaseBuffer();

	// Get "My pictures" path
	CString sMyPicturesFolder;
	LPTSTR lpPicturesBuffer = sMyPicturesFolder.GetBuffer(MAX_PATH);
	::SHGetFolderPath(NULL, CSIDL_MYPICTURES, NULL, SHGFP_TYPE_CURRENT, lpPicturesBuffer);
	sMyPicturesFolder.ReleaseBuffer();

	// Read settings that can be written with SaveSettings()
	ReadWriteableINISettings();

	m_bAutoFullScreen = GetString(_T("ShowFullScreen"), _T("")).CompareNoCase(_T("auto")) == 0;
	m_bShowFullScreen = m_bAutoFullScreen ? true : GetBool(_T("ShowFullScreen"), true);
	m_bShowEXIFDateInTitle = GetBool(_T("ShowEXIFDateInTitle"), true);
	m_bShowHistogram = GetBool(_T("ShowHistogram"), false);
	m_bShowJPEGComments = GetBool(_T("ShowJPEGComments"), true);
	m_bShowBottomPanel = GetBool(_T("ShowBottomPanel"), true);
	m_bShowZoomNavigator = GetBool(_T("ShowZoomNavigator"), true);
	m_fBlendFactorNavPanel = (float)GetDouble(_T("BlendFactorNavPanel"), 0.5, 0.0, 1.0);
	m_fScaleFactorNavPanel = (float)GetDouble(_T("ScaleFactorNavPanel"), 1.0, 0.8, 2.5);

	CString sCPU = GetString(_T("CPUType"), _T("AutoDetect"));
	if (sCPU.CompareNoCase(_T("Generic")) == 0) {
		m_eCPUAlgorithm = Helpers::CPU_Generic;
	}
	else if (sCPU.CompareNoCase(_T("MMX")) == 0) {
		m_eCPUAlgorithm = Helpers::CPU_MMX;
	}
	else if (sCPU.CompareNoCase(_T("SSE")) == 0) {
		m_eCPUAlgorithm = Helpers::CPU_SSE;
	}
	else if (sCPU.CompareNoCase(_T("AVX2")) == 0) {
		m_eCPUAlgorithm = Helpers::CPU_AVX2;
	}
	else {
		m_eCPUAlgorithm = Helpers::ProbeCPU();
	}
	m_nNumCores = GetInt(_T("CPUCoresUsed"), 0, 0, 4);
	if (m_nNumCores == 0) {
		m_nNumCores = Helpers::NumCoresPerPhysicalProc();
		if (m_nNumCores > 4) m_nNumCores = 4;
	}

	CString sDownSampling = GetString(_T("DownSamplingFilter"), _T("BestQuality"));
	if (sDownSampling.CompareNoCase(_T("NoAliasing")) == 0) {
		m_eDownsamplingFilter = Filter_Downsampling_No_Aliasing;
	}
	else if (sDownSampling.CompareNoCase(_T("Narrow")) == 0) {
		m_eDownsamplingFilter = Filter_Downsampling_Narrow;
	}
	else {
		m_eDownsamplingFilter = Filter_Downsampling_Best_Quality;
	}

	m_bNavigateMouseWheel = GetBool(_T("NavigateWithMouseWheel"), false);
	m_dMouseWheelZoomSpeed = GetDouble(_T("MouseWheelZoomSpeed"), 1.0, 0.1, 10);

	CString sDeleteConfirmation = GetString(_T("DeleteConfirmation"), _T("OnlyWhenNoRecycleBin"));
	if (sDeleteConfirmation.CompareNoCase(_T("OnlyWhenNoRecycleBin")) == 0) {
		m_eDeleteConfirmation = Helpers::DC_OnlyWhenNoRecycleBin;
	}
	else if (sDeleteConfirmation.CompareNoCase(_T("Never")) == 0) {
		m_eDeleteConfirmation = Helpers::DC_Never;
	}
	else {
		m_eDeleteConfirmation = Helpers::DC_Always;
	}

	m_nMaxSlideShowFileListSize = GetInt(_T("MaxSlideShowFileListSizeKB"), 200, 100, 10000);
	m_nSlideShowEffectTimeMs = GetInt(_T("SlideShowEffectTime"), 200, 100, 5000);
	m_bForceGDIPlus = GetBool(_T("ForceGDIPlus"), false);
	m_bSingleInstance = GetBool(_T("SingleInstance"), false);
	m_bSingleFullScreenInstance = GetBool(_T("SingleFullScreenInstance"), true);
	m_nJPEGSaveQuality = GetInt(_T("JPEGSaveQuality"), 85, 0, 100);
	m_nWEBPSaveQuality = GetInt(_T("WEBPSaveQuality"), 85, 0, 100);
	m_sDefaultSaveFormat = GetString(_T("DefaultSaveFormat"), _T("jpg"));
	m_sFilesProcessedByWIC = GetString(_T("FilesProcessedByWIC"), _T("*.wdp;*.mdp;*.hdp"));
	m_sFileEndingsRAW = GetString(_T("FileEndingsRAW"), _T("*.pef;*.dng;*.crw;*.nef;*.cr2;*.mrw;*.rw2;*.orf;*.x3f;*.arw;*.kdc;*.nrw;*.dcr;*.sr2;*.raf"));
	m_bCreateParamDBEntryOnSave = GetBool(_T("CreateParamDBEntryOnSave"), true);
	m_bWrapAroundFolder = GetBool(_T("WrapAroundFolder"), true);
	m_bSaveWithoutPrompt = GetBool(_T("OverrideOriginalFileWithoutSaveDialog"), false);
	m_bTrimWithoutPromptLosslessJPEG = GetBool(_T("TrimWithoutPromptLosslessJPEG"), false);
	m_bAllowFileDeletion = GetBool(_T("AllowFileDeletion"), true);
	m_bExchangeXButtons = GetBool(_T("ExchangeXButtons"), true);
	m_bAutoRotateEXIF = GetBool(_T("AutoRotateEXIF"), true);
	m_bUseEmbeddedColorProfiles = GetBool(_T("UseEmbeddedColorProfiles"), false);
	m_nDisplayMonitor = GetInt(_T("DisplayMonitor"), -1, -1, 16);
	m_dAutoContrastAmount = GetDouble(_T("AutoContrastCorrectionAmount"), 0.5, 0.0, 1.0);
	m_dAutoBrightnessAmount = GetDouble(_T("AutoBrightnessCorrectionAmount"), 0.2, 0.0, 1.0);
	m_sLandscapeModeParams = GetString(_T("LandscapeModeParams"), _T("-1 -1 -1 -1 0.5 1.0 0.75 0.4 -1 -1 -1"));
	m_bLandscapeMode = GetBool(_T("LandscapeMode"), false);
	m_sCopyRenamePattern = GetString(_T("CopyRenamePattern"), _T(""));
	m_defaultWindowRect = GetRect(_T("DefaultWindowRect"), CRect(0, 0, 0, 0));
	m_stickyWindowRect = GetRect(_T("StickyWindowRect"), CRect(0, 0, 0, 0));
	m_bDefaultMaximized = false;
	m_bDefaultWndToImage = false;
	m_bStickyWindowSize = false;
	m_bExplicitWindowRect = false;
	if (m_defaultWindowRect.IsRectEmpty()) {
		CString sAuto = GetString(_T("DefaultWindowRect"), _T(""));
		if (sAuto.CompareNoCase(_T("max")) == 0) {
			m_bDefaultMaximized = true;
		}
		else if (sAuto.CompareNoCase(_T("image")) == 0) {
			m_bDefaultWndToImage = true;
		}
		else if (sAuto.CompareNoCase(_T("sticky")) == 0) {
			m_bStickyWindowSize = true;
			m_bExplicitWindowRect = !m_stickyWindowRect.IsRectEmpty();
		}
	}
	else {
		m_bExplicitWindowRect = true;
	}

	m_colorBackground = GetColor(_T("BackgroundColor"), 0);
	m_colorGUI = GetColor(_T("GUIColor"), RGB(243, 242, 231));
	m_colorHighlight = GetColor(_T("HighlightColor"), RGB(255, 205, 0));
	m_colorSelected = GetColor(_T("SelectionColor"), RGB(255, 205, 0));
	m_colorSlider = GetColor(_T("SliderColor"), RGB(255, 0, 80));
	m_colorFileName = GetColor(_T("FileNameColor"), m_colorGUI);
	m_colorTransparency = GetColor(_T("TransparencyColor"), m_colorBackground);

	m_defaultGUIFont = GetString(_T("DefaultGUIFont"), _T("Default"));
	m_fileNameFont = GetString(_T("FileNameFont"), _T("Default"));

	CString sUnsharpMaskParams = GetString(_T("UnsharpMaskParameters"), _T(""));
	float fRadius, fAmount, fThreshold;
	if (_stscanf(sUnsharpMaskParams, _T(" %f %f %f "), &fRadius, &fAmount, &fThreshold) == 3) {
		m_unsharpMaskParms.Radius = fRadius;
		m_unsharpMaskParms.Amount = fAmount;
		m_unsharpMaskParms.Threshold = fThreshold;
	}
	else {
		m_unsharpMaskParms.Radius = 1.0;
		m_unsharpMaskParms.Amount = 1.0;
		m_unsharpMaskParms.Threshold = 4.0;
	}
	m_bRTShowGridLines = GetBool(_T("RTShowGridLines"), true);
	m_bRTAutoCrop = GetBool(_T("RTAutoCrop"), true);
	m_bRTPreserveAspectRatio = GetBool(_T("RTPreserveAspectRatio"), true);
	m_sFileNameFormat = GetString(_T("FileNameFormat"), _T("%index% %filepath%"));
	m_sFileNameFormat = ReplacePlaceholdersFileNameFormat(m_sFileNameFormat);
	m_bReloadWhenDisplayedImageChanged = GetBool(_T("ReloadWhenDisplayedImageChanged"), true);
	m_bAllowEditGlobalSettings = GetBool(_T("AllowEditGlobalSettings"), false);
	m_dPrintMargin = GetDouble(_T("PrintMargin"), 1.0, 0.0, 100.0);
	m_dDefaultPrintWidth = GetDouble(_T("PrintWidth"), -15.0, -1000, 1000);

	m_sIniEditor = GetString(_T("IniEditor"), _T("notepad"));
	if (m_sIniEditor.CompareNoCase(_T("notepad")) == 0) {
		m_eIniEditor = Helpers::INI_Notepad;
	}
	else if (m_sIniEditor.CompareNoCase(_T("system")) == 0) {
		m_eIniEditor = Helpers::INI_System;
	}
	else {
		m_eIniEditor = Helpers::INI_Custom;
	}

	CString sUnit = GetString(_T("Units"), _T("auto"));
	if (sUnit.CompareNoCase(_T("auto")) == 0) {
		TCHAR buffer[2];
		::GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IMEASURE, (LPTSTR)&buffer, 2);
		buffer[1] = 0;
		m_eMeasureUnit = (buffer[0] == _T('1')) ? Helpers::MU_English : Helpers::MU_Metric;
	}
	else if (sUnit.CompareNoCase(_T("english")) == 0) {
		m_eMeasureUnit = Helpers::MU_English;
	}
	else {
		m_eMeasureUnit = Helpers::MU_Metric;
	}

	m_minimalWindowSize = GetSize(_T("MinimalWindowSize"), CSize(320, 240));
	m_minimalDisplayTime = GetDouble(_T("MinimalDisplayTime"), 0.0, 0, 1000);
	m_userCropAspectRatio = GetSize(_T("UserCropAspectRatio"), CSize(1, 1));
	if (m_userCropAspectRatio.cx <= 0 || m_userCropAspectRatio.cy <= 0) {
		m_userCropAspectRatio = CSize(1, 1);
	}

	m_sWallpaperPath = GetString(_T("WallpaperPath"), _T("%temp%"));
	if (m_sWallpaperPath == _T("%temp%")) {
		TCHAR tempPath[MAX_PATH];
		tempPath[0] = 0;
		::GetTempPath(MAX_PATH, tempPath);
		m_sWallpaperPath = tempPath;
	}
	else {
		TCHAR buffer[MAX_PATH];
		if (::ExpandEnvironmentStrings(m_sWallpaperPath, (LPTSTR)buffer, MAX_PATH)) {
			m_sWallpaperPath = buffer;
		}
	}

	m_bSkipFileOpenDialogOnStartup = GetBool(_T("SkipFileOpenDialogOnStartup"), false);

	m_sLanguage = GetString(_T("Language"), _T("auto")); 
	m_sGPSMapProvider = GetString(_T("GPSMapProvider"), _T("https://opentopomap.org/#marker=15/{lat}/{lng}"));

	m_sACCExclude = GetString(_T("ACCExclude"), _T("")); 
	m_sACCExclude.Replace(_T("%mydocuments%"), sMyDocumentsFolder);
	m_sACCExclude.Replace(_T("%mypictures%"), sMyPicturesFolder);
	m_sACCInclude = GetString(_T("ACCInclude"), _T("")); 
	m_sACCInclude.Replace(_T("%mydocuments%"), sMyDocumentsFolder);
	m_sACCInclude.Replace(_T("%mypictures%"), sMyPicturesFolder);
	m_sLDCExclude = GetString(_T("LDCExclude"), _T(""));
	m_sLDCExclude.Replace(_T("%mydocuments%"), sMyDocumentsFolder);
	m_sLDCExclude.Replace(_T("%mypictures%"), sMyPicturesFolder);
	m_sLDCInclude = GetString(_T("LDCInclude"), _T(""));
	m_sLDCInclude.Replace(_T("%mydocuments%"), sMyDocumentsFolder);
	m_sLDCInclude.Replace(_T("%mypictures%"), sMyPicturesFolder);

	CString sColorCorrections = GetString(_T("ColorCorrection"), _T(""));
	if (sColorCorrections.GetLength() > 0 &&
		_stscanf(sColorCorrections, _T("R: %f G: %f B: %f C: %f M: %f Y: %f"), 
		&m_fColorCorrections[0], &m_fColorCorrections[1], &m_fColorCorrections[2],
		&m_fColorCorrections[3], &m_fColorCorrections[4], &m_fColorCorrections[5]) == 6) {
		for (int i = 0; i < 6; i++) {
			m_fColorCorrections[i] = min(1.0f, max(0.0f, m_fColorCorrections[i]));
		}
	} else {
		for (int i = 0; i < 6; i++) {
			m_fColorCorrections[i] = -1;
		}
	}

	m_DefaultFixedCropSize = GetSize(_T("DefaultFixedCropSize"), CSize(320, 200));

	// read all user commands
	CString sCmd;
	int nIndex = 0;
	do {
		CString sKey;
		sKey.Format(_T("UserCmd%d"), nIndex++);
		sCmd = GetString(sKey, _T(""));
		if (sCmd.GetLength() > 0) {
			CUserCommand* pUserCmd = new CUserCommand(nIndex - 1, sCmd, true);
			if (pUserCmd->IsValid()) {
				m_userCommands.push_back(pUserCmd);
			}
		}
	} while (sCmd.GetLength() > 0 || nIndex <= 3);

	// read all open with commands
	nIndex = 0;
	m_nNextOpenWithIndex = 0;
	int nGapIndex = 0;
	do {
		CString sKey;
		sKey.Format(_T("OpenWith%d"), nIndex++);
		sCmd = GetString(sKey, _T(""));
		if (!sCmd.IsEmpty()) {
			if (sCmd != _T("[deleted]")) {
				CUserCommand* pOpenWithCmd = new CUserCommand(nIndex - 1, sCmd, false);
				if (pOpenWithCmd->IsValid()) {
					m_openWithCommands.push_back(pOpenWithCmd);
				}
			}
			m_nNextOpenWithIndex = nIndex;
			nGapIndex = 0;
		} else {
			nGapIndex++;
		}
	} while (nGapIndex <= 2);
	
}

CImageProcessingParams CSettingsProvider::LandscapeModeParams(const CImageProcessingParams& templParams) {
	const float cfUndefined = -1;
	const int cnParams = 12;
	float fParams[cnParams];
	for (int i = 0; i < cnParams; i++) fParams[i] = cfUndefined;
	_stscanf(m_sLandscapeModeParams, _T("%f %f %f %f %f %f %f %f %f %f %f %f"), &fParams[0], &fParams[1], &fParams[2], 
		&fParams[3], &fParams[4], &fParams[5], &fParams[6], &fParams[7], &fParams[8], &fParams[9], &fParams[10], &fParams[11]);
	return CImageProcessingParams(
		(fParams[0] == cfUndefined) ? templParams.Contrast : fParams[0],
		(fParams[1] == cfUndefined) ? templParams.Gamma : fParams[1],
		(fParams[11] == cfUndefined) ? templParams.Saturation : fParams[11],
		(fParams[2] == cfUndefined) ? templParams.Sharpen : fParams[2],
		(fParams[3] == cfUndefined) ? templParams.ColorCorrectionFactor : fParams[3],
		(fParams[4] == cfUndefined) ? templParams.ContrastCorrectionFactor : fParams[4],
		(fParams[5] == cfUndefined) ? templParams.LightenShadows : fParams[5],
		(fParams[6] == cfUndefined) ? templParams.DarkenHighlights : fParams[6],
		(fParams[7] == cfUndefined) ? templParams.LightenShadowSteepness : fParams[7],
		(fParams[8] == cfUndefined) ? templParams.CyanRed : fParams[8],
		(fParams[9] == cfUndefined) ? templParams.MagentaGreen : fParams[9],
		(fParams[10] == cfUndefined) ? templParams.YellowBlue : fParams[10]);
}

void CSettingsProvider::ReadWriteableINISettings() {
	m_dContrast = GetDouble(_T("Contrast"), 0.0, -0.5, 0.5);
	m_dGamma = GetDouble(_T("Gamma"), 1.0, 0.1, 10.0);
	m_dSaturation = GetDouble(_T("Saturation"), 1.0, 0.0, 2.0);
	m_dSharpen = GetDouble(_T("Sharpen"), 0.3, 0.0, 0.5);
	m_dCyanRed = GetDouble(_T("CyanRed"), 0.0, -1.0, 1.0);
	m_dMagentaGreen = GetDouble(_T("MagentaGreen"), 0.0, -1.0, 1.0);
	m_dYellowBlue = GetDouble(_T("YellowBlue"), 0.0, -1.0, 1.0);
	m_bAutoContrastCorrection = GetBool(_T("AutoContrastCorrection"), false);
	m_bLocalDensityCorrection = GetBool(_T("LocalDensityCorrection"), false);
	m_dBrightenShadows = GetDouble(_T("LDCBrightenShadows"), 0.5, 0.0, 1.0);
	m_dDarkenHighlights = GetDouble(_T("LDCDarkenHighlights"), 0.25, 0.0, 1.0);
	m_dBrightenShadowsSteepness = GetDouble(_T("LDCBrightenShadowsSteepness"), 0.5, 0.0, 1.0);
	CString sNavigation = GetString(_T("FolderNavigation"), _T("LoopFolder"));
	if (sNavigation.CompareNoCase(_T("LoopSameFolderLevel")) == 0) {
		m_eNavigation = Helpers::NM_LoopSameDirectoryLevel;
	}
	else if (sNavigation.CompareNoCase(_T("LoopSubFolders")) == 0) {
		m_eNavigation = Helpers::NM_LoopSubDirectories;
	}
	else {
		m_eNavigation = Helpers::NM_LoopDirectory;
	}
	CString sSorting = GetString(_T("FileDisplayOrder"), _T("LastModDate"));
	if (sSorting.CompareNoCase(_T("CreationDate")) == 0) {
		m_eSorting = Helpers::FS_CreationTime;
	}
	else if (sSorting.CompareNoCase(_T("FileName")) == 0) {
		m_eSorting = Helpers::FS_FileName;
	}
	else if (sSorting.CompareNoCase(_T("Random")) == 0) {
		m_eSorting = Helpers::FS_Random;
	}
	else if (sSorting.CompareNoCase(_T("FileSize")) == 0) {
		m_eSorting = Helpers::FS_FileSize;
	}
	else {
		m_eSorting = Helpers::FS_LastModTime;
	}
	m_bIsSortedUpcounting = GetBool(_T("IsSortedUpcounting"), true);
	m_eAutoZoomMode = GetAutoZoomMode(_T("AutoZoomMode"), Helpers::ZM_FitToScreenNoZoom);
	m_eAutoZoomModeFullscreen = GetAutoZoomMode(_T("AutoZoomModeFullscreen"), m_eAutoZoomMode);
	m_bShowNavPanel = GetBool(_T("ShowNavPanel"), true);

	m_bHQRS = GetBool(_T("HighQualityResampling"), true);
	m_bShowFileName = GetBool(_T("ShowFileName"), false);
	m_bShowFileInfo = GetBool(_T("ShowFileInfo"), false);
	m_bKeepParams = GetBool(_T("KeepParameters"), false);
	m_eSlideShowTransitionEffect = Helpers::ConvertTransitionEffectFromString(GetString(_T("SlideShowTransitionEffect"), _T("")));
}

void CSettingsProvider::SaveSettings(const CImageProcessingParams& procParams, 
									 EProcessingFlags eProcFlags,
									 Helpers::ENavigationMode eNavigationMode, Helpers::ESorting eFileSorting, bool isSortedUpcounting,
									 Helpers::EAutoZoomMode eAutoZoomMode, Helpers::EAutoZoomMode eAutoZoomModeFullScreen,
									 bool bShowNavPanel, bool bShowFileName, bool bShowFileInfo,
									 Helpers::ETransitionEffect eSlideShowTransitionEffect) {
	MakeSureUserINIExists();

	WriteDouble(_T("Contrast"), procParams.Contrast);
	WriteDouble(_T("Gamma"), procParams.Gamma);
	WriteDouble(_T("Saturation"), procParams.Saturation);
	WriteDouble(_T("Sharpen"), procParams.Sharpen);
	WriteDouble(_T("CyanRed"), procParams.CyanRed);
	WriteDouble(_T("MagentaGreen"), procParams.MagentaGreen);
	WriteDouble(_T("YellowBlue"), procParams.YellowBlue);
	WriteBool(_T("AutoContrastCorrection"), GetProcessingFlag(eProcFlags, PFLAG_AutoContrast));
	WriteBool(_T("LocalDensityCorrection"), GetProcessingFlag(eProcFlags, PFLAG_LDC));
	if (GetProcessingFlag(eProcFlags, PFLAG_LDC)) {
		WriteDouble(_T("LDCBrightenShadows"), procParams.LightenShadows);
		WriteDouble(_T("LDCDarkenHighlights"), procParams.DarkenHighlights);
		WriteDouble(_T("LDCBrightenShadowsSteepness"), procParams.LightenShadowSteepness);
	}

	LPCTSTR sNavigation = _T("LoopFolder");
	if (eNavigationMode == Helpers::NM_LoopSameDirectoryLevel) {
		sNavigation = _T("LoopSameFolderLevel");
	}
	else if (eNavigationMode == Helpers::NM_LoopSubDirectories) {
		sNavigation = _T("LoopSubFolders");
	}
	WriteString(_T("FolderNavigation"), sNavigation);

	LPCTSTR sSorting = _T("FileName");
	if (eFileSorting == Helpers::FS_CreationTime) {
		sSorting = _T("CreationDate");
	} else if (eFileSorting == Helpers::FS_LastModTime) {
		sSorting = _T("LastModDate");
	} else if (eFileSorting == Helpers::FS_Random) {
		sSorting = _T("Random");
	} else if (eFileSorting == Helpers::FS_FileSize) {
		sSorting = _T("FileSize");
	}
	WriteString(_T("FileDisplayOrder"), sSorting);

	WriteBool(_T("IsSortedUpcounting"), isSortedUpcounting);

	WriteString(_T("AutoZoomMode"), GetAutoZoomModeString(eAutoZoomMode));
	WriteString(_T("AutoZoomModeFullscreen"), GetAutoZoomModeString(eAutoZoomModeFullScreen));

	WriteBool(_T("ShowNavPanel"), bShowNavPanel);

	WriteBool(_T("HighQualityResampling"), GetProcessingFlag(eProcFlags, PFLAG_HighQualityResampling));
	WriteBool(_T("KeepParameters"), GetProcessingFlag(eProcFlags, PFLAG_KeepParams));
	WriteBool(_T("ShowFileName"), bShowFileName);
	WriteBool(_T("ShowFileInfo"), bShowFileInfo);

	WriteString(_T("SlideShowTransitionEffect"), Helpers::ConvertTransitionEffectToString(eSlideShowTransitionEffect));

	m_bUserINIExists = true;
	ReadWriteableINISettings();
}

void CSettingsProvider::SaveCopyRenamePattern(const CString& sPattern) {
	MakeSureUserINIExists();

	m_sCopyRenamePattern = sPattern;
	WriteString(_T("CopyRenamePattern"), sPattern);

	m_bUserINIExists = true;
}

void CSettingsProvider::SaveStickyWindowRect(CRect rect) {
	if (rect != m_stickyWindowRect && !rect.IsRectEmpty()) {
		const int BUFF_SIZE = 64;
		TCHAR buff[BUFF_SIZE];
		_sntprintf(buff, BUFF_SIZE, _T("%d %d %d %d"), rect.left, rect.top, rect.right, rect.bottom);

		MakeSureUserINIExists();

		WriteString(_T("StickyWindowRect"), buff);

		m_bUserINIExists = true;
	}
}

bool CSettingsProvider::ExistsUserINI() {
	LPCTSTR sINIFileName = GetUserINIFileName();
	return ::GetFileAttributes(sINIFileName) != INVALID_FILE_ATTRIBUTES;
}

void CSettingsProvider::CopyUserINIFromTemplate() {
	if (m_bStoreToEXEPath) {
		return; // no user INI file
	}

	LPCTSTR sINIFileName = GetUserINIFileName();
	::CreateDirectory(Helpers::JPEGViewAppDataPath(), NULL);
	::CopyFile(GetINITemplateName(), sINIFileName, TRUE);
}

void CSettingsProvider::UpdateUserSettings() {
	if (m_bStoreToEXEPath) {
		return; // no user INI file
	}

	CString sINIFileName = CString(GetUserINIFileName());
	::DeleteFile(sINIFileName + _T(".old"));
	if (!ExistsUserINI()) {
		CopyUserINIFromTemplate();
		return;
	}

	const int BUFFER_LEN = 16000;
	TCHAR* buffer = new TCHAR[BUFFER_LEN];
	if (::GetPrivateProfileString(SECTION_NAME, NULL, _T(""), buffer, BUFFER_LEN, sINIFileName) == BUFFER_LEN - 2) {
		// buffer was too small, maybe INI file corrupt, do not touch
		delete[] buffer;
		return;
	}

	CString newINIFileName = sINIFileName + _T(".new");
	if (!::CopyFile(GetINITemplateName(), newINIFileName, FALSE)){
		delete[] buffer;
		return;
	}

	TCHAR* currentKey = buffer;
	while (*currentKey != 0) {
		CString value = GetString(currentKey, _T(""));
		if (!::WritePrivateProfileString(SECTION_NAME, currentKey, value, newINIFileName)) {
			delete[] buffer;
			return;
		}
		// go to next key
		while (*currentKey != 0) currentKey++;
		currentKey++;
	}

	delete[] buffer;
	if (::MoveFile(sINIFileName, sINIFileName + _T(".old"))) {
		::MoveFile(newINIFileName, sINIFileName);
	}
}

int CSettingsProvider::DeleteOpenWithCommand(CUserCommand* pCommand) {
	MakeSureUserINIExists();

	int index = 0;
	std::list<CUserCommand*>::iterator iter;
	for (iter = m_openWithCommands.begin( ); iter != m_openWithCommands.end( ); iter++ ) {
		if (*iter == pCommand)
			break;
		index++;
	}

	if (index < m_openWithCommands.size()) {
		CString sKey;
		sKey.Format(_T("OpenWith%d"), pCommand->GetIndex());

		WriteString(sKey, _T("[deleted]"));

		m_openWithCommands.remove(pCommand);
		delete pCommand;

		m_bUserINIExists = true;
		return index;
	}

	return -1;
}

void CSettingsProvider::AddOpenWithCommand(const CString& sCommandString) {
	MakeSureUserINIExists();

	CUserCommand* pNewCommand = new CUserCommand(m_nNextOpenWithIndex, sCommandString, false);
	m_openWithCommands.push_back(pNewCommand);

	CString sKey;
	sKey.Format(_T("OpenWith%d"), m_nNextOpenWithIndex);

	WriteString(sKey, sCommandString);

	m_bUserINIExists = true;
	m_nNextOpenWithIndex++;
}

void CSettingsProvider::ChangeOpenWithCommand(CUserCommand* pCommand, const CString& sCommandString) {
	CUserCommand* pChangedCommand = new CUserCommand(pCommand->GetIndex(), sCommandString, false);
	m_openWithCommands.insert(std::find(m_openWithCommands.begin(), m_openWithCommands.end(), pCommand), pChangedCommand);
	m_openWithCommands.remove(pCommand);
	delete pCommand;

	CString sKey;
	sKey.Format(_T("OpenWith%d"), pChangedCommand->GetIndex());

	WriteString(sKey, sCommandString);
}

CString CSettingsProvider::ReplacePlaceholdersFileNameFormat(const CString& sFileNameFormat) {
	// needed because a file name may contains the original placeholders from INI file
	CString sNewString = sFileNameFormat;
	sNewString.Replace(_T("%filename%"), _T("<f>"));
	sNewString.Replace(_T("%filepath%"), _T("<p>"));
	sNewString.Replace(_T("%index%"), _T("<i>"));
	sNewString.Replace(_T("%zoom%"), _T("<z>"));
	sNewString.Replace(_T("%size%"), _T("<s>"));
	sNewString.Replace(_T("%filesize%"), _T("<l>"));
	return sNewString;
}

void CSettingsProvider::MakeSureUserINIExists() {
	if (m_bStoreToEXEPath) {
		return; // no user INI file needed
	}

	// Create JPEGView appdata directory and copy INI file if it does not exist
	::CreateDirectory(Helpers::JPEGViewAppDataPath(), NULL);
	if (::GetFileAttributes(m_sIniNameUser) == INVALID_FILE_ATTRIBUTES) {
		::CopyFile(GetINITemplateName(), m_sIniNameUser, TRUE);
	}
}

CString CSettingsProvider::GetINITemplateName() {
	CString defaultGlobalIniName = m_sEXEPath + DEFAULT_INI_FILE_NAME;
	CString localizedTemplate = CNLS::GetLocalizedFileName(defaultGlobalIniName, _T(""), _T("tpl"), Language());
	if (::GetFileAttributes(localizedTemplate) != INVALID_FILE_ATTRIBUTES) {
		return localizedTemplate;
	}
	return defaultGlobalIniName + _T(".tpl");
}

LPCTSTR CSettingsProvider::ReadUserIniString(LPCTSTR key) {
	return ReadIniString(key, m_sIniNameUser, m_pUserKeys, m_pIniUserSectionBuffer);
}

LPCTSTR CSettingsProvider::ReadGlobalIniString(LPCTSTR key) {
	return ReadIniString(key, m_sIniNameGlobal, m_pGlobalKeys, m_pIniGlobalSectionBuffer);
}

LPCTSTR CSettingsProvider::ReadIniString(LPCTSTR key, LPCTSTR fileName, IniHashMap*& keyMap, TCHAR*& pBuffer) {
	if (keyMap == NULL) {
		keyMap = new IniHashMap();
		ReadIniFile(m_sIniNameUser, keyMap, pBuffer);
	}
	hash_map<LPCTSTR, LPCTSTR, CHashCompareLPCTSTR>::const_iterator iter;
	iter = keyMap->find(key);
	if (iter == keyMap->end()) {
		return NULL; // not found
	}
	else {
		return iter->second;
	}
}

void CSettingsProvider::ReadIniFile(LPCTSTR fileName, IniHashMap* keyMap, TCHAR*& pBuffer) {
	int bufferSize = 1024 * 2;

	pBuffer = NULL;
	int actualSize;
	do {
		delete[] pBuffer;
		bufferSize = bufferSize * 2;
		pBuffer = new TCHAR[bufferSize];
		actualSize = ::GetPrivateProfileSection(SECTION_NAME, pBuffer, bufferSize, fileName);
	} while (actualSize == bufferSize - 2);

	int index = 0;
	LPTSTR current = pBuffer;
	while (*current != 0) {
		while (*current != 0 && _istspace(*current)) current++;
		LPCTSTR key = current;
		while (*current != 0 && !_istspace(*current) && *current != _T('=')) current++;
		LPCTSTR value = current;
		if (*current != 0) {
			*current++ = 0;
			while (*current != 0 && _istspace(*current)) current++;
			value = current;
		}
		if (*key != 0) {
			(*keyMap)[key] = value;
		}
		current += _tcslen(value) + 1;
	}
}

CString CSettingsProvider::GetString(LPCTSTR sKey, LPCTSTR sDefault) {
	if (m_bUserINIExists) {
		// try first user path
		LPCTSTR value = ReadUserIniString(sKey);
		if (value != NULL) {
			return CString(value);
		}
	}
	// finally global path if not found in user path
	LPCTSTR value = ReadGlobalIniString(sKey);
	if (value == NULL) {
		return CString(sDefault);
	}
	return CString(value);
}

int CSettingsProvider::GetInt(LPCTSTR sKey, int nDefault, int nMin, int nMax) {
	CString s = GetString(sKey, _T(""));
	if (s.IsEmpty()) {
		return nDefault;
	}
	int nValue = (int)_wtof((LPCTSTR)s);
	return min(nMax, max(nMin, nValue));
}

double CSettingsProvider::GetDouble(LPCTSTR sKey, double dDefault, double dMin, double dMax) {
	CString s = GetString(sKey, _T(""));
	if (s.IsEmpty()) {
		return dDefault;
	}
	double dValue = _wtof((LPCTSTR)s);
	return min(dMax, max(dMin, dValue));
}

bool CSettingsProvider::GetBool(LPCTSTR sKey, bool bDefault) {
	CString s = GetString(sKey, _T(""));
	if (s.IsEmpty()) {
		return bDefault;
	}
	if (s.CompareNoCase(_T("true")) == 0) {
		return true;
	} else if (s.CompareNoCase(_T("false")) == 0) {
		return false;
	} else {
		return bDefault;
	}
}

CRect CSettingsProvider::GetRect(LPCTSTR sKey, const CRect& defaultRect) {
	CString s = GetString(sKey, _T(""));
	if (s.IsEmpty()) {
		return defaultRect;
	}
	int nLeft, nTop, nRight, nBottom;
	if (_stscanf((LPCTSTR)s, _T(" %d %d %d %d "), &nLeft, &nTop, &nRight, &nBottom) == 4) {
		CRect newRect = CRect(nLeft, nTop, nRight, nBottom);
		newRect.NormalizeRect();
		return newRect;
	} else {
		return defaultRect;
	}
}

CSize CSettingsProvider::GetSize(LPCTSTR sKey, const CSize& defaultSize) {
	CString s = GetString(sKey, _T(""));
	if (s.IsEmpty()) {
		return defaultSize;
	}
	int nWidth, nHeight;
	if (_stscanf((LPCTSTR)s, _T(" %d %d "), &nWidth, &nHeight) == 2) {
		nWidth = max(1, nWidth);
		nHeight = max(1, nHeight);
		return CSize(nWidth, nHeight);
	} else {
		return defaultSize;
	}
}

COLORREF CSettingsProvider::GetColor(LPCTSTR sKey, COLORREF defaultColor) {
	int nRed, nGreen, nBlue;
	CString sColor = GetString(sKey, _T(""));
	if (sColor.IsEmpty()) {
		return defaultColor;
	}
	if (_stscanf(sColor, _T(" %d %d %d"), &nRed, &nGreen, &nBlue) == 3) {
		return RGB(nRed, nGreen, nBlue);
	} else {
		return defaultColor;
	}
}

Helpers::EAutoZoomMode CSettingsProvider::GetAutoZoomMode(LPCTSTR sKey, Helpers::EAutoZoomMode defaultZoomMode) {
	CString sAutoZoomMode = GetString(sKey, _T("FitNoZoom"));
	if (sAutoZoomMode.CompareNoCase(_T("Fit")) == 0) {
		return Helpers::ZM_FitToScreen;
	}
	else if (sAutoZoomMode.CompareNoCase(_T("Fill")) == 0) {
		return Helpers::ZM_FillScreen;
	}
	else if (sAutoZoomMode.CompareNoCase(_T("FillNoZoom")) == 0) {
		return Helpers::ZM_FillScreenNoZoom;
	}
	else {
		return defaultZoomMode;
	}
}

LPCTSTR CSettingsProvider::GetAutoZoomModeString(Helpers::EAutoZoomMode autoZoomMode) {
	if (autoZoomMode == Helpers::ZM_FillScreen) {
		return _T("Fill");
	}
	else if (autoZoomMode == Helpers::ZM_FitToScreen) {
		return _T("Fit");
	}
	else if (autoZoomMode == Helpers::ZM_FillScreenNoZoom) {
		return _T("FillNoZoom");
	}
	return _T("FitNoZoom");
}

void CSettingsProvider::WriteString(LPCTSTR sKey, LPCTSTR sString) {
	::WritePrivateProfileString(SECTION_NAME, sKey, sString, m_sIniNameUser);
}

void CSettingsProvider::WriteDouble(LPCTSTR sKey, double dValue) {
	TCHAR buff[32];
	_stprintf_s(buff, 32, _T("%.2f"), dValue);
	WriteString(sKey, buff);
}

void CSettingsProvider::WriteBool(LPCTSTR sKey, bool bValue) {
	WriteString(sKey, bValue ? _T("true") : _T("false"));
}

void CSettingsProvider::WriteInt(LPCTSTR sKey, int nValue) {
	TCHAR buff[32];
	_stprintf_s(buff, 32, _T("%d"), nValue);
	WriteString(sKey, buff);
}