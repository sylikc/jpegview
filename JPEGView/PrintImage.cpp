#include "StdAfx.h"
#include "PrintImage.h"
#include "JPEGImage.h"
#include "BasicProcessing.h"
#include "MaxImageDef.h"
#include "Helpers.h"
#include "PrintDlg.h"
#include "PrintParameters.h"

// Converts 1/10 mm to inch
static double Mm10ToInch(double value) {
	return value * 0.01 / 2.54;
}

void CPrintImage::ClearOffsets() {
	if (m_pPrintParameters != NULL) {
		m_pPrintParameters->OffsetX = m_pPrintParameters->OffsetY = 0;
	}
}

bool CPrintImage::Print(HWND hWnd, CJPEGImage * pImage, const CImageProcessingParams& procParams,
	EProcessingFlags eFlags, LPCTSTR fileName) {
	if (pImage == NULL) {
		return false;
	}

	if (m_pPrintParameters == NULL) {
		m_pPrintParameters = new CPrintParameters(m_defaultMargin, m_defaultPrintWidth);
	}

	if (m_pPrinterSelectionDlg == NULL) {
		m_pPrinterSelectionDlg = new CPrintDialogEx(PD_ALLPAGES | PD_USEDEVMODECOPIES | PD_NOPAGENUMS | PD_NOSELECTION | PD_NOCURRENTPAGE, hWnd);
		HCURSOR hOldCursor = ::SetCursor(::LoadCursor(NULL, IDC_WAIT));
		m_pPrinterSelectionDlg->GetDefaults();
		::SetCursor(hOldCursor);
		m_pPrinterSelectionDlg->m_pdex.Flags &= ~PD_RETURNDEFAULT;
	}

	CPrintDlg printDlg(m_pPrintParameters, m_pPrinterSelectionDlg, m_pPrinterSelectionDlg->GetDevMode(), m_pPrinterSelectionDlg->GetDeviceName(),
		m_pPrinterSelectionDlg->GetPortName(), pImage, procParams, eFlags);
	bool bPrinted = false;
	if (printDlg.DoModal(hWnd) == IDOK) {
		bPrinted = DoPrint(printDlg.GetPrinterDC(), m_pPrintParameters, pImage, procParams, eFlags, fileName);
	}

	if (printDlg.GetPrinterDC() != NULL) {
		::DeleteDC(printDlg.GetPrinterDC());
	}
	return bPrinted;
}

