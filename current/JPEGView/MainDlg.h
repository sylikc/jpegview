// MainDlg.h : interface of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "MessageDef.h"
#include "ProcessParams.h"
#include "Helpers.h"
#include "CropCtl.h"

class CFileList;
class CJPEGProvider;
class CJPEGImage;
class CButtonCtrl;
class CTextCtrl;
class CPanel;
class CPanelMgr;
class CEXIFDisplayCtl;
class CImageProcPanelCtl;
class CNavigationPanelCtl;
class CRotationPanelCtl;
class CTiltCorrectionPanelCtl;
class CUnsharpMaskPanelCtl;
class CWndButtonPanelCtl;
class CInfoButtonPanelCtl;
class CZoomNavigatorCtl;
class CCropCtl;
class CKeyMap;
class CDirectoryWatcher;
class CUserCommand;
class CPrintImage;

enum EMouseEvent;

// The main dialog is a full screen modal dialog with no border and no window title
class CMainDlg : public CDialogImpl<CMainDlg>
{
public:
	enum { IDD = IDD_MAINDLG };

	// Used in GotoImage() call
	enum EImagePosition {
		POS_First,
		POS_Last,
		POS_Next,
        POS_NextSlideShow,
        POS_NextAnimation,
		POS_Previous,
		POS_Current,
		POS_Clipboard,
		POS_Toggle,
        POS_AwayFromCurrent
	};

	CMainDlg(bool bForceFullScreen);
	~CMainDlg();

