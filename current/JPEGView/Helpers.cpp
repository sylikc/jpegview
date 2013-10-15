#include "StdAfx.h"
#include "Helpers.h"
#include "NLS.h"
#include "MultiMonitorSupport.h"
#include "JPEGImage.h"
#include "FileList.h"
#include "SettingsProvider.h"
#include <math.h>

namespace Helpers {

float ScreenScaling = -1.0f;

TCHAR CReplacePipe::sm_buffer[MAX_SIZE_REPLACE_PIPE];

CReplacePipe::CReplacePipe(LPCTSTR sText) {
	_tcsncpy_s(sm_buffer, MAX_SIZE_REPLACE_PIPE, sText, MAX_SIZE_REPLACE_PIPE);
	sm_buffer[MAX_SIZE_REPLACE_PIPE-1] = 0;
	TCHAR* pPtr = sm_buffer;
	while (*pPtr != 0) {
		if (*pPtr == _T('|')) *pPtr = 0;
		pPtr++;
	}
}

static TCHAR buffAppPath[MAX_PATH + 32] = _T("");

LPCTSTR JPEGViewAppDataPath() {
	if (buffAppPath[0] == 0) {
		::SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, buffAppPath);
		_tcscat_s(buffAppPath, MAX_PATH + 32, _T("\\JPEGView\\"));
	}
	return buffAppPath;
}

void SetJPEGViewAppDataPath(LPCTSTR sPath) {
	_tcscpy_s(buffAppPath, MAX_PATH + 32, sPath);
}

CSize GetImageRect(int nWidth, int nHeight, int nScreenWidth, int nScreenHeight, 
					bool bAllowZoomIn, bool bFillCrop, bool bLimitAR, double & dZoom) {
	double dAR1 = (double)nWidth/nScreenWidth;
	double dAR2 = (double)nHeight/nScreenHeight;
	double dARMin = min(dAR1, dAR2);
	double dARMax = max(dAR1, dAR2);
	double dAR = bFillCrop ? dARMin : dARMax;
	if (dAR <= 1.0 && !bAllowZoomIn) {
		dZoom = 1.0;
		return CSize(nWidth, nHeight);
	}
	if (bFillCrop && bLimitAR) {
		if (((nWidth/dAR * nHeight/dAR) / (nScreenWidth * nScreenHeight)) > 1.5) {
			dAR = dARMax; // use fit to screen
		}
	}
	dZoom = 1.0/dAR;
	dZoom = min(ZoomMax, dZoom);
	CSize size((int)(nWidth/dAR + 0.5), (int)(nHeight/dAR + 0.5));
	if (abs(size.cx - nScreenWidth) <= 1 && abs(size.cy - nScreenHeight) <= 1) {
		// allow 1 pixel tolerance to screen size - prefer screen size in this case to prevent another resize operation to fit image
		dZoom = (nScreenWidth > nScreenHeight) ? (double)nScreenWidth/nWidth : (double)nScreenHeight/nHeight;
		dZoom = min(ZoomMax, dZoom);
		return CSize(nScreenWidth, nScreenHeight);
	}
	return size;
}

CString SystemTimeToString(const SYSTEMTIME &time) {
	TCHAR sBufferDate[64];
	TCHAR sBufferDay[8];
	TCHAR sBufferTime[64];
	::GetDateFormat(LOCALE_USER_DEFAULT, 0, &time, _T("ddd"), sBufferDay, 8);
	::GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &time, NULL, sBufferDate, 64);
	::GetTimeFormat(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &time, NULL, sBufferTime, 64);
	return CString(sBufferDay) + _T(" ") + sBufferDate + _T(" ") + sBufferTime;
}

CSize GetImageRect(int nWidth, int nHeight, int nScreenWidth, int nScreenHeight, EAutoZoomMode eAutoZoomMode, double & dZoom) {
	CSize newSize = GetImageRect(nWidth, nHeight, nScreenWidth, nScreenHeight,
		eAutoZoomMode == ZM_FitToScreen || eAutoZoomMode == ZM_FillScreen,
		eAutoZoomMode == ZM_FillScreenNoZoom || eAutoZoomMode == ZM_FillScreen,
		eAutoZoomMode == ZM_FillScreenNoZoom, dZoom);
	return CSize((int)(nWidth*dZoom + 0.5), (int)(nHeight*dZoom + 0.5));
}

CSize GetVirtualImageSize(CSize originalImageSize, CSize screenSize, EAutoZoomMode eAutoZoomMode, double & dZoom) {
	CSize newSize;
	if (dZoom < 0.0) {
		// zoom not set, interpret as 'fit to screen'
		// ---------------------------------------------
		newSize = Helpers::GetImageRect(originalImageSize.cx, originalImageSize.cy, screenSize.cx, screenSize.cy, eAutoZoomMode, dZoom);
	} else {
		// zoom set, use this value for the new size
		// ---------------------------------------------
		newSize = CSize((int)(originalImageSize.cx * dZoom + 0.5), (int)(originalImageSize.cy * dZoom + 0.5));
	}
	newSize.cx = max(1, min(65535, newSize.cx));
	newSize.cy = max(1, min(65535, newSize.cy));
	return newSize;
}

