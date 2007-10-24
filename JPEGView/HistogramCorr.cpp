#include "StdAfx.h"
#include "HistogramCorr.h"
#include "JPEGImage.h"
#include "Helpers.h"
#include <math.h>

float CHistogramCorr::sm_ContrastCorrectionStrength = 0.5f;
float CHistogramCorr::sm_BrightnessCorrStrength = 0.2f;

///////////////////////////////////////////////////////////////////////////////////
// CHistogram class
///////////////////////////////////////////////////////////////////////////////////

// Calculates sum of pixel values using the histogram
static int CalculateSum(const int* pHistogram) {
	uint32 nSum = 0;
	for (int i = 0; i < 256; i++) {
		nSum += pHistogram[i]*i;
	}
	return nSum;
}

CHistogram::CHistogram(const CJPEGImage & image, bool bUseOrigPixels) {
	const int NUM_VALUES = 50000;

	memset(m_ChannelB, 0, sizeof(int)*256);
	memset(m_ChannelG, 0, sizeof(int)*256);
	memset(m_ChannelR, 0, sizeof(int)*256);
	memset(m_ChannelGrey, 0, sizeof(int)*256);
	m_nBMean = m_nGMean = m_nRMean = 0;
	m_bUseOrigPixels = bUseOrigPixels;
	m_fNightshot = -1.0f;

	int nWidth, nHeight, nChannels;
	const uint8* pSourcePixels;
	if (bUseOrigPixels || image.DIBPixels() == NULL) {
		nWidth = image.OrigWidth();
		nHeight = image.OrigHeight();
		nChannels = image.IJLChannels();
		pSourcePixels = (const uint8*)image.IJLPixels();
	} else {
		nWidth = image.DIBWidth();
		nHeight = image.DIBHeight();
		nChannels = 4;
		pSourcePixels = (const uint8*)image.DIBPixels();
	}
	
	int nNumPixels = nWidth * nHeight;
	int nGrid = (int) (0.5 + sqrt(1.0 + nNumPixels/NUM_VALUES));
	int nPixPerLine = max(1, nWidth / nGrid);
	int nLines = max(1, nHeight / nGrid);
	int nLineSize = Helpers::DoPadding(nWidth * nChannels, 4);
	for (int j = 0; j < nLines; j++) {
		const uint8* pSrc = pSourcePixels + nLineSize*j*nGrid;
		if (nChannels == 3) {
			for (int i = 0; i < nPixPerLine; i++) {
				m_ChannelB[pSrc[0]]++; m_nBMean += pSrc[0];
				m_ChannelG[pSrc[1]]++; m_nGMean += pSrc[1];
				m_ChannelR[pSrc[2]]++; m_nRMean += pSrc[2];
				m_ChannelGrey[(pSrc[0]*128 + pSrc[1]*640 + pSrc[2]*256) >> 10]++;
				pSrc += nGrid*3;
			}
		} else {
			for (int i = 0; i < nPixPerLine; i++) {
				m_ChannelB[pSrc[0]]++; m_nBMean += pSrc[0];
				m_ChannelG[pSrc[1]]++; m_nGMean += pSrc[1];
				m_ChannelR[pSrc[2]]++; m_nRMean += pSrc[2];
				m_ChannelGrey[(pSrc[0]*128 + pSrc[1]*640 + pSrc[2]*256) >> 10]++;
				pSrc += nGrid*4;
			}
		}
	}
	m_nTotalValues = nPixPerLine*nLines;
	m_nBMean /= m_nTotalValues;
	m_nGMean /= m_nTotalValues;
	m_nRMean /= m_nTotalValues;
}

CHistogram::CHistogram(const int* pChannelB, const int* pChannelG, const int* pChannelR, const int* pChannelGrey) {
	m_nTotalValues = 0;
	m_fNightshot = -1.0f;
	for (int i = 0; i < 256; i++) {
		m_nTotalValues += pChannelGrey[i];
	}
	m_nBMean = CalculateSum(pChannelB)/m_nTotalValues;
	m_nGMean = CalculateSum(pChannelG)/m_nTotalValues;
	m_nRMean = CalculateSum(pChannelR)/m_nTotalValues;
	memcpy(m_ChannelB, pChannelB, sizeof(int)*256);
	memcpy(m_ChannelG, pChannelG, sizeof(int)*256);
	memcpy(m_ChannelR, pChannelR, sizeof(int)*256);
	memcpy(m_ChannelGrey, pChannelGrey, sizeof(int)*256);
	m_bUseOrigPixels = true;
}

