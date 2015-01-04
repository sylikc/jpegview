// ResizeDlg.h : interface of the CPrintDlg class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "resource.h"
#include "ProcessParams.h"
#include "PrintParameters.h"

class CJPEGImage;

class CPrintDlg : public CDialogImpl<CPrintDlg>
{
public:
	enum { IDD = IDD_PRINT };

	BEGIN_MSG_MAP(CPrintDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
		MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
		COMMAND_ID_HANDLER(IDOK, OnPrint)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER(IDC_PD_BTN_CHANGE_PRINTER, OnChangePrinter)
		COMMAND_HANDLER(IDC_PD_ED_WIDTH, EN_SETFOCUS, OnSetFocusToUserSize)
		COMMAND_HANDLER(IDC_PD_ED_HEIGHT, EN_SETFOCUS, OnSetFocusToUserSize)
		COMMAND_HANDLER(IDC_PD_ED_WIDTH, EN_CHANGE, OnWidthChanged)
		COMMAND_HANDLER(IDC_PD_ED_HEIGHT, EN_CHANGE, OnHeightChanged)
		COMMAND_HANDLER(IDC_PD_CB_PAPER, LBN_SELCHANGE, OnSelectedPaperChanged)
		COMMAND_HANDLER(IDC_PD_CB_PAPER_SOURCE, LBN_SELCHANGE, OnSelectedTrayChanged)
		COMMAND_HANDLER(IDC_PD_CB_ORIENTATION, LBN_SELCHANGE, OnSelectedPaperOrientationChanged)
		COMMAND_HANDLER(IDC_PD_ED_LEFT, EN_CHANGE, OnLeftMarginChanged)
		COMMAND_HANDLER(IDC_PD_ED_RIGHT, EN_CHANGE, OnRightMarginChanged)
		COMMAND_HANDLER(IDC_PD_ED_TOP, EN_CHANGE, OnTopMarginChanged)
		COMMAND_HANDLER(IDC_PD_ED_BOTTOM, EN_CHANGE, OnBottomMarginChanged)
		COMMAND_ID_HANDLER(IDC_PD_RB_FIT, OnSizingModeChanged)
		COMMAND_ID_HANDLER(IDC_PD_RB_FIXED, OnSizingModeChanged)
		COMMAND_ID_HANDLER(IDC_PD_RB_00, OnAlignmentButtonClicked)
		COMMAND_ID_HANDLER(IDC_PD_RB_10, OnAlignmentButtonClicked)
		COMMAND_ID_HANDLER(IDC_PD_RB_20, OnAlignmentButtonClicked)
		COMMAND_ID_HANDLER(IDC_PD_RB_01, OnAlignmentButtonClicked)
		COMMAND_ID_HANDLER(IDC_PD_RB_11, OnAlignmentButtonClicked)
		COMMAND_ID_HANDLER(IDC_PD_RB_21, OnAlignmentButtonClicked)
		COMMAND_ID_HANDLER(IDC_PD_RB_02, OnAlignmentButtonClicked)
		COMMAND_ID_HANDLER(IDC_PD_RB_12, OnAlignmentButtonClicked)
		COMMAND_ID_HANDLER(IDC_PD_RB_22, OnAlignmentButtonClicked)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnMouseMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnLButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);