CPoint LimitOffsets(const CPoint& offsets, const CSize & rectSize, const CSize & outerRect) {
	int nMaxOffsetX = (outerRect.cx - rectSize.cx)/2;
	nMaxOffsetX = max(0, nMaxOffsetX);
	int nMaxOffsetY = (outerRect.cy - rectSize.cy)/2;
	nMaxOffsetY = max(0, nMaxOffsetY);
	return CPoint(max(-nMaxOffsetX, min(+nMaxOffsetX, offsets.x)), max(-nMaxOffsetY, min(+nMaxOffsetY, offsets.y)));
}

CRect InflateRect(const CRect& rect, float fAmount) {
	CRect r(rect);
	int nAmount = (int)(fAmount * r.Width());
	r.InflateRect(-nAmount, -nAmount);
	return ((r.Height() & 1) == 1) ? CRect(r.left, r.top, r.right, r.bottom - 1) : r;
}

void GetZoomParameters(float & fZoom, CPoint & offsets, CSize imageSize, CSize windowSize, CRect zoomRect) {
	int nZoomWidth = zoomRect.Width();
	int nZoomHeight = zoomRect.Height();

	bool bTakeW = ((float)windowSize.cx/windowSize.cy < (float)nZoomWidth/nZoomHeight);
	if (bTakeW) {
		// Width is dominating
		fZoom = (float)windowSize.cx/nZoomWidth;
	} else {
		// Height is dominating
		fZoom = (float)windowSize.cy/nZoomHeight;
	}
	if (fZoom < Helpers::ZoomMin || fZoom > Helpers::ZoomMax) {
		fZoom = -1.0f;
		return;
	}

	int nZoomRectMiddleX = (zoomRect.right + zoomRect.left)/2;
	int nOffsetX = Helpers::RoundToInt(fZoom*imageSize.cx*0.5 - fZoom*nZoomRectMiddleX);

	int nZoomRectMiddleY = (zoomRect.bottom + zoomRect.top)/2;
	int nOffsetY = Helpers::RoundToInt(fZoom*imageSize.cy*0.5 - fZoom*nZoomRectMiddleY);

	offsets = CPoint(nOffsetX, nOffsetY);
}

CPUType ProbeCPU(void) {
	static CPUType cpuType = CPU_Unknown;
	if (cpuType != CPU_Unknown) {
		return cpuType;
	}

	// Structured exception handling is mantatory, try/catch(...) does not catch such severe stuff.
	cpuType = CPU_Generic;
	__try {
		uint32 FeatureMask;
		_asm {
			mov eax, 1
			cpuid
			mov FeatureMask, edx
		}
		if ((FeatureMask & (1 << 26)) != 0) {
			cpuType = CPU_SSE; // this means SSE2
		} else if ((FeatureMask & (1 << 25)) != 0) {
			cpuType = CPU_MMX; // yes, we need SSE as the pmax/pmin stuff was coming with the PIII and SSE
		} else {
			// last chance - check if AMD and if yes, test for AMD MMX extensions that also implement pmax/pmin
			_asm {
				mov eax, 0
				cpuid
				cmp ebx, 0x68747541 // is AMD processor?
				jne GiveUp
				mov eax, 0x80000001
				cpuid
				mov FeatureMask, edx
GiveUp:
				mov FeatureMask, 0
			}
			if ((FeatureMask & (1 << 22)) != 0) {
				cpuType = CPU_MMX; // extended AMD MMX instructions
			}
		}
		
	} __except ( EXCEPTION_EXECUTE_HANDLER ) {
		// even CPUID is not supported, use generic code
		return cpuType;
	}
	return cpuType;
}

// returns if the CPU supports some form of hardware multiprocessing, e.g. hyperthreading or multicore
static bool CPUSupportsHWMultiprocessing(void) {   
	unsigned int Regedx      = 0;

	if (ProbeCPU() == CPU_SSE) {
		_asm {
			mov eax, 1
			cpuid
			mov Regedx, edx
		}
		return (Regedx & 0x10000000);  
	} else {
		return false;
	}  
}

