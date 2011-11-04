#pragma once

#include "Helpers.h"
#include "FileList.h"
#include "UserCommand.h"
#include "ProcessParams.h"

// INI settings
// All settings are first searched in a file name JPEGView.ini located in User/AppData/JPEGView
// If not found there, the setting is searched in JPEGView.ini located in the binary path of the application
class CSettingsProvider
{
public:
	// Singleton instance
	static CSettingsProvider& This();

	// The methods correspond to the INI file settings
	bool StoreToEXEPath() { return m_bStoreToEXEPath; }
	double Contrast() { return m_dContrast; }
	double Gamma() { return m_dGamma; }
	double Saturation() { return m_dSaturation; }
	double Sharpen() { return m_dSharpen; }
	double CyanRed() { return m_dCyanRed; }
	double MagentaGreen() { return m_dMagentaGreen; }
	double YellowBlue() { return m_dYellowBlue; }
	bool HighQualityResampling() { return m_bHQRS; }
	bool ShowFileName() { return m_bShowFileName; }
	bool ShowFileInfo() { return m_bShowFileInfo; }
	bool ShowHistogram() { return m_bShowHistogram; }
	bool ShowJPEGComments() { return m_bShowJPEGComments; }
	bool ShowNavPanel() { return m_bShowNavPanel; }
	bool ShowZoomNavigator() { return m_bShowZoomNavigator; }
	bool ShowFullScreen() { return m_bShowFullScreen; }
	bool AutoFullScreen() { return m_bAutoFullScreen; }
	float BlendFactorNavPanel() { return m_fBlendFactorNavPanel; }
	bool KeepParams() { return m_bKeepParams; }
	LPCTSTR Language() { return m_sLanguage; }
	Helpers::CPUType AlgorithmImplementation() { return m_eCPUAlgorithm; }
	int NumberOfCoresToUse() { return m_nNumCores; }
	EFilterType DownsamplingFilter() { return m_eDownsamplingFilter; }
	Helpers::ESorting Sorting() { return m_eSorting; }
	Helpers::ENavigationMode Navigation() { return m_eNavigation; }
	bool NavigateWithMouseWheel() { return m_bNavigateMouseWheel; }
	Helpers::EAutoZoomMode AutoZoomMode() { return m_eAutoZoomMode; }
	int DisplayMonitor() { return m_nDisplayMonitor; }
	bool AutoContrastCorrection() { return m_bAutoContrastCorrection; }
	double AutoContrastAmount() { return m_dAutoContrastAmount; }
	float* ColorCorrectionAmounts(); // can't be declared inline due to compiler bug...sad but true
	double AutoBrightnessAmount() { return m_dAutoBrightnessAmount; }
	bool LocalDensityCorrection() { return m_bLocalDensityCorrection; }
	double BrightenShadows() { return m_dBrightenShadows; }
	double DarkenHighlights() { return m_dDarkenHighlights; }
	double BrightenShadowsSteepness() { return m_dBrightenShadowsSteepness; }
	CImageProcessingParams LandscapeModeParams(const CImageProcessingParams& templParams);
	int JPEGSaveQuality() { return m_nJPEGSaveQuality; }
	int WEBPSaveQuality() { return m_nWEBPSaveQuality; }
	LPCTSTR DefaultSaveFormat() { return m_sDefaultSaveFormat; }
	bool CreateParamDBEntryOnSave() { return m_bCreateParamDBEntryOnSave; }
	bool SaveWithoutPrompt() { return m_bSaveWithoutPrompt; }
	bool WrapAroundFolder() { return m_bWrapAroundFolder; }
	bool ExchangeXButtons() { return m_bExchangeXButtons; }
	LPCTSTR ACCExclude() { return m_sACCExclude; }
	LPCTSTR ACCInclude() { return m_sACCInclude; }
	LPCTSTR LDCExclude() { return m_sLDCExclude; }
	LPCTSTR LDCInclude() { return m_sLDCInclude; }
	LPCTSTR CopyRenamePattern() { return m_sCopyRenamePattern; }
	CRect DefaultWindowRect() { return m_defaultWindowRect; }
	bool DefaultWndToImage() { return m_bDefaultWndToImage; }
	bool DefaultMaximized() { return m_bDefaultMaximized; }
	CSize DefaultFixedCropSize() { return m_DefaultFixedCropSize; }
	COLORREF ColorBackground() { return m_colorBackground; }
	COLORREF ColorGUI() { return m_colorGUI; }
	COLORREF ColorHighlight() { return m_colorHighlight; }
	COLORREF ColorSelected() { return m_colorSelected; }
	COLORREF ColorSlider() { return m_colorSlider; }
	const CUnsharpMaskParams& UnsharpMaskParams() { return m_unsharpMaskParms; }

