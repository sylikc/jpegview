#pragma once

#include "ProcessParams.h"

class CJPEGImage;
class CPrintParameters;

// Allows printing an image. Keep only one instance of this class to retain the user data during different print jobs.
class CPrintImage
{
public:
	// Default margin and print width is in centimeters
	CPrintImage(double defaultMargin, double defaultPrintWidth) {
		m_defaultMargin = defaultMargin;
		m_defaultPrintWidth = defaultPrintWidth;
		m_pPrinterSelectionDlg = NULL;
		m_pPrintParameters = NULL;
	}

	// Shows the print dialog and prints the image if the user confirms to print
	bool Print(HWND hWnd, CJPEGImage * pImage, const CImageProcessingParams& procParams,
		EProcessingFlags eFlags, LPCTSTR fileName);

private:
	CPrintDialogEx* m_pPrinterSelectionDlg;
	CPrintParameters* m_pPrintParameters;
	double m_defaultMargin;
	double m_defaultPrintWidth;

	bool DoPrint(HDC hPrinterDC, CPrintParameters* pPrintParameters, CJPEGImage * pImage, const CImageProcessingParams& procParams,
		EProcessingFlags eFlags, LPCTSTR fileName);

};