int NumCoresPerPhysicalProc(void) {
	unsigned int Regeax = 0;
	
	if (!CPUSupportsHWMultiprocessing()) {
		return 1;
	}
	_asm {
		xor eax, eax
		cpuid
		cmp eax, 4			// check if cpuid supports leaf 4
		jl single_core		// Single core
		mov eax, 4			
		mov ecx, 0			// start with index = 0; Leaf 4 reports
		cpuid				// at least one valid cache level
		mov Regeax, eax
		jmp multi_core
single_core:
		xor eax, eax		
multi_core:
	}
	return (int)((Regeax & 0xFC000000) >> 26) + 1;
}

bool PatternMatch(LPCTSTR & sMatchingPattern, LPCTSTR sString, LPCTSTR sPattern) {
	sMatchingPattern = NULL;
	if (sString == NULL || sPattern == NULL || *sPattern == 0) return false;
	LPCTSTR pInStr = sString;
	LPCTSTR pInPat = sPattern;
	while (*pInPat != 0) {
		bool bThisPatternFails = false;
		while (*pInPat == _T(';')) pInPat++; // skip sequences of separators
		sMatchingPattern = pInPat;
		while (*pInPat != 0 && *pInPat != _T(';')) {
			bool bAsterix = false;
			if (*pInPat == _T('*')) {
				pInPat++;
				bAsterix = true;
			}
			LPCTSTR pPatStart = pInPat;
			while (*pPatStart != 0 && *pPatStart != _T('*') && *pPatStart != _T(';')) pPatStart++;
			int nPatLen = pPatStart - pInPat;
			if (nPatLen > 0) {
				if (bAsterix) {
					while (*pInStr != 0 && _tcsnicmp(pInPat, pInStr, nPatLen) != 0) pInStr++;
					if (*pInStr == 0) {
						bThisPatternFails = true;
						while (*pInPat != 0 && *pInPat != _T(';')) pInPat++;
					} else {
						pInStr += nPatLen;
						pInPat += nPatLen;
					}
				} else {
					if (_tcsnicmp(pInPat, pInStr, nPatLen) != 0) {
						bThisPatternFails = true;
						while (*pInPat != 0 && *pInPat != _T(';')) pInPat++;
					} else {
						pInStr += nPatLen;
						pInPat += nPatLen;
					}
				}
			}
		}
		if (!bThisPatternFails) return true;
		pInStr = sString;
		if (*pInPat != 0) pInPat++;
	}
	return false;
}

int FindMoreSpecificPattern(LPCTSTR sPattern1, LPCTSTR sPattern2) {
	if (sPattern1 == NULL) {
		return (sPattern2 == NULL) ? 0 : -1;
	} else if (sPattern2 == NULL) {
		return 1;
	}
	while (*sPattern1 != 0 && *sPattern1 != _T(';') && *sPattern2 != 0 && *sPattern2 != _T(';') &&
		_totlower(*sPattern1) == _totlower(*sPattern2)) {
		sPattern1++;
		sPattern2++;
	}
	if (*sPattern1 == 0 || *sPattern1 == _T(';')) {
		if (*sPattern2 == 0 || *sPattern2 == _T(';')) {
			return 0; // no one is more specific
		} else {
			return -1; // sPattern2 is more specific
		}
	} else if (*sPattern2 == 0 || *sPattern2 == _T(';')) {
		return 1; // sPattern1 is more specific
	} else {
		if (*sPattern1 == _T('*')) {
			return -1; // pattern 1 accepts any char, thus sPattern2 is more specific
		} else if (*sPattern2 == _T('*')) {
			return 1; // sPattern1 is more specific
		}
		return 0;
	}
}

// calculate CRT table
void CalcCRCTable(unsigned int crc_table[256]) {
     for (int n = 0; n < 256; n++) {
       unsigned int c = (unsigned int) n;
       for (int k = 0; k < 8; k++) {
         if (c & 1)
           c = 0xedb88320L ^ (c >> 1);
         else
           c = c >> 1;
       }
       crc_table[n] = c;
     }
}

void* FindJPEGMarker(void* pJPEGStream, int nStreamLength, unsigned char nMarker) {
	uint8* pStream = (uint8*) pJPEGStream;
	if (pStream == NULL || nStreamLength < 3 || pStream[0] != 0xFF || pStream[1] != 0xD8) {
		return NULL; // not a JPEG
	}
	int nIndex = 2;
	do {
		if (pStream[nIndex] == 0xFF) {
			// block header found, skip padding bytes
			while (pStream[nIndex] == 0xFF && nIndex < nStreamLength) nIndex++;
			if (pStream[nIndex] == 0 || pStream[nIndex] == nMarker) {
				break; // 0xFF 0x00 is part of pixel block, break
			} else {
				// it's a block marker, read length of block and skip the block
				nIndex++;
				if (nIndex+1 < nStreamLength) {
					nIndex += pStream[nIndex]*256 + pStream[nIndex+1];
				} else {
					nIndex = nStreamLength;
				}
			}
		} else {
			break; // block with pixel data found, start hashing from here
		}
	} while (nIndex < nStreamLength);

	if (nMarker == 0 || (pStream[nIndex] == nMarker && pStream[nIndex-1] == 0xFF)) {
		return &(pStream[nIndex-1]); // place on marker start
	} else {
		return NULL;
	}
}

