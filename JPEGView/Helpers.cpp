#include "StdAfx.h"
#include "Helpers.h"
#include <math.h>

namespace Helpers {

float ScreenScaling = -1.0f;

TCHAR CReplacePipe::sm_buffer[MAX_SIZE_REPLACE_PIPE];

CReplacePipe::CReplacePipe(LPCTSTR sText) {
	_tcsncpy(sm_buffer, sText, MAX_SIZE_REPLACE_PIPE);
	sm_buffer[MAX_SIZE_REPLACE_PIPE-1] = 0;
	TCHAR* pPtr = sm_buffer;
	while (*pPtr != 0) {
		if (*pPtr == _T('|')) *pPtr = 0;
		pPtr++;
	}
}

LPCTSTR JPEGViewAppDataPath() {
	static TCHAR buff[MAX_PATH + 32] = _T("");
	if (buff[0] == 0) {
		::SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, buff);
		_tcscat(buff, _T("\\JPEGView\\"));
	}
	return buff;
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
	return CSize((int)(nWidth/dAR + 0.5), (int)(nHeight/dAR + 0.5));
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

	if (nMarker == 0 || pStream[nIndex] == nMarker) {
		return &(pStream[nIndex-1]); // place on marker start
	} else {
		return NULL;
	}
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

}