// Gets relative area of a part of the histogram, 1.0 is full histogram area
static float GetHistogramArea(const int* pHistogram, int nTotalValues, float fStart, float fEnd) {
	int nStart = (int)(fStart*255 + 0.5f);
	int nEnd = (int)(fEnd*255 + 0.5f);
	int nSum = 0;
	for (int i = nStart; i <= nEnd; i++) {
		nSum += pHistogram[i];
	}
	return (float)nSum/nTotalValues;
}

float CHistogram::IsNightShot() const {
	if (m_fNightshot >= 0) {
		return m_fNightshot;
	}

	// 1. Check brightness level with 80% of pixels below
	uint32 nLimit80 = m_nTotalValues*80/100;
	uint32 nPixels = 0;
	int i = 0;
	while (nPixels < nLimit80) {
		nPixels += m_ChannelGrey[i++];
	}
	float fValue80 = (i-1)/255.0f;
	float fArea10 = GetHistogramArea(m_ChannelGrey, m_nTotalValues, 0.0f, 0.1f);
	float fArea50aboveRed = GetHistogramArea(m_ChannelR, m_nTotalValues, 0.4f, 1.0f);
	float fArea50aboveGreen = GetHistogramArea(m_ChannelG, m_nTotalValues, 0.4f, 1.0f);
	float fArea50aboveBlue = GetHistogramArea(m_ChannelB, m_nTotalValues, 0.4f, 1.0f);

	float fNightshot = min(1.0f, (1.0f - fValue80)*1.6f) - max(0.0f, 1 - fArea10*3) - 
		5*max(fArea50aboveRed, max(fArea50aboveGreen, fArea50aboveBlue));
	((CHistogram*)(this))->m_fNightshot = min(1.0f, max(0.0f, fNightshot)); // cache it to speed up

	return m_fNightshot;
}

///////////////////////////////////////////////////////////////////////////////////
// CHistogramCorr class - helpers
///////////////////////////////////////////////////////////////////////////////////

// Calculates the black and white point from the histogram. The given level (> 0) is the number of
// pixels that can be below resp. above black/white point.
static void CalculateBWPoints(float& fBlackPt, float& fWhitePt, const int* pValues, int nLevel) {
	int nSum = 0;
	int i = 0;
	while (nSum < nLevel) {
		nSum += pValues[i];
		i++;
	}
	fBlackPt = (i == 0) ? 0.0f : (i - 1)/255.0f;

	i = 255;
	nSum = 0;
	while (nSum < nLevel) {
		nSum += pValues[i];
		i--;
	}
	fWhitePt = (i == 255) ? 1.0f : (i + 1)/255.0f;
}

// Calculates the 256 entry LUT (in pTarget) from black/white points and the correction values
// The LUT is a linear curve between black and white point, added by a quadratic correction term
static void CalculateLUT(uint8* pTarget, float fBlackPt, float fWhitePt, float fBrightnessCorr, float fMidPt) {
	if (fWhitePt - fBlackPt < 0.001f) {
		fWhitePt = 1.0f;
		fBlackPt = 0.0f;
	}
	float fA = 1.0f/(fWhitePt - fBlackPt);
	float fB = -fBlackPt*fA;

	float fInc = 1.0f/255.0f;
	float fX = 0.0f;
	for (int i = 0; i < 256; i++) {
		float fY = fA*fX + fB;
		if (fY < 0.0f) {
			pTarget[i] = 0;
		} else if (fY > 1.0f) {
			pTarget[i] = 255;
		} else {
			float fTarget;
			if (fX < fMidPt) {
				float fVal = (fX - fMidPt)/(fMidPt - fBlackPt);
				fTarget = fY + fBrightnessCorr*(1.0f - fVal*fVal);
			} else {
				float fVal = (fX - fMidPt)/(fWhitePt - fMidPt);
				fTarget = fY + fBrightnessCorr*(1.0f - fVal*fVal);
			}

			fTarget = max(0.0f, min(1.0f, fTarget));
			pTarget[i] = (uint8)(fTarget*255 + 0.5f);
		}
		fX += fInc;
	}
}

// Calculates the color componsation needed due to black/white point correction
static float CalcBWColorCompensation(float fMidPt, float fBlackPt, float fWhitePt) {
	float fA = 1.0f/max(0.001f, fWhitePt - fBlackPt);
	float fB = -fBlackPt*fA;
	return fA*fMidPt + fB;
}

///////////////////////////////////////////////////////////////////////////////////
// CHistogramCorr class
///////////////////////////////////////////////////////////////////////////////////