void* FindEXIFBlock(void* pJPEGStream, int nStreamLength) {
	uint8* pEXIFBlock = (uint8*)Helpers::FindJPEGMarker(pJPEGStream, nStreamLength, 0xE1);
	if (pEXIFBlock != NULL && strncmp((const char*)(pEXIFBlock + 4), "Exif", 4) != 0) {
		return NULL;
	}
	return pEXIFBlock;
}

__int64 CalculateJPEGFileHash(void* pJPEGStream, int nStreamLength) {
	uint8* pStream = (uint8*) pJPEGStream;
	void* pPixelStart = FindJPEGMarker(pJPEGStream, nStreamLength, 0);
	if (pPixelStart == NULL) {
		return 0;
	}
	int nIndex = (uint8*)pPixelStart - (uint8*)pJPEGStream + 1;
	
	// take whole stream in case of inconsistency or if remaining part is too small
	if (nStreamLength - nIndex < 4) {
		nIndex = 0;
		assert(false);
	}

	// now we can calculate the hash over the compressed pixel data
	// do not look at every byte due to performance reasons
	const int nTotalLookups = 10000;
	int nIncrement = (nStreamLength - nIndex)/nTotalLookups;
	nIncrement = max(1, nIncrement);

	unsigned int crc_table[256];
	CalcCRCTable(crc_table);
	uint32 crcValue = 0xffffffff;
	unsigned int sumValue = 0;
	while (nIndex < nStreamLength) {
		sumValue += pStream[nIndex];
		crcValue = crc_table[(crcValue ^ pStream[nIndex]) & 0xff] ^ (crcValue >> 8);
		nIndex += nIncrement;
	}

	return ((__int64)crcValue << 32) + sumValue;
}

CString TryConvertFromUTF8(uint8* pComment, int nLengthInBytes) {
    wchar_t* pCommentUnicode = new wchar_t[nLengthInBytes + 1];
    char* pCommentBack = new char[nLengthInBytes + 1];
    CString result;
    int nCharsConverted = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (LPCSTR)pComment, nLengthInBytes, pCommentUnicode, nLengthInBytes);
    if (nCharsConverted > 0) {
        pCommentUnicode[nCharsConverted] = 0;
        if (::WideCharToMultiByte(CP_UTF8, 0, pCommentUnicode, -1, pCommentBack, nLengthInBytes + 1, NULL , NULL) > 0) {
            if (memcmp(pComment, pCommentBack, nLengthInBytes) == 0) {
                result = CString((LPCWSTR)pCommentUnicode);
            }
        }
    }
    delete[] pCommentUnicode;
    delete[] pCommentBack;
    return result;
}

CString GetJPEGComment(void* pJPEGStream, int nStreamLength) {
	uint8* pCommentSeg = (uint8*)FindJPEGMarker(pJPEGStream, nStreamLength, 0xFE);
	if (pCommentSeg == NULL) {
		return CString("");
	}
	pCommentSeg += 2;
	int nCommentLen = pCommentSeg[0]*256 + pCommentSeg[1] - 2;
	if (nCommentLen <= 0) {
		return CString("");
	}

	uint8* pComment = &(pCommentSeg[2]);
	if (nCommentLen > 2 && (nCommentLen & 1) == 0) {
		if (pComment[0] == 0xFF && pComment[1] == 0xFE) {
			// UTF16 little endian
			return CString((LPCWSTR)&(pComment[2]), (nCommentLen - 2) / 2);
		} else if (pComment[0] == 0xFE && pComment[1] == 0xFF) {
			// UTF16 big endian -> cannot read
			return CString("");
		}
	}

    CString sConvertedFromUTF8 = TryConvertFromUTF8(pComment, nCommentLen);
    if (!sConvertedFromUTF8.IsEmpty()) {
		return (sConvertedFromUTF8.Find(_T("Intel(R) JPEG Library")) != -1) ? CString("") : sConvertedFromUTF8;
    }

	// check if this is a reasonable string - it must contain enough characters between a and z and A and Z
	if (nCommentLen < 7) {
		return CString(""); // cannot check for such short strings, do not use
	}
	int nGoodChars = 0;
	for (int i = 0; i < nCommentLen; i++) {
		uint8 ch = pComment[i];
		if (ch >= 'a' && ch <= 'z' ||  ch >= 'A' && ch <= 'Z' || ch == ' ' || ch == ',' || ch == '.' || ch >= '0' && ch <= '9') {
			nGoodChars++;
		}
	}
	// The Intel lib puts this useless comment into each JPEG it writes - filter this out as nobody is interested in that...
	if (nCommentLen > 20 && strstr((char*)pComment, "Intel(R) JPEG Library") != NULL) {
		return CString("");
	}

	return (nGoodChars > nCommentLen * 8 / 10) ? CString((LPCSTR)pComment, nCommentLen) : CString("");
}

