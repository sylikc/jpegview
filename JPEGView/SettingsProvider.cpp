#include "StdAfx.h"
#include "SettingsProvider.h"
#include <float.h>
#include <shlobj.h>

static const TCHAR* INI_FILE_NAME = _T("JPEGView.ini");
static const TCHAR* SECTION_NAME = _T("JPEGView");

static float COLOR_CORR_DEFAULT_FACTORS[6] = {0.2f, 0.1f, 0.3f, 0.3f, 0.3f, 0.15f};

CSettingsProvider* CSettingsProvider::sm_instance;

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

	// Global INI file (at EXE location)
	if (nIdx > 0 && nLastIdx > 0) {
		m_sEXEPath = CString(pStart, nLastIdx+1);
		m_sIniNameGlobal = m_sEXEPath + INI_FILE_NAME;
	} else {
		m_sEXEPath = _T(".\\");
		m_sIniNameGlobal = m_sEXEPath + INI_FILE_NAME; // use current dir as startup path
	}

	// User INI file
	m_sIniNameUser = CString(Helpers::JPEGViewAppDataPath()) + INI_FILE_NAME;
	m_bUserINIExists = (::GetFileAttributes(m_sIniNameUser) != INVALID_FILE_ATTRIBUTES);

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

	// read all user commands
	CString sCmd;
	int nIndex = 0;
	do {
		CString sKey;
		sKey.Format(_T("UserCmd%d"), nIndex++);
		sCmd = GetString(sKey, _T(""));
		if (sCmd.GetLength() > 0) {
			CUserCommand* pUserCmd = new CUserCommand(sCmd);
			if (pUserCmd->IsValid()) {
				m_userCommands.push_back(pUserCmd);
			}
		}
	} while (sCmd.GetLength() > 0);
}

void CSettingsProvider::ReadWriteableINISettings() {
	m_dContrast = GetDouble(_T("Contrast"), 0.0, -0.5, 0.5);
	m_dGamma = GetDouble(_T("Gamma"), 1.0, 0.1, 10.0);
	m_dSharpen = GetDouble(_T("Sharpen"), 0.3, 0.0, 0.5);
	m_dCyanRed = GetDouble(_T("CyanRed"), 0.0, -1.0, 1.0);
	m_dMagentaGreen = GetDouble(_T("MagentaGreen"), 0.0, -1.0, 1.0);
	m_dYellowBlue = GetDouble(_T("YellowBlue"), 0.0, -1.0, 1.0);
	m_bHQRS = GetBool(_T("HighQualityResampling"), true);
	m_bShowFileName = GetBool(_T("ShowFileName"), false);
	m_bShowFileInfo = GetBool(_T("ShowFileInfo"), false);
	m_bShowNavPanel = GetBool(_T("ShowNavPanel"), true);
	m_fBlendFactorNavPanel = (float) GetDouble(_T("BlendFactorNavPanel"), 0.5, 0.0, 1.0);
	m_bKeepParams = GetBool(_T("KeepParameters"), false);
	
	CString sCPU = GetString(_T("CPUType"), _T("AutoDetect"));
	if (sCPU.CompareNoCase(_T("Generic")) == 0) {
		m_eCPUAlgorithm = Helpers::CPU_Generic;
	} else if (sCPU.CompareNoCase(_T("MMX")) == 0) {
		m_eCPUAlgorithm = Helpers::CPU_MMX;
	} else if (sCPU.CompareNoCase(_T("SSE")) == 0) {
		m_eCPUAlgorithm = Helpers::CPU_SSE;
	} else {
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
	} else if (sDownSampling.CompareNoCase(_T("Narrow")) == 0) {
		m_eDownsamplingFilter = Filter_Downsampling_Narrow;
	} else {
		m_eDownsamplingFilter = Filter_Downsampling_Best_Quality;
	}

	CString sSorting = GetString(_T("FileDisplayOrder"), _T("LastModDate"));
	if (sSorting.CompareNoCase(_T("CreationDate")) == 0) {
		m_eSorting = Helpers::FS_CreationTime;
	} else if (sSorting.CompareNoCase(_T("FileName")) == 0) {
		m_eSorting = Helpers::FS_FileName;
	} else {
		m_eSorting = Helpers::FS_LastModTime;
	}
	
	CString sNavigation = GetString(_T("FolderNavigation"), _T("LoopFolder"));
	if (sNavigation.CompareNoCase(_T("LoopSameFolderLevel")) == 0) {
		m_eNavigation = Helpers::NM_LoopSameDirectoryLevel;
	} else if (sNavigation.CompareNoCase(_T("LoopSubFolders")) == 0) {
		m_eNavigation = Helpers::NM_LoopSubDirectories;
	} else {
		m_eNavigation = Helpers::NM_LoopDirectory;
	}

	CString sAutoZoomMode = GetString(_T("AutoZoomMode"), _T("FitNoZoom"));
	if (sAutoZoomMode.CompareNoCase(_T("Fit")) == 0) {
		m_eAutoZoomMode = Helpers::ZM_FitToScreen;
	} else if (sAutoZoomMode.CompareNoCase(_T("Fill")) == 0) {
		m_eAutoZoomMode = Helpers::ZM_FillScreen;
	} else if (sAutoZoomMode.CompareNoCase(_T("FillNoZoom")) == 0) {
		m_eAutoZoomMode = Helpers::ZM_FillScreenNoZoom;
	} else {
		m_eAutoZoomMode = Helpers::ZM_FitToScreenNoZoom;
	}

	m_nJPEGSaveQuality = GetInt(_T("JPEGSaveQuality"), 85, 0, 100);
	m_bCreateParamDBEntryOnSave = GetBool(_T("CreateParamDBEntryOnSave"), true);
	m_nDisplayMonitor = GetInt(_T("DisplayMonitor"), -1, -1, 16);
	m_bAutoContrastCorrection = GetBool(_T("AutoContrastCorrection"), false);
	m_dAutoContrastAmount = GetDouble(_T("AutoContrastCorrectionAmount"), 0.5, 0.0, 1.0);
	m_dAutoBrightnessAmount = GetDouble(_T("AutoBrightnessCorrectionAmount"), 0.2, 0.0, 1.0);
	m_bLocalDensityCorrection = GetBool(_T("LocalDensityCorrection"), false);
	m_dBrightenShadows = GetDouble(_T("LDCBrightenShadows"), 0.5, 0.0, 1.0);
	m_dDarkenHighlights = GetDouble(_T("LDCDarkenHighlights"), 0.25, 0.0, 1.0);
	m_dBrightenShadowsSteepness = GetDouble(_T("LDCBrightenShadowsSteepness"), 0.5, 0.0, 1.0);
	m_sCopyRenamePattern = GetString(_T("CopyRenamePattern"), _T(""));
}