	BEGIN_MSG_MAP(CMainDlg)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MESSAGE_HANDLER(WM_NCLBUTTONDOWN, OnNCLButtonDown)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
		MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
		MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnLButtonDblClk)
        MESSAGE_HANDLER(WM_RBUTTONDOWN, OnRButtonDown)
        MESSAGE_HANDLER(WM_RBUTTONUP, OnRButtonUp)
		MESSAGE_HANDLER(WM_MBUTTONDOWN, OnMButtonDown)
		MESSAGE_HANDLER(WM_MBUTTONUP, OnMButtonUp)
		MESSAGE_HANDLER(WM_XBUTTONDOWN, OnXButtonDown)
		MESSAGE_HANDLER(WM_XBUTTONDBLCLK, OnXButtonDown)
		MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
		MESSAGE_HANDLER(WM_MOUSEWHEEL, OnMouseWheel)
		MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown)
		MESSAGE_HANDLER(WM_SYSKEYDOWN, OnSysKeyDown)
		MESSAGE_HANDLER(WM_GETDLGCODE, OnGetDlgCode)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_CONTEXTMENU, OnContextMenu)
		MESSAGE_HANDLER(WM_CTLCOLOREDIT, OnCtlColorEdit)
		MESSAGE_HANDLER(WM_JPEG_LOAD_COMPLETED, OnJPEGLoaded)
		MESSAGE_HANDLER(WM_DISPLAYED_FILE_CHANGED_ON_DISK, OnDisplayedFileChangedOnDisk)
        MESSAGE_HANDLER(WM_ACTIVE_DIRECTORY_FILELIST_CHANGED, OnActiveDirectoryFilelistChanged)
		MESSAGE_HANDLER(WM_DROPFILES, OnDropFiles)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
		//MESSAGE_HANDLER(WM_ANOTHER_INSTANCE_QUIT, OnAnotherInstanceStarted)
        MESSAGE_HANDLER(WM_LOAD_FILE_ASYNCH, OnLoadFileAsynch)
        MESSAGE_HANDLER(WM_COPYDATA, OnAnotherInstanceStarted) 
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)


	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEraseBackground(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnGetMinMaxInfo(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnNCLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
    LRESULT OnRButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
    LRESULT OnRButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnLButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnLButtonDblClk(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnMButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnMButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnXButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnMouseMove(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnMouseWheel(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnKeyDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSysKeyDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnGetDlgCode(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCtlColorEdit(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnJPEGLoaded(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnDisplayedFileChangedOnDisk(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
    LRESULT OnActiveDirectoryFilelistChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnDropFiles(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnAnotherInstanceStarted(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
    LRESULT OnLoadFileAsynch(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
    LRESULT OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);

	// Called by main()
	void SetStartupInfo(LPCTSTR sStartupFile, int nAutostartSlideShow, Helpers::ESorting eSorting, Helpers::ETransitionEffect eEffect, 
        int nTransitionTime, bool bAutoExit, int nDisplayMonitor);

	// Called by the different controller classes
	HWND GetHWND() { return m_hWnd; }
	bool IsShowFileName() { return m_bShowFileName; }
	bool IsShowHelp() { return m_bShowHelp; }
	bool IsInMovieMode() { return m_bMovieMode; }
	bool IsInZoomMode() { return m_bZoomModeOnLeftMouse; }
    bool IsPlayingAnimation() { return m_bIsAnimationPlaying; }
	bool IsFullScreenMode() { return m_bFullScreenMode; }
	bool IsLandscapeMode() { return m_bLandscapeMode; }
	bool IsHQResampling() { return m_bHQResampling; }
	bool IsAutoContrast() { return m_bAutoContrast; }
	bool IsAutoContrastSection() { return m_bAutoContrastSection; }
	bool IsLDC() { return m_bLDC; }
	bool IsKeepParams() { return m_bKeepParams; }
	bool IsSpanVirtualDesktop() { return m_bSpanVirtualDesktop; }
	bool IsCropping() { return m_pCropCtl->IsCropping(); }
	bool IsDoCropping() { return m_pCropCtl->IsDoCropping(); }
	bool IsDoDragging() { return m_bDoDragging; }
	bool IsInZooming() { return m_bInZooming; }
	bool IsShowZoomFactor() { return m_bShowZoomFactor; }
	bool IsPanMouseCursorSet() { return m_bPanMouseCursorSet; }
	bool IsMouseOn() { return m_bMouseOn; }
	CPoint GetMousePos() { return CPoint(m_nMouseX, m_nMouseY); }
	double GetZoom() { return m_dZoom; }
    int GetRotation() { return m_nRotation; }
	CJPEGImage* GetCurrentImage() { return m_pCurrentImage; }
	CPanelMgr* GetPanelMgr() { return m_pPanelMgr; }
	LPCTSTR CurrentFileName(bool bFileTitle);
	CFileList* GetFileList() { return m_pFileList; }
	CNavigationPanelCtl* GetNavPanelCtl() { return m_pNavPanelCtl; }
	CEXIFDisplayCtl* GetEXIFDisplayCtl() { return m_pEXIFDisplayCtl; }
	CUnsharpMaskPanelCtl* GetUnsharpMaskPanelCtl() { return m_pUnsharpMaskPanelCtl; }
	CImageProcPanelCtl* GetImageProcPanelCtl() { return m_pImageProcPanelCtl; }
	CRotationPanelCtl* GetRotationPanelCtl() { return m_pRotationPanelCtl; }
	CTiltCorrectionPanelCtl* GetTiltCorrectionPanelCtl() { return m_pTiltCorrectionPanelCtl; }
	CZoomNavigatorCtl* GetZoomNavigatorCtl() { return m_pZoomNavigatorCtl; }
	CWndButtonPanelCtl* GetWndButtonPanelCtl() { return m_pWndButtonPanelCtl; }
    CInfoButtonPanelCtl* GetInfoButtonPanelCtl() { return m_pInfoButtonPanelCtl; }
	CCropCtl* GetCropCtl() { return m_pCropCtl; }
	const CRect& ClientRect() { return m_clientRect; }
    const CRect& WindowRectOnClose() { return m_windowRectOnClose; } // only valid after having closed the window
	const CRect& MonitorRect() { return m_monitorRect; }
	const CSize& VirtualImageSize() { return m_virtualImageSize; }
	CJPEGProvider* GetJPEGProvider() { return m_pJPEGProvider; }
	CKeyMap* GetKeyMap() { return m_pKeyMap; }
	CPoint GetDIBOffset() { return m_DIBOffsets; }
    double GetZoomMultiplier(CJPEGImage* pImage, const CRect& clientRect);
    Helpers::EAutoZoomMode GetAutoZoomMode() { return m_eAutoZoomMode; }
    CPoint GetOffsets() { return m_offsets; }
    CImageProcessingParams* GetImageProcessingParams() { return m_pImageProcParams; }
    EProcessingFlags CreateDefaultProcessingFlags(bool bKeepParams = false);
    void DisplayErrors(CJPEGImage* pCurrentImage, const CRect& clientRect, CDC& dc);
    void DisplayFileName(const CRect& imageProcessingArea, CDC& dc);
    void BlendBlackRect(CDC & targetDC, CPanel& panel, float fBlendFactor);

	void UpdateWindowTitle();
	void MouseOff();
	void MouseOn();
	void GotoImage(EImagePosition ePos);
	void ReloadImage(bool keepParameters, bool updateWindow = true);
	void ResetZoomTo100Percents(bool bZoomToMouse);
	void ResetZoomToFitScreen(bool bFillWithCrop, bool bAllowEnlarge, bool bAdjustWindowSize);
	bool PerformPan(int dx, int dy, bool bAbsolute);
	void StartDragging(int nX, int nY, bool bDragWithZoomNavigator);
	void DoDragging();
	void EndDragging();
	void SetCursorForMoveSection();
	bool ScreenToImage(float & fX, float & fY); 
	bool ImageToScreen(float & fX, float & fY);
	void ExecuteCommand(int nCommand);
	bool PrepareForModalPanel(); // returns if navigation panel was enabled, turns it off
	int TrackPopupMenu(CPoint pos, HMENU hMenu);
	void AdjustWindowToImage(bool bAfterStartup);
	bool IsAdjustWindowToImage();
    Helpers::ETransitionEffect GetTransitionEffect() { return m_eTransitionEffect; }
    int GetTransitionTime() { return m_nTransitionTime; }
    bool IsInSlideShowWithTransition() { return m_bMovieMode && UseSlideShowTransitionEffect(); }

	// Called by button clicked handlers - must be static
	static void OnExecuteCommand(void* pContext, int nParameter, CButtonCtrl & sender);
	static bool IsCurrentImageFitToScreen(void* pContext);

private:

	CString m_sStartupFile; // file passed on command line
    int m_nAutoStartSlideShow; // if positive: Auto start slide show with given intervall in seconds, passed on command line
    bool m_bAutoExit;
    Helpers::ESorting m_eForcedSorting; // forced sorting mode on command line
	CFileList* m_pFileList; // used for navigation
    CDirectoryWatcher* m_pDirectoryWatcher; // notifies the main window when the current file changed or a file in the current directory was added or deleted
	CJPEGProvider * m_pJPEGProvider; // reads JPEG files (read ahead)
	CJPEGImage * m_pCurrentImage; // currently displayed image
	bool m_bOutOfMemoryLastImage; // true if the last image could not be requested because not enough memory
	int m_nLastLoadError; // one of HelpersGUI::EFileLoadError
	
	// Current parameter set
	int m_nRotation; // this can only be 0, 90, 180 or 270
	bool m_bUserZoom;
	bool m_bUserPan; // user has zoomed and panned away from default values
	bool m_bResizeForNewImage;
	double m_dZoom;
	double m_dStartZoom; // zoom when start zoomin in zoom mode
	double m_dZoomAtResizeStart; // zoom factor when user started resizing JPEGView main window
	double m_dZoomMult;
	bool m_bZoomMode;
	bool m_bZoomModeOnLeftMouse;
	Helpers::EAutoZoomMode m_eAutoZoomMode;
	Helpers::EAutoZoomMode m_autoZoomFitToScreen;
	bool m_isUserFitToScreen;

	CImageProcessingParams* m_pImageProcParams;
	bool m_bHQResampling;
	bool m_bAutoContrast;
	bool m_bAutoContrastSection;
	bool m_bLDC;
	bool m_bLandscapeMode;
	bool m_bKeepParams;

	// used to enable switch between two sets of parameters with CTRL-A
	CImageProcessingParams* m_pImageProcParams2;
	EProcessingFlags m_eProcessingFlags2;

	// set of parameters used when m_bKeepParams is true
	CImageProcessingParams* m_pImageProcParamsKept;
	EProcessingFlags m_eProcessingFlagsKept;
	int m_nRotationKept;
	double m_dZoomKept;
	CPoint m_offsetKept;
	bool m_bCurrentImageInParamDB;
	bool m_bCurrentImageIsSpecialProcessing;
	double m_dCurrentInitialLightenShadows;

	bool m_bDragging;
	bool m_bDoDragging;
	bool m_bMovieMode;
	bool m_bProcFlagsTouched;
	EProcessingFlags m_eProcFlagsBeforeMovie;
	bool m_bInTrackPopupMenu;
	CPoint m_offsets; // Note: These offsets are center of image based
	CPoint m_DIBOffsets;
	int m_nCapturedX, m_nCapturedY;
	int m_nMouseX, m_nMouseY;
	bool m_bShowFileName;
	bool m_bShowHelp;
	bool m_bFullScreenMode;
	bool m_bAutoFitWndToImage;
	bool m_bLockPaint;
	int m_nCurrentTimeout;
	POINT m_startMouse;
	CSize m_virtualImageSize;
	bool m_bInZooming;
	bool m_bTemporaryLowQ;
	bool m_bShowZoomFactor;
	bool m_bSpanVirtualDesktop;
	bool m_bPanMouseCursorSet;
	bool m_bMouseOn;
    bool m_bKeepParametersBeforeAnimation;
    bool m_bIsAnimationPlaying;
	int m_nMonitor;
	WINDOWPLACEMENT m_storedWindowPlacement;
	CRect m_monitorRect;
	CRect m_clientRect;
    CRect m_windowRectOnClose;
	CString m_sSaveDirectory;
	CString m_sSaveExtension;
	CCropCtl* m_pCropCtl;
	CZoomNavigatorCtl* m_pZoomNavigatorCtl;
	CImageProcPanelCtl* m_pImageProcPanelCtl;
	CNavigationPanelCtl* m_pNavPanelCtl;
	CEXIFDisplayCtl* m_pEXIFDisplayCtl;
	CWndButtonPanelCtl* m_pWndButtonPanelCtl;
    CInfoButtonPanelCtl* m_pInfoButtonPanelCtl;
	CUnsharpMaskPanelCtl* m_pUnsharpMaskPanelCtl;
	CRotationPanelCtl* m_pRotationPanelCtl;
	CTiltCorrectionPanelCtl* m_pTiltCorrectionPanelCtl;
	CPanelMgr* m_pPanelMgr;
	CKeyMap* m_pKeyMap;
	CPrintImage* m_pPrintImage;
    Helpers::ETransitionEffect m_eTransitionEffect;
    int m_nTransitionTime;
    DWORD m_nLastSlideShowImageTickCount;
    bool m_bUseLosslessWEBP;
    bool m_isBeforeFileSelected;
	double m_dLastImageDisplayTime;

	bool OpenFile(bool bFullScreen, bool bAfterStartup);
	void OpenFile(LPCTSTR sFileName, bool bAfterStartup);
	bool SaveImage(bool bFullSize);
	bool SaveImageNoPrompt(LPCTSTR sFileName, bool bFullSize);
	void BatchCopy();
    void SetAsDefaultViewer();
	void HandleUserCommands(uint32 virtualKeyCode);
	void ExecuteUserCommand(CUserCommand* pUserCommand);
	void GotoImage(EImagePosition ePos, int nFlags);
	void AdjustLDC(int nMode, double dInc);
	void AdjustGamma(double dFactor);
	void AdjustContrast(double dInc);
	void AdjustSharpen(double dInc);
	void PerformZoom(double dValue, bool bExponent, bool bZoomToMouse, bool bAdjustWindowToImage);
	void ZoomToSelection();
	double GetZoomFactorForFitToScreen(bool bFillWithCrop, bool bAllowEnlarge);
	CProcessParams CreateProcessParams(bool bNoProcessingAfterLoad);
	void ResetParamsToDefault();
	void StartSlideShowTimer(int nMilliSeconds);
	void StopSlideShowTimer(void);
	void StartMovieMode(double dFPS);
	void StopMovieMode();
	void StartLowQTimer(int nTimeout);
	void InitParametersForNewImage();
	void ExchangeProcessingParams();
	void SaveParameters();
	void AfterNewImageLoaded(bool bSynchronize, bool bAfterStartup);
	CRect ScreenToDIB(const CSize& sizeDIB, const CRect& rect);
	void ToggleMonitor();
	CRect GetZoomTextRect(CRect imageProcessingArea);
	void EditINIFile(bool bGlobalINI);
	int GetLoadErrorAfterOpenFile();
	void CheckIfApplyAutoFitWndToImage(bool bInInitDialog);
	void PrefetchDIB(const CRect& clientRect);
    bool HandleMouseButtonByKeymap(int nMouseButton, bool bExecuteCommand = true);
    bool UseSlideShowTransitionEffect();
    void PaintToDC(CDC& dc);
    void AnimateTransition();
    void CleanupAndTerminate();
    // this is for animated GIFs
    void StartAnimation();
    void AdjustAnimationFrameTime();
    void StopAnimation();
};
