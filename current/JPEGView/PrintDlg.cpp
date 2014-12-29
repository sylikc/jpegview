#include "StdAfx.h"
#include "PrintDlg.h"
#include "NLS.h"
#include "Helpers.h"
#include "HelpersGUI.h"
#include "JPEGImage.h"
#include "PrintParameters.h"

// Converts points to 1/10 millimeters
static double PointsToMm10(int value, int dpi) {
	return 10 * ((double)value / dpi) * 25.4;
}

CPrintDlg::CPrintDlg(CPrintParameters* pPrintParameters, CPrintDialogEx* pPrintDlg, LPDEVMODE pDeviceMode, LPCTSTR printerName, LPCTSTR portName,
	CJPEGImage * pImage, const CImageProcessingParams& procParams, EProcessingFlags eFlags) : m_procParams(procParams) {
	m_pPrintParameters = pPrintParameters;
	m_pPrintDlg = pPrintDlg;
	m_pDeviceMode = pDeviceMode;
	m_printerName = printerName;
	m_portName = portName;

	m_pImage = pImage;
	m_eProcessingFlags = eFlags;
	m_dAspectRatioImage = (double)m_pImage->OrigWidth() / m_pImage->OrigHeight();

	m_paperTypes = NULL;
	m_paperSizes = NULL;
	m_paperTrays = NULL;
	m_blockUpdate = false;
	m_lockPrinterDC = false;
	m_dCurrentFixedPaperWidth = pPrintParameters->PrintWidth;

	// save to be able to restore these values when cancelling the dialog
	m_dOrigMarginLeft = pPrintParameters->MarginLeft;
	m_dOrigMarginTop = pPrintParameters->MarginTop;
	m_dOrigMarginRight = pPrintParameters->MarginRight; 
	m_dOrigMarginBottom = pPrintParameters->MarginBottom;

	m_currentPrintableRect = CRect();
	m_currentPaperRect = CRect();
	m_usingNonStandardCursor = false;
	m_handleDragMode = Handle_None;
	m_dPixelsPerMm = 1;
	m_leftMarginValid = m_rightMarginValid = m_topMarginValid = m_bottomMarginValid = true;

	m_hPrinterDC = NULL;
}

CPrintDlg::~CPrintDlg(void) {
	delete[] m_paperTypes;
	delete[] m_paperSizes;
	delete[] m_paperTrays;
}