void ClearJPEGComment(void* pJPEGStream, int nStreamLength) {
	uint8* pCommentSeg = (uint8*)FindJPEGMarker(pJPEGStream, nStreamLength, 0xFE);
	if (pCommentSeg == NULL) {
		return;
	}
	pCommentSeg += 2;
	int nCommentLen = pCommentSeg[0]*256 + pCommentSeg[1] - 2;
	if (nCommentLen <= 0) {
		return;
	}
	pCommentSeg += 2;
	if ((uint8*)pJPEGStream + nStreamLength > pCommentSeg + nCommentLen) {
		memset(pCommentSeg, 0, nCommentLen);
	}
}

double GetExactTickCount() {
	static __int64 nFrequency = -1;
	if (nFrequency == -1) {
		if (!::QueryPerformanceFrequency((LARGE_INTEGER*)&nFrequency)) {
			nFrequency = 0;
		}
	}
	if (nFrequency == 0) {
		return ::GetTickCount();
	} else {
		__int64 nCounter;
		::QueryPerformanceCounter((LARGE_INTEGER*)&nCounter);
		return (1000*(double)nCounter)/nFrequency;
	}
}

CRect GetWindowRectMatchingImageSize(HWND hWnd, CSize minSize, CSize maxSize, double& dZoom, CJPEGImage* pImage, bool bForceCenterWindow, bool bKeepAspectRatio) {
	int nOrigWidth = (pImage == NULL) ? ::GetSystemMetrics(SM_CXSCREEN) / 2 : pImage->OrigWidth();
	int nOrigWidthUnzoomed = nOrigWidth;
	int nOrigHeight = (pImage == NULL) ? ::GetSystemMetrics(SM_CYSCREEN) / 2 : pImage->OrigHeight();
	if (dZoom > 0) {
		nOrigWidth = (int) (nOrigWidth * dZoom + 0.5);
		nOrigHeight = (int) (nOrigHeight * dZoom + 0.5);
	}
	int nBorderWidth = ::GetSystemMetrics(SM_CXSIZEFRAME) * 2;
	int nBorderHeight = ::GetSystemMetrics(SM_CYSIZEFRAME) * 2 + ::GetSystemMetrics(SM_CYCAPTION);
	int nRequiredWidth = nBorderWidth + nOrigWidth;
	int nRequiredHeight = nBorderHeight + nOrigHeight;
	CRect workingArea = CMultiMonitorSupport::GetWorkingRect(hWnd);
	if (bKeepAspectRatio && (nRequiredWidth > workingArea.Width() || nRequiredHeight > workingArea.Height())) {
		double dZoom;
		CSize imageRect = Helpers::GetImageRect(nOrigWidth, nOrigHeight, 
			min(maxSize.cx, workingArea.Width() - nBorderWidth), 
			min(maxSize.cy, workingArea.Height() - nBorderHeight), 
			false, false, false, dZoom);
		nRequiredWidth = imageRect.cx + nBorderWidth;
		nRequiredHeight = imageRect.cy + nBorderHeight;
	} else {
		nRequiredWidth = min(maxSize.cx, min(workingArea.Width(), nRequiredWidth));
		nRequiredHeight = min(maxSize.cy, min(workingArea.Height(), nRequiredHeight));
	}
	nRequiredWidth = max(minSize.cx, nRequiredWidth);
	nRequiredHeight = max(minSize.cy, nRequiredHeight);
	dZoom = ((double)(nRequiredWidth - nBorderWidth))/nOrigWidthUnzoomed;
	CRect wndRect;
	::GetWindowRect(hWnd, &wndRect);
	int nNewLeft = wndRect.CenterPoint().x - nRequiredWidth / 2;
	int nNewTop = wndRect.CenterPoint().y - nRequiredHeight / 2;
	nNewLeft = max(workingArea.left, min(workingArea.right - nRequiredWidth, nNewLeft));
	nNewTop = max(workingArea.top, min(workingArea.bottom - nRequiredHeight, nNewTop));
	CRect windowRect;
	if (!bForceCenterWindow) {
		return CRect(CPoint(nNewLeft, nNewTop), CSize(nRequiredWidth, nRequiredHeight));
	} else {
		return CRect(CPoint(workingArea.left + (workingArea.Width() - nRequiredWidth) / 2, workingArea.top + (workingArea.Height() - nRequiredHeight) / 2), CSize(nRequiredWidth, nRequiredHeight));
	}
}