bool CPrintImage::DoPrint(HDC hPrinterDC, CPrintParameters* pPrintParameters, CJPEGImage * pImage, const CImageProcessingParams& procParams,
	EProcessingFlags eFlags, LPCTSTR fileName) {
	bool bSuccess = false;
	DOCINFO docInfo;
	memset(&docInfo, 0, sizeof(DOCINFO));
	docInfo.cbSize = sizeof(DOCINFO);
	docInfo.lpszDocName = fileName;

	if (::StartDoc(hPrinterDC, &docInfo)) {
		HCURSOR hOldCursor = ::SetCursor(::LoadCursor(NULL, IDC_WAIT));
		int dpiX = ::GetDeviceCaps(hPrinterDC, LOGPIXELSX);
		int dpiY = ::GetDeviceCaps(hPrinterDC, LOGPIXELSY);
		int pixelsX = ::GetDeviceCaps(hPrinterDC, HORZRES);
		int pixelsY = ::GetDeviceCaps(hPrinterDC, VERTRES);
		double dpiRatio = (double)dpiY / dpiX;

		int physicalWidth = ::GetDeviceCaps(hPrinterDC, PHYSICALWIDTH);
		int physicalHeight = ::GetDeviceCaps(hPrinterDC, PHYSICALHEIGHT);
		int physicalOffsetX = ::GetDeviceCaps(hPrinterDC, PHYSICALOFFSETX);
		int physicalOffsetY = ::GetDeviceCaps(hPrinterDC, PHYSICALOFFSETY);

		CRect margins(Helpers::RoundToInt(dpiX * Mm10ToInch(pPrintParameters->MarginLeft)),
			Helpers::RoundToInt(dpiY * Mm10ToInch(pPrintParameters->MarginTop)),
			Helpers::RoundToInt(dpiX * Mm10ToInch(pPrintParameters->MarginRight)),
			Helpers::RoundToInt(dpiY * Mm10ToInch(pPrintParameters->MarginBottom)));

		CRect paperArea(margins.left - physicalOffsetX, margins.top - physicalOffsetY,
			pixelsX - (margins.right - (physicalWidth - physicalOffsetX - pixelsX)),
			pixelsY - (margins.bottom - (physicalHeight - physicalOffsetY - pixelsY)));

		// clip against physical borders
		paperArea = CRect(max(0, paperArea.left), max(0, paperArea.top), min(pixelsX, paperArea.right), min(pixelsY, paperArea.bottom));

		if (::StartPage(hPrinterDC) >= 0) {
			CRect printableRect = paperArea;
			CSize paperSize(physicalWidth, physicalHeight);
			CSize imageSize;
			if (pPrintParameters->FitToPaper) {
				imageSize = Helpers::FitRectIntoRect(CSize(pImage->OrigWidth(), Helpers::RoundToInt(pImage->OrigHeight() * dpiRatio)),
					printableRect).Size();
			} else if (pPrintParameters->FillWithCrop) {
				imageSize = Helpers::FillRectAroundRect(CSize(pImage->OrigWidth(), Helpers::RoundToInt(pImage->OrigHeight() * dpiRatio)),
					printableRect).Size(); 
			} else {
				double imageAR = (double)pImage->OrigHeight() / pImage->OrigWidth();
				imageSize = CSize(Helpers::RoundToInt(dpiX * Mm10ToInch(pPrintParameters->PrintWidth)),
					Helpers::RoundToInt(dpiY * Mm10ToInch(pPrintParameters->PrintWidth) * imageAR));
			}
			
			CRect imageRect = CPrintDlg::Align(imageSize, printableRect, pPrintParameters->AlignmentX, pPrintParameters->AlignmentY);
			CPoint offsets(Helpers::RoundToInt(dpiX * Mm10ToInch(pPrintParameters->OffsetX)), Helpers::RoundToInt(dpiY * Mm10ToInch(pPrintParameters->OffsetY)));
			imageRect.OffsetRect(CPrintDlg::LimitOffset(imageRect, printableRect, offsets));

			CRect clippedRect;
			clippedRect.IntersectRect(imageRect, printableRect);
			CSize clippedImageSize = clippedRect.Size();

			CSize dibSize = pPrintParameters->ScaleByPrinterDriver ? pImage->OrigSize() : clippedImageSize;
			CSize fullTargetSize = pPrintParameters->ScaleByPrinterDriver ? dibSize : imageSize;
			CPoint offset = pPrintParameters->ScaleByPrinterDriver ? CPoint(0, 0) : CPoint(max(0, printableRect.left - imageRect.left), max(0, printableRect.top - imageRect.top));

			pImage->EnableDimming(false);
			void* pDIBData = pImage->GetDIB(fullTargetSize, dibSize, offset, procParams, eFlags);
			pImage->EnableDimming(true);

			if (pDIBData != NULL) {
				BITMAPINFO bmInfo;
				memset(&bmInfo, 0, sizeof(BITMAPINFO));
				bmInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
				bmInfo.bmiHeader.biWidth = dibSize.cx;
				bmInfo.bmiHeader.biHeight = -dibSize.cy;
				bmInfo.bmiHeader.biPlanes = 1;
				bmInfo.bmiHeader.biBitCount = 32;
				bmInfo.bmiHeader.biCompression = BI_RGB;
				::IntersectClipRect(hPrinterDC, printableRect.left, printableRect.top, printableRect.right, printableRect.bottom);
				if (pPrintParameters->ScaleByPrinterDriver) {
					::StretchDIBits(hPrinterDC, imageRect.left, imageRect.top, imageRect.Width(), imageRect.Height(),
						0, 0, pImage->OrigWidth(), pImage->OrigHeight(), pDIBData, &bmInfo, DIB_RGB_COLORS, SRCCOPY);
				} else {
					::SetDIBitsToDevice(hPrinterDC, clippedRect.left, clippedRect.top, clippedRect.Width(), clippedRect.Height(), 0, 0, 0,
						clippedRect.Height(), pDIBData, &bmInfo, DIB_RGB_COLORS);
				}
				bSuccess = true;
			}

			::EndPage(hPrinterDC);
		}

		::EndDoc(hPrinterDC);

		::SetCursor(hOldCursor);
	}
	return bSuccess;
}