LRESULT CPrintDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	this->SetWindowText(CNLS::GetString(_T("Print Image")));

	m_btnPrint.Attach(GetDlgItem(IDOK));
	m_btnCancel.Attach(GetDlgItem(IDCANCEL));
	m_btnChangePrinter.Attach(GetDlgItem(IDC_PD_BTN_CHANGE_PRINTER));
	m_lblPrinterTitle.Attach(GetDlgItem(IDC_PD_LABEL_PRINTER));
	m_lblPrinter.Attach(GetDlgItem(IDC_PD_PRINTER));
	m_lblPaper.Attach(GetDlgItem(IDC_PD_PAPER_TITLE));
	m_cbPaper.Attach(GetDlgItem(IDC_PD_CB_PAPER));
	m_lblPaperTray.Attach(GetDlgItem(IDC_PD_PAPER_SOURCE_TITLE));
	m_cbPaperTray.Attach(GetDlgItem(IDC_PD_CB_PAPER_SOURCE));
	m_lblPaperOrientation.Attach(GetDlgItem(IDC_PD_TITLE_ORIENTATION));
	m_cbPaperOrientation.Attach(GetDlgItem(IDC_PD_CB_ORIENTATION));
	m_lblSize.Attach(GetDlgItem(IDC_PD_SIZE_TITLE));
	m_rbFitToPaper.Attach(GetDlgItem(IDC_PD_RB_FIT));
	m_rbSize.Attach(GetDlgItem(IDC_PD_RB_FIXED));
	m_edtWidth.Attach(GetDlgItem(IDC_PD_ED_WIDTH));
	m_edtHeight.Attach(GetDlgItem(IDC_PD_ED_HEIGHT));
	m_lblCM.Attach(GetDlgItem(IDC_PD_CM));
	m_lblMargins.Attach(GetDlgItem(IDC_PD_MARGINS_TITLE));
	m_lblLeft.Attach(GetDlgItem(IDC_PD_LEFT_TITLE));
	m_lblRight.Attach(GetDlgItem(IDC_PD_RIGHT_TITLE));
	m_lblTop.Attach(GetDlgItem(IDC_PD_TOP_TITLE));
	m_lblBottom.Attach(GetDlgItem(IDC_PD_BOTTOM_TITLE));
	m_edtLeft.Attach(GetDlgItem(IDC_PD_ED_LEFT));
	m_edtRight.Attach(GetDlgItem(IDC_PD_ED_RIGHT));
	m_edtTop.Attach(GetDlgItem(IDC_PD_ED_TOP));
	m_edtBottom.Attach(GetDlgItem(IDC_PD_ED_BOTTOM));
	m_lblCM2.Attach(GetDlgItem(IDC_PD_CM2));
	m_lblCM3.Attach(GetDlgItem(IDC_PD_CM3));
	m_lblScalingMode.Attach(GetDlgItem(IDC_PD_SCALING_TITLE));
	m_rbScalingModePrinterDriver.Attach(GetDlgItem(IDC_PD_RB_SCALING_DRIVER));
	m_rbScalingModeJPEGView.Attach(GetDlgItem(IDC_PD_RB_SCALING_JPEGVIEW));
	m_lblAlignment.Attach(GetDlgItem(IDC_PD_ALIGNMENT_TITLE));
	m_rbAlign00.Attach(GetDlgItem(IDC_PD_RB_00));
	m_rbAlign01.Attach(GetDlgItem(IDC_PD_RB_01));
	m_rbAlign02.Attach(GetDlgItem(IDC_PD_RB_02));
	m_rbAlign10.Attach(GetDlgItem(IDC_PD_RB_10));
	m_rbAlign11.Attach(GetDlgItem(IDC_PD_RB_11));
	m_rbAlign12.Attach(GetDlgItem(IDC_PD_RB_12));
	m_rbAlign20.Attach(GetDlgItem(IDC_PD_RB_20));
	m_rbAlign21.Attach(GetDlgItem(IDC_PD_RB_21));
	m_rbAlign22.Attach(GetDlgItem(IDC_PD_RB_22));

	m_btnPrint.SetWindowText(CNLS::GetString(_T("Print")));
	m_btnCancel.SetWindowText(CNLS::GetString(_T("Cancel")));
	m_btnChangePrinter.SetWindowText(CNLS::GetString(_T("Select Printer...")));

	m_lblPrinterTitle.SetWindowText(CNLS::GetString(_T("Printer:")));
	m_lblPaper.SetWindowText(CNLS::GetString(_T("Paper:")));
	m_lblPaperTray.SetWindowText(CNLS::GetString(_T("Source:")));
	m_lblPaperOrientation.SetWindowText(CNLS::GetString(_T("Orientation:")));
	m_lblSize.SetWindowText(CNLS::GetString(_T("Size:")));
	m_rbFitToPaper.SetWindowText(CNLS::GetString(_T("Fit to paper")));
	m_lblCM.SetWindowText(CNLS::GetString(_T("cm")));
	m_lblScalingMode.SetWindowText(CNLS::GetString(_T("Scaling mode:")));
	m_rbScalingModePrinterDriver.SetWindowText(CNLS::GetString(_T("Printer driver")));
	m_lblAlignment.SetWindowText(CNLS::GetString(_T("Alignment:")));
	m_lblMargins.SetWindowText(CNLS::GetString(_T("Margins:")));
	m_lblLeft.SetWindowText(CNLS::GetString(_T("Left")));
	m_lblRight.SetWindowText(CNLS::GetString(_T("Right")));
	m_lblTop.SetWindowText(CNLS::GetString(_T("Top")));
	m_lblBottom.SetWindowText(CNLS::GetString(_T("Bottom")));
	m_lblCM2.SetWindowText(CNLS::GetString(_T("cm")));
	m_lblCM3.SetWindowText(CNLS::GetString(_T("cm")));

	m_edtWidth.SetLimitText(6);
	m_edtHeight.SetLimitText(6);
	m_edtLeft.SetLimitText(6);
	m_edtRight.SetLimitText(6);
	m_edtTop.SetLimitText(6);
	m_edtBottom.SetLimitText(6);

	const double Mm10ToCm = 0.01; // 1/10 mm to cm
	SetNumber(m_edtWidth, m_pPrintParameters->PrintWidth * Mm10ToCm);
	m_blockUpdate = true;
	SetNumber(m_edtLeft, m_pPrintParameters->MarginLeft * Mm10ToCm);
	SetNumber(m_edtRight, m_pPrintParameters->MarginRight * Mm10ToCm);
	SetNumber(m_edtTop, m_pPrintParameters->MarginTop * Mm10ToCm);
	SetNumber(m_edtBottom, m_pPrintParameters->MarginBottom * Mm10ToCm);
	m_blockUpdate = false;

	if (m_pPrintParameters->FitToPaper) m_rbFitToPaper.SetCheck(TRUE);
	else m_rbSize.SetCheck(TRUE);

	if (m_pPrintParameters->ScaleByPrinterDriver) m_rbScalingModePrinterDriver.SetCheck(TRUE);
	else m_rbScalingModeJPEGView.SetCheck(TRUE);

	if (m_pPrintParameters->AlignmentX == CPrintParameters::HA_Left) {
		switch (m_pPrintParameters->AlignmentY) {
		case CPrintParameters::VA_Top:
			m_rbAlign00.SetCheck(TRUE); break;
		case CPrintParameters::VA_Center:
			m_rbAlign01.SetCheck(TRUE); break;
		default:
			m_rbAlign02.SetCheck(TRUE); break;
		}
	} else if (m_pPrintParameters->AlignmentX == CPrintParameters::HA_Center) {
		switch (m_pPrintParameters->AlignmentY) {
		case CPrintParameters::VA_Top:
			m_rbAlign10.SetCheck(TRUE); break;
		case CPrintParameters::VA_Center:
			m_rbAlign11.SetCheck(TRUE); break;
		default:
			m_rbAlign12.SetCheck(TRUE); break;
		}
	} else {
		switch (m_pPrintParameters->AlignmentY) {
		case CPrintParameters::VA_Top:
			m_rbAlign20.SetCheck(TRUE); break;
		case CPrintParameters::VA_Center:
			m_rbAlign21.SetCheck(TRUE); break;
		default:
			m_rbAlign22.SetCheck(TRUE); break;
		}
	}

	SetupDataForCurrentPrinter();
	Validate();

	return TRUE;
}

