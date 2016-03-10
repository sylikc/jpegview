#pragma once

#include "ProcessParams.h"

class CJPEGImage;
class CPrintParameters;

// Allows printing an image. Keep the instance of this class between multiple print jobs to retain
// the user-entered data between the print jobs.
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

	// Shows the print dialog and prints the specified image if the user confirms to print.
	// Returns if the image was successfully printed.
	bool Print(HWND hWnd, CJPEGImage * pImage, const CImageProcessingParams& procParams,
		EProcessingFlags eFlags, LPCTSTR fileName);

	// Clears the user offsets for printing, called when changing the image
	void ClearOffsets();

private:
	CPrintDialogEx* m_pPrinterSelectionDlg;
	CPrintParameters* m_pPrintParameters;
	double m_defaultMargin;
	double m_defaultPrintWidth;

	bool DoPrint(HDC hPrinterDC, CPrintParameters* pPrintParameters, CJPEGImage * pImage, const CImageProcessingParams& procParams,
		EProcessingFlags eFlags, LPCTSTR fileName);

};