	LRESULT OnPrint(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnChangePrinter(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSetFocusToUserSize(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnWidthChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnHeightChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSelectedPaperChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSelectedTrayChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSelectedPaperOrientationChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSizingModeChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAlignmentButtonClicked(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnLeftMarginChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRightMarginChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTopMarginChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBottomMarginChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CPrintDlg(CPrintParameters* pPrintParameters, CPrintDialogEx* pPrintDlg, LPDEVMODE pDeviceMode, LPCTSTR printerName, LPCTSTR portName,
		CJPEGImage * pImage, const CImageProcessingParams& procParams, EProcessingFlags eFlags);
	~CPrintDlg(void);

	// Returns the printer device context to be used for printing
	HDC GetPrinterDC() { return m_hPrinterDC; }

	static CRect Align(const CSize& size, const CRect& rect, CPrintParameters::HorizontalAlignment horizontalAlignment, CPrintParameters::VerticalAlignment verticalAlignment);

private:
	enum EHandle {
		Handle_None,
		Handle_Left,
		Handle_Right,
		Handle_Top,
		Handle_Bottom,
		Handle_TopLeft,
		Handle_TopRight,
		Handle_BottomLeft,
		Handle_BottomRight
	};

	CButton m_btnPrint;
	CButton m_btnCancel;
	CButton m_btnChangePrinter;

	CStatic m_lblPrinterTitle;
	CStatic m_lblPrinter;
	CStatic m_lblPaper;
	CComboBox m_cbPaper;
	CStatic m_lblPaperTray;
	CComboBox m_cbPaperTray;
	CStatic m_lblPaperOrientation;
	CComboBox m_cbPaperOrientation;
	CStatic m_lblSize;
	CButton m_rbFitToPaper;
	CButton m_rbSize;
	CEdit m_edtWidth;
	CEdit m_edtHeight;
	CStatic m_lblCM;
	CStatic m_lblMargins;
	CStatic m_lblLeft;
	CStatic m_lblRight;
	CStatic m_lblTop;
	CStatic m_lblBottom;
	CEdit m_edtLeft;
	CEdit m_edtRight;
	CEdit m_edtTop;
	CEdit m_edtBottom;
	CStatic m_lblCM2;
	CStatic m_lblCM3;
	CStatic m_lblScalingMode;
	CButton m_rbScalingModePrinterDriver;
	CButton m_rbScalingModeJPEGView;
	CStatic m_lblAlignment;
	CButton m_rbAlign00;
	CButton m_rbAlign01;
	CButton m_rbAlign02;
	CButton m_rbAlign10;
	CButton m_rbAlign11;
	CButton m_rbAlign12;
	CButton m_rbAlign20;
	CButton m_rbAlign21;
	CButton m_rbAlign22;

	CPrintParameters* m_pPrintParameters;
	CPrintDialogEx* m_pPrintDlg;
	LPDEVMODE m_pDeviceMode;
	LPCTSTR m_printerName;
	LPCTSTR m_portName;

	HDC m_hPrinterDC;

	CJPEGImage * m_pImage;
	const CImageProcessingParams& m_procParams;
	EProcessingFlags m_eProcessingFlags;
	double m_dAspectRatioImage;

	int m_numberOfPapers;
	WORD* m_paperTypes;
	POINT* m_paperSizes;

	int m_numberOfTrays;
	WORD* m_paperTrays;

	// all in 1/10 mm
	double m_dDeviceMarginLeft, m_dDeviceMarginTop, m_dDeviceMarginRight, m_dDeviceMarginBottom;
	double m_dOrigMarginLeft, m_dOrigMarginTop, m_dOrigMarginRight, m_dOrigMarginBottom;
	double m_dCurrentFixedPaperWidth;
	double m_useInches; // not for internal values, only for user display

	CRect m_currentPrintableRect;
	CRect m_currentPaperRect;
	bool m_usingNonStandardCursor;
	EHandle m_handleDragMode;
	double m_dPixelsPerMm;
	CPoint m_startDragPoint;
	double m_startDragMargin, m_startDragMargin2; // in 1/10 mm
	double m_maxMargin, m_maxMargin2; // in 1/10 mm
	bool m_leftMarginValid, m_rightMarginValid, m_topMarginValid, m_bottomMarginValid;

	bool m_blockUpdate;
	bool m_lockPrinterDC;

	void SetupDataForCurrentPrinter();
	LPCTSTR* GetPaperNames(int& numberOfPapers, WORD*& paperTypes, POINT*& paperSizes);
	LPCTSTR* GetTrayNames(int& numberOfTrays, WORD*& paperTrays);
	int GetIndexOfPaper(WORD paper); // That is the index in the m_paperTypes and m_paperSizes arrays
	POINT GetUsedPaperSize(); // as set in m_pDeviceMode, in 1/10 mm
	CRect GetPaperPaintBoundingBox(); // in pixels on screen
	CRect GetPrintableBoundingBox(const CRect& pixelPaperRect, const CSize& paperSize); // in pixels on screen, paperSize is in 1/10 mm
	CSize GetFixedImageSize(const CSize& imageSize, const CSize& paperSizeMm10, const CSize& paperSizePixels);
	CRect GetLocalCoordinates(HWND hWnd);
	void RecreatePrinterDC();
	void GetAlignment(CPrintParameters::HorizontalAlignment& horizontalAlignment, CPrintParameters::VerticalAlignment& verticalAlignment);
	bool SetMarginCursor(int x, int y);
	EHandle PrintAreaHandleHit(int x, int y);
	void DragMargin(int x, int y);
	void SetLeftMargin(double maxMargin, double startDrag, const CPoint& delta);
	void SetRightMargin(double maxMargin, double startDrag, const CPoint& delta);
	void SetTopMargin(double maxMargin, double startDrag, const CPoint& delta);
	void SetBottomMargin(double maxMargin, double startDrag, const CPoint& delta);
	void Validate();
	bool ValidateFixedImageSize(double width, const CSize& paperSize); // width and paper size in 1/10 mm
	double CalculateMaxLeftMargin();
	double CalculateMaxRightMargin();
	double CalculateMaxTopMargin();
	double CalculateMaxBottomMargin();

	double ConvertToNumber(CEdit& editControl);
	void SetNumber(CEdit& editControl, double number);
};