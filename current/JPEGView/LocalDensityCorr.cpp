#include "StdAfx.h"
#include "LocalDensityCorr.h"
#include "HistogramCorr.h"
#include "JPEGImage.h"
#include "Helpers.h"
#include <math.h>
#include <assert.h>

/////////////////////////////////////////////////////////////////////////////////////////////
// static helpers
/////////////////////////////////////////////////////////////////////////////////////////////

// Calculates the LDC response LUT used to map mean brightness values to LDC correction
// values. The curve is piecewise polynominal and has the form of a saddle.
static void CalculateLDCResponseLUT(uint8* pLUT) {
	const float fExp = 2.5f;
	float fDivisor = pow(127.5f, fExp - 1);
	for (int i = 0; i < 128; i++) {
		float fVal = pow(i + 0.5f, fExp)/fDivisor;
		pLUT[127-i] = (uint8) (127.0f + fVal + 0.49f);
		pLUT[128+i] = (uint8) (127.0f - fVal + 0.49f);
	}
}

// Uses the HSV color model to determine if a color is in the typical hue range for sunsets
static bool IsSunsetPixel(int nR, int nG, int nB) {
	float fR = nR*(1.0f/255.0f);
	float fG = nG*(1.0f/255.0f);
	float fB = nB*(1.0f/255.0f);

	// Calculate HSV value from RGB
	float minVal = min(min(fR, fG), fB);
	float maxVal = max(max(fR, fG), fB);
	float fValue = maxVal;
	float delta = maxVal - minVal;
	float fSaturation, fHue;
	if ( maxVal != 0 )
		fSaturation = delta / maxVal;
	else {
		// r = g = b = 0
		return false;
	}
	if (delta == 0)
		return false;
	if( fR == maxVal )
		fHue = ( fG - fB ) / delta;		// between yellow & magenta
	else if( fG == maxVal )
		fHue = 2 + ( fB - fR ) / delta;	// between cyan & yellow
	else
		fHue = 4 + ( fR - fG ) / delta;	// between magenta & cyan
	fHue *= 60;				// degrees
	if (fHue < 0 )
		fHue += 360;
	if ((fHue > 350 || fHue < 30) && fValue > 0.2f && fSaturation > 0.1f)
		return true;
	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Public
/////////////////////////////////////////////////////////////////////////////////////////////

CLocalDensityCorr::CLocalDensityCorr(const CJPEGImage & image, bool bFullConstruct) {

	// Caution: If something is changed in this code, this breaks all existing image DBs

	// Number of pixels to pick from the original image with point sampling and to be
	// used as starting image for LDC. This image is then resampled by a factor of 4 in
	// each dimension.
	const int NUM_VALUES = 120000;

	m_nLDCWidth = m_nLDCHeight = -1;
	m_pLDCMap = NULL;
	m_pLDCMapMultiplied = NULL;
	m_fIsSunset = m_fMiddleGrey = m_fSunsetPixels = -1.0f;

	// LDC also needs the histogram of the image
	int channelR[256], channelG[256], channelB[256];
	int channelGrey[256];
	memset(channelR, 0, sizeof(int)*256);
	memset(channelG, 0, sizeof(int)*256);
	memset(channelB, 0, sizeof(int)*256);
	memset(channelGrey, 0, sizeof(int)*256);

	int nWidth = image.OrigWidth();
	int nHeight = image.OrigHeight();
	int nChannels = image.IJLChannels();
	const uint8* pSourcePixels = (const uint8*)image.IJLPixels();

	double dFactor = (double)nWidth/nHeight;
	m_nPSIWidth  = Helpers::DoPadding((int)(dFactor*sqrt(NUM_VALUES/dFactor)), 4);
	m_nPSIHeight = Helpers::DoPadding((int)(m_nPSIWidth/dFactor), 4);

	uint32 nY = 0;
	uint32 nIncX = nWidth*65536/m_nPSIWidth;
	uint32 nIncY = nHeight*65536/m_nPSIHeight;
	int nLineSize = Helpers::DoPadding(nWidth * nChannels, 4);

	// The subsampled image has 16 bits per channel and three line interleaved channels B, G, R
	m_pPointSampledImage = new uint16[m_nPSIWidth*m_nPSIHeight*3];

	for (int j = 0; j < m_nPSIHeight; j++) {
		uint32 nX = 0;
		const uint8* pSrcStart = pSourcePixels + nLineSize*(nY >> 16);
		uint16* pSubSampImage = m_pPointSampledImage + j*m_nPSIWidth*3;
		for (int i = 0; i < m_nPSIWidth; i++) {
			const uint8* pSrc = (nChannels == 3) ? pSrcStart + (nX >> 16)*3 : pSrcStart + (nX >> 16)*4;
			channelB[pSrc[0]]++;
			channelG[pSrc[1]]++;
			channelR[pSrc[2]]++;
			channelGrey[(pSrc[0]*128 + pSrc[1]*640 + pSrc[2]*256) >> 10]++;
			pSubSampImage[0] = pSrc[0];
			pSubSampImage[m_nPSIWidth] = pSrc[1];
			pSubSampImage[m_nPSIWidth*2] = pSrc[2];
			pSubSampImage++;
			nX += nIncX;
		}
		nY += nIncY;
	}

	// Calculate a CRC over the histograms
	uint32 crc_table[256];
	Helpers::CalcCRCTable(crc_table);
	uint32 crcValue = 0xffffffff;
	uint8* pHistB = (uint8*)&channelB;
	uint8* pHistG = (uint8*)&channelG;
	uint8* pHistR = (uint8*)&channelR;
	for (int n = 0; n < 256*4; n++) {
		crcValue = crc_table[(crcValue ^ pHistB[n]) & 0xff] ^ (crcValue >> 8);
		crcValue = crc_table[(crcValue ^ pHistG[n]) & 0xff] ^ (crcValue >> 8);
		crcValue = crc_table[(crcValue ^ pHistR[n]) & 0xff] ^ (crcValue >> 8);
	}
	// The CRC of the histogram of an image rotated by 90, 180 or 270 deg is identical
	// -> calculate CRC of one line at 1/4 of the image height
	int nLine = m_nPSIHeight/4;
	uint8* pLinePtr = (uint8*)(m_pPointSampledImage + nLine*m_nPSIWidth*3);
	for (int n = 0; n < m_nPSIWidth*2; n++) {
		crcValue = crc_table[(crcValue ^ *pLinePtr) & 0xff] ^ (crcValue >> 8);
		pLinePtr++;
	}
	// Calculate the sum of all pixels
	uint32 nSumValue = 0;
	for (int n = 0; n < 256; n++) {
		nSumValue += channelB[n]*n + channelG[n]*n + channelR[n]*n;
	}
	// finally calculate the hash value, a sum of 0 is invalid (fully black image - no DB entry)
	// as this is used as a marker
	if (nSumValue == 0) {
		m_nPixelHash = 0;
	} else {
		m_nPixelHash = ((__int64)crcValue << 32) + nSumValue;
	}

	// Calculate grey black and white points
	int nLimit = (int) (m_nPSIWidth*m_nPSIHeight*0.01);
	int nNum = 0, i = 0;
	while (nNum < nLimit) nNum += channelGrey[i++];
	m_fBlackPt = (i-1)/255.0f;
	nNum = 0; i = 255;
	while (nNum < nLimit) nNum += channelGrey[i--];
	m_fWhitePt = (i+1)/255.0f;

	m_pHistogramm = new CHistogram(channelB, channelG, channelR, channelGrey);

	if (bFullConstruct) {
		CreateLDCMap();
	}
}

CLocalDensityCorr::~CLocalDensityCorr(void) {
	delete[] m_pPointSampledImage;
	m_pPointSampledImage = NULL;
	delete m_pHistogramm;
	m_pHistogramm = NULL;
	delete[] m_pLDCMap;
	m_pLDCMap = NULL;
	delete[] m_pLDCMapMultiplied;
	m_pLDCMapMultiplied = NULL;
}

void* CLocalDensityCorr::GetPSImageAsDIB() {
	uint32* pDIBStart = new uint32[m_nPSIWidth*m_nPSIHeight];
	uint32* pDIB = pDIBStart;
	for (int j = 0; j < m_nPSIHeight; j++) {
		uint16* pSrc = m_pPointSampledImage + m_nPSIWidth*j*3;
		for (int i = 0; i < m_nPSIWidth; i++) {
			*pDIB = (pSrc[m_nPSIWidth*2] << 16) + (pSrc[m_nPSIWidth] << 8) + pSrc[0];
			pDIB++;
			pSrc++;
		}
	}
	return pDIBStart;
}

void CLocalDensityCorr::VerifyFullyConstructed() {
	if (m_pLDCMap == NULL) {
		CreateLDCMap();
	}
}

const uint8* CLocalDensityCorr::GetLDCMap() {
	assert(m_pLDCMap != NULL);
	if (m_pLDCMapMultiplied == NULL) {
		m_pLDCMapMultiplied = MultiplyMap(0.5, 0.25);
	}
	return m_pLDCMapMultiplied; 
}

void CLocalDensityCorr::SetLDCAmount(double dLightenShadows, double dDarkenHighlights) {
	assert(m_pLDCMap != NULL);
	delete[] m_pLDCMapMultiplied;
	m_pLDCMapMultiplied = MultiplyMap(min(1.0, max(0.0, dLightenShadows)), min(1.0, max(0.0, dDarkenHighlights)));
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Private
/////////////////////////////////////////////////////////////////////////////////////////////

// Second phase construction of LDC map using the point sampled image
void CLocalDensityCorr::CreateLDCMap() {
	assert(m_pLDCMap == NULL);

	uint8 ldcResponseLUT[256];
	CalculateLDCResponseLUT(ldcResponseLUT);

	// now subsample the small image by factor 4 in each dimension
	int nSumSunSetPixels = 0; // number of pixels with hue in sunset range
	m_nLDCWidth = m_nPSIWidth/4;
	m_nLDCHeight = m_nPSIHeight/4;
	uint32* pLDCRowSum = new uint32[m_nLDCHeight*3]; // mean value of each row
	memset(pLDCRowSum, 0, sizeof(uint32)*m_nLDCHeight*3);
	uint8* pLDCImageStart = new uint8[m_nLDCWidth*m_nLDCHeight];
	uint8* pLDCImage = pLDCImageStart;
	int nSubSampLineWidth = m_nLDCWidth * 2 * 3; // in 32 bit elements
	for (int i = 0; i < m_nLDCHeight; i++) {
		// note that we access the 16 bits/channel subsampled image using 32 bit pointers
		// and thus always reading 2 elements at once
		uint32* pSubSampImage32 = (uint32*) m_pPointSampledImage + i * 4 * nSubSampLineWidth;
		uint32* pCurrentRowSum = pLDCRowSum + i*3;
		for (int j = 0; j < m_nLDCWidth; j++) {
			uint32 nValueB = pSubSampImage32[0] + pSubSampImage32[1] +
				pSubSampImage32[nSubSampLineWidth] + pSubSampImage32[nSubSampLineWidth+1] +
				pSubSampImage32[nSubSampLineWidth*2] + pSubSampImage32[nSubSampLineWidth*2+1] +
				pSubSampImage32[nSubSampLineWidth*3] + pSubSampImage32[nSubSampLineWidth*3+1];
			nValueB = ((nValueB & 0xFFFF) + (nValueB >> 16)) >> 4;
			pSubSampImage32 += m_nLDCWidth * 2; // next channel
			uint32 nValueG = pSubSampImage32[0] + pSubSampImage32[1] +
				pSubSampImage32[nSubSampLineWidth] + pSubSampImage32[nSubSampLineWidth+1] +
				pSubSampImage32[nSubSampLineWidth*2] + pSubSampImage32[nSubSampLineWidth*2+1] +
				pSubSampImage32[nSubSampLineWidth*3] + pSubSampImage32[nSubSampLineWidth*3+1];
			nValueG = ((nValueG & 0xFFFF) + (nValueG >> 16)) >> 4;
			pSubSampImage32 += m_nLDCWidth * 2; // next channel
			uint32 nValueR = pSubSampImage32[0] + pSubSampImage32[1] +
				pSubSampImage32[nSubSampLineWidth] + pSubSampImage32[nSubSampLineWidth+1] +
				pSubSampImage32[nSubSampLineWidth*2] + pSubSampImage32[nSubSampLineWidth*2+1] +
				pSubSampImage32[nSubSampLineWidth*3] + pSubSampImage32[nSubSampLineWidth*3+1];
			nValueR = ((nValueR & 0xFFFF) + (nValueR >> 16)) >> 4;

			// Calculations for sunset detection
			pCurrentRowSum[0] += nValueB;
			pCurrentRowSum[1] += nValueG;
			pCurrentRowSum[2] += nValueR;
			if (IsSunsetPixel(nValueR, nValueG, nValueB)) {
				nSumSunSetPixels++;
			}

			// Give blue a bit more weight than normally, else blue is lighted too much
			int nValueGrey = (nValueB*256 + nValueG*512 + nValueR*256) >> 10;
			
			// do not lighten dark blue shades too much - this will produce ugly blue shadows
			int nBlueCast = nValueB - max(nValueG, nValueR);
			if (nBlueCast > 0 && nValueGrey < 128) {
				if (nBlueCast <= 30) {
					nValueGrey += (128 - nValueGrey)*nBlueCast/30;
				} else {
					nValueGrey += (128 - nValueGrey)*(nBlueCast - 255)/(30 - 255);
				}
			}

			*pLDCImage++ = (uint8) ldcResponseLUT[nValueGrey];
			pSubSampImage32 = (pSubSampImage32 - m_nLDCWidth * 4 + 2); // go back to red channel and next 4 pixels

			/*
			uint32* pDIBPixel = (uint32*) image.DIBPixels() + j + i * image.DIBWidth();
			//*pDIBPixel = nValueB + nValueG*256 + nValueR*256*256;
			*pDIBPixel = pLDCImage[-1] + pLDCImage[-1]*256 + pLDCImage[-1]*256*256;
			*/
		}
	}

	m_fSunsetPixels = (float)nSumSunSetPixels/(m_nLDCHeight*m_nLDCWidth);

	// Check if this could be a sunset picture (LDC brighten shadows must be reduced then)
	if (m_nLDCHeight > 4) {
		int nGreySum = 0;
		for (int i = 0; i < m_nLDCHeight; i++) {
			int j = i*3;
			pLDCRowSum[j] /= m_nLDCWidth;
			pLDCRowSum[j+1] /= m_nLDCWidth;
			pLDCRowSum[j+2] /= m_nLDCWidth;
			nGreySum += (pLDCRowSum[j]*128 + pLDCRowSum[j+1]*640 + pLDCRowSum[j+2]*256) >> 10;
		}
		m_fMiddleGrey = (float)nGreySum/m_nLDCHeight/255.0f;
		m_fIsSunset = CheckIfSunset(pLDCRowSum, m_nLDCHeight);
	} else {
		m_fIsSunset = 0.0f;
		m_fMiddleGrey = 0.5f;
	}
	delete[] pLDCRowSum;

	m_pLDCMap = pLDCImageStart;
	m_pLDCMapMultiplied = NULL;

	SmoothLDCMask();
}

// Multiply the LDC map by given factors in lower and upper range and return new LDC map
uint8* CLocalDensityCorr::MultiplyMap(double dLightenShadows, double dDarkenHighlights) {
	if (m_pLDCMap == NULL) { return NULL; }
	int nLightenValue = (int)(dLightenShadows * 65536 + 0.5);
	int nDarkenValue = (int)(dDarkenHighlights * 65536 + 0.5);
	uint8* pSrc = m_pLDCMap;
	uint8* pDst = new uint8[m_nLDCHeight*m_nLDCWidth];
	uint8* pNewLDC = pDst;
	for (int i = 0; i < m_nLDCHeight; i++) {
		for (int j = 0; j < m_nLDCWidth; j++) {
			if (*pSrc <= 127) {
				*pDst = 127 - ((127 - *pSrc)*nDarkenValue >> 16);
			} else {
				*pDst = 127 + ((*pSrc - 127)*nLightenValue >> 16);
			}
			pSrc++; pDst++;
		}
	}

	return pNewLDC;
}

// Smooth LDC mask by a triangle filter of length 3
void CLocalDensityCorr::SmoothLDCMask() {
	uint8* pNewLDC = new uint8[m_nLDCHeight*m_nLDCWidth];
	uint8* pSrc = m_pLDCMap;
	uint8* pDst = pNewLDC;
	for (int i = 0; i < m_nLDCHeight; i++) {
		for (int j = 0; j < m_nLDCWidth; j++) {
			if (j == 0) {
				*pDst = ((int)pSrc[0]*768 + (int)pSrc[1]*256) >> 10;
			} else if (j == m_nLDCWidth-1) {
				*pDst = ((int)pSrc[0]*768 + (int)pSrc[-1]*256) >> 10;
			} else {
				*pDst = ((int)pSrc[-1]*128 + (int)pSrc[0]*768 + (int)pSrc[1]*128) >> 10;
			}
			pDst++; pSrc++;
		}
	}
	pSrc = pNewLDC;
	pDst = m_pLDCMap;
	for (int i = 0; i < m_nLDCHeight; i++) {
		for (int j = 0; j < m_nLDCWidth; j++) {
			if (i == 0) {
				*pDst = ((int)pSrc[0]*768 + (int)pSrc[m_nLDCWidth]*256) >> 10;
			} else if (i == m_nLDCHeight-1) {
				*pDst = ((int)pSrc[0]*768 + (int)pSrc[-m_nLDCWidth]*256) >> 10;
			} else {
				*pDst = ((int)pSrc[-m_nLDCWidth]*128 + (int)pSrc[0]*768 + (int)pSrc[m_nLDCWidth]*128) >> 10;
			}
			pDst++; pSrc++;
		}
	}

	delete[] pNewLDC;
}

float CLocalDensityCorr::CheckIfSunset(uint32* pRowBGR, int nHeight) {
	int nStart15 = nHeight/5;
	uint32* pSrc = pRowBGR + (nHeight-1)*3;
	int nSign = -1;
	int nLower = 0, nUpper = 0;
	int nLR = 0, nLG = 0, nLB = 0;
	int nUR = 0, nUG = 0, nUB = 0;
	for (int i = 0; i < nHeight; i++) {
		uint32 nGrey = (pSrc[0]*128 + pSrc[1]*768 + pSrc[2]*128) >> 10;
		if (i < nStart15) {
			nLB += pSrc[0];
			nLG += pSrc[1];
			nLR += pSrc[2];
			nLower += nGrey;
		} else {
			nUB += pSrc[0];
			nUG += pSrc[1];
			nUR += pSrc[2];
			nUpper += nGrey;
		}
		pSrc += nSign*3;
	}

	// we now have the values in the lower 1/5 of the image and in the upper 4/5
	int nNumLower = nStart15;
	int nNumUpper = nHeight - nStart15;
	nLower /= nNumLower; nUpper /= nNumUpper;
	nLR /= nNumLower; nLG /= nNumLower; nLB /= nNumLower;
	nUR /= nNumUpper; nUG /= nNumUpper; nUB /= nNumUpper;

	// apply some heuristics
	float fGreyFac = (m_fMiddleGrey > 0.35f) ? 0.0f : 1.0f; // image not too bright
	float fUL = (nLower == 0) ? 1.0f : (float)nUpper/nLower; // lower part darker than upper part
	float fFac = (fUL < 1) ? 0.0f : min(1.0f, fUL);
	float fUpperFac = min(255, max(0, 512 - nUpper*2))/255.0f; // upper part not too bright
	float fLowerFac = min(128, max(0, 128 - nLower))/128.0f;  // lower part not too bright
	float fFacRed = (m_fSunsetPixels > 0.2f) ? 2.0f : (m_fSunsetPixels > 0.05f) ? 1.0f : 0.0f; // sunset colors
	float fVal = fGreyFac*fFac*fFac*fUpperFac*fLowerFac*fLowerFac*fLowerFac*min(1.0f, max(0.0f, fFacRed));
	return min(1.0f, fVal*2);
}