LRESULT CPrintDlg::OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	CPaintDC dc(m_hWnd);

	m_currentPrintableRect = CRect();
	if (m_pDeviceMode == NULL) return 0;
	if (m_paperSizes == NULL) return 0;
	if (m_hPrinterDC == NULL) return 0;

	POINT paperSize = GetUsedPaperSize();
	if (paperSize.x == 0 || paperSize.y == 0) return 0;

	CRect paintBox = GetPaperPaintBoundingBox();
	CRect fittedPaper = Helpers::FitRectIntoRect(CSize(paperSize), paintBox);
	m_currentPaperRect = fittedPaper;

	m_dPixelsPerMm = (double)fittedPaper.Width() / (paperSize.x * 0.1);

	CRect printableRect = GetPrintableBoundingBox(fittedPaper, CSize(paperSize));
	m_currentPrintableRect = printableRect;
	
	bool fitToPaper = m_rbFitToPaper.GetCheck() != 0;
	CSize imageSize = fitToPaper ? Helpers::FitRectIntoRect(m_pImage->OrigSize(), printableRect).Size() : GetFixedImageSize(m_pImage->OrigSize(), CSize(paperSize), fittedPaper.Size());

	CPrintParameters::HorizontalAlignment horizontalAlignment;
	CPrintParameters::VerticalAlignment verticalAlignment;
	GetAlignment(horizontalAlignment, verticalAlignment);
	CRect imageRect = Align(imageSize, printableRect, horizontalAlignment, verticalAlignment);

	void* pDIBData = m_pImage->GetThumbnailDIB(imageRect.Size(), m_procParams, m_eProcessingFlags);

	dc.SelectStockPen(BLACK_PEN);

	dc.FillSolidRect(fittedPaper, RGB(255, 255, 255));
	dc.Rectangle(fittedPaper);

	if (pDIBData != NULL) {
		int xDest = imageRect.left;
		int yDest = imageRect.top;
		int xSize = imageRect.Width();
		int ySize = imageRect.Height();
		BITMAPINFO bmInfo;
		memset(&bmInfo, 0, sizeof(BITMAPINFO));
		bmInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmInfo.bmiHeader.biWidth = xSize;
		bmInfo.bmiHeader.biHeight = -ySize;
		bmInfo.bmiHeader.biPlanes = 1;
		bmInfo.bmiHeader.biBitCount = 32;
		bmInfo.bmiHeader.biCompression = BI_RGB;
		dc.IntersectClipRect(printableRect);
		dc.SetDIBitsToDevice(xDest, yDest, xSize, ySize, 0, 0, 0, ySize, pDIBData,
			&bmInfo, DIB_RGB_COLORS);
	}

	CPen dottedPen;
	dottedPen.CreatePen(PS_DOT, 1, RGB(190, 190, 190));
	dc.SelectPen(dottedPen);
	dc.SelectStockBrush(HOLLOW_BRUSH);

	dc.Rectangle(printableRect);

	dc.SelectStockPen(BLACK_PEN);
	return 0;
}

LRESULT CPrintDlg::OnMouseMove(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/) {
	int mouseX = GET_X_LPARAM(lParam);
	int mouseY = GET_Y_LPARAM(lParam);
	bool mouseCursorSet = SetMarginCursor(mouseX, mouseY);
	if (mouseCursorSet) {
		m_usingNonStandardCursor = true;
	} else if (m_usingNonStandardCursor) {
		::SetCursor(::LoadCursor(NULL, MAKEINTRESOURCE(IDC_ARROW)));
		m_usingNonStandardCursor = false;
	}
	DragMargin(mouseX, mouseY);
	return 0;
}

LRESULT CPrintDlg::OnLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	int mouseX = GET_X_LPARAM(lParam);
	int mouseY = GET_Y_LPARAM(lParam);
	EHandle handleHit = PrintAreaHandleHit(mouseX, mouseY);
	if (handleHit != Handle_None) {
		m_handleDragMode = handleHit;
		m_startDragPoint = CPoint(mouseX, mouseY);
		switch (handleHit)
		{
		case CPrintDlg::Handle_Left:
			m_startDragMargin = m_pPrintParameters->MarginLeft;
			m_maxMargin = 10 * (m_currentPrintableRect.right - m_currentPaperRect.left - 2) / m_dPixelsPerMm;
			break;
		case CPrintDlg::Handle_Right:
			m_startDragMargin = m_pPrintParameters->MarginRight;
			m_maxMargin = 10 * (m_currentPaperRect.right - m_currentPrintableRect.left - 2) / m_dPixelsPerMm;
			break;
		case CPrintDlg::Handle_Top:
			m_startDragMargin = m_pPrintParameters->MarginTop;
			m_maxMargin = 10 * (m_currentPrintableRect.bottom - m_currentPaperRect.top - 2) / m_dPixelsPerMm;
			break;
		case CPrintDlg::Handle_Bottom:
			m_startDragMargin = m_pPrintParameters->MarginBottom;
			m_maxMargin = 10 * (m_currentPaperRect.bottom - m_currentPrintableRect.top - 2) / m_dPixelsPerMm;
			break;
		}
		InvalidateRect(GetPaperPaintBoundingBox());
	}
	return 0;
}

LRESULT CPrintDlg::OnLButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	if (m_handleDragMode != Handle_None) {
		m_handleDragMode = Handle_None;
		InvalidateRect(GetPaperPaintBoundingBox());
		Validate();
	}
	return 0;
}

