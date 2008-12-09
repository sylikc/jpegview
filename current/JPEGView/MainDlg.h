// MainDlg.h : interface of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "MessageDef.h"
#include "ProcessParams.h"
#include "Helpers.h"

class CFileList;
class CJPEGProvider;
class CJPEGImage;
class CSliderMgr;
class CNavigationPanel;
class CWndButtonPanel;
class CButtonCtrl;
class CTextCtrl;
class CEXIFDisplay;
class CHelpDisplay;

// The main dialog is a full screen modal dialog with no border and no window title
class CMainDlg : public CDialogImpl<CMainDlg>
{
public:
	enum { IDD = IDD_MAINDLG };

	CMainDlg();
	~CMainDlg();

	BEGIN_MSG_MAP(CMainDlg)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
		MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
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
		MESSAGE_HANDLER(WM_DROPFILES, OnDropFiles)
		MESSAGE_HANDLER(WM_ANOTHER_INSTANCE_QUIT, OnAnotherInstanceStarted)
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
	LRESULT OnLButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
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
	LRESULT OnDropFiles(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnAnotherInstanceStarted(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);

	// Called by main()
	void SetStartupFile(LPCTSTR sStartupFile);

private:

	// Used in GotoImage() call
	enum EImagePosition {
		POS_First,
		POS_Last,
		POS_Next,
		POS_Previous,
		POS_Current,
		POS_Clipboard
	};

	static CMainDlg* sm_instance; // single instance of the main dialog

	CString m_sStartupFile;
	CFileList* m_pFileList; // used for navigation
	CJPEGProvider * m_pJPEGProvider; // reads JPEG files (read ahead)
	CJPEGImage * m_pCurrentImage; // currently displayed image

	// DPI scaling factor
	float m_fScaling;
	
	// Current parameter set
	int m_nRotation;
	bool m_bUserZoom;
	bool m_bUserPan;
	double m_dZoom;
	double m_dZoomMult;
	Helpers::EAutoZoomMode m_eAutoZoomMode;

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

	// cropping
	CPoint m_cropStart;
	CPoint m_cropEnd;

	bool m_bPasteFromClipboardFailed;
	bool m_bDragging;
	bool m_bDoDragging;
	bool m_bDraggingWithZoomNavigator;
	bool m_bCropping;
	bool m_bDoCropping;
	CPoint m_cropMouse;
	bool m_bMovieMode;
	bool m_bProcFlagsTouched;
	EProcessingFlags m_eProcFlagsBeforeMovie;
	bool m_bInTrackPopupMenu;
	int m_nOffsetX;
	int m_nOffsetY;
	int m_nCapturedX, m_nCapturedY;
	int m_nMouseX, m_nMouseY;
	bool m_bShowFileInfo;
	bool m_bShowFileName;
	bool m_bShowHelp;
	bool m_bFullScreenMode;
	bool m_bLockPaint;
	int m_nCurrentTimeout;
	POINT m_startMouse;
	CSize m_virtualImageSize;
	CPoint m_capturedPosZoomNavSection;
	bool m_bInZooming;
	bool m_bTemporaryLowQ;
	bool m_bShowZoomFactor;
	bool m_bSearchSubDirsOnEnter;
	bool m_bSpanVirtualDesktop;
	bool m_bShowIPTools;
	bool m_bShowNavPanel;
	bool m_bShowWndButtonPanel;
	bool m_bMouseInNavPanel;
	bool m_bArrowCursorSet;
	bool m_bMouseOn;
	bool m_bInInitialOpenFile;
	int m_nMonitor;
	WINDOWPLACEMENT m_storedWindowPlacement;
	WINDOWPLACEMENT m_storedWindowPlacement2;
	CRect m_monitorRect;
	CRect m_clientRect;
	CSliderMgr * m_pSliderMgr;
	CButtonCtrl* m_btnSaveToDB;
	CButtonCtrl* m_btnRemoveFromDB;
	CTextCtrl* m_txtFileName;
	CTextCtrl* m_txtParamDB;
	CTextCtrl* m_txtRename;
	CTextCtrl* m_txtAcqDate;
	CButtonCtrl* m_btnInfo;
	CButtonCtrl* m_btnLandScape;
	CButtonCtrl* m_btnWindowMode;
	CNavigationPanel* m_pNavPanel;
	CWndButtonPanel* m_pWndButtonPanel;
	CString m_sSaveDirectory;
	CString m_sSaveExtension;

	bool OpenFile(bool bFullScreen);
	void OpenFile(LPCTSTR sFileName);
	bool SaveImage(bool bFullSize);
	void BatchCopy();
	void HandleUserCommands(uint32 virtualKeyCode);
	void StartDragging(int nX, int nY, bool bDragWithZoomNavigator);
	void DoDragging(int nX, int nY);
	void EndDragging();
	void StartCropping(int nX, int nY);
	void ShowCroppingRect(int nX, int nY, HDC hPaintDC);
	void EndCropping();
	void PaintCropRect(HDC hPaintDC);
	void GotoImage(EImagePosition ePos);
	void GotoImage(EImagePosition ePos, int nFlags);
	void AdjustLDC(int nMode, double dInc);
	void AdjustGamma(double dFactor);
	void AdjustContrast(double dInc);
	void AdjustSharpen(double dInc);
	void PerformZoom(double dValue, bool bExponent);
	bool PerformPan(int dx, int dy, bool bAbsolute);
	void ZoomToSelection();
	void LimitOffsets(const CRect & rect, const CSize & size);
	void ResetZoomToFitScreen(bool bFillWithCrop);
	void ResetZoomTo100Percents();
	CProcessParams CreateProcessParams();
	void ResetParamsToDefault();
	void StartTimer(int nMilliSeconds);
	void StopTimer(void);
	void StartMovieMode(double dFPS);
	void StopMovieMode();
	void StartLowQTimer(int nTimeout);
	void StartNavPanelTimer(int nTimeout);
	void MouseOff();
	void MouseOn();
	void InitParametersForNewImage();
	void ExecuteCommand(int nCommand);
	void DrawTextBordered(CPaintDC& dc, LPCTSTR sText, const CRect& rect, UINT nFormat);
	void ExchangeProcessingParams();
	void SaveParameters();
	void ShowHideSaveDBButtons();
	void AfterNewImageLoaded(bool bSynchronize);
	bool RenameCurrentFile(LPCTSTR sNewFileTitle);
	int Scale(int nValue) { return (int)(m_fScaling*nValue); }
	CRect ScreenToDIB(const CSize& sizeDIB, const CRect& rect);
	bool ScreenToImage(float & fX, float & fY); 
	bool ImageToScreen(float & fX, float & fY);
	LPCTSTR CurrentFileName(bool bFileTitle);
	void FillEXIFDataDisplay(CEXIFDisplay* pEXIFDisplay);
	void GenerateHelpDisplay(CHelpDisplay & helpDisplay);
	HBITMAP PrepareRectForMemDCPainting(CDC & memDC, CDC & paintDC, const CRect& rect);
	void InvalidateZoomNavigatorRect();
	bool IsZoomNavigatorCurrentlyShown();
	void SetCursorForMoveSection();
	void ToggleMonitor();
	CRect GetZoomTextRect(CRect imageProcessingArea);
	void EditINIFile(bool bGlobalINI);
	void BackupParamDB();
	void RestoreParamDB();

	static void OnSaveToDB(CButtonCtrl & sender);
	static void OnRemoveFromDB(CButtonCtrl & sender);
	static bool OnRenameFile(CTextCtrl & sender, LPCTSTR sChangedText);
	static void OnHome(CButtonCtrl & sender);
	static void OnPrev(CButtonCtrl & sender);
	static void OnNext(CButtonCtrl & sender);
	static void OnEnd(CButtonCtrl & sender);
	static void OnToggleZoomFit(CButtonCtrl & sender);
	static void OnToggleWindowMode(CButtonCtrl & sender);
	static void OnRotateCW(CButtonCtrl & sender);
	static void OnRotateCCW(CButtonCtrl & sender);
	static void OnShowInfo(CButtonCtrl & sender);
	static void OnLandscapeMode(CButtonCtrl & sender);
	static void ToggleWindowMode(bool bSetCursor);

	static void OnMinimize(CButtonCtrl & sender);
	static void OnRestore(CButtonCtrl & sender);
	static void OnClose(CButtonCtrl & sender);

	static void PaintZoomFitToggleBtn(const CRect& rect, CDC& dc);
	static LPCTSTR ZoomFitToggleTooltip();
	static LPCTSTR WindowModeTooltip();
};