void CSettingsProvider::SaveSettings(const CImageProcessingParams& procParams, 
									 EProcessingFlags eProcFlags, Helpers::ESorting eFileSorting,
									 Helpers::EAutoZoomMode eAutoZoomMode,
									 bool bShowNavPanel) {
	SHCreateDirectoryEx(NULL, Helpers::JPEGViewAppDataPath(), NULL);

	WriteDouble(_T("Contrast"), procParams.Contrast);
	WriteDouble(_T("Gamma"), procParams.Gamma);
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
	LPCTSTR sSorting = _T("FileName");
	if (eFileSorting == Helpers::FS_CreationTime) {
		sSorting = _T("CreationDate");
	} else if (eFileSorting == Helpers::FS_LastModTime) {
		sSorting = _T("LastModDate");
	}
	WriteString(_T("FileDisplayOrder"), sSorting);

	LPCTSTR sAutoZoomMode = _T("FitNoZoom");
	if (eAutoZoomMode == Helpers::ZM_FillScreen) {
		sAutoZoomMode = _T("Fill");
	} else if (eAutoZoomMode == Helpers::ZM_FitToScreen) {
		sAutoZoomMode = _T("Fit");
	} else if (eAutoZoomMode == Helpers::ZM_FillScreenNoZoom) {
		sAutoZoomMode = _T("FillNoZoom");
	}
	WriteString(_T("AutoZoomMode"), sAutoZoomMode);

	WriteBool(_T("ShowNavPanel"), bShowNavPanel);

	m_bUserINIExists = true;
	ReadWriteableINISettings();
}

void CSettingsProvider::SaveCopyRenamePattern(const CString& sPattern) {
	SHCreateDirectoryEx(NULL, Helpers::JPEGViewAppDataPath(), NULL);

	m_sCopyRenamePattern = sPattern;
	WriteString(_T("CopyRenamePattern"), sPattern);
}

CString CSettingsProvider::GetString(LPCTSTR sKey, LPCTSTR sDefault) {
	const int BUFF_LEN = 1024;
	TCHAR buff[BUFF_LEN];
	buff[BUFF_LEN-1] = 0;
	if (m_bUserINIExists) {
		// try first user path
		::GetPrivateProfileString(SECTION_NAME, sKey, _T(""), buff, BUFF_LEN - 1, m_sIniNameUser);
		if (buff[0] != 0) {
			return CString(buff);
		}
	}
	// finally global path if not found in user path
	::GetPrivateProfileString(SECTION_NAME, sKey, _T(""), buff, BUFF_LEN - 1, m_sIniNameGlobal);
	if (buff[0] == 0) {
		return CString(sDefault);
	}
	return CString(buff);
}

int CSettingsProvider::GetInt(LPCTSTR sKey, int nDefault, int nMin, int nMax) {
	CString s = GetString(sKey, _T(""));
	if (s.IsEmpty()) {
		return nDefault;
	}
#ifdef _UNICODE
	int nValue = (int)_wtof((LPCTSTR)s);
#else
	int nValue = atoi((LPCTSTR)s);
#endif
	return min(nMax, max(nMin, nValue));
}

double CSettingsProvider::GetDouble(LPCTSTR sKey, double dDefault, double dMin, double dMax) {
	CString s = GetString(sKey, _T(""));
	if (s.IsEmpty()) {
		return dDefault;
	}
#ifdef _UNICODE
	double dValue = _wtof((LPCTSTR)s);
#else
	double dValue = atof((LPCTSTR)s);
#endif
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