LRESULT CPrintDlg::OnChangePrinter(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	m_pPrintDlg->m_pdex.dwResultAction = 0;
	m_pPrintDlg->m_pdex.Flags = PD_ALLPAGES | PD_USEDEVMODECOPIES | PD_NOPAGENUMS | PD_NOSELECTION | PD_NOCURRENTPAGE | PD_USEDEVMODECOPIESANDCOLLATE;
	if (m_pPrintDlg->DoModal(m_hWnd) == S_OK && m_pPrintDlg->m_pdex.dwResultAction != PD_RESULT_CANCEL) {
		m_pDeviceMode = m_pPrintDlg->GetDevMode();
		m_printerName = m_pPrintDlg->GetDeviceName();
		m_portName = m_pPrintDlg->GetPortName();
		SetupDataForCurrentPrinter();
		InvalidateRect(GetPaperPaintBoundingBox());
		Validate();
	}
	return 0;
}

LRESULT CPrintDlg::OnSetFocusToUserSize(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	m_rbFitToPaper.SetCheck(FALSE);
	m_rbSize.SetCheck(TRUE);
	InvalidateRect(GetPaperPaintBoundingBox());
	Validate();
	return 0;
}

LRESULT CPrintDlg::OnWidthChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (m_blockUpdate) return 0;
	m_blockUpdate = true;
	double dWidth = ConvertToNumber(m_edtWidth);
	SetNumber(m_edtHeight, dWidth / m_dAspectRatioImage);
	m_blockUpdate = false;
	InvalidateRect(GetPaperPaintBoundingBox());
	Validate();
	return 0;
}

LRESULT CPrintDlg::OnHeightChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (m_blockUpdate) return 0;
	m_blockUpdate = true;
	double dHeight = ConvertToNumber(m_edtHeight);
	SetNumber(m_edtWidth, dHeight * m_dAspectRatioImage);
	m_blockUpdate = false;
	InvalidateRect(GetPaperPaintBoundingBox());
	Validate();
	return 0;
}

LRESULT CPrintDlg::OnLeftMarginChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (m_blockUpdate) return 0;
	double left = ConvertToNumber(m_edtLeft) * 100;
	double maxMargin = 10 * (m_currentPrintableRect.right - m_currentPaperRect.left - 2) / m_dPixelsPerMm;
	m_leftMarginValid = left >= 0 && left <= maxMargin;
	if (m_leftMarginValid) m_pPrintParameters->MarginLeft = left;
	InvalidateRect(GetPaperPaintBoundingBox());
	Validate();
	return 0;
}

LRESULT CPrintDlg::OnRightMarginChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (m_blockUpdate) return 0;
	double right = ConvertToNumber(m_edtRight) * 100;
	double maxMargin = 10 * (m_currentPaperRect.right - m_currentPrintableRect.left - 2) / m_dPixelsPerMm;
	m_rightMarginValid = right >= 0 && right <= maxMargin;
	if (m_rightMarginValid) m_pPrintParameters->MarginRight = right;
	InvalidateRect(GetPaperPaintBoundingBox());
	Validate();
	return 0;
}

LRESULT CPrintDlg::OnTopMarginChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (m_blockUpdate) return 0;
	double top = ConvertToNumber(m_edtTop) * 100;
	double maxMargin = 10 * (m_currentPrintableRect.bottom - m_currentPaperRect.top - 2) / m_dPixelsPerMm;
	m_topMarginValid = top >= 0 && top <= maxMargin;
	if (m_topMarginValid) m_pPrintParameters->MarginTop = top;
	InvalidateRect(GetPaperPaintBoundingBox());
	Validate();
	return 0;
}

LRESULT CPrintDlg::OnBottomMarginChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (m_blockUpdate) return 0;
	double bottom = ConvertToNumber(m_edtBottom) * 100;
	double maxMargin = 10 * (m_currentPaperRect.bottom - m_currentPrintableRect.top - 2) / m_dPixelsPerMm;
	m_bottomMarginValid = bottom >= 0 && bottom <= maxMargin;
	if (m_bottomMarginValid) m_pPrintParameters->MarginBottom = bottom;
	InvalidateRect(GetPaperPaintBoundingBox());
	Validate();
	return 0;
}

LRESULT CPrintDlg::OnSelectedPaperChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (m_pDeviceMode == NULL || m_paperTypes == NULL || m_lockPrinterDC) return 0;
	
	int selectedPaper = m_cbPaper.GetCurSel();
	if (selectedPaper >= 0) {
		m_pDeviceMode->dmFields |= DM_PAPERSIZE;
		m_pDeviceMode->dmPaperSize = m_paperTypes[selectedPaper];
		RecreatePrinterDC();
		InvalidateRect(GetPaperPaintBoundingBox());
	}
	return 0;
}

LRESULT CPrintDlg::OnSelectedTrayChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (m_pDeviceMode == NULL || m_paperTrays == NULL || m_lockPrinterDC) return 0;

	int selectedTray = m_cbPaperTray.GetCurSel();
	if (selectedTray >= 0) {
		m_pDeviceMode->dmFields |= DM_DEFAULTSOURCE;
		m_pDeviceMode->dmDefaultSource = m_paperTrays[selectedTray];
		RecreatePrinterDC();
		InvalidateRect(GetPaperPaintBoundingBox());
	}
	return 0;
}

LRESULT CPrintDlg::OnSelectedPaperOrientationChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (m_pDeviceMode == NULL || m_lockPrinterDC) return 0;

	int selectedOrientation = m_cbPaperOrientation.GetCurSel();
	if (selectedOrientation >= 0) {
		m_pDeviceMode->dmFields |= DM_ORIENTATION;
		m_pDeviceMode->dmOrientation = (selectedOrientation == 0) ? DMORIENT_PORTRAIT : DMORIENT_LANDSCAPE;
		RecreatePrinterDC();
		InvalidateRect(GetPaperPaintBoundingBox());
	}
	return 0;
}

