#include "StdAfx.h"
#include "BasicProcessing.h"
#include "ResizeFilter.h"
#include "XMMImage.h"
#include "Helpers.h"
#include "WorkThread.h"
#include "ProcessingThreadPool.h"
#include <math.h>


// This macro allows for aligned definition of a 16 byte value with initialization of the 8 components
// to a single value
#define DECLARE_ALIGNED_DQWORD(name, initializer) \
	int16 _tempVal##name[16]; \
	int16* name = (int16*)((((PTR_INTEGRAL_TYPE)&(_tempVal##name) + 15) & ~15)); \
	name[0] = name[1] = name[2] = name[3] = name[4] = name[5] = name[6] = name[7] = initializer;

#define ALPHA_OPAQUE 0xFF000000

// holds last resize timing info
static TCHAR s_TimingInfo[256];

/////////////////////////////////////////////////////////////////////////////////////////////
// LUT creation for saturation, contrast and brightness and application of LUT
/////////////////////////////////////////////////////////////////////////////////////////////

uint8* CBasicProcessing::CreateSingleChannelLUT(double dContrastEnh, double dGamma) {
	dContrastEnh = min(0.5, max(-0.5, dContrastEnh));
	dGamma = min(10.0, max(0.1, dGamma));

	double dX = 0.0;
	double dInc = 1.0/255.0;
	double dValue;
	uint8* pLUT = new uint8[256];

	if (dContrastEnh >= 0) {
		// we define a piecewise function with quadratic terms on the left and right
		// and a linear term between
		dContrastEnh = dContrastEnh*0.5;
		double dA = 1.0/(1 - dContrastEnh);
		double dB = -dA*dContrastEnh/2;

		double dXs = (0.25 - dB)/dA;
		double dXe = (0.75 - dB)/dA;

		double dAs = (dA*dXs - 0.25)/(dXs*dXs);
		double dBs = dA - 2*dAs*dXs;

		for (int i = 0; i < 256; i++) {
			if (dX < dXs) {
				dValue = dAs*dX*dX + dBs*dX;
			} else if (dX > dXe) {
				dValue = 1.0 - (dAs*(1.0 - dX)*(1.0 - dX) + dBs*(1.0 - dX));
			} else {
				dValue = dA*dX + dB;
			}
			pLUT[i] = (uint8) (0.5 + 255*pow(min(1.0, max(0.0, dValue)), dGamma));
			dX += dInc;
		}
	} else {
		// define a quadratic piecewise S-curve
		double dFactor = -dContrastEnh*1.5;
		for (int i = 0; i < 256; i++) {
			if (dX < 0.5) {
				dValue = dX + dFactor*(0.25*0.25 - (dX - 0.25)*(dX - 0.25));
			} else  {
				dValue = dX - dFactor*(0.25*0.25 - (dX - 0.75)*(dX - 0.75));
			}
			pLUT[i] = (uint8) (0.5 + 255*pow(min(1.0, max(0.0, dValue)), dGamma));
			dX += dInc;
		}
	}

	return pLUT;
}

int32* CBasicProcessing::CreateColorSaturationLUTs(double dSaturation) {
	const double cdScaler = 1 << 16;
	int32* pLUTs = new int32[6 * 256];
	for (int i = 0; i < 256; i++) {
		pLUTs[i] = Helpers::RoundToInt(i * (0.299 + 0.701 * dSaturation) * cdScaler);
		pLUTs[i + 256] = Helpers::RoundToInt(i * 0.587 * (1.0 - dSaturation) * cdScaler);
		pLUTs[i + 512] = Helpers::RoundToInt(i * 0.114 * (1.0 - dSaturation) * cdScaler);
		pLUTs[i + 768] = Helpers::RoundToInt(i * 0.299 * (1.0 - dSaturation) * cdScaler);
		pLUTs[i + 1024] = Helpers::RoundToInt(i * (0.587 + 0.413 * dSaturation) * cdScaler);
		pLUTs[i + 1280] = Helpers::RoundToInt(i * (0.114 + 0.886 * dSaturation) * cdScaler);
	}

	return pLUTs;
}

int16* CBasicProcessing::Create1Channel16bppGrayscaleImage(int nWidth, int nHeight, const void* pDIBPixels, int nChannels) {
	// Create LUTs for grayscale conversion
	const double cdScaler = 1 << 16; // 1.0 is represented as 2^16
	uint32 LUTs[3 * 256];
	for (int i = 0; i < 256; i++) {
		LUTs[i] = (uint32)(0.299 * i * cdScaler + 0.5);
		LUTs[256 + i] = (uint32)(0.587 * i * cdScaler + 0.5);
		LUTs[512 + i] = (uint32)(0.114 * i * cdScaler + 0.5);
	}

	int16* pNewImage = new(std::nothrow) int16[nWidth * nHeight];
	if (pNewImage == NULL) return NULL;
	int nPadSrc = Helpers::DoPadding(nWidth*nChannels, 4) - nWidth*nChannels;
	int16* pTarget = pNewImage;
	const uint8* pSource = (uint8*)pDIBPixels;
	for (int j = 0; j < nHeight; j++) {
		for (int i = 0; i < nWidth; i++) {
			*pTarget++ = (LUTs[pSource[0]] + LUTs[256 + pSource[1]] + LUTs[512 + pSource[2]] + 32767) >> 10; // from 24 to 14 bits
			pSource += nChannels;
		}
		pSource += nPadSrc;
	}
	return pNewImage;
}

void* CBasicProcessing::Apply3ChannelLUT32bpp(int nWidth, int nHeight, const void* pDIBPixels, const uint8* pLUT) {
	if (pDIBPixels == NULL || pLUT == NULL) {
		return NULL;
	}

	uint32* pTarget = new(std::nothrow) uint32[nWidth * nHeight];
	if (pTarget == NULL) return NULL;
	const uint32* pSrc = (uint32*)pDIBPixels;
	uint32* pTgt = pTarget;
	for (int j = 0; j < nHeight; j++) {
		for (int i = 0; i < nWidth; i++) {
			uint32 nSrcPixels = *pSrc;
			*pTgt = pLUT[nSrcPixels & 0xFF] + pLUT[256 + ((nSrcPixels >> 8) & 0xFF)] * 256 + 
				pLUT[512 + ((nSrcPixels >> 16) & 0xFF)] * 65536 + ALPHA_OPAQUE;
			pTgt++; pSrc++;
		}
	}
	return pTarget;
}

void* CBasicProcessing::ApplySaturationAnd3ChannelLUT32bpp(int nWidth, int nHeight, const void* pDIBPixels, const int32* pSatLUTs, const uint8* pLUT) {
	if (pDIBPixels == NULL || pSatLUTs == NULL || pLUT == NULL) {
		return NULL;
	}

	const int cnScaler = 1 << 16;
	const int cnMax = 255 * cnScaler;
	uint32* pTarget = new(std::nothrow) uint32[nWidth * nHeight];
	if (pTarget == NULL) return NULL;
	const uint32* pSrc = (uint32*)pDIBPixels;
	uint32* pTgt = pTarget;
	for (int j = 0; j < nHeight; j++) {
		for (int i = 0; i < nWidth; i++) {
			uint32 nSrcPixels = *pSrc;
			int32 nSrcBlue = nSrcPixels & 0xFF;
			int32 nSrcGreen = (nSrcPixels >> 8) & 0xFF;
			int32 nSrcRed = (nSrcPixels >> 16) & 0xFF;
			int32 nRed = pSatLUTs[nSrcRed] + pSatLUTs[256 + nSrcGreen] + pSatLUTs[512 + nSrcBlue];
			int32 nGreen = pSatLUTs[768 + nSrcRed] + pSatLUTs[1024 + nSrcGreen] + pSatLUTs[512 + nSrcBlue];
			int32 nBlue = pSatLUTs[768 + nSrcRed] + pSatLUTs[256 + nSrcGreen] + pSatLUTs[1280 + nSrcBlue];
			*pTgt = pLUT[max(0, min(cnMax, nBlue)) >> 16] + pLUT[256 + (max(0, min(cnMax, nGreen)) >> 16)] * 256 + 
				pLUT[512 + (max(0, min(cnMax, nRed)) >> 16)] * 65536 + ALPHA_OPAQUE;
			pTgt++; pSrc++;
		}
	}
	return pTarget;
}

// Create the LDC response LUT between black and white points. This LUT makes sure
// that neither black nor white point is altered by the LDC.
static int32* CreateMulLUT(float fBlackPt, float fWhitePt, float fBlackPtSteepness) {
	const float cfFactor = 0.8f; // multiplication factor -> Strength of LDC
	const float cfSteepnessBlack = 0.6f; // how fast is the full strength reached after black pt
	const float cfSteepnessWhite = 0.6f; // how fast is the full strength reached after white pt

	int32* pNewLUT = new int32[256];
	if (fWhitePt <= fBlackPt) {
		memset(pNewLUT, 0, 256*sizeof(int32));
		return pNewLUT;
	}
	fBlackPtSteepness = max(0.0f, min(1.0f, (1.0f - 0.98f*fBlackPtSteepness)));
	float fMid = (fBlackPt + fWhitePt)/2;
	float fEndSlopeB = fBlackPt +cfSteepnessBlack*(fMid - fBlackPt);
	float fEndSlopeW = fWhitePt - cfSteepnessWhite*(fWhitePt - fMid);
	for (int i = 0; i < 256; i++) {
		int nLUTValue;
		float f = i*(1.0f/255.0f);
		if (f < fBlackPt) {
			nLUTValue = 0;
		} else if (f >= fWhitePt) {
			nLUTValue = 0;
		} else {
			if (f < fEndSlopeB) {
				float fValue = (f - fBlackPt)/(fEndSlopeB - fBlackPt);
				nLUTValue = (int)(cfFactor*16384*powf(fValue, fBlackPtSteepness) + 0.5f);
			} else if (f > fEndSlopeW) {
				nLUTValue = (int)(cfFactor*16384*(1.0f - (f - fEndSlopeW)/(fWhitePt - fEndSlopeW)) + 0.5f);
			} else {
				nLUTValue = (int)(cfFactor*16384 + 0.5f);
			}
		}
		pNewLUT[i] = (int32)nLUTValue;
	}
	return pNewLUT;
}

void* CBasicProcessing::ApplyLDC32bpp_Core(CSize fullTargetSize, CPoint fullTargetOffset, CSize dibSize,
									  CSize ldcMapSize, const void* pDIBPixels, const int32* pSatLUTs, const uint8* pLUT, const uint8* pLDCMap,
									  float fBlackPt, float fWhitePt, float fBlackPtSteepness, uint32* pTarget) {

	uint32 nIncrementX, nIncrementY;
	nIncrementX = (ldcMapSize.cx == 1) ? 0 : (uint32)((65536*(uint32)(ldcMapSize.cx - 1))/(fullTargetSize.cx - 1) - 1);
	nIncrementY = (ldcMapSize.cy == 1) ? 0 : (uint32)((65536*(uint32)(ldcMapSize.cy - 1))/(fullTargetSize.cy - 1) - 1);

	uint32 nCurY = fullTargetOffset.y*nIncrementY;
	uint32 nStartX = fullTargetOffset.x*nIncrementX;

	const int cnScaler = 1 << 16;
	const int cnMax = 255 * cnScaler;
	const int32* pMulLUT = CreateMulLUT(fBlackPt, fWhitePt, fBlackPtSteepness);
	const uint32* pSrc = (uint32*)pDIBPixels;
	uint32* pTgt = pTarget;
	for (int j = 0; j < dibSize.cy; j++) {
		uint32 nCurYTrunc = nCurY >> 16;
		uint32 nCurYFrac = nCurY & 0xFFFF;
		const uint8* pLDCMapSrc = pLDCMap + ldcMapSize.cx * nCurYTrunc;
		uint32 nCurX = nStartX;
		if (pSatLUTs == NULL) {
			for (int i = 0; i < dibSize.cx; i++) {
				// perform bilinear interpolation of mask
				uint32 nCurXTrunc = nCurX >> 16;
				uint32 nCurXFrac = nCurX & 0xFFFF;
				uint32 nMaskTopLeft  = pLDCMapSrc[nCurXTrunc];
				uint32 nMaskTopRight = pLDCMapSrc[nCurXTrunc + 1];
				uint32 nMaskBottomLeft = pLDCMapSrc[nCurXTrunc + ldcMapSize.cx];
				uint32 nMaskBottomRight = pLDCMapSrc[nCurXTrunc + ldcMapSize.cx + 1];
				uint32 nLeft = ((int)nCurYFrac*(int)(nMaskBottomLeft - nMaskTopLeft) >> 16) + nMaskTopLeft;
				uint32 nRight = ((int)nCurYFrac*(int)(nMaskBottomRight - nMaskTopRight) >> 16) + nMaskTopRight;
				int32 nMaskValue = ((int)nCurXFrac*(int)(nRight - nLeft) >> 16) + nLeft - 127;

				uint32 nSrcPixels = *pSrc;
				uint32 nBlue = pLUT[nSrcPixels & 0xFF];
				nBlue = nBlue + (nMaskValue*pMulLUT[nBlue] >> 14);
				uint32 nGreen = pLUT[((nSrcPixels >> 8) & 0xFF) + 256];
				nGreen = nGreen + (nMaskValue*pMulLUT[nGreen] >> 14);
				uint32 nRed = pLUT[((nSrcPixels >> 16) & 0xFF) + 512];
				nRed = nRed + (nMaskValue*pMulLUT[nRed] >> 14);

				*pTgt = max(0, min(255, (int)nBlue)) + max(0, min(255, (int)nGreen))*256 + max(0, min(255, (int)nRed))*65536 + ALPHA_OPAQUE;
				pTgt++; pSrc++;
				nCurX += nIncrementX;
			}
		} else {
			for (int i = 0; i < dibSize.cx; i++) {
				// perform bilinear interpolation of mask
				uint32 nCurXTrunc = nCurX >> 16;
				uint32 nCurXFrac = nCurX & 0xFFFF;
				uint32 nMaskTopLeft  = pLDCMapSrc[nCurXTrunc];
				uint32 nMaskTopRight = pLDCMapSrc[nCurXTrunc + 1];
				uint32 nMaskBottomLeft = pLDCMapSrc[nCurXTrunc + ldcMapSize.cx];
				uint32 nMaskBottomRight = pLDCMapSrc[nCurXTrunc + ldcMapSize.cx + 1];
				uint32 nLeft = ((int)nCurYFrac*(int)(nMaskBottomLeft - nMaskTopLeft) >> 16) + nMaskTopLeft;
				uint32 nRight = ((int)nCurYFrac*(int)(nMaskBottomRight - nMaskTopRight) >> 16) + nMaskTopRight;
				int32 nMaskValue = ((int)nCurXFrac*(int)(nRight - nLeft) >> 16) + nLeft - 127;

				// Apply saturation LUTs and three channel LUT
				uint32 nSrcPixels = *pSrc;
				int32 nSrcBlue = nSrcPixels & 0xFF;
				int32 nSrcGreen = (nSrcPixels >> 8) & 0xFF;
				int32 nSrcRed = (nSrcPixels >> 16) & 0xFF;
				int32 nRed = pSatLUTs[nSrcRed] + pSatLUTs[256 + nSrcGreen] + pSatLUTs[512 + nSrcBlue];
				int32 nGreen = pSatLUTs[768 + nSrcRed] + pSatLUTs[1024 + nSrcGreen] + pSatLUTs[512 + nSrcBlue];
				int32 nBlue = pSatLUTs[768 + nSrcRed] + pSatLUTs[256 + nSrcGreen] + pSatLUTs[1280 + nSrcBlue];
				nBlue = pLUT[max(0, min(cnMax, nBlue)) >> 16];
				nBlue = nBlue + (nMaskValue*pMulLUT[nBlue] >> 14);
				nGreen = pLUT[(max(0, min(cnMax, nGreen)) >> 16) + 256];
				nGreen = nGreen + (nMaskValue*pMulLUT[nGreen] >> 14);
				nRed = pLUT[(max(0, min(cnMax, nRed)) >> 16) + 512];
				nRed = nRed + (nMaskValue*pMulLUT[nRed] >> 14);

				*pTgt = max(0, min(255, (int)nBlue)) + max(0, min(255, (int)nGreen))*256 + max(0, min(255, (int)nRed))*65536 + ALPHA_OPAQUE;
				pTgt++; pSrc++;
				nCurX += nIncrementX;
			}
		}
		nCurY += nIncrementY;
	}
	delete[] pMulLUT;
	return pTarget;
}

void* CBasicProcessing::ApplyLDC32bpp(CSize fullTargetSize, CPoint fullTargetOffset, CSize dibSize,
									  CSize ldcMapSize, const void* pDIBPixels, const int32* pSatLUTs, const uint8* pLUT, const uint8* pLDCMap,
									  float fBlackPt, float fWhitePt, float fBlackPtSteepness) {

	if (pDIBPixels == NULL || pLUT == NULL || pLDCMap == NULL) {
	  return NULL;
	}
	if (fullTargetSize.cx <= 2 || fullTargetSize.cy <= 2) {
		// cannot apply to tiny images
		return Apply3ChannelLUT32bpp(dibSize.cx, dibSize.cy, pDIBPixels, pLUT);
	}

	uint32* pTarget = new(std::nothrow) uint32[dibSize.cx * dibSize.cy];
	if (pTarget == NULL) return NULL;
	CProcessingThreadPool& threadPool = CProcessingThreadPool::This();
	CRequestLDC request(pDIBPixels, dibSize, pTarget, fullTargetSize, fullTargetOffset,
		ldcMapSize, pSatLUTs, pLUT, pLDCMap, fBlackPt, fWhitePt, fBlackPtSteepness);
	bool bSuccess = threadPool.Process(&request);

	return bSuccess ? pTarget : NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Dimming of part of image and drawing of rectangles
/////////////////////////////////////////////////////////////////////////////////////////////

void CBasicProcessing::DimRectangle32bpp(int nWidth, int nHeight, void* pDIBPixels, CRect rect, float fDimValue) {
	CRect rectTarget;
	rectTarget.IntersectRect(CRect(0, 0, nWidth, nHeight), rect);
	if (pDIBPixels == NULL || rectTarget.IsRectEmpty()) {
		return;
	}
	uint8 alphaLUT[256];
	int nFactor = (int)(fDimValue*65536 + 0.5f);
	for (int a = 0; a < 256; a++) {
		alphaLUT[a] = a*nFactor >> 16;
	}

	uint32* pSrc = (uint32*) pDIBPixels + rectTarget.top*nWidth + rectTarget.left;
	for (int j = 0; j < rectTarget.Height(); j++) {
		uint32* pLine = pSrc;
		for (int i = 0; i < rectTarget.Width(); i++) {
			uint32 nPixel = *pLine;
			*pLine++ = alphaLUT[nPixel & 0xFF] + 256*alphaLUT[(nPixel >> 8) & 0xFF] + 65536*alphaLUT[(nPixel >> 16) & 0xFF] + ALPHA_OPAQUE;
		}
		pSrc += nWidth;
	}
}

void CBasicProcessing::FillRectangle32bpp(int nWidth, int nHeight, void* pDIBPixels, CRect rect, COLORREF color) {
	CRect rectTarget;
	rectTarget.IntersectRect(CRect(0, 0, nWidth, nHeight), rect);
	if (pDIBPixels == NULL || rectTarget.IsRectEmpty()) {
		return;
	}

	color = color | ALPHA_OPAQUE;
	uint32* pSrc = (uint32*) pDIBPixels + rectTarget.top*nWidth + rectTarget.left;
	for (int j = 0; j < rectTarget.Height(); j++) {
		uint32* pLine = pSrc;
		for (int i = 0; i < rectTarget.Width(); i++) {
			uint32 nPixel = *pLine;
			*pLine++ = color;
		}
		pSrc += nWidth;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Conversion and rotation methods
/////////////////////////////////////////////////////////////////////////////////////////////

// Rotate a block of a 32 bit DIB from source to target by 90 or 270 degrees
static void RotateBlock32bpp(const uint32* pSrc, uint32* pTgt, int nWidth, int nHeight,
							 int nXStart, int nYStart, int nBlockWidth, int nBlockHeight, bool bCW) {
	int nIncTargetLine = nHeight;
	int nIncSource = nWidth - nBlockWidth;
	const uint32* pSource = pSrc + nWidth * nYStart + nXStart;
	uint32* pTarget = bCW ? pTgt + nIncTargetLine * nXStart + (nHeight - 1 - nYStart) :
		pTgt + nIncTargetLine * (nWidth - 1 - nXStart) + nYStart;
	uint32* pStartYPtr = pTarget;
	if (!bCW) nIncTargetLine = -nIncTargetLine;
	int nIncStartYPtr = bCW ? -1 : +1;

	for (uint32 i = 0; i < nBlockHeight; i++) {
		for (uint32 j = 0; j < nBlockWidth; j++) {
			*pTarget = *pSource;
			pTarget += nIncTargetLine;
			pSource++;
		}
		pStartYPtr += nIncStartYPtr;
		pTarget = pStartYPtr;
		pSource += nIncSource;
	}
}

// 180 degrees rotation
static void* Rotate32bpp180(int nWidth, int nHeight, const void* pDIBPixels) {
	uint32* pTarget = new(std::nothrow) uint32[nWidth * nHeight];
	if (pTarget == NULL) return NULL;
	const uint32* pSource = (uint32*)pDIBPixels;
	for (uint32 i = 0; i < nHeight; i++) {
		uint32* pTgt = pTarget + nWidth*(nHeight - 1 - i) + (nWidth - 1);
		const uint32* pSrc = pSource + nWidth*i;
		for (uint32 j = 0; j < nWidth; j++) {
			*pTgt = *pSrc;
			pTgt -= 1;
			pSrc += 1;
		}
	}
	return pTarget;
}

void* CBasicProcessing::Rotate32bpp(int nWidth, int nHeight, const void* pDIBPixels, int nRotationAngleCW) {
	if (pDIBPixels == NULL) {
		return NULL;
	}
	if (nRotationAngleCW != 90 && nRotationAngleCW != 180 && nRotationAngleCW != 270) {
		return NULL; // not allowed
	} else if (nRotationAngleCW == 180) {
		return Rotate32bpp180(nWidth, nHeight, pDIBPixels);
	}

	uint32* pTarget = new(std::nothrow) uint32[nHeight * nWidth];
	if (pTarget == NULL) return NULL;
	const uint32* pSource = (uint32*)pDIBPixels;

	const int cnBlockSize = 32;
	int nX = 0, nY = 0;
	while (nY < nHeight) {
		nX = 0;
		while (nX < nWidth) {
			RotateBlock32bpp(pSource, pTarget, nWidth, nHeight,
				nX, nY, 
				min(cnBlockSize, nWidth - nX),
				min(cnBlockSize, nHeight - nY), nRotationAngleCW == 90);
			nX += cnBlockSize;
		}
		nY += cnBlockSize;
	}

	return pTarget;
}

void* CBasicProcessing::MirrorH32bpp(int nWidth, int nHeight, const void* pDIBPixels) {
	uint32* pTarget = new(std::nothrow) uint32[nWidth * nHeight];
	if (pTarget == NULL) return NULL;
	uint32* pTgt = pTarget;
	for (int j = 0; j < nHeight; j++) {
		const uint32* pSource = (uint32*)pDIBPixels + (j + 1) * nWidth - 1;
		for (int i = 0; i < nWidth; i++) {
			*pTgt = *pSource;
			pTgt++; pSource--;
		}
	}
	return pTarget;
}

void* CBasicProcessing::MirrorV32bpp(int nWidth, int nHeight, const void* pDIBPixels) {
	uint32* pTarget = new(std::nothrow) uint32[nWidth * nHeight];
	if (pTarget == NULL) return NULL;
	uint32* pTgt = pTarget;
	for (int j = 0; j < nHeight; j++) {
		const uint32* pSource = (uint32*)pDIBPixels + (nHeight - 1 - j) * nWidth;
		for (int i = 0; i < nWidth; i++) {
			*pTgt = *pSource;
			pTgt++; pSource++;
		}
	}
	return pTarget;
}

void* CBasicProcessing::Convert8bppTo32bppDIB(int nWidth, int nHeight, const void* pDIBPixels, const uint8* pPalette) {
	if (pDIBPixels == NULL || pPalette == NULL) {
		return NULL;
	}
	int nPaddedWidthS = Helpers::DoPadding(nWidth, 4);
	int nPadS = nPaddedWidthS - nWidth;
	uint32* pNewDIB = new(std::nothrow) uint32[nWidth * nHeight];
	if (pNewDIB == NULL) return NULL;
	uint32* pTargetDIB = pNewDIB;
	const uint8* pSourceDIB = (uint8*)pDIBPixels;
	for (int j = 0; j < nHeight; j++) {
		for (int i = 0; i < nWidth; i++) {
			*pTargetDIB++ = pPalette[4 * *pSourceDIB] + pPalette[4 * *pSourceDIB + 1] * 256 + pPalette[4 * *pSourceDIB + 2] * 65536 + ALPHA_OPAQUE;
			pSourceDIB ++;
		}
		pSourceDIB += nPadS;
	}
	return pNewDIB;
}

void CBasicProcessing::Convert32bppTo24bppDIB(int nWidth, int nHeight, void* pDIBTarget, 
											  const void* pDIBSource, bool bFlip) {
	if (pDIBTarget == NULL || pDIBSource == NULL) {
		return;
	}
	int nPaddedWidth = Helpers::DoPadding(nWidth*3, 4);
	int nPad = nPaddedWidth - nWidth*3;
	uint8* pTargetDIB = (uint8*)pDIBTarget;
	for (int j = 0; j < nHeight; j++) {
		const uint8* pSourceDIB;
		if (bFlip) {
			pSourceDIB = (uint8*)pDIBSource + (nHeight - 1 - j)*nWidth*4;
		} else {
			pSourceDIB = (uint8*)pDIBSource + j*nWidth*4;
		}
		for (int i = 0; i < nWidth; i++) {
			*pTargetDIB++ = pSourceDIB[0];
			*pTargetDIB++ = pSourceDIB[1];
			*pTargetDIB++ = pSourceDIB[2];
			pSourceDIB += 4;
		}
		pTargetDIB += nPad;
	}
}

void* CBasicProcessing::Convert1To4Channels(int nWidth, int nHeight, const void* pIJLPixels) {
	if (pIJLPixels == NULL) {
		return NULL;
	}
	int nPadSrc = Helpers::DoPadding(nWidth, 4) - nWidth;
	uint32* pNewDIB = new(std::nothrow) uint32[nWidth * nHeight];
	if (pNewDIB == NULL) return NULL;
	uint32* pTarget = pNewDIB;
	const uint8* pSource = (uint8*)pIJLPixels;
	for (int j = 0; j < nHeight; j++) {
		for (int i = 0; i < nWidth; i++) {
			*pTarget++ = *pSource + *pSource * 256 + *pSource * 65536 + ALPHA_OPAQUE;
			pSource++;
		}
		pSource += nPadSrc;
	}
	return pNewDIB;
}

void* CBasicProcessing::Convert16bppGrayTo32bppDIB(int nWidth, int nHeight, const int16* pPixels) {
	uint32* pNewDIB = new(std::nothrow) uint32[nWidth * nHeight];
	if (pNewDIB == NULL) return NULL;
	uint32* pTarget = pNewDIB;
	const int16* pSource = pPixels;
	for (int j = 0; j < nHeight; j++) {
		for (int i = 0; i < nWidth; i++) {
			int nSourceValue = *pSource++ >> 6; // from 14 to 8 bits
			*pTarget++ = nSourceValue + nSourceValue * 256 + nSourceValue * 65536 + ALPHA_OPAQUE;
		}
	}
	return pNewDIB;
}


void* CBasicProcessing::Convert3To4Channels(int nWidth, int nHeight, const void* pIJLPixels) {
	if (pIJLPixels == NULL) {
		return NULL;
	}
	int nPadSrc = Helpers::DoPadding(nWidth*3, 4) - nWidth*3;
	uint32* pNewDIB = new(std::nothrow) uint32[nWidth * nHeight];
	if (pNewDIB == NULL) return NULL;
	uint32* pTarget = pNewDIB;
	const uint8* pSource = (uint8*)pIJLPixels;
	for (int j = 0; j < nHeight; j++) {
		for (int i = 0; i < nWidth; i++) {
			*pTarget++ = pSource[0] + pSource[1] * 256 + pSource[2] * 65536 + ALPHA_OPAQUE;
			pSource += 3;
		}
		pSource += nPadSrc;
	}
	return pNewDIB;
}

void* CBasicProcessing::ConvertGdiplus32bppRGB(int nWidth, int nHeight, int nStride, const void* pGdiplusPixels) {
	if (pGdiplusPixels == NULL || nWidth*4 > abs(nStride)) {
		return NULL;
	}
	uint32* pNewDIB = new(std::nothrow) uint32[nWidth * nHeight];
	if (pNewDIB == NULL) return NULL;
	uint32* pTgt = pNewDIB;
	const uint8* pSrc = (const uint8*)pGdiplusPixels;
	for (int j = 0; j < nHeight; j++) {
        for (int i = 0; i < nWidth; i++)
            pTgt[i] = ((uint32*)pSrc)[i] | ALPHA_OPAQUE;
		pTgt += nWidth;
		pSrc += nStride;
	}
	return pNewDIB;
}

void* CBasicProcessing::CopyRect32bpp(void* pTarget, const void* pSource,  CSize targetSize, CRect targetRect,
									  CSize sourceSize, CRect sourceRect) {
	if (pSource == NULL || sourceRect.Size() != targetRect.Size() || 
		sourceRect.left < 0 || sourceRect.right > sourceSize.cx ||
		sourceRect.top < 0 || sourceRect.bottom > sourceSize.cy ||
		targetRect.left < 0 || targetRect.right > targetSize.cx ||
		targetRect.top < 0 || targetRect.bottom > targetSize.cy) {
		return NULL;
	}
	uint32* pSourceDIB = (uint32*)pSource;
	uint32* pTargetDIB = (uint32*)pTarget;
	if (pTargetDIB == NULL) {
		pTargetDIB = new(std::nothrow) uint32[targetSize.cx * targetSize.cy];
		if (pTargetDIB == NULL) return NULL;
		pTarget = pTargetDIB;
	}

	pTargetDIB += (targetRect.top * targetSize.cx) + targetRect.left;
	pSourceDIB += (sourceRect.top * sourceSize.cx) + sourceRect.left;
	for (int y = 0; y < sourceRect.Height(); y++) {
		memcpy(pTargetDIB, pSourceDIB, sourceRect.Width()*sizeof(uint32));
		pTargetDIB += targetSize.cx;
		pSourceDIB += sourceSize.cx;
	}

	return pTarget;
}

void* CBasicProcessing::Crop32bpp(int nWidth, int nHeight, const void* pDIBPixels, CRect cropRect) {
	if (pDIBPixels == NULL || cropRect.Width() == 0 || cropRect.Height() == 0) {
		return NULL;
	}

	return CopyRect32bpp(NULL, pDIBPixels, cropRect.Size(), CRect(0, 0, cropRect.Width(), cropRect.Height()),
		CSize(nWidth, nHeight), cropRect);
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Simple point sampling resize and rotation methods
/////////////////////////////////////////////////////////////////////////////////////////////

void* CBasicProcessing::PointSample(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize, 
						 CSize sourceSize, const void* pIJLPixels, int nChannels) {
 	if (fullTargetSize.cx < 1 || fullTargetSize.cy < 1 ||
		clippedTargetSize.cx < 1 || clippedTargetSize.cy < 1 ||
		fullTargetOffset.x < 0 || fullTargetOffset.x < 0 ||
		clippedTargetSize.cx + fullTargetOffset.x > fullTargetSize.cx ||
		clippedTargetSize.cy + fullTargetOffset.y > fullTargetSize.cy ||
		pIJLPixels == NULL || (nChannels != 3 && nChannels != 4)) {
		return NULL;
	}

	uint8* pDIB = new(std::nothrow) uint8[clippedTargetSize.cx*4 * clippedTargetSize.cy];
	if (pDIB == NULL) return NULL;

	uint32 nIncrementX, nIncrementY;
	if (fullTargetSize.cx <= sourceSize.cx) {
		// Downsampling
		nIncrementX = (uint32)(sourceSize.cx << 16)/fullTargetSize.cx + 1;
		nIncrementY = (uint32)(sourceSize.cy << 16)/fullTargetSize.cy + 1;
	} else {
		// Upsampling
		nIncrementX = (fullTargetSize.cx == 1) ? 0 : (uint32)((65536*(uint32)(sourceSize.cx - 1) + 65535)/(fullTargetSize.cx - 1));
		nIncrementY = (fullTargetSize.cy == 1) ? 0 : (uint32)((65536*(uint32)(sourceSize.cy - 1) + 65535)/(fullTargetSize.cy - 1));
	}

	int nPaddedSourceWidth = Helpers::DoPadding(sourceSize.cx * nChannels, 4);
	const uint8* pSrc = NULL;
	uint8* pDst = pDIB;
	uint32 nCurY = fullTargetOffset.y*nIncrementY;
	uint32 nStartX = fullTargetOffset.x*nIncrementX;
	for (int j = 0; j < clippedTargetSize.cy; j++) {
		pSrc = (uint8*)pIJLPixels + nPaddedSourceWidth * (nCurY >> 16);
		uint32 nCurX = nStartX;
		if (nChannels == 3) {
			for (int i = 0; i < clippedTargetSize.cx; i++) {
				uint32 sx = nCurX >> 16; 
				uint32 s = sx*3;
				uint32 d = i*4;
				pDst[d] = pSrc[s];
				pDst[d+1] = pSrc[s+1];
				pDst[d+2] = pSrc[s+2];
				pDst[d+3] = 0xFF;
				nCurX += nIncrementX;
			}
		} else {
			for (int i = 0; i < clippedTargetSize.cx; i++) {
				uint32 sx = nCurX >> 16; 
				((uint32*)pDst)[i] = ((uint32*)pSrc)[sx];
				nCurX += nIncrementX;
			}
		}
		pDst += clippedTargetSize.cx*4;
		nCurY += nIncrementY;
	}
	return pDIB;
}

static void RotateInplace(double& dX, double& dY, double dAngle) {
	double dXr = cos(dAngle) * dX - sin(dAngle) * dY;
	double dYr = sin(dAngle) * dX + cos(dAngle) * dY;
	dX = dXr;
	dY = dYr;
}

void* CBasicProcessing::PointSampleWithRotation(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize, 
						 CSize sourceSize, double dRotation, const void* pIJLPixels, int nChannels, COLORREF backColor) {
 	if (fullTargetSize.cx < 1 || fullTargetSize.cy < 1 ||
		clippedTargetSize.cx < 1 || clippedTargetSize.cy < 1 ||
		fullTargetOffset.x < 0 || fullTargetOffset.x < 0 ||
		clippedTargetSize.cx + fullTargetOffset.x > fullTargetSize.cx ||
		clippedTargetSize.cy + fullTargetOffset.y > fullTargetSize.cy ||
		pIJLPixels == NULL || (nChannels != 3 && nChannels != 4)) {
		return NULL;
	}

	uint8* pDIB = new(std::nothrow) uint8[clippedTargetSize.cx*4 * clippedTargetSize.cy];
	if (pDIB == NULL) return NULL;

	uint32 nBackColor = (GetRValue(backColor) << 16) + (GetGValue(backColor) << 8) + GetBValue(backColor) + ALPHA_OPAQUE;
	double dFirstX = -(sourceSize.cx - 1) * 0.5;
	double dFirstY = -(sourceSize.cy - 1) * 0.5;
	double dIncX1 = dFirstX + ((fullTargetSize.cx == 1) ? 0 : (double)(sourceSize.cx)/(fullTargetSize.cx - 1));
	double dIncY1 = dFirstY;
	double dIncX2 = dFirstX;
	double dIncY2 = dFirstY + ((fullTargetSize.cy == 1) ? 0 : (double)(sourceSize.cy)/(fullTargetSize.cy - 1));
	RotateInplace(dFirstX, dFirstY, -dRotation);
	RotateInplace(dIncX1, dIncY1, -dRotation);
	RotateInplace(dIncX2, dIncY2, -dRotation);
	dIncX1 = dIncX1 - dFirstX;
	dIncY1 = dIncY1 - dFirstY;
	dIncX2 = dIncX2 - dFirstX;
	dIncY2 = dIncY2 - dFirstY;

	int32 nFirstX = Helpers::RoundToInt(65536 * (dFirstX + (sourceSize.cx - 1) * 0.5));
	int32 nFirstY = Helpers::RoundToInt(65536 * (dFirstY + (sourceSize.cy - 1) * 0.5));
	int32 nIncrementX1 = Helpers::RoundToInt(65536 * dIncX1);
	int32 nIncrementY1 = Helpers::RoundToInt(65536 * dIncY1);
	int32 nIncrementX2 = Helpers::RoundToInt(65536 * dIncX2);
	int32 nIncrementY2 = Helpers::RoundToInt(65536 * dIncY2);

	int nPaddedSourceWidth = Helpers::DoPadding(sourceSize.cx * nChannels, 4);
	const uint8* pSrc = NULL;
	uint8* pDst = pDIB;
	int32 nX = nFirstX + fullTargetOffset.x * nIncrementX1 + fullTargetOffset.y * nIncrementX2;
	int32 nY = nFirstY + fullTargetOffset.x * nIncrementY1 + fullTargetOffset.y * nIncrementY2;
	for (int j = 0; j < clippedTargetSize.cy; j++) {
		int nCurX = nX;
		int nCurY = nY;
		if (nChannels == 3) {
			for (int i = 0; i < clippedTargetSize.cx; i++) {
				int32 nCurRealX = nCurX >> 16;
				int32 nCurRealY = nCurY >> 16;
				if (nCurRealX >= 0 && nCurRealX < sourceSize.cx && nCurRealY >= 0 && nCurRealY < sourceSize.cy) {
					pSrc = (uint8*)pIJLPixels + nPaddedSourceWidth * nCurRealY + nCurRealX * 3;
					uint32 d = i*4;
					pDst[d] = pSrc[0];
					pDst[d+1] = pSrc[1];
					pDst[d+2] = pSrc[2];
					pDst[d+3] = 0xFF;
				} else {
					*((uint32*)pDst + i) = nBackColor;
				}
				nCurX += nIncrementX1;
				nCurY += nIncrementY1;
			}
		} else {
			for (int i = 0; i < clippedTargetSize.cx; i++) {
				int32 nCurRealX = nCurX >> 16;
				int32 nCurRealY = nCurY >> 16;
				if (nCurRealX >= 0 && nCurRealX < sourceSize.cx && nCurRealY >= 0 && nCurRealY < sourceSize.cy) {
					pSrc = (uint8*)pIJLPixels + nPaddedSourceWidth * nCurRealY + nCurRealX * 4;
					*((uint32*)pDst + i) = *((uint32*)pSrc);
				} else {
					*((uint32*)pDst + i) = nBackColor;
				}
				nCurX += nIncrementX1;
				nCurY += nIncrementY1;
			}
		}
		pDst += clippedTargetSize.cx*4;
		nX += nIncrementX2;
		nY += nIncrementY2;
	}
	return pDIB;
}

// The prepective correction can be accelerated enourmously by taking into account that the projection
// rectangle is aligned to the camera plane (Doom engine did the same, only allowing horizontal floor and vertical walls)
// We only have to precalculate a map for the intersection of the y-scanlines (pTableY, in 16.16 fixed point format)
// The caller needs to delete the returned map when no longer needed
static int* CalculateTrapezoidYIntersectionTable(const CTrapezoid& trapezoid, int nTableSize, int nSourceSizeY, int nTargetSizeY, int nOffsetInTargetSizeY) {
	int* pTableY = new int[nTableSize];
	int nW1 = trapezoid.x1e - trapezoid.x1s;
	int nW2 = trapezoid.x2e - trapezoid.x2s;
	int nD1 = trapezoid.x1s - trapezoid.x2s;
	int nD2 = trapezoid.x1e - trapezoid.x2e;
	int nSign = (nW1 < nW2) ? 1 : -1;
	int nW = nSign * min(nW1, nW2);
	// First step is to find out the height of the triangle that is built by extending the trapezoid
	// left and right sides until they intersect. dHeightTria == 0 means no intersection (parallel projection)
	double dHeightTria;
	if (nD1 == 0) {
		if (nD2 == 0) {
			dHeightTria = 0;
		} else {
			dHeightTria = -nW * (double)trapezoid.Height()/nD2;
		}
	} else if (nD2 == 0) {
		dHeightTria = nW * (double)trapezoid.Height()/nD1;
	} else {
		double dM1 = (double)trapezoid.Height()/nD1;
		double dM2 = (double)trapezoid.Height()/nD2;
		dHeightTria = (fabs(dM2 - dM1) < 1e-9) ? 0.0 : nW * dM1 * dM2/(dM2 - dM1);
	}
	if (dHeightTria > 0) {
		// Perspective correction needed, y-scanlines are not homogenous.
		// The unknowns in the projection equation are found using the following conditions:
		// pTable(y) = a/(y + c) + b
		// The pole of the projection is at position H, respecively h - H (depending on the sign of the trapezoid)
		// -> This directly leads to: H + c = 0, thus c = -H, respectively c = H - h
		// a and b are found using the following two border conditions:
		// 0 = a/c + b
		// h = a/(h + c) + b
		int h = trapezoid.Height();
		double dH = dHeightTria + h;
		double dB = (nSign > 0) ? dH : (h - dH);
		double dA = dH*(h - dH);
		double dAdd = ((nSign > 0) ? dH - h : -dH) + nOffsetInTargetSizeY;
		double dFactor = 65536.0*(nSourceSizeY - 1)/(nTargetSizeY - 1);
		int nMaxValue = (nSourceSizeY - 1) << 16;
		for (int j = 0; j < nTableSize; j++) {
			int nYLineIndex = (int)(dFactor*(dA/(j + dAdd) + dB) + 0.5);
			pTableY[j] = max(0, min(nMaxValue, nYLineIndex));
		}
	} else {
		int nIncrementY;
		if (nTargetSizeY <= nSourceSizeY) {
			// Downsampling
			nIncrementY = (nSourceSizeY << 16)/nTargetSizeY + 1;
		} else {
			// Upsampling
			nIncrementY = (nTargetSizeY == 1) ? 0 : ((65536*(uint32)(nSourceSizeY - 1) + 65535)/(nTargetSizeY - 1));
		}
		// only point resampling into a new rectangle, all y-scanlines have the same distance
		int nCurY = nOffsetInTargetSizeY*nIncrementY;
		for (int j = 0; j < nTableSize; j++) {
			pTableY[j] = nCurY;
			nCurY += nIncrementY;
		}
	}
	return pTableY;
}

void* CBasicProcessing::PointSampleTrapezoid(CSize fullTargetSize, const CTrapezoid& fullTargetTrapezoid, CPoint fullTargetOffset, CSize clippedTargetSize, 
											 CSize sourceSize, const void* pIJLPixels, int nChannels, COLORREF backColor) {
  	if (fullTargetSize.cx < 1 || fullTargetSize.cy < 1 ||
		(fullTargetTrapezoid.x1e - fullTargetTrapezoid.x1s) <= 0  ||
		(fullTargetTrapezoid.x2e - fullTargetTrapezoid.x2s) <= 0  ||
		clippedTargetSize.cx < 1 || clippedTargetSize.cy < 1 ||
		fullTargetOffset.x < 0 || fullTargetOffset.x < 0 ||
		clippedTargetSize.cx + fullTargetOffset.x > fullTargetSize.cx ||
		clippedTargetSize.cy + fullTargetOffset.y > fullTargetSize.cy ||
		pIJLPixels == NULL || (nChannels != 3 && nChannels != 4)) {
		return NULL;
	}

	uint8* pDIB = new(std::nothrow) uint8[clippedTargetSize.cx*4 * clippedTargetSize.cy];
	if (pDIB == NULL) return NULL;

	int* pTableY = CalculateTrapezoidYIntersectionTable(fullTargetTrapezoid, clippedTargetSize.cy, sourceSize.cy, fullTargetSize.cy, fullTargetOffset.y);

	float fTx1 = (fullTargetTrapezoid.x1s - fullTargetTrapezoid.x2s)*0.5f;
	float fTx2 = fTx1 + fullTargetTrapezoid.x1e - fullTargetTrapezoid.x1s;
	float fIncrementTx1 = ((float)(fullTargetTrapezoid.x2s - fullTargetTrapezoid.x1s))/fullTargetTrapezoid.Height();
	float fIncrementTx2 = ((float)(fullTargetTrapezoid.x2e - fullTargetTrapezoid.x1e))/fullTargetTrapezoid.Height();

	uint32 nBackColor = (GetRValue(backColor) << 16) + (GetGValue(backColor) << 8) + GetBValue(backColor) + ALPHA_OPAQUE;
	int nPaddedSourceWidth = Helpers::DoPadding(sourceSize.cx * nChannels, 4);
	const uint8* pSrc = NULL;
	uint8* pDst = pDIB;
	fTx1 = fTx1 + fullTargetOffset.y*fIncrementTx1;
	fTx2 = fTx2 + fullTargetOffset.y*fIncrementTx2;
	int nSourceSizeXFP16 = sourceSize.cx << 16;
	for (int j = 0; j < clippedTargetSize.cy; j++) {
		pSrc = (uint8*)pIJLPixels + nPaddedSourceWidth * (pTableY[j] >> 16);
		int nIncrementX = (int)(nSourceSizeXFP16/(fTx2 - fTx1 + 1)) + 1;
		int nStartX = (int)((fullTargetOffset.x - fTx1)*nIncrementX);
		int nCurX = nStartX;
		if (nChannels == 3) {
			for (int i = 0; i < clippedTargetSize.cx; i++) {
				int sx = nCurX >> 16;
				if (sx >= 0 && sx < sourceSize.cx) {
					int s = sx*3;
					int d = i*4;
					pDst[d] = pSrc[s];
					pDst[d+1] = pSrc[s+1];
					pDst[d+2] = pSrc[s+2];
					pDst[d+3] = 0xFF;
				} else {
					((uint32*)pDst)[i] = nBackColor;
				}
				nCurX += nIncrementX;
			}
		} else {
			for (int i = 0; i < clippedTargetSize.cx; i++) {
				int sx = nCurX >> 16; 
				if (sx >= 0 && sx < sourceSize.cx) {
					((uint32*)pDst)[i] = ((uint32*)pSrc)[sx];
				} else {
					((uint32*)pDst)[i] = nBackColor;
				}
				nCurX += nIncrementX;
			}
		}
		pDst += clippedTargetSize.cx*4;
		fTx1 += fIncrementTx1;
		fTx2 += fIncrementTx2;
	}

	delete[] pTableY;
	return pDIB;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// High quality rotation using bicubic sampling
/////////////////////////////////////////////////////////////////////////////////////////////

#define NUM_KERNELS_LOG2 6
#define NUM_KERNELS_BICUBIC 65
#define FP_HALF 8192

// Bicubic resampling of one pixel at pSource (in 3 or 4 channel format) into one destination pixel in 32 bit format
// The fractional part of the pixel position is given in .16 fixed point format
// pKernels contains NUM_KERNELS_BICUBIC precalculated bicubic filter kernels each of lenght 4, applied with offset -1 to the source pixels
static void InterpolateBicubic(const uint8* pSource, uint8* pDest, int16* pKernels, int32 nFracX, int32 nFracY, int nPaddedSourceWidth, int nChannels) {
	int16* pKernelX = &(pKernels[4*(nFracX >> (16 - NUM_KERNELS_LOG2))]);
	int16* pKernelY = &(pKernels[4*(nFracY >> (16 - NUM_KERNELS_LOG2))]);
	int nChannels2 = nChannels * 2;
	for (int i = 0; i < nChannels; i++) {
		const uint8* pSrc = pSource;
		pSrc -= nPaddedSourceWidth;

		int32 nSum1 = pSrc[-nChannels] * pKernelX[0] + pSrc[0] * pKernelX[1] + pSrc[nChannels] * pKernelX[2] + pSrc[nChannels2] * pKernelX[3] + FP_HALF;
		pSrc += nPaddedSourceWidth;
		int32 nSum2 = pSrc[-nChannels] * pKernelX[0] + pSrc[0] * pKernelX[1] + pSrc[nChannels] * pKernelX[2] + pSrc[nChannels2] * pKernelX[3] + FP_HALF;
		pSrc += nPaddedSourceWidth;
		int32 nSum3 = pSrc[-nChannels] * pKernelX[0] + pSrc[0] * pKernelX[1] + pSrc[nChannels] * pKernelX[2] + pSrc[nChannels2] * pKernelX[3] + FP_HALF;
		pSrc += nPaddedSourceWidth;
		int32 nSum4 = pSrc[-nChannels] * pKernelX[0] + pSrc[0] * pKernelX[1] + pSrc[nChannels] * pKernelX[2] + pSrc[nChannels2] * pKernelX[3] + FP_HALF;

		int32 nSum = (nSum1 >> 14) * pKernelY[0] + (nSum2 >> 14) * pKernelY[1] + (nSum3 >> 14) * pKernelY[2] + (nSum4 >> 14) * pKernelY[3] + FP_HALF;
		nSum = nSum >> 14;
		*pDest++ = min(255, max(0, nSum));
		pSource++;
	}
	if (nChannels == 3) *pDest = 0xFF;
}

static inline void Get4Pixels(const uint8* pSrc, uint8 dest[4], int nFrom, int nTo, int nChannels, uint32 nFill) {
	int nC = -nChannels;
	for (int i = 0; i < 4; i++) {
		dest[i] = (i >= nFrom + 1 && i <= nTo + 1) ? pSrc[nC] : (uint8)nFill;
		nC += nChannels;
	}
}

// Same as method above but with border handling, assuming that the filter kernels are only evaluated from nXFrom to nXTo and nYFrom to nYTo
// Pixels that are outside this range are assumed to have value nBackColor
static void InterpolateBicubicBorder(const uint8* pSource, uint8* pDest, int16* pKernels, 
									 int32 nFracX, int32 nFracY, int nXFrom, int nXTo, int nYFrom, int nYTo, int nPaddedSourceWidth, int nChannels, uint32 nBackColor) {
	int16* pKernelX = &(pKernels[4*(nFracX >> (16 - NUM_KERNELS_LOG2))]);
	int16* pKernelY = &(pKernels[4*(nFracY >> (16 - NUM_KERNELS_LOG2))]);
	uint32 nBackColorShifted = nBackColor;
	for (int i = 0; i < nChannels; i++) {
		const uint8* pSrc = pSource;
		pSrc -= nPaddedSourceWidth;
		uint32 nFill = nBackColorShifted & 0xFF;
		int32 nFillShifted = nFill << 14;
		nBackColorShifted = nBackColorShifted >> 8;

		uint8 pixels[4];

		int32 nSum1 = nFillShifted;
		if (nYFrom <= -1) {
			Get4Pixels(pSrc, pixels, nXFrom, nXTo, nChannels, nFill); 
			nSum1 = pixels[0] * pKernelX[0] + pixels[1] * pKernelX[1] + pixels[2] * pKernelX[2] + pixels[3] * pKernelX[3] + FP_HALF;
		}
		pSrc += nPaddedSourceWidth;
		int nSum2 = nFillShifted;
		if (nYFrom <= 0 && nYTo >= 0) {
			Get4Pixels(pSrc, pixels, nXFrom, nXTo, nChannels, nFill); 
			nSum2 = pixels[0] * pKernelX[0] + pixels[1] * pKernelX[1] + pixels[2] * pKernelX[2] + pixels[3] * pKernelX[3] + FP_HALF;
		}
		pSrc += nPaddedSourceWidth;
		int32 nSum3 = nFillShifted;
		if (nYFrom <= 1 && nYTo >= 1) {
			Get4Pixels(pSrc, pixels, nXFrom, nXTo, nChannels, nFill); 
			nSum3 = pixels[0] * pKernelX[0] + pixels[1] * pKernelX[1] + pixels[2] * pKernelX[2] + pixels[3] * pKernelX[3] + FP_HALF;
		}
		pSrc += nPaddedSourceWidth;
		int32 nSum4 = nFillShifted;
		if (nYTo >= 2) {
			Get4Pixels(pSrc, pixels, nXFrom, nXTo, nChannels, nFill); 
			nSum4 = pixels[0] * pKernelX[0] + pixels[1] * pKernelX[1] + pixels[2] * pKernelX[2] + pixels[3] * pKernelX[3] + FP_HALF;
		}

		int32 nSum = (nSum1 >> 14) * pKernelY[0] + (nSum2 >> 14) * pKernelY[1] + (nSum3 >> 14) * pKernelY[2] + (nSum4 >> 14) * pKernelY[3] + FP_HALF;
		nSum = nSum >> 14;

		*pDest++ = min(255, max(0, nSum));
		pSource++;
	}
	if (nChannels == 3) *pDest = 0xFF;
}

void* CBasicProcessing::RotateHQ_Core(CPoint targetOffset, CSize targetSize, double dRotation, CSize sourceSize, 
									  const void* pSourcePixels, void* pTargetPixels, int nChannels, COLORREF backColor){

	double dFirstX = -(sourceSize.cx - 1) * 0.5;
	double dFirstY = -(sourceSize.cy - 1) * 0.5;
	double dIncX1 = dFirstX + ((targetSize.cx == 1) ? 0 : 1);
	double dIncY1 = dFirstY;
	double dIncX2 = dFirstX;
	double dIncY2 = dFirstY + ((targetSize.cy == 1) ? 0 : 1);
	RotateInplace(dFirstX, dFirstY, -dRotation);
	RotateInplace(dIncX1, dIncY1, -dRotation);
	RotateInplace(dIncX2, dIncY2, -dRotation);
	dIncX1 = dIncX1 - dFirstX;
	dIncY1 = dIncY1 - dFirstY;
	dIncX2 = dIncX2 - dFirstX;
	dIncY2 = dIncY2 - dFirstY;

	int32 nFirstX = Helpers::RoundToInt(65536 * (dFirstX + (sourceSize.cx - 1) * 0.5));
	int32 nFirstY = Helpers::RoundToInt(65536 * (dFirstY + (sourceSize.cy - 1) * 0.5));
	int32 nIncrementX1 = Helpers::RoundToInt(65536 * dIncX1);
	int32 nIncrementY1 = Helpers::RoundToInt(65536 * dIncY1);
	int32 nIncrementX2 = Helpers::RoundToInt(65536 * dIncX2);
	int32 nIncrementY2 = Helpers::RoundToInt(65536 * dIncY2);

	int16* pKernels = new int16[NUM_KERNELS_BICUBIC * 4];
	CResizeFilter::GetBicubicFilterKernels(NUM_KERNELS_BICUBIC, pKernels);

	uint32 nBackColor = (GetRValue(backColor) << 16) + (GetGValue(backColor) << 8) + GetBValue(backColor) + ALPHA_OPAQUE;
	int nPaddedSourceWidth = Helpers::DoPadding(sourceSize.cx * nChannels, 4);
	const uint8* pSrc = NULL;
	uint8* pDst = (uint8*)pTargetPixels;
	int32 nX = nFirstX + targetOffset.x * nIncrementX1 + targetOffset.y * nIncrementX2;
	int32 nY = nFirstY + targetOffset.x * nIncrementY1 + targetOffset.y * nIncrementY2;
	for (int j = 0; j < targetSize.cy; j++) {
		int nCurX = nX;
		int nCurY = nY;
		for (int i = 0; i < targetSize.cx; i++) {
			int32 nCurRealX = nCurX >> 16;
			int32 nCurRealY = nCurY >> 16;
			int32 nFracX = nCurX & 0xFFFF;
			int32 nFracY = nCurY & 0xFFFF;
			if (nCurRealX >= -1 && nCurRealX <= sourceSize.cx && nCurRealY >= -1 && nCurRealY <= sourceSize.cy) {
				pSrc = (uint8*)pSourcePixels + nPaddedSourceWidth * nCurRealY + nCurRealX * nChannels;
				if (nCurRealX > 0 && nCurRealX < sourceSize.cx - 2 && nCurRealY > 0 && nCurRealY < sourceSize.cy - 2) {
					InterpolateBicubic(pSrc, pDst + i*4, pKernels, nFracX, nFracY, nPaddedSourceWidth, nChannels);
				} else {
					int nXFrom = (nCurRealX > 0) ? -1 : -nCurRealX;
					int nXTo = (nCurRealX < sourceSize.cx - 2) ? 2 : sourceSize.cx - nCurRealX - 1;
					int nYFrom = (nCurRealY > 0) ? -1 : -nCurRealY;
					int nYTo = (nCurRealY < sourceSize.cy - 2) ? 2 : sourceSize.cy - nCurRealY - 1;
					InterpolateBicubicBorder(pSrc, pDst + i*4, pKernels, nFracX, nFracY, nXFrom, nXTo, nYFrom, nYTo, nPaddedSourceWidth, nChannels, nBackColor);
				}
			} else {
				*((uint32*)pDst + i) = nBackColor;
			}
			nCurX += nIncrementX1;
			nCurY += nIncrementY1;
		}
		pDst += targetSize.cx*4;
		nX += nIncrementX2;
		nY += nIncrementY2;
	}
	delete[] pKernels;
	return pTargetPixels;
}

void* CBasicProcessing::RotateHQ(CPoint targetOffset, CSize targetSize, double dRotation, CSize sourceSize, const void* pSourcePixels, int nChannels, COLORREF backColor) {
	 if (pSourcePixels == NULL || (nChannels != 3 && nChannels != 4)) {
		return NULL;
	}

	uint8* pTargetPixels = new(std::nothrow) uint8[targetSize.cx * 4 * targetSize.cy];
	if (pTargetPixels == NULL) return NULL;

	CProcessingThreadPool& threadPool = CProcessingThreadPool::This();
	CRequestRotate request(pSourcePixels, targetOffset, targetSize, dRotation, sourceSize, pTargetPixels, nChannels, backColor);
	bool bSuccess = threadPool.Process(&request);

	return bSuccess ? pTargetPixels : NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// High quality trapezoid correction using bicubic sampling
/////////////////////////////////////////////////////////////////////////////////////////////

static inline void Get4Pixels16(const uint16* pSrc, uint16 dest[4], int nFrom, int nTo, uint32 nFill) {
	int nC = -3;
	for (int i = 0; i < 4; i++) {
		dest[i] = (i >= nFrom + 1 && i <= nTo + 1) ? pSrc[nC] : (uint16)nFill;
		nC += 3;
	}
}

// Bicubic resampling of one pixel at pSource (in 3 or 4 channel format) into one destination pixel in 32 bit format
// Interpolates only in x-direction
// The fractional part of the pixel position is given in .16 fixed point format
// pKernels contains NUM_KERNELS_BICUBIC precalculated bicubic filter kernels each of lenght 4, applied with offset -1 to the source pixels
static void InterpolateBicubicX(const uint16* pSource, uint8* pDest, int16* pKernels, int32 nFracX) {
	int16* pKernelX = &(pKernels[4*(nFracX >> (16 - NUM_KERNELS_LOG2))]);
	for (int i = 0; i < 3; i++) {
		int32 nSum = pSource[-3] * pKernelX[0] + pSource[0] * pKernelX[1] + pSource[3] * pKernelX[2] + pSource[6] * pKernelX[3] + FP_HALF;
		nSum = nSum >> 22;
		*pDest++ = min(255, max(0, nSum));
		pSource++;
	}
	*pDest = 0XFF;
}

// Same as method above but with border handling, assuming that the filter kernels are only evaluated from nXFrom to nXTo
// Pixels that are outside this range are assumed to have value nBackColor
static void InterpolateBicubicBorderX(const uint16* pSource, uint8* pDest, int16* pKernels, 
									  int32 nFracX, int nXFrom, int nXTo, uint32 nBackColor) {
	int16* pKernelX = &(pKernels[4*(nFracX >> (16 - NUM_KERNELS_LOG2))]);
	uint32 nBackColorShifted = nBackColor;
	for (int i = 0; i < 3; i++) {
		uint32 nFill = (nBackColorShifted & 0xFF) << 8;
		nBackColorShifted = nBackColorShifted >> 8;

		uint16 pixels[4];
		Get4Pixels16(pSource, pixels, nXFrom, nXTo, nFill); 
		int32 nSum = pixels[0] * pKernelX[0] + pixels[1] * pKernelX[1] + pixels[2] * pKernelX[2] + pixels[3] * pKernelX[3] + FP_HALF;
		nSum = nSum >> 22;

		*pDest++ = min(255, max(0, nSum));
		pSource++;
	}
	*pDest = 0xFF;
}

// Bicubic interpolation in y of one line in the image
static void InterpolateBicubicY(const uint8* pSourcePixels, int nChannelsSource, int nPaddedSourceWidth, uint16* pTarget, int nPixelsPerLine, 
								const int16* pKernels, 
								int nCurY, int nCurYFrac, int nSizeY) {
	if (nCurY > 0 && nCurY < nSizeY - 2) {
		const int16* pKernelY = &(pKernels[4*(nCurYFrac >> (16 - NUM_KERNELS_LOG2))]);
		int nPaddedSourceWidth2 = nPaddedSourceWidth * 2;
		for (int i = 0; i < nPixelsPerLine; i++) {
			for (int c = 0; c < 3; c++) {
				int32 nSum = pSourcePixels[-nPaddedSourceWidth + c] * pKernelY[0] + pSourcePixels[c] * pKernelY[1] + 
					pSourcePixels[nPaddedSourceWidth + c] * pKernelY[2] + pSourcePixels[nPaddedSourceWidth2 + c] * pKernelY[3] + FP_HALF;
				nSum = nSum >> 6;
				*pTarget++ = min(65535, max(0, nSum));
			}
			pSourcePixels += nChannelsSource;
		}
	} else {
		// border handling...there is none ;-) just copy the line and hope that nobody will realize it
		for (int i = 0; i < nPixelsPerLine; i++) {
			for (int c = 0; c < 3; c++) {
				*pTarget++ = (uint16)(pSourcePixels[c] << 8);
			}
			pSourcePixels += nChannelsSource;
		}
	}
}

void* CBasicProcessing::TrapezoidHQ_Core(CPoint targetOffset, CSize targetSize, const CTrapezoid& trapezoid, CSize sourceSize, 
										 const void* pSourcePixels, void* pTargetPixels, int nChannels, COLORREF backColor) {

	float fTx1 = (float)trapezoid.x1s;
	float fTx2 = (float)trapezoid.x1e;
	float fIncrementTx1 = ((float)(trapezoid.x2s - trapezoid.x1s))/trapezoid.Height();
	float fIncrementTx2 = ((float)(trapezoid.x2e - trapezoid.x1e))/trapezoid.Height();

	uint32 nBackColor = (GetRValue(backColor) << 16) + (GetGValue(backColor) << 8) + GetBValue(backColor) + ALPHA_OPAQUE;
	int nPaddedSourceWidth = Helpers::DoPadding(sourceSize.cx * nChannels, 4);
	uint8* pDst = (uint8*)pTargetPixels;
	fTx1 = fTx1 + targetOffset.y*fIncrementTx1;
	fTx2 = fTx2 + targetOffset.y*fIncrementTx2;
	int nSourceSizeXFP16 = (sourceSize.cx - 1) << 16;

	int* pTableY = CalculateTrapezoidYIntersectionTable(trapezoid, targetSize.cy, sourceSize.cy, trapezoid.Height() + 1, targetOffset.y);
	uint16* pLine = new uint16[sourceSize.cx * 3];
	
	int16* pKernels = new int16[NUM_KERNELS_BICUBIC * 4];
	CResizeFilter::GetBicubicFilterKernels(NUM_KERNELS_BICUBIC, pKernels);

	for (int j = 0; j < targetSize.cy; j++) {
		float fTxDiffInv = 1.0f/(fTx2 - fTx1);
		int nIncrementX = (int)(nSourceSizeXFP16 * fTxDiffInv);
		int nStartX = (int)(nSourceSizeXFP16 * (targetOffset.x - fTx1) * fTxDiffInv);
		int nCurX = nStartX;
		int nCurY = pTableY[j] >> 16;
		int nCurYFrac = pTableY[j] & 0xFFFF;
		InterpolateBicubicY((uint8*)pSourcePixels + nPaddedSourceWidth * nCurY, nChannels, nPaddedSourceWidth, pLine, sourceSize.cx,
			pKernels, nCurY, nCurYFrac, trapezoid.Height() + 1);
		for (int i = 0; i < targetSize.cx; i++) {
			int nCurXInt = nCurX >> 16;
			int nCurXFrac = nCurX & 0xFFFF;
			if (nCurXInt >= -1 && nCurXInt <= sourceSize.cx) {
				const uint16* pSourceLineStart = pLine + nCurXInt*3;
				if (nCurXInt > 0 && nCurXInt < sourceSize.cx - 2) {
					InterpolateBicubicX(pSourceLineStart, pDst + i*4, pKernels, nCurXFrac);
				} else {
					int nXFrom = (nCurXInt > 0) ? -1 : -nCurXInt;
					int nXTo = (nCurXInt < sourceSize.cx - 2) ? 2 : sourceSize.cx - nCurXInt - 1;
					InterpolateBicubicBorderX(pSourceLineStart, pDst + i*4, pKernels, nCurXFrac, nXFrom, nXTo, nBackColor);
				}
			} else {
				*((uint32*)pDst + i) = nBackColor;
			}

			nCurX += nIncrementX;
		}

		pDst += targetSize.cx*4;
		fTx1 += fIncrementTx1;
		fTx2 += fIncrementTx2;
	}

	delete[] pTableY;
	delete[] pLine;

	return pTargetPixels;
}

void* CBasicProcessing::TrapezoidHQ(CPoint targetOffset, CSize targetSize, const CTrapezoid& trapezoid, CSize sourceSize, 
									const void* pSourcePixels, int nChannels, COLORREF backColor) {
	 if (pSourcePixels == NULL || (nChannels != 3 && nChannels != 4)) {
		return NULL;
	}

	uint8* pTargetPixels = new(std::nothrow) uint8[targetSize.cx * 4 * targetSize.cy];
	if (pTargetPixels == NULL) return NULL;

	CProcessingThreadPool& threadPool = CProcessingThreadPool::This();
	CRequestTrapezoid request(pSourcePixels, targetOffset, targetSize, trapezoid, sourceSize, pTargetPixels, nChannels, backColor);
	bool bSuccess = threadPool.Process(&request);

	return bSuccess ? pTargetPixels : NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Helper methods for high quality resizing (C++ implementation)
/////////////////////////////////////////////////////////////////////////////////////////////

// Apply filtering in x-direction and rotate
// nSourceWidth: Width of the image in pSource in pixels
// nTargetWidth: Target width of the image after filtering.
// nHeight: Height of the target image in pixels
// nSourceBytesPerPixel: 3 or 4, the target will always have 4 bytes per pixel
// nStartX_FP: 16.16 fixed point number, start of filtering in source image
// nStartY: First row to filter in source image (Offset, not in FP format)
// nIncrementX_FP: 16.16 fixed point number, increment in x-direction in source image
// filter: Filter to apply (in x direction)
// nFilterOffset: Offset into filter (to filter.Indices array)
// pSource: Source image
// Returns the filtered image of size(nHeight, nTargetWidth)
static uint8* ApplyFilter(int nSourceWidth, int nTargetWidth, int nHeight,
						  int nSourceBytesPerPixel,
						  int nStartX_FP, int nStartY, int nIncrementX_FP,
						  const FilterKernelBlock& filter,
						  int nFilterOffset,
						  const uint8* pSource) {

	uint8* pTarget = new(std::nothrow) uint8[nTargetWidth*4*nHeight];
	if (pTarget == NULL) return NULL;

	// width of new image is (after rotation) : nHeight
	// height of new image is (after rotation) : nTargetWidth
	
	int nPaddedSourceWidth = Helpers::DoPadding(nSourceWidth * nSourceBytesPerPixel, 4);
	const uint8* pSourcePixelLine = NULL;
	uint8* pTargetPixelLine = NULL;
	for (int j = 0; j < nHeight; j++) {
		pSourcePixelLine = ((uint8*) pSource) + nPaddedSourceWidth * (j + nStartY);
		pTargetPixelLine = pTarget + 4*j;
		uint8* pTargetPixel = pTargetPixelLine;
		uint32 nX = nStartX_FP;
		for (int i = 0; i < nTargetWidth; i++) {
			uint32 nXSourceInt = nX >> 16;
			FilterKernel* pKernel = filter.Indices[i + nFilterOffset];
			const uint8* pSourcePixel = pSourcePixelLine + nSourceBytesPerPixel*(nXSourceInt - pKernel->FilterOffset);
			int nPixelValue1 = 0;
			int nPixelValue2 = 0;
			int nPixelValue3 = 0;
			for (int n = 0; n < pKernel->FilterLen; n++) {
				nPixelValue1 += pKernel->Kernel[n] * pSourcePixel[0];
				nPixelValue2 += pKernel->Kernel[n] * pSourcePixel[1];
				nPixelValue3 += pKernel->Kernel[n] * pSourcePixel[2];
				pSourcePixel += nSourceBytesPerPixel;
			}
			nPixelValue1 = nPixelValue1 >> 14;
			nPixelValue2 = nPixelValue2 >> 14;
			nPixelValue3 = nPixelValue3 >> 14;

			*pTargetPixel++ = (uint8)max(0, min(255, nPixelValue1));
			*pTargetPixel++ = (uint8)max(0, min(255, nPixelValue2));
			*pTargetPixel++ = (uint8)max(0, min(255, nPixelValue3));
			*pTargetPixel++ = 0xFF;
			// rotate: go to next row in target - width of target is nHeight
			pTargetPixel = pTargetPixel - 4 + nHeight*4;
			nX += nIncrementX_FP;
		}
	}

	return pTarget;
}

// Apply filtering in x-direction and rotate, 16 bpp 1 channel image
// NOTE: No saturation is done, only valid for filters that do not require saturation.
// The filter is applied to the pixel centers, therefore no resize filters are possible.
// nSourceWidth : Width of source in pixels
// nTargetWidth: Width of target image in pixels (note that target image is rotated compared to source)
// nStartX, nStartY: Start position for filtering in source image
// nRunX, nRunY: Number of pixels in x and y to filter
// filter: Filter to apply (in x direction)
// pSource: Source image
// pTarget: Target image, width must be nTargetWidth
static void ApplyFilter1C16bpp(int nSourceWidth, int nTargetWidth,
							   int nStartX, int nStartY,
							   int nRunX, int nRunY,
							   const FilterKernelBlock& filter,
						       const int16* pSource, int16* pTarget) {

	for (int j = 0; j < nRunY; j++) {
		const int16* pSourcePixelLine = pSource + nStartX + nSourceWidth * (j + nStartY);
		int16* pTargetPixelLine = pTarget + j;
		int16* pTargetPixel = pTargetPixelLine;
		for (int i = 0; i < nRunX; i++) {
			FilterKernel* pKernel = filter.Indices[i + nStartX];
			const int16* pSourcePixel = pSourcePixelLine + i - pKernel->FilterOffset;
			int nPixelValue = 0;
			for (int n = 0; n < pKernel->FilterLen; n++) {
				nPixelValue += pKernel->Kernel[n] * pSourcePixel[n];
			}
			nPixelValue = nPixelValue >> 14;

			*pTargetPixel = nPixelValue;
			// rotate: go to next row in target
			pTargetPixel += nTargetWidth;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Gauss filter (C++ implementation)
/////////////////////////////////////////////////////////////////////////////////////////////

int16* CBasicProcessing::GaussFilter16bpp1Channel_Core(CSize fullSize, CPoint offset, CSize rect, int nTargetWidth, double dRadius, 
													  const int16* pSourcePixels, int16* pTargetPixels) {
	CGaussFilter filterX(fullSize.cx, dRadius);
	ApplyFilter1C16bpp(fullSize.cx, nTargetWidth, offset.x, offset.y, rect.cx, rect.cy, filterX.GetFilterKernels(), pSourcePixels, pTargetPixels);
	return pTargetPixels;
}

int16* CBasicProcessing::GaussFilter16bpp1Channel(CSize fullSize, CPoint offset, CSize rect, double dRadius, const int16* pPixels) {
	if (pPixels == NULL) {
		return NULL;
	}

	// Gauss filter x-direction
	CProcessingThreadPool& threadPool = CProcessingThreadPool::This();
	int16* pIntermediate = new(std::nothrow) int16[rect.cx * rect.cy];
	if (pIntermediate == NULL) return NULL;
	CRequestGauss requestX(pPixels, fullSize, offset, rect, dRadius, pIntermediate);
	if (!threadPool.Process(&requestX)) {
		delete[] pIntermediate;
		return NULL;
	}

	// Gauss filter y-direction
	int16* pTargetPixels = new(std::nothrow) int16[rect.cx * rect.cy];
	if (pTargetPixels == NULL) {
		delete[] pIntermediate;
		return NULL;
	}
	CRequestGauss requestY(pIntermediate, CSize(rect.cy, rect.cx), CPoint(0, 0), CSize(rect.cy, rect.cx), dRadius, pTargetPixels);
	bool bSuccess = threadPool.Process(&requestY);
	delete[] pIntermediate;

	return bSuccess ? pTargetPixels : NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Bicubic resize (C++ implementation)
/////////////////////////////////////////////////////////////////////////////////////////////

void* CBasicProcessing::SampleUp_HQ(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize,
									CSize sourceSize, const void* pIJLPixels, int nChannels) {

	// Resizing consists of resize in x direction followed by resize in y direction.
	// To simplify implementation, the method performs a 90 degree rotation/flip while resizing,
	// thus enabling to use the same loop on the rows for both resize directions.
	// The image is correctly orientated again after two rotation/flip operations!

	if (pIJLPixels == NULL || fullTargetSize.cx < 2 || fullTargetSize.cy < 2 || clippedTargetSize.cx <= 0 || clippedTargetSize.cy <= 0) {
		return NULL;
	}
	int nTargetWidth = clippedTargetSize.cx;
	int nTargetHeight = clippedTargetSize.cy;
	int nSourceWidth = sourceSize.cx;
	int nSourceHeight = sourceSize.cy;

	uint32 nIncrementX = (uint32)(65536*(uint32)(nSourceWidth - 1)/(fullTargetSize.cx - 1));
	uint32 nIncrementY = (uint32)(65536*(uint32)(nSourceHeight - 1)/(fullTargetSize.cy - 1));

	// Caution: This code assumes a upsampling filter kernel of length 4, with a filter offset of 1
	int nFirstY = max(0, int((uint32)(nIncrementY*fullTargetOffset.y) >> 16) - 1);
	int nLastY = min(sourceSize.cy - 1, int(((uint32)(nIncrementY*(fullTargetOffset.y + nTargetHeight - 1)) >> 16) + 2));
	int nTempTargetWidth = nLastY - nFirstY + 1;
	int nTempTargetHeight = nTargetWidth;
	int nFilterOffsetX = fullTargetOffset.x;
	int nFilterOffsetY = fullTargetOffset.y;
	int nStartX = nIncrementX*fullTargetOffset.x;
	int nStartY = nIncrementY*fullTargetOffset.y - 65536*nFirstY;

	CResizeFilter filterX(nSourceWidth, fullTargetSize.cx, 0.0, Filter_Upsampling_Bicubic, false);
	const FilterKernelBlock& kernelsX = filterX.GetFilterKernels();

	uint8* pTemp = ApplyFilter(nSourceWidth, nTempTargetHeight, nTempTargetWidth,
		nChannels, nStartX, nFirstY, nIncrementX,
		kernelsX, nFilterOffsetX, (const uint8*) pIJLPixels);
	if (pTemp == NULL) return NULL;

	CResizeFilter filterY(nSourceHeight, fullTargetSize.cy, 0.0, Filter_Upsampling_Bicubic, false);
	const FilterKernelBlock& kernelsY = filterY.GetFilterKernels();

	uint8* pDIB = ApplyFilter(nTempTargetWidth, nTargetHeight, nTargetWidth,
			4, nStartY, 0, nIncrementY,
			kernelsY, nFilterOffsetY, pTemp);

	delete[] pTemp;

	return pDIB;
}


/////////////////////////////////////////////////////////////////////////////////////////////
// High quality downsampling (C++ implementation)
/////////////////////////////////////////////////////////////////////////////////////////////

void* CBasicProcessing::SampleDown_HQ(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize,
									  CSize sourceSize, const void* pIJLPixels, int nChannels, double dSharpen, 
									  EFilterType eFilter) {
	// Resizing consists of resize in x direction followed by resize in y direction.
	// To simplify implementation, the method performs a 90 degree rotation/flip while resizing,
	// thus enabling to use the same loop on the rows for both resize directions.
	// The image is correctly orientated again after two rotation/flip operations!

	if (pIJLPixels == NULL || clippedTargetSize.cx <= 0 || clippedTargetSize.cy <= 0) {
		return NULL;
	}
	CResizeFilter filterX(sourceSize.cx, fullTargetSize.cx, dSharpen, eFilter, false);
	const FilterKernelBlock& kernelsX = filterX.GetFilterKernels();
	CResizeFilter filterY(sourceSize.cy, fullTargetSize.cy, dSharpen, eFilter, false);
	const FilterKernelBlock& kernelsY = filterY.GetFilterKernels();

	uint32 nIncrementX = (uint32)(sourceSize.cx << 16)/fullTargetSize.cx + 1;
	uint32 nIncrementY = (uint32)(sourceSize.cy << 16)/fullTargetSize.cy + 1;

	int nIncOffsetX = (nIncrementX - 65536) >> 1;
	int nIncOffsetY = (nIncrementY - 65536) >> 1;
	int nFirstY = (uint32)(nIncOffsetY + nIncrementY*fullTargetOffset.y) >> 16;
	nFirstY = max(0, nFirstY - kernelsY.Indices[fullTargetOffset.y]->FilterOffset);
	int nLastY  = (uint32)(nIncOffsetY + nIncrementY*(fullTargetOffset.y + clippedTargetSize.cy - 1)) >> 16;
	FilterKernel* pLastYFilter = kernelsY.Indices[fullTargetOffset.y + clippedTargetSize.cy - 1];
	nLastY  = min(sourceSize.cy - 1, nLastY - pLastYFilter->FilterOffset + pLastYFilter->FilterLen - 1);
	int nTempTargetWidth = nLastY - nFirstY + 1;
	int nTempTargetHeight = clippedTargetSize.cx;
	int nFilterOffsetX = fullTargetOffset.x;
	int nFilterOffsetY = fullTargetOffset.y;
	int nStartX = nIncOffsetX + nIncrementX*fullTargetOffset.x;
	int nStartY = nIncOffsetY + nIncrementY*fullTargetOffset.y - 65536*nFirstY;

	uint8* pTemp = ApplyFilter(sourceSize.cx, nTempTargetHeight, nTempTargetWidth,
		nChannels, nStartX, nFirstY, nIncrementX,
		kernelsX, nFilterOffsetX, (const uint8*) pIJLPixels);
	if (pTemp == NULL) return NULL;

	uint8* pDIB = ApplyFilter(nTempTargetWidth, clippedTargetSize.cy, clippedTargetSize.cx,
			4, nStartY, 0, nIncrementY,
			kernelsY, nFilterOffsetY, pTemp);

	delete[] pTemp;

	return pDIB;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// High quality downsampling (Helpers for SSE and MMX implementation)
/////////////////////////////////////////////////////////////////////////////////////////////

// Rotate a block in a CXMMImage. Blockwise rotation is needed because with normal
// rotation, trashing occurs, making rotation a very slow operation.
// The input format is what the ResizeYCore() method outputs:
// RRRRRRRRGGGGGGGGBBBBBBBB... (blocks of eight pixels of a channel).
// After rotation, the format is line interleaved again:
// RRRRRRRRRRR...
// GGGGGGGGGGG...
// BBBBBBBBBBB...
static void RotateBlock(const int16* pSrc, int16* pTgt, int nWidth, int nHeight,
						int nXStart, int nYStart, int nBlockWidth, int nBlockHeight) {
	int nPaddedWidth = Helpers::DoPadding(nWidth, 8);
	int nPaddedHeight = Helpers::DoPadding(nHeight, 8);
	int nIncTargetChannel = nPaddedHeight;
	int nIncTargetLine = nIncTargetChannel * 3;
	int nIncSource = nPaddedWidth * 3 - nBlockWidth * 3;
	const int16* pSource = pSrc + nPaddedWidth * 3 * nYStart + nXStart * 3;
	int16* pTarget = pTgt + nPaddedHeight * 3 * nXStart + nYStart;
	int16* pStartYPtr = pTarget;
	int nLoopX = Helpers::DoPadding(nBlockWidth, 8) / 8;

	for (int i = 0; i < nBlockHeight; i++) {
		for (int j = 0; j < nLoopX; j++) {
			int16* pSaveTarget = pTarget;
			*pTarget = *pSource++; pTarget += nIncTargetLine;
			*pTarget = *pSource++; pTarget += nIncTargetLine;
			*pTarget = *pSource++; pTarget += nIncTargetLine;
			*pTarget = *pSource++; pTarget += nIncTargetLine;
			*pTarget = *pSource++; pTarget += nIncTargetLine;
			*pTarget = *pSource++; pTarget += nIncTargetLine;
			*pTarget = *pSource++; pTarget += nIncTargetLine;
			*pTarget = *pSource++;
			pSaveTarget += nIncTargetChannel;
			pTarget = pSaveTarget;
			*pTarget = *pSource++; pTarget += nIncTargetLine;
			*pTarget = *pSource++; pTarget += nIncTargetLine;
			*pTarget = *pSource++; pTarget += nIncTargetLine;
			*pTarget = *pSource++; pTarget += nIncTargetLine;
			*pTarget = *pSource++; pTarget += nIncTargetLine;
			*pTarget = *pSource++; pTarget += nIncTargetLine;
			*pTarget = *pSource++; pTarget += nIncTargetLine;
			*pTarget = *pSource++;
			pSaveTarget += nIncTargetChannel;
			pTarget = pSaveTarget;
			*pTarget = *pSource++; pTarget += nIncTargetLine;
			*pTarget = *pSource++; pTarget += nIncTargetLine;
			*pTarget = *pSource++; pTarget += nIncTargetLine;
			*pTarget = *pSource++; pTarget += nIncTargetLine;
			*pTarget = *pSource++; pTarget += nIncTargetLine;
			*pTarget = *pSource++; pTarget += nIncTargetLine;
			*pTarget = *pSource++; pTarget += nIncTargetLine;
			*pTarget = *pSource++; 
			pTarget += nIncTargetChannel;
		}
		pStartYPtr++;
		pTarget = pStartYPtr;
		pSource += nIncSource;
	}
}

// Same as above, directly rotates into a 32 bpp DIB
static void RotateBlockToDIB(const int16* pSrc, uint8* pTgt, int nWidth, int nHeight,
							 int nXStart, int nYStart, int nBlockWidth, int nBlockHeight) {
	int nPaddedWidth = Helpers::DoPadding(nWidth, 8);
	int nPaddedHeight = Helpers::DoPadding(nHeight, 8);
	int nIncTargetLine = nHeight * 4;
	int nIncSource = nPaddedWidth * 3 - nBlockWidth * 3;
	const int16* pSource = pSrc + nPaddedWidth * 3 * nYStart + nXStart * 3;
	uint8* pTarget = pTgt + nHeight * 4 * nXStart + nYStart * 4;
	uint8* pStartYPtr = pTarget;
	int nLoopX = Helpers::DoPadding(nBlockWidth, 8) / 8;

	for (int i = 0; i < nBlockHeight; i++) {
		for (int j = 0; j < nLoopX; j++) {
			uint8* pSaveTarget = pTarget;
			*((uint32*)pTarget) = ALPHA_OPAQUE; *pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*((uint32*)pTarget) = ALPHA_OPAQUE; *pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*((uint32*)pTarget) = ALPHA_OPAQUE; *pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*((uint32*)pTarget) = ALPHA_OPAQUE; *pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*((uint32*)pTarget) = ALPHA_OPAQUE; *pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*((uint32*)pTarget) = ALPHA_OPAQUE; *pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*((uint32*)pTarget) = ALPHA_OPAQUE; *pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*((uint32*)pTarget) = ALPHA_OPAQUE; *pTarget = *pSource++ >> 6;
			pSaveTarget++;
			pTarget = pSaveTarget;
			*pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*pTarget = *pSource++ >> 6;
			pSaveTarget++;
			pTarget = pSaveTarget;
			*pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			pTarget -= 2;
		}
		pStartYPtr += 4;
		pTarget = pStartYPtr;
		pSource += nIncSource;
	}
}

// RotateFlip the source image by 90 deg and return rotated image
// RotateFlip is invertible: img = RotateFlip(RotateFlip(img))
static CXMMImage* Rotate(const CXMMImage* pSourceImg) {
	CXMMImage* targetImage = new CXMMImage(pSourceImg->GetHeight(), pSourceImg->GetWidth(), true);
	if (targetImage->AlignedPtr() == NULL) {
		delete targetImage;
		return NULL;
	}
	const int16* pSource = (const int16*) pSourceImg->AlignedPtr();
	int16* pTarget = (int16*) targetImage->AlignedPtr();

	const int cnBlockSize = 32;
	int nX = 0, nY = 0;
	while (nY < pSourceImg->GetHeight()) {
		nX = 0;
		while (nX < pSourceImg->GetWidth()) {
			RotateBlock(pSource, pTarget, pSourceImg->GetWidth(), pSourceImg->GetHeight(),
				nX, nY, 
				min(cnBlockSize, pSourceImg->GetPaddedWidth() - nX), // !! here we need to use the padded width
				min(cnBlockSize, pSourceImg->GetHeight() - nY));
			nX += cnBlockSize;
		}
		nY += cnBlockSize;
	}

	return targetImage;
}

// RotateFlip the source image by 90 deg and return rotated image as 32 bpp DIB
static void* RotateToDIB(const CXMMImage* pSourceImg, uint8* pTarget = NULL) {

	const int16* pSource = (const int16*) pSourceImg->AlignedPtr();
	if (pTarget == NULL) {
		pTarget = new(std::nothrow) uint8[pSourceImg->GetHeight() * 4 * Helpers::DoPadding(pSourceImg->GetWidth(), 8)];
		if (pTarget == NULL) return NULL;
	}

	const int cnBlockSize = 32;
	int nX = 0, nY = 0;
	while (nY < pSourceImg->GetHeight()) {
		nX = 0;
		while (nX < pSourceImg->GetWidth()) {
			RotateBlockToDIB(pSource, pTarget, pSourceImg->GetWidth(), pSourceImg->GetHeight(),
				nX, nY, 
				min(cnBlockSize, pSourceImg->GetPaddedWidth() - nX),  // !! here we need to use the padded width
				min(cnBlockSize, pSourceImg->GetHeight() - nY));

			nX += cnBlockSize;
		}
		nY += cnBlockSize;
	}

	return pTarget;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// High quality filtering (SSE implementation)
/////////////////////////////////////////////////////////////////////////////////////////////

// Apply filter in y direction in SSE
// nSourceHeight: Height of source image, only here to match interface of C++ implementation
// nTargetHeight: Height of target image after resampling
// nWidth: Width of source image
// nStartY_FP: 16.16 fixed point number, denoting the y-start subpixel coordinate
// nStartX: Start of filtering in x-direction (not an FP number)
// nIncrementY_FP: 16.16 fixed point number, denoting the increment for the y coordinates
// filter: filter to apply
// nFilterOffset: Offset into filter (to filter.Indices array)
// pSourceImg: Source image
// The filter is applied to 'nTargetHeight' rows of the input image at positions
// nStartY_FP, nStartY_FP+nIncrementY_FP, ... 
// In X direction, the filter is applied starting from column /nStartX\ to (including) \nStartX+nWidth-1/
// where the /\ symbol denotes padding to lower 8 pixel boundary and \/ padding to the upper 8 pixel
// boundary.
static CXMMImage* ApplyFilter_SSE(int nSourceHeight, int nTargetHeight, int nWidth,
								  int nStartY_FP, int nStartX, int nIncrementY_FP,
								  const XMMFilterKernelBlock& filter,
								  int nFilterOffset, const CXMMImage* pSourceImg) {
	
	int nStartXAligned = nStartX & ~7;
	int nEndXAligned = (nStartX + nWidth + 7) & ~7;
	CXMMImage* tempImage = new CXMMImage(nEndXAligned - nStartXAligned, nTargetHeight);
	if (tempImage->AlignedPtr() == NULL) {
		delete tempImage;
		return NULL;
	}
	int16* pDest = (int16*) tempImage->AlignedPtr();

	int nCurY = nStartY_FP;
	int nChannelLenBytes = pSourceImg->GetPaddedWidth()*2;
	int nRowLenBytes = nChannelLenBytes*3;
	int nLoopY = 0;
	int nLoopXStart = (nEndXAligned - nStartXAligned) >> 3;
	int nLoopX = nLoopXStart;
	const uint8* pSourceStart = (const uint8*)pSourceImg->AlignedPtr() + nStartXAligned*2;
	void* pKernelIndexStart = &(filter.Indices[0]);
	int   nFilterLen;
	void* pFilterStart;
	DECLARE_ALIGNED_DQWORD(pFPONE, 16383);

	_asm {
		mov ebx, pFPONE
		movdqa xmm0, [ebx] // this value remains in mm0, it represents 1.0 in fixed point format
		mov edi, pDest
		
startYLoop:
		mov eax, nCurY
		shr eax, 16        // integer part of Y

		mov ebx, nLoopY
		add ebx, nFilterOffset
		mov ecx, pKernelIndexStart
		mov ecx, [ecx + 4*ebx] // now the address of the XMM filter kernel to use is in ecx
		mov ebx, [ecx] // filter lenght is in ebx
		mov edx, [ecx + 4] // filter offset is in edx
		add ecx, 16 // ecx now on first filter element

		sub eax, edx      // subtract the filter offset
		mul nRowLenBytes
		add eax, pSourceStart // source start in eax

		// For the row loop, the following register allocations are done.
		// These registers are not touched in the pixel loop.
		// EDX: Lenght of one row per channel, fixed
		// ESI: Source pixel ptr for row loop
		// EDI: Target pixel ptr for row loop
		mov edx, nChannelLenBytes
		mov esi, eax
		mov nFilterLen, ebx
		mov pFilterStart, ecx

startXLoop:
		// For the pixel loop, the following register allocations are done.
		// EAX: Source pixel ptr, incremented
		// EBX: Filter length, decremented
		// ECX: Filter kernel ptr, incremented

		// pixel loop init, eight pixels are calculated with SSE
		pxor   xmm4, xmm4   // filter sum R
		pxor   xmm5, xmm5   // filter sum G
		pxor   xmm6, xmm6   // filter sum B

FilterKernelLoop:
		movdqa xmm7, [ecx]  // load kernel element
		movdqa xmm2, [eax]  // the pixel data RED channel
		paddw  xmm2, xmm2
		pmulhw xmm2, xmm7
		paddw  xmm2, xmm2
		paddsw xmm4, xmm2
		add    eax, edx
		movdqa xmm3, [eax]  // the pixel data GREEN channel
		paddw  xmm3, xmm3
		pmulhw xmm3, xmm7
		paddw  xmm3, xmm3
		paddsw xmm5, xmm3
		add    eax, edx
		movdqa xmm2, [eax]  // the pixel data BLUE channel
		paddw  xmm2, xmm2
		pmulhw xmm2, xmm7
		paddw  xmm2, xmm2
		paddsw xmm6, xmm2
		add    eax, edx
		add    ecx, 16      // next kernel element
		sub    ebx, 1
		jnz    FilterKernelLoop

		// min-max to 0, 16383
		pminsw xmm4, xmm0
		pminsw xmm5, xmm0
		pminsw xmm6, xmm0
		pxor   xmm1, xmm1
		pmaxsw xmm4, xmm1
		pmaxsw xmm5, xmm1
		pmaxsw xmm6, xmm1

		// store result in blocks
		movdqa [edi], xmm4
		movdqa [edi + 16], xmm5
		movdqa [edi + 32], xmm6
		add   edi, 48
		add   esi, 16

		mov eax, esi  // prepare for next 8 pixels of row
		mov ebx, nFilterLen
		mov ecx, pFilterStart

		dec nLoopX
		jnz startXLoop

		mov eax, nLoopXStart
		mov nLoopX, eax
		mov eax, nCurY
		add eax, nIncrementY_FP
		mov nCurY, eax
		mov ecx, nLoopY
		inc ecx
		mov nLoopY, ecx
		cmp ecx, nTargetHeight
		jl  startYLoop
	}

	return tempImage;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// High quality filtering (MMX implementation)
/////////////////////////////////////////////////////////////////////////////////////////////

// Apply filter in y direction in MMX
static CXMMImage* ApplyFilter_MMX(int nSourceHeight, int nTargetHeight, int nWidth,
								  int nStartY_FP, int nStartX, int nIncrementY_FP,
								  const XMMFilterKernelBlock& filter,
								  int nFilterOffset, const CXMMImage* pSourceImg) {

	int nStartXAligned = nStartX & ~7;
	int nEndXAligned = (nStartX + nWidth + 7) & ~7;
	CXMMImage* tempImage = new CXMMImage(nEndXAligned - nStartXAligned, nTargetHeight);
	if (tempImage->AlignedPtr() == NULL) {
		delete tempImage;
		return NULL;
	}
	int16* pDest = (int16*) tempImage->AlignedPtr();

	int nCurY = nStartY_FP;
	int nChannelLenBytes = pSourceImg->GetPaddedWidth()*2;
	int nRowLenBytes = nChannelLenBytes*3;
	int nLoopY = 0;
	int nLoopXStart = (nEndXAligned - nStartXAligned)/8;
	int nLoopX = nLoopXStart;
	const uint8* pSourceStart = (const uint8*)pSourceImg->AlignedPtr() + nStartXAligned*2;
	void* pKernelIndexStart = &(filter.Indices[0]);
	int   nFilterLen;
	void* pFilterStart;
	int nSaveESI;
	DECLARE_ALIGNED_DQWORD(pFPONE, 16383);

	_asm {
		mov ebx, pFPONE
		movq mm0, [ebx] // this value remains in mm0, it represents 1.0 in fixed point format
		mov edi, pDest
		
startYLoop:
		mov eax, nCurY
		shr eax, 16        // integer part of Y

		mov ebx, nLoopY
		add ebx, nFilterOffset
		mov ecx, pKernelIndexStart
		mov ecx, [ecx + 4*ebx] // now the address of the XMM filter kernel to use is in ecx
		mov ebx, [ecx] // filter lenght is in ebx
		mov edx, [ecx + 4] // filter offset is in edx
		add ecx, 16  // ecx now on first filter element

		sub eax, edx      // subtract the filter offset
		mul nRowLenBytes
		add eax, pSourceStart // source start in eax

		// For the row loop, the following register allocations are done.
		// These registers are not touched in the pixel loop.
		// EDX: Lenght of one row per channel, fixed
		// ESI: Source pixel ptr for row loop
		// EDI: Target pixel ptr for row loop
		mov edx, nChannelLenBytes
		mov esi, eax
		mov nFilterLen, ebx
		mov pFilterStart, ecx

startXLoop:
		// For the pixel loop, the following register allocations are done.
		// EAX: Source pixel ptr, incremented
		// EBX: Filter length, decremented
		// ECX: Filter kernel ptr, incremented

		mov    nSaveESI, esi
		mov    esi, 2

Loop8Pixels:
		// pixel loop init, eight pixels are calculated with SSE
		pxor   mm4, mm4   // filter sum R
		pxor   mm5, mm5   // filter sum G
		pxor   mm6, mm6   // filter sum B

FilterKernelLoop:
		movq   mm7, [ecx]  // load kernel element
		movq   mm2, [eax]  // the pixel data RED channel
		paddw  mm2, mm2
		pmulhw mm2, mm7
		paddw  mm2, mm2
		paddsw mm4, mm2
		add    eax, edx
		movq   mm3, [eax]  // the pixel data GREEN channel
		paddw  mm3, mm3
		pmulhw mm3, mm7
		paddw  mm3, mm3
		paddsw mm5, mm3
		add    eax, edx
		movq   mm2, [eax]  // the pixel data BLUE channel
		paddw  mm2, mm2
		pmulhw mm2, mm7
		paddw  mm2, mm2
		paddsw mm6, mm2
		add    eax, edx
		add    ecx, 16      // next kernel element
		sub    ebx, 1
		jnz    FilterKernelLoop

		// min-max to 0, 16383
		pminsw mm4, mm0
		pminsw mm5, mm0
		pminsw mm6, mm0
		pxor   mm1, mm1
		pmaxsw mm4, mm1
		pmaxsw mm5, mm1
		pmaxsw mm6, mm1

		// store result in blocks
		movq [edi], mm4
		movq [edi + 16], mm5
		movq [edi + 32], mm6
		add   edi, 8
		mov   eax, nSaveESI
		add   eax, 8
		mov   ebx, nFilterLen
		mov   ecx, pFilterStart

		dec   esi
		jnz   Loop8Pixels
		
		mov   esi, nSaveESI
		add   esi, 16
		add   edi, 32

		mov eax, esi  // prepare for next 8 pixels of row
		mov ebx, nFilterLen
		mov ecx, pFilterStart

		dec nLoopX
		jnz startXLoop

		mov eax, nLoopXStart
		mov nLoopX, eax
		mov eax, nCurY
		add eax, nIncrementY_FP
		mov nCurY, eax
		mov ecx, nLoopY
		inc ecx
		mov nLoopY, ecx
		cmp ecx, nTargetHeight
		jl  startYLoop
		emms
	}

	return tempImage;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// High quality downsampling (SSE/MMX implementation)
/////////////////////////////////////////////////////////////////////////////////////////////

void* CBasicProcessing::SampleDown_HQ_SSE_MMX_Core(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize,
										CSize sourceSize, const void* pIJLPixels, int nChannels, double dSharpen, 
										EFilterType eFilter, bool bSSE, uint8* pTarget) {
 	CAutoXMMFilter filterY(sourceSize.cy, fullTargetSize.cy, dSharpen, eFilter);
	const XMMFilterKernelBlock& kernelsY = filterY.Kernels();
	CAutoXMMFilter filterX(sourceSize.cx, fullTargetSize.cx, dSharpen, eFilter);
	const XMMFilterKernelBlock& kernelsX = filterX.Kernels();

	uint32 nIncrementX = (uint32)(sourceSize.cx << 16)/fullTargetSize.cx + 1;
	uint32 nIncrementY = (uint32)(sourceSize.cy << 16)/fullTargetSize.cy + 1;

	int nIncOffsetX = (nIncrementX - 65536) >> 1;
	int nIncOffsetY = (nIncrementY - 65536) >> 1;
	int nFirstX = (uint32)(nIncOffsetX + nIncrementX*fullTargetOffset.x) >> 16;
	nFirstX = max(0, nFirstX - kernelsX.Indices[fullTargetOffset.x]->FilterOffset);
	int nLastX  = (uint32)(nIncOffsetX + nIncrementX*(fullTargetOffset.x + clippedTargetSize.cx - 1)) >> 16;
	XMMFilterKernel* pLastXFilter = kernelsX.Indices[fullTargetOffset.x + clippedTargetSize.cx - 1];
	nLastX  = min(sourceSize.cx - 1, nLastX - pLastXFilter->FilterOffset + pLastXFilter->FilterLen - 1);
	int nFirstY = (uint32)(nIncOffsetY + nIncrementY*fullTargetOffset.y) >> 16;
	nFirstY = max(0, nFirstY - kernelsY.Indices[fullTargetOffset.y]->FilterOffset);
	int nLastY  = (uint32)(nIncOffsetY + nIncrementY*(fullTargetOffset.y + clippedTargetSize.cy - 1)) >> 16;
	XMMFilterKernel* pLastYFilter = kernelsY.Indices[fullTargetOffset.y + clippedTargetSize.cy - 1];
	nLastY  = min(sourceSize.cy - 1, nLastY - pLastYFilter->FilterOffset + pLastYFilter->FilterLen - 1);
	int nFilterOffsetX = fullTargetOffset.x;
	int nFilterOffsetY = fullTargetOffset.y;
	int nStartX = nIncOffsetX + nIncrementX*fullTargetOffset.x - 65536*nFirstX;
	int nStartY = nIncOffsetY + nIncrementY*fullTargetOffset.y - 65536*nFirstY;

	// Resize Y
	double t1 = Helpers::GetExactTickCount();
	CXMMImage* pImage1 = new CXMMImage(sourceSize.cx, sourceSize.cy, nFirstX, nLastX, nFirstY, nLastY, pIJLPixels, nChannels);
	if (pImage1->AlignedPtr() == NULL) {
		delete pImage1;
		return NULL;
	}
	double t2 = Helpers::GetExactTickCount();
	CXMMImage* pImage2 = bSSE ? ApplyFilter_SSE(pImage1->GetHeight(), clippedTargetSize.cy, pImage1->GetWidth(), nStartY, 0, nIncrementY, kernelsY, nFilterOffsetY, pImage1) :
		ApplyFilter_MMX(pImage1->GetHeight(), clippedTargetSize.cy, pImage1->GetWidth(), nStartY, 0, nIncrementY, kernelsY, nFilterOffsetY, pImage1);
	delete pImage1;
	if (pImage2 == NULL) return NULL;
	double t3 = Helpers::GetExactTickCount();
	// Rotate
	CXMMImage* pImage3 = Rotate(pImage2);
	delete pImage2;
	if (pImage3 == NULL) return NULL;
	double t4 = Helpers::GetExactTickCount();
	// Resize Y again
	CXMMImage* pImage4 = bSSE ? ApplyFilter_SSE(pImage3->GetHeight(), clippedTargetSize.cx, clippedTargetSize.cy, nStartX, 0, nIncrementX, kernelsX, nFilterOffsetX, pImage3) :
		ApplyFilter_MMX(pImage3->GetHeight(), clippedTargetSize.cx, clippedTargetSize.cy, nStartX, 0, nIncrementX, kernelsX, nFilterOffsetX, pImage3);
	delete pImage3;
	if (pImage4 == NULL) return NULL;
	double t5 = Helpers::GetExactTickCount();
	// Rotate back
	void* pTargetDIB = RotateToDIB(pImage4, pTarget);
	double t6 = Helpers::GetExactTickCount();

	delete pImage4;

	_stprintf_s(s_TimingInfo, 256, _T("Create: %.2f, Filter1: %.2f, Rotate: %.2f, Filter2: %.2f, Rotate: %.2f"), t2 - t1, t3 - t2, t4 - t3, t5 - t4, t6 - t5);
	
	return pTargetDIB;
}

void* CBasicProcessing::SampleUp_HQ_SSE_MMX_Core(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize,
											CSize sourceSize, const void* pIJLPixels, int nChannels, bool bSSE, uint8* pTarget) {
	int nTargetWidth = clippedTargetSize.cx;
	int nTargetHeight = clippedTargetSize.cy;
	int nSourceWidth = sourceSize.cx;
	int nSourceHeight = sourceSize.cy;

	uint32 nIncrementX = (uint32)(65536*(uint32)(nSourceWidth - 1)/(fullTargetSize.cx - 1));
	uint32 nIncrementY = (uint32)(65536*(uint32)(nSourceHeight - 1)/(fullTargetSize.cy - 1));

	int nFirstX = max(0, int((uint32)(nIncrementX*fullTargetOffset.x) >> 16) - 1);
	int nLastX = min(sourceSize.cx - 1, int(((uint32)(nIncrementX*(fullTargetOffset.x + nTargetWidth - 1)) >> 16) + 2));
	int nFirstY = max(0, int((uint32)(nIncrementY*fullTargetOffset.y) >> 16) - 1);
	int nLastY = min(sourceSize.cy - 1, int(((uint32)(nIncrementY*(fullTargetOffset.y + nTargetHeight - 1)) >> 16) + 2));
	int nFirstTargetWidth = nLastX - nFirstX + 1;
	int nFirstTargetHeight = nTargetHeight;
	int nFilterOffsetX = fullTargetOffset.x;
	int nFilterOffsetY = fullTargetOffset.y;
	int nStartX = nIncrementX*fullTargetOffset.x - 65536*nFirstX;
	int nStartY = nIncrementY*fullTargetOffset.y - 65536*nFirstY;

	CAutoXMMFilter filterY(nSourceHeight, fullTargetSize.cy, 0.0, Filter_Upsampling_Bicubic);
	const XMMFilterKernelBlock& kernelsY = filterY.Kernels();

	CAutoXMMFilter filterX(nSourceWidth, fullTargetSize.cx, 0.0, Filter_Upsampling_Bicubic);
	const XMMFilterKernelBlock& kernelsX = filterX.Kernels();

	// Resize Y
	CXMMImage* pImage1 = new CXMMImage(nSourceWidth, nSourceHeight, nFirstX, nLastX, nFirstY, nLastY, pIJLPixels, nChannels);
	if (pImage1->AlignedPtr() == NULL) {
		delete pImage1;
		return NULL;
	}
	CXMMImage* pImage2 = bSSE ? ApplyFilter_SSE(pImage1->GetHeight(), nTargetHeight, pImage1->GetWidth(), nStartY, 0, nIncrementY, kernelsY, nFilterOffsetY, pImage1) :
		ApplyFilter_MMX(pImage1->GetHeight(), nTargetHeight, pImage1->GetWidth(), nStartY, 0, nIncrementY, kernelsY, nFilterOffsetY, pImage1);
	delete pImage1;
	if (pImage2 == NULL) return NULL;
	CXMMImage* pImage3 = Rotate(pImage2);
	delete pImage2;
	if (pImage3 == NULL) return NULL;
	CXMMImage* pImage4 = bSSE ? ApplyFilter_SSE(pImage3->GetHeight(), nTargetWidth, nTargetHeight, nStartX, 0, nIncrementX, kernelsX, nFilterOffsetX, pImage3) :
		ApplyFilter_MMX(pImage3->GetHeight(), nTargetWidth, nTargetHeight, nStartX, 0, nIncrementX, kernelsX, nFilterOffsetX, pImage3);
	delete pImage3;
	if (pImage4 == NULL) return NULL;
	void* pTargetDIB = RotateToDIB(pImage4, pTarget);
	delete pImage4;
	
	return pTargetDIB;
}

void* CBasicProcessing::SampleDown_HQ_SSE_MMX(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize,
											  CSize sourceSize, const void* pIJLPixels, int nChannels, double dSharpen, 
											  EFilterType eFilter, bool bSSE) {
	if (pIJLPixels == NULL || clippedTargetSize.cx <= 0 || clippedTargetSize.cy <= 0) {
		return NULL;
	}
	uint8* pTarget = new(std::nothrow) uint8[clippedTargetSize.cx * 4 * Helpers::DoPadding(clippedTargetSize.cy, 8)];
	if (pTarget == NULL) return NULL;
	CProcessingThreadPool& threadPool = CProcessingThreadPool::This();
	CRequestUpDownSampling request(CProcessingRequest::Downsampling, pIJLPixels, sourceSize, 
		pTarget, fullTargetSize, fullTargetOffset, clippedTargetSize,
		nChannels, dSharpen, eFilter, bSSE);
	bool bSuccess = threadPool.Process(&request);

	return bSuccess ? pTarget : NULL;
}

void* CBasicProcessing::SampleUp_HQ_SSE_MMX(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize,
											CSize sourceSize, const void* pIJLPixels, int nChannels, bool bSSE) {
	if (pIJLPixels == NULL || fullTargetSize.cx < 2 || fullTargetSize.cy < 2 || clippedTargetSize.cx <= 0 || clippedTargetSize.cy <= 0) {
		return NULL;
	}
	uint8* pTarget = new(std::nothrow) uint8[clippedTargetSize.cx * 4 * Helpers::DoPadding(clippedTargetSize.cy, 8)];
	if (pTarget == NULL) return NULL;
	CProcessingThreadPool& threadPool = CProcessingThreadPool::This();
	CRequestUpDownSampling request(CProcessingRequest::Upsampling, pIJLPixels, sourceSize, 
		pTarget, fullTargetSize, fullTargetOffset, clippedTargetSize,
		nChannels, 0.0, Filter_Upsampling_Bicubic, bSSE);
	bool bSuccess = threadPool.Process(&request);

	return bSuccess ? pTarget : NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Unsharp mask
/////////////////////////////////////////////////////////////////////////////////////////////

// dX must be between 0 and 1, return value is in [0, dMaxPos]
static double TransferFunc(double dX, double dThreshold, double dMaxPos) {
	if (dX < dThreshold) {
		double dXScaled = dX / dThreshold;
		return dXScaled * dXScaled * dX;
	} else if (dX < dMaxPos) {
		return dX;
	} else {
		return (dX * dMaxPos - dMaxPos) / (dMaxPos - 1);
	}
}

// Calculates a threshold LUT for sharpening, nNumEntriesPerSide*2 entries.
static int16* CalculateThresholdLUT(int nNumEntriesPerSide, double dThreshold8bit, int16* & pLUTCenter) {
	int16* pLUT = new int16[nNumEntriesPerSide*2];
	const double dMin = -1.0 * (1 << 14); // Minimal value of LUT
	const double dMax =  0.6 * (1 << 14); // Maximal value of LUT
	const double cdPosXMaxValue = 0.2; // x-position where minimal respective maximal value is reached (normalized to 0, 1)
	double dThreshold = dThreshold8bit / 255.0;
	if (dThreshold > cdPosXMaxValue) dThreshold = cdPosXMaxValue;
	double dNormFac = 1.0 / (double)nNumEntriesPerSide;
	for (int nIndex = 0; nIndex < nNumEntriesPerSide*2; nIndex++) {
		if (nIndex <= nNumEntriesPerSide) {
			double dXNorm = (nNumEntriesPerSide - nIndex) * dNormFac; // goes 1 -> 0
			pLUT[nIndex] = (int)(dMin * TransferFunc(dXNorm, dThreshold, cdPosXMaxValue) - 0.5);
		} else {
			double dXNorm = (nIndex - nNumEntriesPerSide) * dNormFac; // goes 0 -> 1
			pLUT[nIndex] = (int)(dMax * TransferFunc(dXNorm, dThreshold, cdPosXMaxValue) + 0.5);
		}
	}
	pLUTCenter = &(pLUT[nNumEntriesPerSide]);
	return pLUT;
}

void* CBasicProcessing::UnsharpMask_Core(CSize fullSize, CPoint offset, CSize rect, double dAmount, const int16* pThresholdLUT,
										 const int16* pGrayImage, const int16* pSmoothedGrayImage, const void* pSourcePixels, void* pTargetPixels, int nChannels) {
	int nDIBLineLen = Helpers::DoPadding(fullSize.cx * nChannels, 4);
	int nAmount = (int)(dAmount * (1 << 12) + 0.5);

	for (int j = 0; j < rect.cy; j++) {
		int nStartOffsetGray = offset.x + (offset.y + j) * fullSize.cx;
		const int16* pGrayPtr = pGrayImage + nStartOffsetGray;
		const int16* pSmoothPtr = pSmoothedGrayImage + nStartOffsetGray;

		int nStartOffsetDIB = offset.x * nChannels + (offset.y + j)* nDIBLineLen;
		uint8* pTargetPixelLine = (uint8*)pTargetPixels + nStartOffsetDIB;
		uint8* pSourcePixelLine = (uint8*)pSourcePixels + nStartOffsetDIB;

		for (int i = 0; i < rect.cx; i++) {
			int nDiff = pThresholdLUT[(*pGrayPtr++ - *pSmoothPtr++) >> 4]; // Note: LUT contains 2^11 entries, subtraction of two 14 bit values can be 15 bit
			int nSharpen = (nDiff * nAmount) >> 18; // nAmount 12 bit, nDiff 14 bit, shift back to 8 bit
			int nBlue = pSourcePixelLine[0];
			nBlue = nBlue + ((nSharpen * nBlue) >> 8);
			int nGreen = pSourcePixelLine[1];
			nGreen = nGreen + ((nSharpen * nGreen) >> 8);
			int nRed = pSourcePixelLine[2];
			nRed = nRed + ((nSharpen * nRed) >> 8);
			pTargetPixelLine[0] = min(255, max(0, nBlue));
			pTargetPixelLine[1] = min(255, max(0, nGreen));
			pTargetPixelLine[2] = min(255, max(0, nRed));
			if (nChannels == 4) {
				pTargetPixelLine[3] = 0xFF;
			}
			pSourcePixelLine += nChannels;
			pTargetPixelLine += nChannels;
		}
	}

	return pTargetPixels;
}

void* CBasicProcessing::UnsharpMask(CSize fullSize, CPoint offset, CSize rect, double dAmount, double dThreshold, 
									const int16* pGrayImage, const int16* pSmoothedGrayImage, const void* pSourcePixels, void* pTargetPixels, int nChannels) {
	if (pSourcePixels == NULL || pTargetPixels == NULL || pGrayImage == NULL || pSmoothedGrayImage == NULL) {
		return NULL;
	}
	int16* pThresholdLUT;
	int16* pThresholdLUTBase = CalculateThresholdLUT(1024, dThreshold, pThresholdLUT);

	CProcessingThreadPool& threadPool = CProcessingThreadPool::This();
	CRequestUnsharpMask request(pSourcePixels, fullSize, offset, rect, dAmount, dThreshold, pThresholdLUT, pGrayImage, pSmoothedGrayImage, pTargetPixels, nChannels);
	bool bSuccess = threadPool.Process(&request);

	delete[] pThresholdLUTBase;
	return bSuccess ? pTargetPixels : NULL;
}


LPCTSTR CBasicProcessing::TimingInfo() {
	return s_TimingInfo;
}