bool CanDisplayImageWithoutResize(HWND hWnd, CJPEGImage* pImage) {
	if (pImage == NULL) {
		return true;
	}
	CRect workingArea = CMultiMonitorSupport::GetWorkingRect(hWnd);
	int nBorderWidth = ::GetSystemMetrics(SM_CXSIZEFRAME) * 2;
	int nBorderHeight = ::GetSystemMetrics(SM_CYSIZEFRAME) * 2 + ::GetSystemMetrics(SM_CYCAPTION);
	return pImage->OrigWidth() + nBorderWidth <= workingArea.Width() && pImage->OrigHeight() + nBorderHeight <= workingArea.Height();
}

CRect CalculateMaxIncludedRectKeepAR(const CTrapezoid& trapezoid, double dAspectRatio) {
	int w1 = trapezoid.x1e - trapezoid.x1s;
	int w2 = trapezoid.x2e - trapezoid.x2s;
	if ((trapezoid.x1s >= trapezoid.x2s && trapezoid.x1e <= trapezoid.x2e) || (trapezoid.x1s <= trapezoid.x2s && trapezoid.x1e >= trapezoid.x2e)) {
		int h = trapezoid.y2 - trapezoid.y1;
		double dTerm = (double)(w2 - w1)/h;
		bool bPyramid = w1 < w2;
		double dY =  bPyramid ? (dAspectRatio * h - w1)/(dAspectRatio + dTerm) : w1/(dAspectRatio - dTerm);
		double dAlpha = dY/h;
		double dX = (trapezoid.x2s - trapezoid.x1s)*dAlpha;
		dX += trapezoid.x1s;
		dY += trapezoid.y1;
		return CRect((int)(dX + 0.5), bPyramid ? (int)(dY + 0.5) : trapezoid.y1, 
			(int)(trapezoid.x1e + (trapezoid.x2e - trapezoid.x1e)*dAlpha + 0.5), bPyramid ? trapezoid.y2 : (int)(dY + 0.5));
	}
	if (trapezoid.x1s < trapezoid.x2s) {
		if (w1 < w2) 
			return CalculateMaxIncludedRectKeepAR(CTrapezoid(trapezoid.x2s, trapezoid.x1e, trapezoid.y1, trapezoid.x2s, trapezoid.x2e, trapezoid.y2), dAspectRatio);
		else
			return CalculateMaxIncludedRectKeepAR(CTrapezoid(trapezoid.x1s, trapezoid.x1e, trapezoid.y1, trapezoid.x2s, trapezoid.x1e, trapezoid.y2), dAspectRatio);
	} else {
		if (w1 < w2) 
			return CalculateMaxIncludedRectKeepAR(CTrapezoid(trapezoid.x1s, trapezoid.x2e, trapezoid.y1, trapezoid.x2s, trapezoid.x2e, trapezoid.y2), dAspectRatio);
		else
			return CalculateMaxIncludedRectKeepAR(CTrapezoid(trapezoid.x1s, trapezoid.x1e, trapezoid.y1, trapezoid.x1s, trapezoid.x2e, trapezoid.y2), dAspectRatio);
	}
}

CSize GetMaxClientSize(HWND hWnd) {
	CRect workingArea = CMultiMonitorSupport::GetWorkingRect(hWnd);
	return CSize(workingArea.Width() - 2 * ::GetSystemMetrics(SM_CXSIZEFRAME), 
		workingArea.Height() - 2 * ::GetSystemMetrics(SM_CYSIZEFRAME) - ::GetSystemMetrics(SM_CYCAPTION));
}

ETransitionEffect ConvertTransitionEffectFromString(LPCTSTR str) {
    if (_tcsicmp(str, _T("Blend")) == 0) {
        return TE_Blend;
    } else if (_tcsicmp(str, _T("SlideRL")) == 0) {
        return TE_SlideRL;
    } else if (_tcsicmp(str, _T("SlideLR")) == 0) {
        return TE_SlideLR;
    } else if (_tcsicmp(str, _T("SlideTB")) == 0) {
        return TE_SlideTB;
    } else if (_tcsicmp(str, _T("SlideBT")) == 0) {
        return TE_SlideBT;
    } else if (_tcsicmp(str, _T("RollRL")) == 0) {
        return TE_RollRL;
    } else if (_tcsicmp(str, _T("RollLR")) == 0) {
        return TE_RollLR;
    } else if (_tcsicmp(str, _T("RollTB")) == 0) {
        return TE_RollTB;
    } else if (_tcsicmp(str, _T("RollBT")) == 0) {
        return TE_RollBT;
    } else if (_tcsicmp(str, _T("ScrollRL")) == 0) {
        return TE_ScrollRL;
    } else if (_tcsicmp(str, _T("ScrollLR")) == 0) {
        return TE_ScrollLR;
    } else if (_tcsicmp(str, _T("ScrollTB")) == 0) {
        return TE_ScrollTB;
    } else if (_tcsicmp(str, _T("ScrollBT")) == 0) {
        return TE_ScrollBT;
    }
    return TE_None;
}