LRESULT CPrintDlg::OnSizingModeChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	InvalidateRect(GetPaperPaintBoundingBox());
	Validate();
	return 0;
}

LRESULT CPrintDlg::OnAlignmentButtonClicked(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	InvalidateRect(GetPaperPaintBoundingBox());
	return 0;
}

LRESULT CPrintDlg::OnPrint(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	// Store the user parameters
	m_pPrintParameters->PrintWidth = m_dCurrentFixedPaperWidth;

	m_pPrintParameters->FitToPaper = m_rbFitToPaper.GetCheck() != 0;
	m_pPrintParameters->ScaleByPrinterDriver = m_rbScalingModePrinterDriver.GetCheck() != 0;

	GetAlignment(m_pPrintParameters->AlignmentX, m_pPrintParameters->AlignmentY);

	EndDialog(IDOK);
	return 0;
}

LRESULT CPrintDlg::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	// Restore these values
	m_pPrintParameters->MarginLeft = m_dOrigMarginLeft;
	m_pPrintParameters->MarginTop = m_dOrigMarginTop;
	m_pPrintParameters->MarginRight = m_dOrigMarginRight;
	m_pPrintParameters->MarginBottom = m_dOrigMarginBottom;
	EndDialog(IDCANCEL);
	return 0;
}

double CPrintDlg::ConvertToNumber(CEdit& editControl) {
	const int MAX_LEN = 16;
	TCHAR sNumber[MAX_LEN];
	editControl.GetWindowText(sNumber, MAX_LEN);
	double value = -1;
	_stscanf_s(sNumber, _T("%lf"), &value);
	return value;
}

void CPrintDlg::SetNumber(CEdit& editControl, double number) {
	if (number >= 0) {
		const int MAX_LEN = 16;
		TCHAR buffer[MAX_LEN];
		_stprintf_s(buffer, MAX_LEN, _T("%.2f"), number);
		editControl.SetWindowText(buffer);
	} else {
		editControl.SetWindowText(_T(""));
	}
}

void CPrintDlg::SetupDataForCurrentPrinter() {
	m_lblPrinter.SetWindowText(m_printerName);

	RecreatePrinterDC();

	m_lockPrinterDC = true;

	delete[] m_paperTypes;
	delete[] m_paperSizes;
	m_cbPaper.ResetContent();

	LPCTSTR* paperNames = GetPaperNames(m_numberOfPapers, m_paperTypes, m_paperSizes);
	if (paperNames != NULL) {
		for (int i = 0; i < m_numberOfPapers; i++) {
			m_cbPaper.AddString(paperNames[i]);
		}
		delete (char*)paperNames;

		int index;
		if ((m_pDeviceMode->dmFields & DM_PAPERSIZE) != 0 && (index = GetIndexOfPaper(m_pDeviceMode->dmPaperSize)) >= 0) {
			m_cbPaper.SetCurSel(index);
		} else {
			m_cbPaper.SetCurSel(0);
			m_lockPrinterDC = false;
			BOOL dummy; OnSelectedPaperChanged(0, 0, m_cbPaper.m_hWnd, dummy);
			m_lockPrinterDC = true;
		}
	}

	delete[] m_paperTrays;
	m_cbPaperTray.ResetContent();

	LPCTSTR* trayNames = GetTrayNames(m_numberOfTrays, m_paperTrays);
	if (trayNames != NULL) {
		for (int i = 0; i < m_numberOfTrays; i++) {
			m_cbPaperTray.AddString(trayNames[i]);
		}
		delete (char*)trayNames;

		if ((m_pDeviceMode->dmFields & DM_DEFAULTSOURCE) != 0) {
			for (int i = 0; i < m_numberOfTrays; i++) {
				if (m_pDeviceMode->dmDefaultSource == m_paperTrays[i]) {
					m_cbPaperTray.SetCurSel(i);
					break;
				}
			}
		} else {
			m_cbPaperTray.SetCurSel(0);
			m_lockPrinterDC = false;
			BOOL dummy; OnSelectedTrayChanged(0, 0, m_cbPaperTray.m_hWnd, dummy);
			m_lockPrinterDC = true;
		}
	}

	m_cbPaperOrientation.ResetContent();
	m_cbPaperOrientation.AddString(CNLS::GetString(_T("Portrait")));
	m_cbPaperOrientation.AddString(CNLS::GetString(_T("Landscape")));
	if (m_pDeviceMode != NULL && (m_pDeviceMode->dmFields & DM_ORIENTATION) != 0) {
		m_cbPaperOrientation.SetCurSel((m_pDeviceMode->dmOrientation == DMORIENT_PORTRAIT) ? 0 : 1);
	} else {
		m_cbPaperOrientation.SetCurSel(0);
		m_lockPrinterDC = false;
		BOOL dummy; OnSelectedPaperOrientationChanged(0, 0, m_cbPaperOrientation.m_hWnd, dummy);
		m_lockPrinterDC = true;
	}

	m_lockPrinterDC = false;
}