	std::list<CUserCommand*> & UserCommandList() { return m_userCommands; }

	// This will only save a subset of settings to the inifile located in AppData\JPEGView\JPEGView.ini.
	// Note that this INI file has precedence over the ini file at the program directory
	void SaveSettings(const CImageProcessingParams& procParams, EProcessingFlags eProcFlags, 
		Helpers::ESorting eFileSorting, Helpers::EAutoZoomMode eAutoZoomMode, bool bShowNavPanel);

	// Saves the pattern used in the copy/rename dialog to rename files
	void SaveCopyRenamePattern(const CString& sPattern);

	// Gets the path where the global INI file and the EXE is located
	LPCTSTR GetEXEPath() { return m_sEXEPath; }

	// Get the file name with path of the global INI file (in EXE path)
	LPCTSTR GetGlobalINIFileName() { return m_sIniNameGlobal; }
	// Get the file name with path of the user INI file (in AppData path)
	LPCTSTR GetUserINIFileName() { return m_sIniNameUser; }

private:
	static CSettingsProvider* sm_instance;
	CString m_sEXEPath;
	CString m_sIniNameGlobal;
	CString m_sIniNameUser;
	bool m_bUserINIExists;
	bool m_bStoreToEXEPath;

	double m_dContrast;
	double m_dGamma;
	double m_dSaturation;
	double m_dSharpen;
	double m_dCyanRed;
	double m_dMagentaGreen;
	double m_dYellowBlue;
	bool m_bHQRS;
	bool m_bShowFileName;
	bool m_bShowFileInfo;
	bool m_bShowHistogram;
	bool m_bShowJPEGComments;
	bool m_bShowNavPanel;
	bool m_bShowZoomNavigator;
	bool m_bShowFullScreen;
	bool m_bAutoFullScreen;
	float m_fBlendFactorNavPanel;
	bool m_bKeepParams;
	CString m_sLanguage;
	Helpers::CPUType m_eCPUAlgorithm;
	int m_nNumCores;
	EFilterType m_eDownsamplingFilter;
	Helpers::ESorting m_eSorting;
	Helpers::ENavigationMode m_eNavigation;
	bool m_bNavigateMouseWheel;
	Helpers::EAutoZoomMode m_eAutoZoomMode;
	int m_nDisplayMonitor;
	bool m_bAutoContrastCorrection;
	double m_dAutoContrastAmount;
	float m_fColorCorrections[6];
	double m_dAutoBrightnessAmount;
	bool m_bLocalDensityCorrection;
	double m_dBrightenShadows;
	double m_dDarkenHighlights;
	double m_dBrightenShadowsSteepness;
	CString m_sLandscapeModeParams;
	int m_nJPEGSaveQuality;
	int m_nWEBPSaveQuality;
	CString m_sDefaultSaveFormat;
	bool m_bCreateParamDBEntryOnSave;
	bool m_bWrapAroundFolder;
	bool m_bSaveWithoutPrompt;
	bool m_bExchangeXButtons;
	CString m_sACCExclude;
	CString m_sACCInclude;
	CString m_sLDCExclude;
	CString m_sLDCInclude;
	CString m_sCopyRenamePattern;
	CRect m_defaultWindowRect;
	bool m_bDefaultMaximized;
	bool m_bDefaultWndToImage;
	CSize m_DefaultFixedCropSize;
	COLORREF m_colorBackground;
	COLORREF m_colorGUI;
	COLORREF m_colorHighlight;
	COLORREF m_colorSelected;
	COLORREF m_colorSlider;
	CUnsharpMaskParams m_unsharpMaskParms;

	std::list<CUserCommand*> m_userCommands;

	void MakeSureUserINIExists();
	void ReadWriteableINISettings();
	CString GetString(LPCTSTR sKey, LPCTSTR sDefault);
	int GetInt(LPCTSTR sKey, int nDefault, int nMin, int nMax);
	double GetDouble(LPCTSTR sKey, double dDefault, double dMin, double dMax);
	bool GetBool(LPCTSTR sKey, bool bDefault);
	CRect GetRect(LPCTSTR sKey, const CRect& defaultRect);
	CSize GetSize(LPCTSTR sKey, const CSize& defaultSize);
	COLORREF GetColor(LPCTSTR sKey, COLORREF defaultColor);
	void WriteString(LPCTSTR sKey, LPCTSTR sString);
	void WriteDouble(LPCTSTR sKey, double dValue);
	void WriteBool(LPCTSTR sKey, bool bValue);

	CSettingsProvider(void);
};