static bool IsInFileEndingList(LPCTSTR sFileEndings, LPCTSTR sEnding) {
	const int BUFFER_SIZE = 256;
    TCHAR buffer[BUFFER_SIZE];
    _tcsncpy_s(buffer, BUFFER_SIZE, sFileEndings, _TRUNCATE);
    LPTSTR sStart = buffer, sCurrent = buffer;
    while (*sCurrent != 0) {
        while (*sCurrent != 0 && *sCurrent != _T(';')) {
            sCurrent++;
        }
        if (*sCurrent == _T(';')) {
            *sCurrent = 0;
            sCurrent++;
        }
        if (_tcsicmp(sStart + 1, sEnding - 1) == 0) {
            return true;
        }
        sStart = sCurrent;
    }
	return false;
}

EImageFormat GetImageFormat(LPCTSTR sFileName) {
	LPCTSTR sEnding = _tcsrchr(sFileName, _T('.'));
	if (sEnding != NULL) {
		sEnding += 1;
		if (_tcsicmp(sEnding, _T("JPG")) == 0 || _tcsicmp(sEnding, _T("JPEG")) == 0) {
			return IF_JPEG;
		} else if (_tcsicmp(sEnding, _T("BMP")) == 0) {
			return IF_WindowsBMP;
		} else if (_tcsicmp(sEnding, _T("PNG")) == 0) {
			return IF_PNG;
		} else if (_tcsicmp(sEnding, _T("TIF")) == 0 || _tcsicmp(sEnding, _T("TIFF")) == 0) {
			return IF_TIFF;
		} else if (_tcsicmp(sEnding, _T("WEBP")) == 0) {
			return IF_WEBP;
		} else if (IsInFileEndingList(CSettingsProvider::This().FilesProcessedByWIC(), sEnding)) {
			return IF_WIC;
		} else if (IsInFileEndingList(CSettingsProvider::This().FileEndingsRAW(), sEnding)) {
			return IF_CameraRAW;
		}
	}
	return IF_Unknown;
}

// Returns the short form of the path (including the file name)
CString GetShortFilePath(LPCTSTR sPath) {
	TCHAR shortPath[MAX_PATH];
	if (::GetShortPathName(sPath, (LPTSTR)(&shortPath), MAX_PATH) != 0) {
		return CString(shortPath);
	} else {
		int nError = ::GetLastError();
		return CString(sPath);
	}
}

// Returns the short form of the path but the file name remains untouched
CString ReplacePathByShortForm(LPCTSTR sPath) {
	LPCTSTR sLast = _tcsrchr(sPath, _T('\\'));
	if (sLast != NULL) {
		CString sShortPath = GetShortFilePath(CString(sPath).Left(sLast - sPath));
		return sShortPath + sLast;
	}
	return CString(sPath);
}

// strstr() ignoring case
LPTSTR stristr(LPCTSTR szStringToBeSearched, LPCTSTR szSubstringToSearchFor) {
    LPCTSTR pPos = NULL;
    LPCTSTR szCopy1 = NULL;
    LPCTSTR szCopy2 = NULL;
     
    // verify parameters
    if ( szStringToBeSearched == NULL || szSubstringToSearchFor == NULL ) {
        return (LPTSTR)szStringToBeSearched;
    }
     
    // empty substring - return input (consistent with strstr)
    if ( _tcslen(szSubstringToSearchFor) == 0 ) {
        return (LPTSTR)szStringToBeSearched;
    }
     
    szCopy1 = _tcslwr(_tcsdup(szStringToBeSearched));
    szCopy2 = _tcslwr(_tcsdup(szSubstringToSearchFor));
     
    if ( szCopy1 == NULL || szCopy2 == NULL ) {
        // another option is to raise an exception here
        free((void*)szCopy1);
        free((void*)szCopy2);
        return NULL;
    }
     
    pPos = _tcsstr(szCopy1, szCopy2);
     
    if ( pPos != NULL ) {
        // map to the original string
        pPos = szStringToBeSearched + (pPos - szCopy1);
    }
     
    free((void*)szCopy1);
    free((void*)szCopy2);
     
    return (LPTSTR)pPos;
 } // stristr(...)

__int64 GetFileSize(LPCTSTR sPath) {
    HANDLE hFile = ::CreateFile(sPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		return 0;
	}
    __int64 fileSize = 0;
    ::GetFileSizeEx(hFile, (PLARGE_INTEGER)&fileSize);
    ::CloseHandle(hFile);
    return fileSize;
}