LPCTSTR* CPrintDlg::GetPaperNames(int& numberOfPapers, WORD*& paperTypes, POINT*& paperSizes) {
	if (m_pDeviceMode == NULL) return NULL;
	numberOfPapers = ::DeviceCapabilities(m_printerName, m_portName, DC_PAPERNAMES, NULL, m_pDeviceMode);
	if (numberOfPapers <= 0) return NULL;

	char* pBuffer = new char[sizeof(LPCTSTR)* numberOfPapers + sizeof(TCHAR)* 64 * numberOfPapers];
	LPTSTR papersStart = (LPTSTR)(pBuffer + sizeof(LPCTSTR)* numberOfPapers);

	if (::DeviceCapabilities(m_printerName, m_portName, DC_PAPERNAMES, papersStart, m_pDeviceMode) <= 0) {
		delete[] pBuffer;
		return NULL;
	}

	int numberOfEntries = ::DeviceCapabilities(m_printerName, m_portName, DC_PAPERS, NULL, m_pDeviceMode);
	if (numberOfEntries != numberOfPapers) {
		delete[] pBuffer;
		return NULL;
	}
	paperTypes = new WORD[numberOfEntries];
	if (::DeviceCapabilities(m_printerName, m_portName, DC_PAPERS, (LPWSTR)paperTypes, m_pDeviceMode) <= 0) {
		delete[] pBuffer;
		delete[] paperTypes;
		return NULL;
	}

	paperSizes = new POINT[numberOfEntries];
	if (::DeviceCapabilities(m_printerName, m_portName, DC_PAPERSIZE, (LPWSTR)paperSizes, m_pDeviceMode) <= 0) {
		delete[] pBuffer;
		delete[] paperTypes;
		delete[] paperSizes;
		return NULL;
	}

	LPCTSTR* papers = (LPCTSTR*)pBuffer;
	for (int i = 0; i < numberOfPapers; i++) {
		papersStart[i * 64 + 63] = 0; // force zero termination
		papers[i] = &(papersStart[i * 64]);
	}

	return papers;
}

LPCTSTR* CPrintDlg::GetTrayNames(int& numberOfTrays, WORD*& paperTrays)  {
	if (m_pDeviceMode == NULL) return NULL;
	numberOfTrays = ::DeviceCapabilities(m_printerName, m_portName, DC_BINNAMES, NULL, m_pDeviceMode);
	if (numberOfTrays <= 0) return NULL;

	char* pBuffer = new char[sizeof(LPCTSTR)* numberOfTrays + sizeof(TCHAR)* 24 * numberOfTrays];
	LPTSTR traysStart = (LPTSTR)(pBuffer + sizeof(LPCTSTR)* numberOfTrays);

	if (::DeviceCapabilities(m_printerName, m_portName, DC_BINNAMES, traysStart, m_pDeviceMode) <= 0) {
		delete[] pBuffer;
		return NULL;
	}

	int numberOfEntries = ::DeviceCapabilities(m_printerName, m_portName, DC_BINS, NULL, m_pDeviceMode);
	if (numberOfEntries != numberOfTrays) {
		delete[] pBuffer;
		return NULL;
	}
	paperTrays = new WORD[numberOfEntries];
	if (::DeviceCapabilities(m_printerName, m_portName, DC_BINS, (LPWSTR)paperTrays, m_pDeviceMode) <= 0) {
		delete[] pBuffer;
		delete[] paperTrays;
		return NULL;
	}

	LPCTSTR* trays = (LPCTSTR*)pBuffer;
	for (int i = 0; i < numberOfTrays; i++) {
		traysStart[i * 24 + 23] = 0; // force zero termination
		trays[i] = &(traysStart[i * 24]);
	}

	return trays;
}

int CPrintDlg::GetIndexOfPaper(WORD paper) {
	for (int i = 0; i < m_numberOfPapers; i++) {
		if (paper == m_paperTypes[i]) {
			return i;
		}
	}
	return -1;
}

POINT CPrintDlg::GetUsedPaperSize() {
	int index;
	POINT paperSize;
	paperSize.x = paperSize.y = 0;
	if ((m_pDeviceMode->dmFields & DM_PAPERLENGTH) && (m_pDeviceMode->dmFields & DM_PAPERWIDTH)) {
		paperSize.x = m_pDeviceMode->dmPaperWidth;
		paperSize.y = m_pDeviceMode->dmPaperLength;
	} else if ((m_pDeviceMode->dmFields & DM_PAPERSIZE) != 0 && (index = GetIndexOfPaper(m_pDeviceMode->dmPaperSize)) >= 0) {
		paperSize = m_paperSizes[index];
	}
	bool isLandscape = (m_pDeviceMode->dmFields & DM_ORIENTATION) ? m_pDeviceMode->dmOrientation == DMORIENT_LANDSCAPE : false;
	if (isLandscape)
	{
		LONG t = paperSize.x;
		paperSize.x = paperSize.y;
		paperSize.y = t;
	}
	return paperSize;
}

CRect CPrintDlg::GetPaperPaintBoundingBox() {
	CRect printerTitleRect = GetLocalCoordinates(m_lblPrinterTitle.m_hWnd);
	CRect printButtonRect = GetLocalCoordinates(m_btnPrint.m_hWnd);;

	return CRect(printerTitleRect.top, printerTitleRect.top, printerTitleRect.left - printerTitleRect.top, printButtonRect.top);
}

