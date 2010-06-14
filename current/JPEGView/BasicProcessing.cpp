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

void* CBasicProcessing::Apply3ChannelLUT32bpp(int nWidth, int nHeight, const void* pDIBPixels, const uint8* pLUT) {
	if (pDIBPixels == NULL || pLUT == NULL) {
		return NULL;
	}

	uint32* pTarget = new uint32[nWidth * nHeight];
	const uint32* pSrc = (uint32*)pDIBPixels;
	uint32* pTgt = pTarget;
	for (int j = 0; j < nHeight; j++) {
		for (int i = 0; i < nWidth; i++) {
			uint32 nSrcPixels = *pSrc;
			*pTgt = pLUT[nSrcPixels & 0xFF] + pLUT[256 + ((nSrcPixels >> 8) & 0xFF)] * 256 + 
				pLUT[512 + ((nSrcPixels >> 16) & 0xFF)] * 65536;
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
	uint32* pTarget = new uint32[nWidth * nHeight];
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
				pLUT[512 + (max(0, min(cnMax, nRed)) >> 16)] * 65536;
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
	nIncrementX = (ldcMapSize.cx == 1) ? 0 : (uint32)((65536*(ldcMapSize.cx - 1))/(fullTargetSize.cx - 1) - 1);
	nIncrementY = (ldcMapSize.cy == 1) ? 0 : (uint32)((65536*(ldcMapSize.cy - 1))/(fullTargetSize.cy - 1) - 1);

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

				*pTgt = max(0, min(255, (int)nBlue)) + max(0, min(255, (int)nGreen))*256 + max(0, min(255, (int)nRed))*65536;
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

				*pTgt = max(0, min(255, (int)nBlue)) + max(0, min(255, (int)nGreen))*256 + max(0, min(255, (int)nRed))*65536;
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

	uint32* pTarget = new uint32[dibSize.cx * dibSize.cy];
	CProcessingThreadPool& threadPool = CProcessingThreadPool::This();
	CRequestLDC request(pDIBPixels, dibSize, pTarget, fullTargetSize, fullTargetOffset,
		ldcMapSize, pSatLUTs, pLUT, pLDCMap, fBlackPt, fWhitePt, fBlackPtSteepness);
	threadPool.Process(&request);

	return pTarget;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Dimming of part of image
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
			*pLine++ = alphaLUT[nPixel & 0xFF] + 256*alphaLUT[(nPixel >> 8) & 0xFF] + 65536*alphaLUT[(nPixel >> 16) & 0xFF];
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
	uint32* pTarget = new uint32[nWidth * nHeight];
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

	uint32* pTarget = new uint32[nHeight * nWidth];
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

void* CBasicProcessing::Convert8bppTo32bppDIB(int nWidth, int nHeight, const void* pDIBPixels, const uint8* pPalette) {
	if (pDIBPixels == NULL || pPalette == NULL) {
		return NULL;
	}
	int nPaddedWidthS = Helpers::DoPadding(nWidth, 4);
	int nPadS = nPaddedWidthS - nWidth;
	uint32* pNewDIB = new uint32[nWidth * nHeight];
	uint32* pTargetDIB = pNewDIB;
	const uint8* pSourceDIB = (uint8*)pDIBPixels;
	for (int j = 0; j < nHeight; j++) {
		for (int i = 0; i < nWidth; i++) {
			*pTargetDIB++ = pPalette[4 * *pSourceDIB] + pPalette[4 * *pSourceDIB + 1] * 256 + pPalette[4 * *pSourceDIB + 2] * 65536;
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
	uint32* pNewDIB = new uint32[nWidth * nHeight];
	uint32* pTarget = pNewDIB;
	const uint8* pSource = (uint8*)pIJLPixels;
	for (int j = 0; j < nHeight; j++) {
		for (int i = 0; i < nWidth; i++) {
			*pTarget++ = *pSource + *pSource * 256 + *pSource * 65536;
			pSource++;
		}
		pSource += nPadSrc;
	}
	return pNewDIB;
}

void* CBasicProcessing::Convert3To4Channels(int nWidth, int nHeight, const void* pIJLPixels) {
	if (pIJLPixels == NULL) {
		return NULL;
	}
	int nPadSrc = Helpers::DoPadding(nWidth*3, 4) - nWidth*3;
	uint32* pNewDIB = new uint32[nWidth * nHeight];
	uint32* pTarget = pNewDIB;
	const uint8* pSource = (uint8*)pIJLPixels;
	for (int j = 0; j < nHeight; j++) {
		for (int i = 0; i < nWidth; i++) {
			*pTarget++ = pSource[0] + pSource[1] * 256 + pSource[2] * 65536;
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
	uint32* pNewDIB = new uint32[nWidth * nHeight];
	uint32* pTgt = pNewDIB;
	const uint8* pSrc = (const uint8*)pGdiplusPixels;
	for (int j = 0; j < nHeight; j++) {
		memcpy(pTgt, pSrc, nWidth*4);
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
		pTargetDIB = new uint32[targetSize.cx * targetSize.cy];
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
// Simple point sampling resize methods
/////////////////////////////////////////////////////////////////////////////////////////////

static void* PointSample(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize, 
						 CSize sourceSize, const void* pIJLPixels, int nChannels) {
 	if (fullTargetSize.cx < 1 || fullTargetSize.cy < 1 ||
		clippedTargetSize.cx < 1 || clippedTargetSize.cy < 1 ||
		fullTargetOffset.x < 0 || fullTargetOffset.x < 0 ||
		clippedTargetSize.cx + fullTargetOffset.x > fullTargetSize.cx ||
		clippedTargetSize.cy + fullTargetOffset.y > fullTargetSize.cy ||
		pIJLPixels == NULL || (nChannels != 3 && nChannels != 4)) {
		return NULL;
	}

	uint8* pDIB = new uint8[clippedTargetSize.cx*4 * clippedTargetSize.cy];

	uint32 nIncrementX, nIncrementY;
	if (fullTargetSize.cx <= sourceSize.cx) {
		// Downsampling
		nIncrementX = (uint32)(sourceSize.cx << 16)/fullTargetSize.cx + 1;
		nIncrementY = (uint32)(sourceSize.cy << 16)/fullTargetSize.cy + 1;
	} else {
		// Upsampling
		nIncrementX = (fullTargetSize.cx == 1) ? 0 : (uint32)((65536*(sourceSize.cx - 1) + 65535)/(fullTargetSize.cx - 1));
		nIncrementY = (fullTargetSize.cy == 1) ? 0 : (uint32)((65536*(sourceSize.cy - 1) + 65535)/(fullTargetSize.cy - 1));
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
				pDst[d+3] = 0;
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

void* CBasicProcessing::SampleDownFast(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize, 
									   CSize sourceSize, const void* pIJLPixels, int nChannels) {
	return PointSample(fullTargetSize, fullTargetOffset, clippedTargetSize, sourceSize, pIJLPixels, nChannels);
}

void* CBasicProcessing::SampleUpFast(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize, 
									 CSize sourceSize, const void* pIJLPixels, int nChannels) {
	return PointSample(fullTargetSize, fullTargetOffset, clippedTargetSize, sourceSize, pIJLPixels, nChannels);
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
						  const ResizeFilterKernels& filter,
						  int nFilterOffset,
						  const uint8* pSource) {

	uint8* pTarget = new uint8[nTargetWidth*4*nHeight];

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
			*pTargetPixel++ = 0;
			// rotate: go to next row in target - width of target is nHeight
			pTargetPixel = pTargetPixel - 4 + nHeight*4;
			nX += nIncrementX_FP;
		}
	}

	return pTarget;
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

	if (pIJLPixels == NULL || fullTargetSize.cx < 2 || fullTargetSize.cy < 2) {
		return NULL;
	}
	int nTargetWidth = clippedTargetSize.cx;
	int nTargetHeight = clippedTargetSize.cy;
	int nSourceWidth = sourceSize.cx;
	int nSourceHeight = sourceSize.cy;

	uint32 nIncrementX = (uint32)(65536*(nSourceWidth - 1)/(fullTargetSize.cx - 1));
	uint32 nIncrementY = (uint32)(65536*(nSourceHeight - 1)/(fullTargetSize.cy - 1));

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
	const ResizeFilterKernels& kernelsX = filterX.GetFilterKernels();

	uint8* pTemp = ApplyFilter(nSourceWidth, nTempTargetHeight, nTempTargetWidth,
		nChannels, nStartX, nFirstY, nIncrementX,
		kernelsX, nFilterOffsetX, (const uint8*) pIJLPixels);

	CResizeFilter filterY(nSourceHeight, fullTargetSize.cy, 0.0, Filter_Upsampling_Bicubic, false);
	const ResizeFilterKernels& kernelsY = filterY.GetFilterKernels();

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

	if (pIJLPixels == NULL) {
		return NULL;
	}
	CResizeFilter filterX(sourceSize.cx, fullTargetSize.cx, dSharpen, eFilter, false);
	const ResizeFilterKernels& kernelsX = filterX.GetFilterKernels();
	CResizeFilter filterY(sourceSize.cy, fullTargetSize.cy, dSharpen, eFilter, false);
	const ResizeFilterKernels& kernelsY = filterY.GetFilterKernels();

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
			*((uint32*)pTarget) = 0; *pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*((uint32*)pTarget) = 0; *pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*((uint32*)pTarget) = 0; *pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*((uint32*)pTarget) = 0; *pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*((uint32*)pTarget) = 0; *pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*((uint32*)pTarget) = 0; *pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*((uint32*)pTarget) = 0; *pTarget = *pSource++ >> 6; pTarget += nIncTargetLine;
			*((uint32*)pTarget) = 0; *pTarget = *pSource++ >> 6;
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
		pTarget = new uint8[pSourceImg->GetHeight() * 4 * Helpers::DoPadding(pSourceImg->GetWidth(), 8)];
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
								  const XMMResizeFilterKernels& filter,
								  int nFilterOffset, const CXMMImage* pSourceImg) {
	
	int nStartXAligned = nStartX & ~7;
	int nEndXAligned = (nStartX + nWidth + 7) & ~7;
	CXMMImage* tempImage = new CXMMImage(nEndXAligned - nStartXAligned, nTargetHeight);
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
								  const XMMResizeFilterKernels& filter,
								  int nFilterOffset, const CXMMImage* pSourceImg) {

	int nStartXAligned = nStartX & ~7;
	int nEndXAligned = (nStartX + nWidth + 7) & ~7;
	CXMMImage* tempImage = new CXMMImage(nEndXAligned - nStartXAligned, nTargetHeight);
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
	const XMMResizeFilterKernels& kernelsY = filterY.Kernels();
	CAutoXMMFilter filterX(sourceSize.cx, fullTargetSize.cx, dSharpen, eFilter);
	const XMMResizeFilterKernels& kernelsX = filterX.Kernels();

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
	double t2 = Helpers::GetExactTickCount();
	CXMMImage* pImage2 = bSSE ? ApplyFilter_SSE(pImage1->GetHeight(), clippedTargetSize.cy, pImage1->GetWidth(), nStartY, 0, nIncrementY, kernelsY, nFilterOffsetY, pImage1) :
		ApplyFilter_MMX(pImage1->GetHeight(), clippedTargetSize.cy, pImage1->GetWidth(), nStartY, 0, nIncrementY, kernelsY, nFilterOffsetY, pImage1);
	delete pImage1;
	double t3 = Helpers::GetExactTickCount();
	// Rotate
	CXMMImage* pImage3 = Rotate(pImage2);
	delete pImage2;
	double t4 = Helpers::GetExactTickCount();
	// Resize Y again
	CXMMImage* pImage4 = bSSE ? ApplyFilter_SSE(pImage3->GetHeight(), clippedTargetSize.cx, clippedTargetSize.cy, nStartX, 0, nIncrementX, kernelsX, nFilterOffsetX, pImage3) :
		ApplyFilter_MMX(pImage3->GetHeight(), clippedTargetSize.cx, clippedTargetSize.cy, nStartX, 0, nIncrementX, kernelsX, nFilterOffsetX, pImage3);
	delete pImage3;
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

	uint32 nIncrementX = (uint32)(65536*(nSourceWidth - 1)/(fullTargetSize.cx - 1));
	uint32 nIncrementY = (uint32)(65536*(nSourceHeight - 1)/(fullTargetSize.cy - 1));

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
	const XMMResizeFilterKernels& kernelsY = filterY.Kernels();

	CAutoXMMFilter filterX(nSourceWidth, fullTargetSize.cx, 0.0, Filter_Upsampling_Bicubic);
	const XMMResizeFilterKernels& kernelsX = filterX.Kernels();

	// Resize Y
	CXMMImage* pImage1 = new CXMMImage(nSourceWidth, nSourceHeight, nFirstX, nLastX, nFirstY, nLastY, pIJLPixels, nChannels);
	CXMMImage* pImage2 = bSSE ? ApplyFilter_SSE(pImage1->GetHeight(), nTargetHeight, pImage1->GetWidth(), nStartY, 0, nIncrementY, kernelsY, nFilterOffsetY, pImage1) :
		ApplyFilter_MMX(pImage1->GetHeight(), nTargetHeight, pImage1->GetWidth(), nStartY, 0, nIncrementY, kernelsY, nFilterOffsetY, pImage1);
	delete pImage1;
	CXMMImage* pImage3 = Rotate(pImage2);
	delete pImage2;
	CXMMImage* pImage4 = bSSE ? ApplyFilter_SSE(pImage3->GetHeight(), nTargetWidth, nTargetHeight, nStartX, 0, nIncrementX, kernelsX, nFilterOffsetX, pImage3) :
		ApplyFilter_MMX(pImage3->GetHeight(), nTargetWidth, nTargetHeight, nStartX, 0, nIncrementX, kernelsX, nFilterOffsetX, pImage3);
	delete pImage3;
	void* pTargetDIB = RotateToDIB(pImage4, pTarget);
	delete pImage4;
	
	return pTargetDIB;
}

void* CBasicProcessing::SampleDown_HQ_SSE_MMX(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize,
											  CSize sourceSize, const void* pIJLPixels, int nChannels, double dSharpen, 
											  EFilterType eFilter, bool bSSE) {
	if (pIJLPixels == NULL) {
		return NULL;
	}
	uint8* pTarget = new uint8[clippedTargetSize.cx * 4 * Helpers::DoPadding(clippedTargetSize.cy, 8)];
	CProcessingThreadPool& threadPool = CProcessingThreadPool::This();
	CRequestUpDownSampling request(CProcessingRequest::Downsampling, pIJLPixels, sourceSize, 
		pTarget, fullTargetSize, fullTargetOffset, clippedTargetSize,
		nChannels, dSharpen, eFilter, bSSE);
	threadPool.Process(&request);

	return pTarget;
}

void* CBasicProcessing::SampleUp_HQ_SSE_MMX(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize,
											CSize sourceSize, const void* pIJLPixels, int nChannels, bool bSSE) {
	if (pIJLPixels == NULL || fullTargetSize.cx < 2 || fullTargetSize.cy < 2) {
		return NULL;
	}
	uint8* pTarget = new uint8[clippedTargetSize.cx * 4 * Helpers::DoPadding(clippedTargetSize.cy, 8)];
	CProcessingThreadPool& threadPool = CProcessingThreadPool::This();
	CRequestUpDownSampling request(CProcessingRequest::Upsampling, pIJLPixels, sourceSize, 
		pTarget, fullTargetSize, fullTargetOffset, clippedTargetSize,
		nChannels, 0.0, Filter_Upsampling_Bicubic, bSSE);
	threadPool.Process(&request);

	return pTarget;
}

LPCTSTR CBasicProcessing::TimingInfo() {
	return s_TimingInfo;
}