// Gets the frame index of the next frame, depending on the index of the last image (relevant if the image is a multiframe image)
int GetFrameIndex(CJPEGImage* pImage, bool bNext, bool bPlayAnimation, bool & switchImage) {
    bool isMultiFrame = pImage != NULL && pImage->NumberOfFrames() > 1;
    bool isAnimation = pImage != NULL && pImage->IsAnimation();
    int nFrameIndex = 0;
    switchImage = true;
    if (isMultiFrame) {
        switchImage = false;
        nFrameIndex = pImage->FrameIndex() + (bNext ? 1 : -1);
        if (isAnimation) {
            if (bPlayAnimation) {
                if (nFrameIndex < 0) {
                    nFrameIndex = pImage->NumberOfFrames() - 1;
                } else if (nFrameIndex > pImage->NumberOfFrames() - 1) {
                    nFrameIndex = 0;
                }
            } else {
                switchImage = true;
                nFrameIndex = 0;
            }
        } else {
            if (nFrameIndex < 0 || nFrameIndex >= pImage->NumberOfFrames()) {
                nFrameIndex = 0;
                switchImage = true;
            }
        }
    }
    if (bPlayAnimation && pImage == NULL) {
        switchImage = false; // never switch image when error during animation playing
    }

    return nFrameIndex;
}

// Gets an index string of the form [a/b] for multiframe images, empty string for single frame images
CString GetMultiframeIndex(CJPEGImage* pImage) {
    bool isMultiFrame = pImage != NULL && pImage->NumberOfFrames() > 1;
    if (isMultiFrame && !pImage->IsAnimation()) {
        CString s;
        s.Format(_T(" [%d/%d]"), pImage->FrameIndex() + 1, pImage->NumberOfFrames());
        return s;
    }
    return CString(_T(""));
}

CString GetFileInfoString(LPCTSTR sFormat, CJPEGImage* pImage, CFileList* pFilelist, double dZoom) {
    if (pImage == NULL) {
        return CString(_T(""));
    }
    bool isClipboardImage = pImage->IsClipboardImage();
    if (_tcscmp(sFormat, _T("<i>  <p>")) == 0) {
        if (isClipboardImage) return CString(_T("Clipboard Image"));
        CString sFileInfo;
        sFileInfo.Format(_T("[%d/%d]  %s"), pFilelist->CurrentIndex() + 1, pFilelist->Size(), pFilelist->Current() + GetMultiframeIndex(pImage));
        return sFileInfo;
    }
    
    CString sFileInfo(sFormat);
    if (_tcsstr(sFormat, _T("<f>")) != NULL) {
        CString sFileName = isClipboardImage ? _T("Clipboard Image") : pFilelist->CurrentFileTitle() + GetMultiframeIndex(pImage);
        sFileInfo.Replace(_T("<f>"), sFileName);
    }
    if (_tcsstr(sFormat, _T("<p>")) != NULL) {
        CString sFilePath = isClipboardImage ? _T("Clipboard Image") : pFilelist->Current() + GetMultiframeIndex(pImage);
        sFileInfo.Replace(_T("<p>"), sFilePath);
    }
    if (_tcsstr(sFormat, _T("<i>")) != NULL) {
        CString sIndex;
        if (!isClipboardImage) sIndex.Format(_T("[%d/%d]"), pFilelist->CurrentIndex() + 1, pFilelist->Size());
        sFileInfo.Replace(_T("<i>"), sIndex);
    }
    if (_tcsstr(sFormat, _T("<z>")) != NULL) {
        TCHAR buff[32];
		_stprintf_s(buff, 32, _T("%d %%"), int(dZoom*100 + 0.5));
        sFileInfo.Replace(_T("<z>"), buff);
    }
    if (_tcsstr(sFormat, _T("<s>")) != NULL) {
        TCHAR buff[48];
		_stprintf_s(buff, 48, _T("%d x %d"), pImage->OrigWidth(), pImage->OrigHeight());
        sFileInfo.Replace(_T("<s>"), buff);
    }
    if (_tcsstr(sFormat, _T("<l>")) != NULL) {
        __int64 fileSize = isClipboardImage ? 0 : GetFileSize(pFilelist->Current());
        CString sFileSize;
        if (fileSize >= 1024 * 1024 * 100) {
            sFileSize.Format(_T("%d MB"), (int)(fileSize >> 20));
        } else if (fileSize >= 1024) {
            sFileSize.Format(_T("%d KB"), (int)(fileSize >> 10));
        } else if (fileSize > 0) {
            sFileSize.Format(_T("%d b"), (int)fileSize);
        }
        sFileInfo.Replace(_T("<l>"), sFileSize);
    }
    sFileInfo.Replace(_T("\\t"), _T("        "));
    sFileInfo.TrimLeft();
    sFileInfo.TrimRight();
    return sFileInfo;
}

}