CRect CPrintDlg::GetPrintableBoundingBox(const CRect& pixelPaperRect, const CSize& paperSize) {
	double marginLeft = max(m_pPrintParameters->MarginLeft, m_dDeviceMarginLeft);
	double marginTop = max(m_pPrintParameters->MarginTop, m_dDeviceMarginTop);
	double marginRight = max(m_pPrintParameters->MarginRight, m_dDeviceMarginRight);
	double marginBottom = max(m_pPrintParameters->MarginBottom, m_dDeviceMarginBottom);

	marginLeft = pixelPaperRect.Width() * marginLeft / paperSize.cx;
	marginRight = pixelPaperRect.Width() * marginRight / paperSize.cx;
	marginTop = pixelPaperRect.Height() * marginTop / paperSize.cy;
	marginBottom = pixelPaperRect.Height() * marginBottom / paperSize.cy;

	int left = pixelPaperRect.left + Helpers::RoundToInt(marginLeft);
	int top = pixelPaperRect.top + Helpers::RoundToInt(marginTop);
	int right = pixelPaperRect.right - Helpers::RoundToInt(marginRight);
	int bottom = pixelPaperRect.bottom - Helpers::RoundToInt(marginBottom);
	if (left >= right) {
		int middle = (left + right) / 2;
		left = middle - 1;
		right = middle + 1;
	}
	if (top >= bottom) {
		int middle = (top + bottom) / 2;
		top = middle - 1;
		bottom = middle + 1;
	}
	return CRect(left, top, right, bottom);
}

CSize CPrintDlg::GetFixedImageSize(const CSize& imageSize, const CSize& paperSizeMm10, const CSize& paperSizePixels) {
	double width = ConvertToNumber(m_edtWidth) * 100; // now in 1/10 mm
	if (ValidateFixedImageSize(width, paperSizeMm10)) m_dCurrentFixedPaperWidth = width;
	double imageAR = (double)imageSize.cy / imageSize.cx;
	double widthInPixels = paperSizePixels.cx * width / paperSizeMm10.cx;
	return CSize(Helpers::RoundToInt(widthInPixels), Helpers::RoundToInt(widthInPixels * imageAR));
}

CRect CPrintDlg::Align(const CSize& size, const CRect& rect, CPrintParameters::HorizontalAlignment horizontalAlignment,
	CPrintParameters::VerticalAlignment verticalAlignment) {
	int xOffset = 0, yOffset = 0;
	switch (horizontalAlignment) {
	case CPrintParameters::HA_Center:
		xOffset = (rect.Width() - size.cx) / 2;
		break;
	case CPrintParameters::HA_Right:
		xOffset = rect.Width() - size.cx;
		break;
	}
	switch (verticalAlignment) {
	case CPrintParameters::VA_Center:
		yOffset = (rect.Height() - size.cy) / 2;
		break;
	case CPrintParameters::HA_Right:
		yOffset = rect.Height() - size.cy;
		break;
	}
	return CRect(rect.left + xOffset, rect.top + yOffset, rect.left + xOffset + size.cx, rect.top + yOffset + size.cy);
}

CRect CPrintDlg::GetLocalCoordinates(HWND hWnd)
{
	CRect rect;
	::GetWindowRect(hWnd, &rect);
	::MapWindowPoints(HWND_DESKTOP, ::GetParent(hWnd), (LPPOINT)&rect, 2);
	return rect;
}

void CPrintDlg::RecreatePrinterDC() {
	if (m_hPrinterDC != NULL) {
		::DeleteDC(m_hPrinterDC);
	}
	m_hPrinterDC = (m_pDeviceMode == NULL) ? NULL : ::CreateDC(NULL, m_printerName, NULL, m_pDeviceMode);

	if (m_hPrinterDC != NULL) {
		::SetMapMode(m_hPrinterDC, MM_TEXT);

		int dpiX = ::GetDeviceCaps(m_hPrinterDC, LOGPIXELSX);
		int dpiY = ::GetDeviceCaps(m_hPrinterDC, LOGPIXELSY);
		int pixelsX = ::GetDeviceCaps(m_hPrinterDC, HORZRES);
		int pixelsY = ::GetDeviceCaps(m_hPrinterDC, VERTRES);

		int physicalWidth = ::GetDeviceCaps(m_hPrinterDC, PHYSICALWIDTH);
		int physicalHeight = ::GetDeviceCaps(m_hPrinterDC, PHYSICALHEIGHT);
		int physicalOffsetX = ::GetDeviceCaps(m_hPrinterDC, PHYSICALOFFSETX);
		int physicalOffsetY = ::GetDeviceCaps(m_hPrinterDC, PHYSICALOFFSETY);

		m_dDeviceMarginLeft = PointsToMm10(physicalOffsetX, dpiX);
		m_dDeviceMarginTop = PointsToMm10(physicalOffsetY, dpiY);
		m_dDeviceMarginRight = PointsToMm10(physicalWidth - physicalOffsetX - pixelsX, dpiX);
		m_dDeviceMarginBottom = PointsToMm10(physicalHeight - physicalOffsetY - pixelsY, dpiY);
	}
}

