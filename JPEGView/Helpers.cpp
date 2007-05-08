#include "StdAfx.h"
#include "Helpers.h"
#include <math.h>

namespace Helpers {

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

CSize GetImageRectSampledDown(int nWidth, int nHeight, int nScreenWidth, int nScreenHeight, 
							  double dARTolerance, bool bAllowZoomIn) {
	double dAR1 = (double)nWidth/nScreenWidth;
	double dAR2 = (double)nHeight/nScreenHeight;
	double dARMax = max(dAR1, dAR2);
	if (dARMax <= 1.0 && !bAllowZoomIn) {
		return CSize(nWidth, nHeight);
	}
	if (fabs(1.0 - dAR2/dAR1) < dARTolerance) {
		return CSize(nScreenWidth, nScreenHeight);
	}

	return CSize((int)(nWidth/dARMax + 0.5), (int)(nHeight/dARMax + 0.5));
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

bool PatternMatch(LPCTSTR sString, LPCTSTR sPattern) {
	if (sString == NULL || sPattern == NULL || *sPattern == 0) return false;
	LPCTSTR pInStr = sString;
	LPCTSTR pInPat = sPattern;
	while (*pInPat != 0) {
		bool bThisPatternFails = false;
		while (*pInPat == _T(';')) pInPat++; // skip sequences of separators
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

__int64 CalculateJPEGFileHash(void* pJPEGStream, int nStreamLength) {
	uint8* pStream = (uint8*) pJPEGStream;
	if (pStream == NULL || nStreamLength < 3 || pStream[0] != 0xFF || pStream[1] != 0xD8) {
		return 0;
	}
	int nIndex = 2;
	do {
		if (pStream[nIndex] == 0xFF) {
			// block header found, skip padding bytes
			while (pStream[nIndex] == 0xFF && nIndex < nStreamLength) nIndex++;
			if (pStream[nIndex] == 0) {
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

}