uint8* CHistogramCorr::CalculateCorrectionLUT(const CHistogram & histogram, float fColorCorrectionFactor, float fBrightnessCorrectionFactor,
											  const float fColorCastCorrection[3], const float fColorCorrectionStrength[6],
											  float fContrastCorrectionFactor) {
	const float cCuttingLevel = 0.001f; // black/white point cutting level
	const float cBrightnessCorr = 1.0f; // strenght of brighness correction
	const float cLog0dot5 = log10f(0.5f);
	const float cCastFactor = 0.3f;

	float fContrastStrengthLog = (sm_ContrastCorrectionStrength == 0) ? 100 : log10f(sm_ContrastCorrectionStrength)/cLog0dot5;
	float fContrastFactorLog = (fContrastCorrectionFactor == 0) ? 100 : log10f(fContrastCorrectionFactor)/cLog0dot5;
	float fStrength = max(1e-4f, fContrastStrengthLog*fContrastFactorLog);

	int nLevel = (int) (histogram.GetTotalValues()*cCuttingLevel + 0.5f);

	uint8* pLUT = new uint8[3*256];

	// Calculate black point for R,G,B and Grey channel
	float fBlackPtB, fBlackPtG, fBlackPtR, fBlackPtGrey;
	float fWhitePtB, fWhitePtG, fWhitePtR, fWhitePtGrey;
	CalculateBWPoints(fBlackPtB, fWhitePtB, histogram.GetChannelB(), nLevel);
	CalculateBWPoints(fBlackPtG, fWhitePtG, histogram.GetChannelG(), nLevel);
	CalculateBWPoints(fBlackPtR, fWhitePtR, histogram.GetChannelR(), nLevel);
	CalculateBWPoints(fBlackPtGrey, fWhitePtGrey, histogram.GetChannelGrey(), nLevel);

	// Reduce the maximal applied correction - if the black or white point is far from 0,
	// respectively 1, we only apply a decreasing fraction of the full correction.
	float fHistogramWidthFactor = pow((fWhitePtGrey - fBlackPtGrey), fStrength*0.5f);
	fBlackPtB = fBlackPtB*pow((1 - fBlackPtB)*(1 - fBlackPtGrey), fStrength)*fHistogramWidthFactor;
	fBlackPtG = fBlackPtG*pow((1 - fBlackPtG)*(1 - fBlackPtGrey), fStrength)*fHistogramWidthFactor;
	fBlackPtR = fBlackPtR*pow((1 - fBlackPtR)*(1 - fBlackPtGrey), fStrength)*fHistogramWidthFactor;
	fWhitePtB = 1 - (1 - fWhitePtB)*pow(fWhitePtB*fWhitePtGrey, fStrength)*fHistogramWidthFactor;
	fWhitePtG = 1 - (1 - fWhitePtG)*pow(fWhitePtG*fWhitePtGrey, fStrength)*fHistogramWidthFactor;
	fWhitePtR = 1 - (1 - fWhitePtR)*pow(fWhitePtR*fWhitePtGrey, fStrength)*fHistogramWidthFactor;

	// If the brighness decreased by the correction, undo this
	float fMeanBlackPt = (fBlackPtB + fBlackPtG + fBlackPtR)/3;
	float fMeanWhitePt = (fWhitePtB + fWhitePtG + fWhitePtR)/3; 
	float fA = 1.0f/(fMeanWhitePt - fMeanBlackPt);
	float fB = -fMeanBlackPt*fA;
	float fMidPt = (fMeanWhitePt + fMeanBlackPt)*0.5f;
	float fCorr = cBrightnessCorr*(fMidPt*(1.0f - fA) - fB);
	fCorr = max(0.0f, fCorr);

	// make sure not to change the color in the middle area or the histogram
	float fMiddleGrey = (histogram.GetBMean() + histogram.GetGMean() + histogram.GetRMean())/(255.0f*3);
	float fBYCast = histogram.GetBMean()/255.0f - fMiddleGrey;
	float fGMCast = histogram.GetGMean()/255.0f - fMiddleGrey;
	float fRCCast = histogram.GetRMean()/255.0f - fMiddleGrey;

	float fYMean = fA*fMidPt + fB;
	float fCorrBlue  = fYMean - CalcBWColorCompensation(fMidPt, fBlackPtB, fWhitePtB);
	float fCorrGreen = fYMean - CalcBWColorCompensation(fMidPt, fBlackPtG, fWhitePtG);
	float fCorrRed   = fYMean - CalcBWColorCompensation(fMidPt, fBlackPtR, fWhitePtR);

	float fCastCorrB = CalcColorCastCorr(0, fBYCast, fColorCorrectionStrength); 
	float fCastCorrG = CalcColorCastCorr(1, fGMCast, fColorCorrectionStrength);
	float fCastCorrR = CalcColorCastCorr(2, fRCCast, fColorCorrectionStrength);
	float fCastMean = (fCastCorrB + fCastCorrG + fCastCorrR)/3;
	float fMaxCast = max(abs(fCastCorrB - fCastMean), max(abs(fCastCorrG - fCastMean), abs(fCastCorrR - fCastMean)));
	float fCastFact = 0.05f/max(0.00001f, fMaxCast);
	float fCastMult;
	if (fColorCorrectionFactor > 0) {
		fCastMult = 2*(2.0f*fCastFact - 1)*fColorCorrectionFactor + 1;
	} else {
		fCastMult = 2*(fCastFact + 1)*fColorCorrectionFactor + 1;
	}
	fCorrBlue = fCorrBlue - fCastMult*fCastCorrB + cCastFactor*fColorCastCorrection[2];
	fCorrGreen = fCorrGreen - fCastMult*fCastCorrG + cCastFactor*fColorCastCorrection[1];
	fCorrRed = fCorrRed - fCastMult*fCastCorrR + cCastFactor*fColorCastCorrection[0];

	float fMeanGrey = (histogram.GetBMean()*0.1f + histogram.GetGMean()*0.6f + histogram.GetRMean()*0.3f)/255;
	float fBrightnessAdd = (fMeanGrey < 0.5f) ? (0.5f - fMeanGrey)*sm_BrightnessCorrStrength*fBrightnessCorrectionFactor : 0.0f;
	fCorr = (fCorr - fCorrBlue)*0.1f + (fCorr - fCorrGreen)*0.6f + (fCorr - fCorrRed)*0.3f + fBrightnessAdd;

	CalculateLUT(&(pLUT[0]), fBlackPtB, fWhitePtB, fCorrBlue + fCorr, fMidPt);
	CalculateLUT(&(pLUT[256]), fBlackPtG, fWhitePtG, fCorrGreen + fCorr, fMidPt);
	CalculateLUT(&(pLUT[512]), fBlackPtR, fWhitePtR, fCorrRed + fCorr, fMidPt);

	/*
	// The blue channel needs some special attention as we do not want to make an image
	// more blue than it was
	float fYMean = fA*fMidPt + fB;
	float fABlue = 1.0f/(fWhitePtB - fBlackPtB);
	float fBBlue = -fBlackPtB*fABlue;
	float fMidPtBlue = (fWhitePtB - fBlackPtB)*0.5f;
	float fYBlue = fABlue*fMidPtBlue + fBBlue;
	float fCorrBlue;
	if (fYBlue > fYMean) {
		fCorrBlue = fYMean - fYBlue;
	} else {
		fCorrBlue = fCorr;
	}

	CalculateLUT(&(pLUT[0]), fBlackPtB, fWhitePtB, fCorrBlue);
	CalculateLUT(&(pLUT[256]), fBlackPtG, fWhitePtG, fCorr);
	CalculateLUT(&(pLUT[512]), fBlackPtR, fWhitePtR, fCorr);
	*/
	return pLUT;
}