void CPrintDlg::GetAlignment(CPrintParameters::HorizontalAlignment& horizontalAlignment, CPrintParameters::VerticalAlignment& verticalAlignment) {
	if (m_rbAlign00.GetCheck()) {
		horizontalAlignment = CPrintParameters::HA_Left;
		verticalAlignment = CPrintParameters::VA_Top;
	} else if (m_rbAlign10.GetCheck()) {
		horizontalAlignment = CPrintParameters::HA_Center;
		verticalAlignment = CPrintParameters::VA_Top;
	} else if (m_rbAlign20.GetCheck()) {
		horizontalAlignment = CPrintParameters::HA_Right;
		verticalAlignment = CPrintParameters::VA_Top;
	} else if (m_rbAlign01.GetCheck()) {
		horizontalAlignment = CPrintParameters::HA_Left;
		verticalAlignment = CPrintParameters::VA_Center;
	} else if (m_rbAlign11.GetCheck()) {
		horizontalAlignment = CPrintParameters::HA_Center;
		verticalAlignment = CPrintParameters::VA_Center;
	} else if (m_rbAlign21.GetCheck()) {
		horizontalAlignment = CPrintParameters::HA_Right;
		verticalAlignment = CPrintParameters::VA_Center;
	} else if (m_rbAlign02.GetCheck()) {
		horizontalAlignment = CPrintParameters::HA_Left;
		verticalAlignment = CPrintParameters::VA_Bottom;
	} else if (m_rbAlign12.GetCheck()) {
		horizontalAlignment = CPrintParameters::HA_Center;
		verticalAlignment = CPrintParameters::VA_Bottom;
	} else if (m_rbAlign22.GetCheck()) {
		horizontalAlignment = CPrintParameters::HA_Right;
		verticalAlignment = CPrintParameters::VA_Bottom;
	} else {
		horizontalAlignment = CPrintParameters::HA_Center;
		verticalAlignment = CPrintParameters::VA_Center;
	}
}

bool CPrintDlg::SetMarginCursor(int x, int y) {
	EHandle handleHit = (m_handleDragMode != Handle_None) ? m_handleDragMode : PrintAreaHandleHit(x, y);
	switch (handleHit) {
	case Handle_Left:
	case Handle_Right:
		::SetCursor(::LoadCursor(NULL, IDC_SIZEWE));
		return true;
	case Handle_Top:
	case Handle_Bottom:
		::SetCursor(::LoadCursor(NULL, IDC_SIZENS));
		return true;
	}
	return false;
}

CPrintDlg::EHandle CPrintDlg::PrintAreaHandleHit(int x, int y) {
	if (m_currentPrintableRect.IsRectNull()) return Handle_None;
	int gap = HelpersGUI::ScaleToScreen(5);
	if (x > m_currentPrintableRect.left - gap && x < m_currentPrintableRect.left + gap && y > m_currentPrintableRect.top && y < m_currentPrintableRect.bottom) {
		return Handle_Left;
	}
	if (x > m_currentPrintableRect.right - gap && x < m_currentPrintableRect.right + gap && y > m_currentPrintableRect.top && y < m_currentPrintableRect.bottom) {
		return Handle_Right;
	}
	if (y > m_currentPrintableRect.top - gap && y < m_currentPrintableRect.top + gap && x > m_currentPrintableRect.left && x < m_currentPrintableRect.right) {
		return Handle_Top;
	}
	if (y > m_currentPrintableRect.bottom - gap && y < m_currentPrintableRect.bottom + gap && x > m_currentPrintableRect.left && x < m_currentPrintableRect.right) {
		return Handle_Bottom;
	}
	return Handle_None;
}

void CPrintDlg::DragMargin(int x, int y) {
	if (m_handleDragMode != Handle_None) {
		const double Mm10ToCm = 0.01; // 1/10 mm to cm
		CPoint delta = CPoint(x - m_startDragPoint.x, y - m_startDragPoint.y);
		m_blockUpdate = true;
		switch (m_handleDragMode) {
		case Handle_Left:
			m_pPrintParameters->MarginLeft = max(0.0, min(m_maxMargin, m_startDragMargin + 10 * delta.x / m_dPixelsPerMm));
			m_leftMarginValid = true;
			SetNumber(m_edtLeft, m_pPrintParameters->MarginLeft * Mm10ToCm);
			break;
		case Handle_Right:
			m_pPrintParameters->MarginRight = max(0.0, min(m_maxMargin, m_startDragMargin - 10 * delta.x / m_dPixelsPerMm));
			m_rightMarginValid = true;
			SetNumber(m_edtRight, m_pPrintParameters->MarginRight * Mm10ToCm);
			break;
		case Handle_Top:
			m_pPrintParameters->MarginTop = max(0.0, min(m_maxMargin, m_startDragMargin + 10 * delta.y / m_dPixelsPerMm));
			m_topMarginValid = true;
			SetNumber(m_edtTop, m_pPrintParameters->MarginTop * Mm10ToCm);
			break;
		case Handle_Bottom:
			m_pPrintParameters->MarginBottom = max(0.0, min(m_maxMargin, m_startDragMargin - 10 * delta.y / m_dPixelsPerMm));
			m_bottomMarginValid = true;
			SetNumber(m_edtBottom, m_pPrintParameters->MarginBottom * Mm10ToCm);
			break;
		}
		m_blockUpdate = false;
		InvalidateRect(GetPaperPaintBoundingBox(), FALSE);
	}
}

void CPrintDlg::Validate() {
	if (m_paperSizes == NULL || m_pDeviceMode == NULL && m_hPrinterDC == NULL) {
		m_btnPrint.EnableWindow(FALSE);
		return;
	}
	bool fitToPaper = m_rbFitToPaper.GetCheck() != 0;
	bool isValid = fitToPaper || ValidateFixedImageSize(ConvertToNumber(m_edtWidth) * 100, CSize(GetUsedPaperSize()));
	isValid = isValid && m_leftMarginValid && m_rightMarginValid && m_topMarginValid && m_bottomMarginValid;
	m_btnPrint.EnableWindow(isValid);
}

bool CPrintDlg::ValidateFixedImageSize(double width, const CSize& paperSize) {
	return width > 0 && Helpers::RoundToInt(width) <= paperSize.cx * 2;
}
