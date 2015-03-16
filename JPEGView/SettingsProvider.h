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
	bool ShowBottomPanel() { return m_bShowBottomPanel; }
	bool ShowZoomNavigator() { return m_bShowZoomNavigator; }
	bool ShowFullScreen() { return m_bShowFullScreen; }
	bool AutoFullScreen() { return m_bAutoFullScreen; }
	float BlendFactorNavPanel() { return m_fBlendFactorNavPanel; }
	float ScaleFactorNavPanel() { return m_fScaleFactorNavPanel; }
	bool KeepParams() { return m_bKeepParams; }
	LPCTSTR Language() { return m_sLanguage; }
	Helpers::CPUType AlgorithmImplementation() { return m_eCPUAlgorithm; }
	int NumberOfCoresToUse() { return m_nNumCores; }
	EFilterType DownsamplingFilter() { return m_eDownsamplingFilter; }
	Helpers::ESorting Sorting() { return m_eSorting; }
	bool IsSortedUpcounting() { return m_bIsSortedUpcounting; }
	Helpers::ENavigationMode Navigation() { return m_eNavigation; }
	bool NavigateWithMouseWheel() { return m_bNavigateMouseWheel; }
	Helpers::EAutoZoomMode AutoZoomMode() { return m_eAutoZoomMode; }
	int DisplayMonitor() { return m_nDisplayMonitor; }
    void SetMonitorOverride(int nMonitor) { m_nDisplayMonitor = nMonitor; }
	bool AutoContrastCorrection() { return m_bAutoContrastCorrection; }
	double AutoContrastAmount() { return m_dAutoContrastAmount; }
	float* ColorCorrectionAmounts(); // can't be declared inline due to compiler bug...sad but true
	double AutoBrightnessAmount() { return m_dAutoBrightnessAmount; }
	bool LocalDensityCorrection() { return m_bLocalDensityCorrection; }
	double BrightenShadows() { return m_dBrightenShadows; }
	double DarkenHighlights() { return m_dDarkenHighlights; }
	double BrightenShadowsSteepness() { return m_dBrightenShadowsSteepness; }
	CImageProcessingParams LandscapeModeParams(const CImageProcessingParams& templParams);
	int MaxSlideShowFileListSize() { return m_nMaxSlideShowFileListSize; }
    Helpers::ETransitionEffect SlideShowTransitionEffect() { return m_eSlideShowTransitionEffect; }
    int SlideShowEffectTimeMs() { return m_nSlideShowEffectTimeMs; }
	bool ForceGDIPlus() { return m_bForceGDIPlus; }
    bool SingleInstance() { return m_bSingleInstance; }
	bool SingleFullScreenInstance() { return m_bSingleFullScreenInstance; }
	int JPEGSaveQuality() { return m_nJPEGSaveQuality; }
	int WEBPSaveQuality() { return m_nWEBPSaveQuality; }
	LPCTSTR DefaultSaveFormat() { return m_sDefaultSaveFormat; }
    LPCTSTR FilesProcessedByWIC() { return m_sFilesProcessedByWIC; }
	LPCTSTR FileEndingsRAW() { return m_sFileEndingsRAW; }
    void AddTemporaryRAWFileEnding(LPCTSTR sEnding) { m_sFileEndingsRAW += CString(_T(";*.")) + sEnding; }
	bool CreateParamDBEntryOnSave() { return m_bCreateParamDBEntryOnSave; }
	bool SaveWithoutPrompt() { return m_bSaveWithoutPrompt; }
    bool TrimWithoutPromptLosslessJPEG() { return m_bTrimWithoutPromptLosslessJPEG; }
	Helpers::EDeleteConfirmation DeleteConfirmation() { return m_eDeleteConfirmation; }
	bool AllowFileDeletion() { return m_bAllowFileDeletion; }
	bool WrapAroundFolder() { return m_bWrapAroundFolder && m_eNavigation == Helpers::NM_LoopDirectory; }
	bool ExchangeXButtons() { return m_bExchangeXButtons; }
	bool AutoRotateEXIF() { return m_bAutoRotateEXIF; }
    bool UseEmbeddedColorProfiles() { return m_bUseEmbeddedColorProfiles; }
	LPCTSTR ACCExclude() { return m_sACCExclude; }
	LPCTSTR ACCInclude() { return m_sACCInclude; }
	LPCTSTR LDCExclude() { return m_sLDCExclude; }
	LPCTSTR LDCInclude() { return m_sLDCInclude; }
	LPCTSTR CopyRenamePattern() { return m_sCopyRenamePattern; }
	CRect DefaultWindowRect() { return m_defaultWindowRect; }
    CRect StickyWindowRect() { return m_stickyWindowRect; }
    bool StickyWindowSize() { return m_bStickyWindowSize; }
	bool DefaultWndToImage() { return m_bDefaultWndToImage; }
	bool DefaultMaximized() { return m_bDefaultMaximized; }
    bool ExplicitWindowRect() { return m_bExplicitWindowRect; }
	CSize DefaultFixedCropSize() { return m_DefaultFixedCropSize; }
	COLORREF ColorBackground() { return m_colorBackground; }
	COLORREF ColorGUI() { return m_colorGUI; }
	COLORREF ColorHighlight() { return m_colorHighlight; }
	COLORREF ColorSelected() { return m_colorSelected; }
	COLORREF ColorSlider() { return m_colorSlider; }
    COLORREF ColorFileName() { return m_colorFileName; }
	LPCTSTR DefaultGUIFont() { return m_defaultGUIFont; }
	LPCTSTR FileNameFont() { return m_fileNameFont; }
	const CUnsharpMaskParams& UnsharpMaskParams() { return m_unsharpMaskParms; }
	bool RTShowGridLines() { return m_bRTShowGridLines; }
	bool RTAutoCrop() { return m_bRTAutoCrop; }
	bool RTPreserveAspectRatio() { return m_bRTPreserveAspectRatio; }
    LPCTSTR FileNameFormat() { return m_sFileNameFormat; }
    bool ReloadWhenDisplayedImageChanged() { return m_bReloadWhenDisplayedImageChanged; }
	bool AllowEditGlobalSettings() { return m_bAllowEditGlobalSettings; }
	double PrintMargin() { return m_dPrintMargin; }
	double DefaultPrintWidth() { return m_dDefaultPrintWidth; }
	Helpers::EMeasureUnits MeasureUnit() { return m_eMeasureUnit; }
	CSize MinimalWindowSize() { return m_minimalWindowSize; }
	double MinimalDisplayTime() { return m_minimalDisplayTime; }
	CSize UserCropAspectRatio() { return m_userCropAspectRatio; }



	std::list<CUserCommand*> & UserCommandList() { return m_userCommands; }
	std::list<CUserCommand*> & OpenWithCommandList() { return m_openWithCommands; }

	// This will only save a subset of settings to the inifile located in AppData\JPEGView\JPEGView.ini.
	// Note that this INI file has precedence over the ini file at the program directory
	void SaveSettings(const CImageProcessingParams& procParams, EProcessingFlags eProcFlags, 
		Helpers::ESorting eFileSorting, bool isSortedUpcounting, Helpers::EAutoZoomMode eAutoZoomMode, bool bShowNavPanel);

	// Saves the pattern used in the copy/rename dialog to rename files
	void SaveCopyRenamePattern(const CString& sPattern);

    // Saves the sticky window size to the INI file
    void SaveStickyWindowRect(CRect rect);

	// Deletes the open with command, returns index of the deleted command, -1 if invalid
	int DeleteOpenWithCommand(CUserCommand* pCommand);

	// Adds a new open with command
	void AddOpenWithCommand(const CString& sCommandString);

	// Replaces the existing open with command by a new command
	void ChangeOpenWithCommand(CUserCommand* pCommand, const CString& sCommandString);

	// Returns the next index for an open with entry
	int GetNextOpenWithIndex() { return m_nNextOpenWithIndex; }

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
	int m_nNextOpenWithIndex;

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
	bool m_bShowBottomPanel;
	bool m_bShowZoomNavigator;
	bool m_bShowFullScreen;
	bool m_bAutoFullScreen;
	float m_fBlendFactorNavPanel;
	float m_fScaleFactorNavPanel;
	bool m_bKeepParams;
	CString m_sLanguage;
	Helpers::CPUType m_eCPUAlgorithm;
	int m_nNumCores;
	EFilterType m_eDownsamplingFilter;
	Helpers::ESorting m_eSorting;
	bool m_bIsSortedUpcounting;
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
	int m_nMaxSlideShowFileListSize;
    Helpers::ETransitionEffect m_eSlideShowTransitionEffect;
    int m_nSlideShowEffectTimeMs;
	bool m_bForceGDIPlus;
    bool m_bSingleInstance;
	bool m_bSingleFullScreenInstance;
	int m_nJPEGSaveQuality;
	int m_nWEBPSaveQuality;
	CString m_sDefaultSaveFormat;
    CString m_sFilesProcessedByWIC;
	CString m_sFileEndingsRAW;
	bool m_bCreateParamDBEntryOnSave;
	bool m_bWrapAroundFolder;
	bool m_bSaveWithoutPrompt;
	Helpers::EDeleteConfirmation m_eDeleteConfirmation;
	bool m_bAllowFileDeletion;
    bool m_bTrimWithoutPromptLosslessJPEG;
	bool m_bExchangeXButtons;
	bool m_bAutoRotateEXIF;
    bool m_bUseEmbeddedColorProfiles;
	CString m_sACCExclude;
	CString m_sACCInclude;
	CString m_sLDCExclude;
	CString m_sLDCInclude;
	CString m_sCopyRenamePattern;
	CRect m_defaultWindowRect;
    CRect m_stickyWindowRect;
	bool m_bDefaultMaximized;
	bool m_bDefaultWndToImage;
    bool m_bStickyWindowSize;
    bool m_bExplicitWindowRect;
	CSize m_DefaultFixedCropSize;
	COLORREF m_colorBackground;
	COLORREF m_colorGUI;
	COLORREF m_colorHighlight;
	COLORREF m_colorSelected;
	COLORREF m_colorSlider;
    COLORREF m_colorFileName;
	CString m_defaultGUIFont;
	CString m_fileNameFont;
	CUnsharpMaskParams m_unsharpMaskParms;
	bool m_bRTShowGridLines;
	bool m_bRTAutoCrop;
	bool m_bRTPreserveAspectRatio;
    CString m_sFileNameFormat;
    bool m_bReloadWhenDisplayedImageChanged;
	bool m_bAllowEditGlobalSettings;
	double m_dPrintMargin;
	double m_dDefaultPrintWidth;
	Helpers::EMeasureUnits m_eMeasureUnit;
	CSize m_minimalWindowSize;
	double m_minimalDisplayTime;
	CSize m_userCropAspectRatio;

	std::list<CUserCommand*> m_userCommands;
	std::list<CUserCommand*> m_openWithCommands;

	void MakeSureUserINIExists();
    CString ReplacePlaceholdersFileNameFormat(const CString& sFileNameFormat);
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