float CHistogramCorr::CalcColorCastCorr(int nChannel, float fCast, const float fColorCorrectionStrength[6]) {
	if (nChannel == 0) {
		if (fCast > 0.0f) {
			// Blue cast
			return fCast*fColorCorrectionStrength[2];
		} else {
			// Yellow cast
			return fCast*fColorCorrectionStrength[5];
		}
	} else if (nChannel == 1) {
		if (fCast > 0.0f) {
			// Green cast
			return fCast*fColorCorrectionStrength[1];
		} else {
			// Magenta cast
			return fCast*fColorCorrectionStrength[4];
		}
	} else {
		if (fCast > 0.0f) {
			// Red cast
			return fCast*fColorCorrectionStrength[0];
		} else {
			// Cyan cast
			return fCast*fColorCorrectionStrength[3];
		}
	}
}

uint8* CHistogramCorr::CombineLUTs(const uint8* pSingleChannelLUT, const uint8* pThreeChannelLUT) {
	uint8* pLUT = new uint8[256*3];
	if (pSingleChannelLUT == NULL && pThreeChannelLUT == NULL) {
		// return identity LUT
		for (int i = 0; i < 256; i++) {
			pLUT[i] = pLUT[i + 256] = pLUT[i + 256*2] = i;
		}
	} else if (pSingleChannelLUT == NULL) {
		memcpy(pLUT, pThreeChannelLUT, 3*256);
	} else if (pThreeChannelLUT == NULL) {
		memcpy(pLUT, pSingleChannelLUT, 256);
		memcpy(pLUT + 256, pSingleChannelLUT, 256);
		memcpy(pLUT + 256*2, pSingleChannelLUT, 256);
	} else {
		for (int i = 0; i < 256; i++) {
			pLUT[i] = pThreeChannelLUT[pSingleChannelLUT[i]];
			pLUT[i + 256] = pThreeChannelLUT[pSingleChannelLUT[i] + 256];
			pLUT[i + 256*2] = pThreeChannelLUT[pSingleChannelLUT[i] + 256*2];
		}
	}
	return pLUT;
}