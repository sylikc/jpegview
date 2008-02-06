#include "StdAfx.h"
#include "JPEGImage.h"
#include "BasicProcessing.h"
#include "XMMImage.h"
#include "Helpers.h"
#include "SettingsProvider.h"
#include "HistogramCorr.h"
#include "LocalDensityCorr.h"
#include "ParameterDB.h"
#include "EXIFReader.h"
#include <math.h>
#include <assert.h>

///////////////////////////////////////////////////////////////////////////////////
// Public interface
///////////////////////////////////////////////////////////////////////////////////

CJPEGImage::CJPEGImage(int nWidth, int nHeight, void* pIJLPixels, void* pEXIFData, int nChannels, 
					   __int64 nJPEGHash, EImageFormat eImageFormat, CLocalDensityCorr* pLDC) {
	if (nChannels == 3 || nChannels == 4) {
		m_pIJLPixels = pIJLPixels;
		m_nIJLChannels = nChannels;
	} else if (nChannels == 1) {
		m_pIJLPixels = CBasicProcessing::Convert1To4Channels(nWidth, nHeight, pIJLPixels);
		delete[] pIJLPixels;
		m_nIJLChannels = 4;
	} else {
		assert(false);
		m_pIJLPixels = NULL;
		m_nIJLChannels = 0;
	}

	if (pEXIFData != NULL) {
		unsigned char * pEXIF = (unsigned char *)pEXIFData;
		m_nEXIFSize = pEXIF[2]*256 + pEXIF[3] + 2;
		m_pEXIFData = new char[m_nEXIFSize];
		memcpy(m_pEXIFData, pEXIFData, m_nEXIFSize);
		m_pEXIFReader = new CEXIFReader(m_pEXIFData);
	} else {
		m_nEXIFSize = 0;
		m_pEXIFData = NULL;
		m_pEXIFReader = NULL;
	}

	m_nPixelHash = nJPEGHash;
	m_eImageFormat = eImageFormat;

	m_nOrigWidth = nWidth;
	m_nOrigHeight = nHeight;
	m_pDIBPixels = NULL;
	m_pDIBPixelsLUTProcessed = NULL;
	m_pLastDIB = NULL;
	m_pThumbnail = NULL;
	
	m_pLUTAllChannels = NULL;
	m_pLUTRGB = NULL;
	m_eProcFlags = PFLAG_None;
	m_eProcFlagsInitial = PFLAG_None;
	m_nInitialRotation = 0;
	m_dInitialZoom = -1;
	m_initialOffsets = CPoint(0, 0);
	m_pDimRects = 0;
	m_nNumDimRects = 0;
	m_bEnableDimming = true;
	m_bInParamDB = false;
	m_bHasZoomStoredInParamDB = false;

	m_bCropped = false;
	m_nRotation = 0;
	m_bFirstReprocessing = true;
	m_dLastOpTickCount = 0;
	m_FullTargetSize = CSize(0, 0);
	m_ClippingSize = CSize(0, 0);
	m_TargetOffset = CPoint(0, 0);

	// Create the LDC object on the image
	m_pLDC = (pLDC == NULL) ? (new CLocalDensityCorr(*this, true)) : pLDC;
	m_bLDCOwned = pLDC == NULL;
	if (nJPEGHash == 0) {
		// Use the decompressed pixel hash in this case
		m_nPixelHash = m_pLDC->GetPixelHash();
	}
	m_fLightenShadowFactor = (1.0f - m_pLDC->GetHistogram()->IsNightShot())*(1.0f - m_pLDC->IsSunset());

	// Initialize to INI value, may be overriden later by parameter DB
	memcpy(m_fColorCorrectionFactors, CSettingsProvider::This().ColorCorrectionAmounts(), sizeof(m_fColorCorrectionFactors));
	memset(m_fColorCorrectionFactorsNull, 0, sizeof(m_fColorCorrectionFactorsNull));
}

CJPEGImage::~CJPEGImage(void) {
	delete[] m_pIJLPixels;
	m_pIJLPixels = NULL;
	delete[] m_pDIBPixels;
	m_pDIBPixels = NULL;
	delete[] m_pDIBPixelsLUTProcessed;
	m_pDIBPixelsLUTProcessed = NULL;
	delete[] m_pLUTAllChannels;
	m_pLUTAllChannels = NULL;
	delete[] m_pLUTRGB;
	m_pLUTRGB = NULL;
	if (m_bLDCOwned) delete m_pLDC;
	m_pLDC = NULL;
	m_pLastDIB = NULL;
	delete[] m_pEXIFData;
	m_pEXIFData = NULL;
	delete m_pEXIFReader;
	m_pEXIFReader = NULL;
	delete[] m_pDimRects;
	m_pDimRects = NULL;
	delete m_pThumbnail;
	m_pThumbnail = NULL;
}

void* CJPEGImage::GetDIB(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset,
						 const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags) {

	// Check if resampling due to bHighQualityResampling parameter change is needed
	bool bMustResampleQuality = GetProcessingFlag(eProcFlags, PFLAG_HighQualityResampling) != GetProcessingFlag(m_eProcFlags, PFLAG_HighQualityResampling);
	bool bTargetSizeChanged = fullTargetSize != m_FullTargetSize;
	// Check if resampling due to change of geometric parameters is needed
	bool bMustResampleGeometry = bTargetSizeChanged || clippingSize != m_ClippingSize || targetOffset != m_TargetOffset;
	// Check if resampling due to change of processing parameters is needed
	bool bMustResampleProcessings = fabs(imageProcParams.Sharpen - m_imageProcParams.Sharpen) > 1e-2;

	EResizeType eResizeType = GetResizeType(fullTargetSize, CSize(m_nOrigWidth, m_nOrigHeight));

	// the geometrical parameters must be set before calling ApplyCorrectionLUT()
	CRect oldClippingRect = CRect(m_TargetOffset, m_ClippingSize);
	m_FullTargetSize = fullTargetSize;
	m_ClippingSize = clippingSize;
	m_TargetOffset = targetOffset;

	double dStartTickCount = Helpers::GetExactTickCount();

	// Check if only the LUT must be reapplied but no resampling (resampling is much slower than the LUTs)
	void * pDIB = NULL;
	if (!bMustResampleQuality && !bMustResampleGeometry && !bMustResampleProcessings) {
		// only LUT must be reapplied (or even nothing must be done)
		pDIB = ApplyCorrectionLUTandLDC(imageProcParams, eProcFlags, m_pDIBPixelsLUTProcessed, 
			fullTargetSize, targetOffset, m_pDIBPixels, clippingSize, bMustResampleGeometry, false);
	}
	// ApplyCorrectionLUTandLDC() could have failed, then recreate the DIBs
	if (pDIB == NULL) {
		// if the image is reprocessed more than once, it is worth to convert the original to 4 channels
		// as this is faster for further processing
		if (!m_bFirstReprocessing) {
			ConvertSrcTo4Channels();
		}

		// If we only pan, we can resample far more efficiently by only calculating the newly visible areas
		bool bPanningOnly = !m_bFirstReprocessing && !bMustResampleProcessings && !bTargetSizeChanged && !bMustResampleQuality;
		m_bFirstReprocessing = false;
		if (bPanningOnly) {
			ResampleWithPan(m_pDIBPixels, m_pDIBPixelsLUTProcessed, fullTargetSize, clippingSize, targetOffset, 
				oldClippingRect, eProcFlags, imageProcParams, eResizeType);
		} else {
			delete[] m_pDIBPixelsLUTProcessed; m_pDIBPixelsLUTProcessed = NULL;
			delete[] m_pDIBPixels; m_pDIBPixels = NULL;
		}

		// both DIBs are NULL, do normal resampling
		if (m_pDIBPixels == NULL && m_pDIBPixelsLUTProcessed == NULL) {
			m_pDIBPixels = Resample(fullTargetSize, clippingSize, targetOffset, eProcFlags, imageProcParams.Sharpen, eResizeType);
		}

		// if ResampleWithPan() has preseved this DIB, we can reuse it
		if (m_pDIBPixelsLUTProcessed == NULL) {
			pDIB = ApplyCorrectionLUTandLDC(imageProcParams, eProcFlags, m_pDIBPixelsLUTProcessed, fullTargetSize, 
				targetOffset, m_pDIBPixels, clippingSize, bMustResampleGeometry, false);
		} else {
			pDIB = m_pDIBPixelsLUTProcessed;
		}
	}

	double dLastOpTickCount = Helpers::GetExactTickCount() - dStartTickCount; 
	if (dLastOpTickCount > 1) {
		m_dLastOpTickCount = dLastOpTickCount;
	}

	// set these parameters after ApplyCorrectionLUT() - else it cannot be detected that the parameters changed
	double dOldSharpen = m_imageProcParams.Sharpen;
	m_imageProcParams = imageProcParams;
	// do not touch sharpen parameter if no resampling done - avoids cummulative error propagation
	if (!bMustResampleProcessings) {
		m_imageProcParams.Sharpen = dOldSharpen;
	}
	m_eProcFlags = eProcFlags;

	m_pLastDIB = pDIB;

	return pDIB;
}

void* CJPEGImage::GetThumbnailDIB(CSize size, const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags) {
	if (m_pThumbnail == NULL) {
		if (m_pLDC == NULL) {
			m_pLDC = new CLocalDensityCorr(*this, true);
		}
		void* pPixels = NULL;
		int nWidth, nHeight;
		if (m_nOrigWidth*m_nOrigHeight < 120000) {
			// take a copy of the original pixels
			nWidth = m_nOrigWidth;
			nHeight = m_nOrigHeight;
			if (m_nIJLChannels == 3) {
				pPixels = CBasicProcessing::Convert3To4Channels(m_nOrigWidth, m_nOrigHeight, m_pIJLPixels);
			} else {
				int nSizeBytes = m_nOrigWidth*m_nOrigHeight*4;
				pPixels = new uint8[nSizeBytes];
				memcpy(pPixels, m_pIJLPixels, nSizeBytes);
			}
		} else {
			// take the small image from the LDC
			CSize psiSize = m_pLDC->GetPSISize();
			nWidth = psiSize.cx;
			nHeight = psiSize.cy;
			pPixels = m_pLDC->GetPSImageAsDIB();
		}
		m_pThumbnail = new CJPEGImage(nWidth, nHeight, pPixels, NULL, 4, -1, IF_CLIPBOARD, m_pLDC);
	}
	return m_pThumbnail->GetDIB(size, size, CPoint(0, 0), imageProcParams, eProcFlags);
}

void CJPEGImage::ResampleWithPan(void* & pDIBPixels, void* & pDIBPixelsLUTProcessed, CSize fullTargetSize, 
								 CSize clippingSize, CPoint targetOffset, CRect oldClippingRect,
								 EProcessingFlags eProcFlags, const CImageProcessingParams & imageProcParams, EResizeType eResizeType) {
	CPoint oldOffset = oldClippingRect.TopLeft();
	CSize oldSize = oldClippingRect.Size();
	CRect newClippingRect = CRect(targetOffset, clippingSize);
	CRect sourceRect;
	if (sourceRect.IntersectRect(oldClippingRect, newClippingRect)) {
		// there is an intersection, reuse the non LUT processed DIB
		sourceRect.OffsetRect(-oldOffset.x, -oldOffset.y);
		CRect targetRect = CRect(CPoint(max(0, oldOffset.x - newClippingRect.left), max(0, oldOffset.y - newClippingRect.top)), 
			CSize(sourceRect.Width(), sourceRect.Height()));

		bool bCanUseLUTProcDIB = ApplyCorrectionLUTandLDC(imageProcParams, eProcFlags, pDIBPixelsLUTProcessed, fullTargetSize, 
			targetOffset, pDIBPixels, clippingSize, false, true) != NULL && (m_pDimRects == NULL || !m_bEnableDimming);

		// the LUT processed pixels cannot be used and the original pixels are not available -
		// full recreation of DIBs is needed
		if (!bCanUseLUTProcDIB && pDIBPixels == NULL) {
			delete[] pDIBPixelsLUTProcessed; pDIBPixelsLUTProcessed = NULL;
			return;
		}

		// Copy the reusable part of original DIB pixels
		void* pPannedPixels = (bCanUseLUTProcDIB == false) ? 
			CBasicProcessing::CopyRect32bpp(NULL, pDIBPixels, clippingSize, targetRect, oldSize, sourceRect) :
			NULL;

		// get rid of original DIB, will we recreated automatically when needed
		delete[] pDIBPixels; pDIBPixels = NULL;

		// Copy the reusable part of processed DIB pixels
		void* pPannedPixelsLUTProcessed = bCanUseLUTProcDIB ? 
			CBasicProcessing::CopyRect32bpp(NULL, pDIBPixelsLUTProcessed, clippingSize, targetRect, oldSize, sourceRect) :
			NULL;

		// Delete old LUT processed DIB, we copied the part that can be reused to a new DIB (pPannedPixelsLUTProcessed)
		delete[] pDIBPixelsLUTProcessed; pDIBPixelsLUTProcessed = NULL;

		if (targetRect.top > 0) {
			CSize clipSize(clippingSize.cx, targetRect.top);
			void* pTop = Resample(fullTargetSize, clipSize, targetOffset, eProcFlags, imageProcParams.Sharpen, eResizeType);
			
			if (!bCanUseLUTProcDIB) {
				CBasicProcessing::CopyRect32bpp(pPannedPixels, pTop,
					clippingSize, CRect(CPoint(0, 0), clipSize),
					clipSize, CRect(CPoint(0, 0), clipSize));
			} else {
				void* pTopProc = NULL;
				ApplyCorrectionLUTandLDC(imageProcParams, eProcFlags, pTopProc, fullTargetSize, targetOffset, pTop, clipSize, false, false);
				CBasicProcessing::CopyRect32bpp(pPannedPixelsLUTProcessed, pTopProc,
					clippingSize, CRect(CPoint(0, 0), clipSize),
					clipSize, CRect(CPoint(0, 0), clipSize));
				delete[] pTopProc;
			}

			delete[] pTop;
		}
		if (targetRect.bottom < clippingSize.cy) {
			CSize clipSize(clippingSize.cx, clippingSize.cy -  targetRect.bottom);
			CPoint offset(targetOffset.x, targetOffset.y + targetRect.bottom);
			void* pBottom = Resample(fullTargetSize, clipSize, offset, eProcFlags, imageProcParams.Sharpen, eResizeType);
			
			if (!bCanUseLUTProcDIB) {
				CBasicProcessing::CopyRect32bpp(pPannedPixels, pBottom,
					clippingSize, CRect(CPoint(0, targetRect.bottom), clipSize),
					clipSize, CRect(CPoint(0, 0), clipSize));
			} else {
				void* pBottomProc = NULL;
				ApplyCorrectionLUTandLDC(imageProcParams, eProcFlags, pBottomProc, fullTargetSize, offset, pBottom, clipSize, false, false);
				CBasicProcessing::CopyRect32bpp(pPannedPixelsLUTProcessed, pBottomProc,
					clippingSize, CRect(CPoint(0, targetRect.bottom), clipSize),
					clipSize, CRect(CPoint(0, 0), clipSize));
				delete[] pBottomProc;
			}

			delete[] pBottom;
		}
		if (targetRect.left > 0) {
			CSize clipSize(targetRect.left, clippingSize.cy);
			void* pLeft = Resample(fullTargetSize, clipSize, targetOffset, eProcFlags, imageProcParams.Sharpen, eResizeType);
			
			if (!bCanUseLUTProcDIB) {
				CBasicProcessing::CopyRect32bpp(pPannedPixels, pLeft,
					clippingSize, CRect(CPoint(0, 0), clipSize),
					clipSize, CRect(CPoint(0, 0), clipSize));
			} else {
				void* pLeftProc = NULL;
				ApplyCorrectionLUTandLDC(imageProcParams, eProcFlags, pLeftProc, fullTargetSize, targetOffset, pLeft, clipSize, false, false);
				CBasicProcessing::CopyRect32bpp(pPannedPixelsLUTProcessed, pLeftProc,
					clippingSize, CRect(CPoint(0, 0), clipSize),
					clipSize, CRect(CPoint(0, 0), clipSize));
				delete[] pLeftProc;
			}

			delete[] pLeft;
		}
		if (targetRect.right < clippingSize.cx) {
			CSize clipSize(clippingSize.cx -  targetRect.right, clippingSize.cy);
			CPoint offset(targetOffset.x + targetRect.right, targetOffset.y);
			void* pRight = Resample(fullTargetSize, clipSize, offset, eProcFlags, imageProcParams.Sharpen, eResizeType);
			
			if (!bCanUseLUTProcDIB) {
				CBasicProcessing::CopyRect32bpp(pPannedPixels, pRight,
					clippingSize, CRect(CPoint(targetRect.right, 0), clipSize),
					clipSize, CRect(CPoint(0, 0), clipSize));
			} else {
				void* pRigthProc = NULL;
				ApplyCorrectionLUTandLDC(imageProcParams, eProcFlags, pRigthProc, fullTargetSize, offset, pRight, clipSize, false, false);
				CBasicProcessing::CopyRect32bpp(pPannedPixelsLUTProcessed, pRigthProc,
					clippingSize, CRect(CPoint(targetRect.right, 0), clipSize),
					clipSize, CRect(CPoint(0, 0), clipSize));
				delete[] pRigthProc;
			}

			delete[] pRight;
		}
		pDIBPixels = pPannedPixels;
		pDIBPixelsLUTProcessed = pPannedPixelsLUTProcessed;
		return;
	}

	delete[] pDIBPixels; pDIBPixels = NULL;
	delete[] pDIBPixelsLUTProcessed; pDIBPixelsLUTProcessed = NULL;
}

void* CJPEGImage::Resample(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset, 
						  EProcessingFlags eProcFlags, double dSharpen, EResizeType eResizeType) {
	Helpers::CPUType cpu = CSettingsProvider::This().AlgorithmImplementation();
	EFilterType filter = CSettingsProvider::This().DownsamplingFilter();

	if (GetProcessingFlag(eProcFlags, PFLAG_HighQualityResampling) && 
		!(eResizeType == NoResize && filter == Filter_Downsampling_Best_Quality)) {
		if (cpu == Helpers::CPU_SSE || cpu == Helpers::CPU_MMX) {
			if (eResizeType == UpSample) {
				return CBasicProcessing::SampleUp_HQ_SSE_MMX(fullTargetSize, targetOffset, clippingSize, 
					CSize(m_nOrigWidth, m_nOrigHeight), m_pIJLPixels, m_nIJLChannels, cpu == Helpers::CPU_SSE);
			} else {
				return CBasicProcessing::SampleDown_HQ_SSE_MMX(fullTargetSize, targetOffset, clippingSize,
					CSize(m_nOrigWidth, m_nOrigHeight), m_pIJLPixels, m_nIJLChannels, dSharpen, filter, cpu == Helpers::CPU_SSE);
			}
		} else {
			if (eResizeType == UpSample) {
				return CBasicProcessing::SampleUp_HQ(fullTargetSize, targetOffset, clippingSize, 
					CSize(m_nOrigWidth, m_nOrigHeight), m_pIJLPixels, m_nIJLChannels);
			} else {
				return CBasicProcessing::SampleDown_HQ(fullTargetSize, targetOffset, clippingSize, 
					CSize(m_nOrigWidth, m_nOrigHeight), m_pIJLPixels, m_nIJLChannels, dSharpen, filter);
			}
		}
	} else {
		if (eResizeType == UpSample) {
			return CBasicProcessing::SampleUpFast(fullTargetSize, targetOffset, clippingSize, 
				CSize(m_nOrigWidth, m_nOrigHeight), m_pIJLPixels, m_nIJLChannels);
		} else {
			return CBasicProcessing::SampleDownFast(fullTargetSize, targetOffset, clippingSize, 
				CSize(m_nOrigWidth, m_nOrigHeight), m_pIJLPixels, m_nIJLChannels);
		}
	}
}

CPoint CJPEGImage::ConvertOffset(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset) {
	int nStartX = (fullTargetSize.cx - clippingSize.cx)/2 - targetOffset.x;
	int nStartY = (fullTargetSize.cy - clippingSize.cy)/2 - targetOffset.y;
	return CSize(nStartX, nStartY);
}

void CJPEGImage::VerifyRotation(int nRotation) {
	int nDiff = ((nRotation - m_nRotation) + 360) % 360;
	if (nDiff != 0) {
		Rotate(nDiff);
	}
}

void CJPEGImage::Rotate(int nRotation) {
	double dStartTickCount = Helpers::GetExactTickCount();

	// Rotation can only be done in 32 bpp
	ConvertSrcTo4Channels();

	m_pLastDIB = NULL;
	if (m_bLDCOwned) delete m_pLDC; // LDC mask must be recalculated!
	m_pLDC = NULL;
	delete[] m_pDIBPixels; 
	m_pDIBPixels = NULL;
	delete[] m_pDIBPixelsLUTProcessed; 
	m_pDIBPixelsLUTProcessed = NULL;
	void* pNewIJL = CBasicProcessing::Rotate32bpp(m_nOrigWidth, m_nOrigHeight, m_pIJLPixels, nRotation);
	delete[] m_pIJLPixels;
	delete m_pThumbnail;
	m_pThumbnail = NULL;
	m_pIJLPixels = pNewIJL;
	m_ClippingSize = CSize(0, 0);
	if (nRotation != 180) {
		// swap width and height
		int nTemp = m_nOrigWidth;
		m_nOrigWidth = m_nOrigHeight;
		m_nOrigHeight = nTemp;
	}
	m_nRotation = (m_nRotation + nRotation) % 360;

	m_dLastOpTickCount = Helpers::GetExactTickCount() - dStartTickCount; 
}

void CJPEGImage::Crop(CRect cropRect) {
	// Cropping can only be done in 32 bpp
	ConvertSrcTo4Channels();

	m_pLastDIB = NULL;
	if (m_bLDCOwned) delete m_pLDC; // LDC mask must be recalculated!
	m_pLDC = NULL;
	delete[] m_pDIBPixels; 
	m_pDIBPixels = NULL;
	delete[] m_pDIBPixelsLUTProcessed; 
	m_pDIBPixelsLUTProcessed = NULL;
	void* pNewIJL = CBasicProcessing::Crop32bpp(m_nOrigWidth, m_nOrigHeight, m_pIJLPixels, cropRect);
	delete[] m_pIJLPixels;
	delete m_pThumbnail;
	m_pThumbnail = NULL;
	m_pIJLPixels = pNewIJL;
	m_ClippingSize = CSize(0, 0);
	m_nOrigWidth = cropRect.Width();
	m_nOrigHeight = cropRect.Height();
	m_bCropped = true;
}

void CJPEGImage::SetDimRects(const CDimRect* dimRects, int nSize) {
	bool bIdentical = false;
	if (m_pDIBPixelsLUTProcessed) {
		bool bCanReuseDIB = false;
		if (nSize >= m_nNumDimRects) {
			bCanReuseDIB = true;
			for (int i = 0; i < m_nNumDimRects; i++) {
				if (dimRects[i].Rect != m_pDimRects[i].Rect || dimRects[i].Factor != m_pDimRects[i].Factor) {
					bCanReuseDIB = false;
					break;
				}
			}
		}
		if (bCanReuseDIB) {
			bIdentical = nSize == m_nNumDimRects;
			// only dim the new rectangles
			for (int i = m_nNumDimRects; i < nSize; i++) {
				CBasicProcessing::DimRectangle32bpp(DIBWidth(), DIBHeight(), m_pDIBPixelsLUTProcessed,
					dimRects[i].Rect, dimRects[i].Factor);
			}
		} else {
			// force to recreate processed DIB on next access
			delete[] m_pDIBPixelsLUTProcessed;
			m_pDIBPixelsLUTProcessed = NULL;
			m_pLastDIB = NULL;
		}
	}
	if (!bIdentical) {
		delete[] m_pDimRects;
		m_pDimRects = NULL;
		m_nNumDimRects = nSize;
		if (nSize > 0) {
			m_pDimRects = new CDimRect[nSize];
			memcpy(m_pDimRects, dimRects, nSize*sizeof(CDimRect));
		}
	}
}

void CJPEGImage::EnableDimming(bool bEnable) {
	if (bEnable != m_bEnableDimming && m_pDimRects != NULL) {
		m_bEnableDimming = bEnable;
		delete[] m_pDIBPixelsLUTProcessed;
		m_pDIBPixelsLUTProcessed = NULL;
		m_pLastDIB = NULL;
	}
}

void* CJPEGImage::DIBPixelsLastProcessed() {
	if (m_pLastDIB == NULL) {
		m_pLastDIB = GetDIB(m_FullTargetSize, m_ClippingSize, m_TargetOffset, m_imageProcParams, m_eProcFlags);
	}
	return m_pLastDIB;
}

void CJPEGImage::VerifyDIBPixelsCreated() {
	if (m_pDIBPixels == NULL) {
		EResizeType eResizeType = GetResizeType(m_FullTargetSize, CSize(m_nOrigWidth, m_nOrigHeight));
		m_pDIBPixels = Resample(m_FullTargetSize, m_ClippingSize, m_TargetOffset, m_eProcFlags, m_imageProcParams.Sharpen, eResizeType);
	}
}

float CJPEGImage::IsNightShot() const {
	if (m_pLDC != NULL) {
		return m_pLDC->GetHistogram()->IsNightShot();
	} else {
		return -1;
	}
}

float CJPEGImage::IsSunset() const {
	if (m_pLDC != NULL) {
		return m_pLDC->IsSunset();
	} else {
		return -1;
	}
}

void CJPEGImage::SetInitialParameters(const CImageProcessingParams& imageProcParams, 
									  EProcessingFlags procFlags, int nRotation, double dZoom, CPoint offsets) {
	m_nInitialRotation = nRotation;
	m_eProcFlagsInitial = procFlags;
	m_imageProcParamsInitial = imageProcParams;
	m_dInitialZoom = dZoom;
	m_initialOffsets = offsets;
}

void CJPEGImage::RestoreInitialParameters(LPCTSTR sFileName, const CImageProcessingParams& imageProcParams, 
										  EProcessingFlags & procFlags, int nRotation, double dZoom, 
										  CPoint offsets, CSize targetSize) {
	// zoom and offsets must be initialized in all cases as they may be not in param DB even when
	// other parameters are
	m_dInitialZoom = dZoom;
	m_initialOffsets = offsets;

	CParameterDBEntry* dbEntry = CParameterDB::This().FindEntry(GetPixelHash());
	m_bInParamDB = dbEntry != NULL;
	m_bHasZoomStoredInParamDB = m_bInParamDB && dbEntry->HasZoomOffsetStored();
	bool bKeepParams = ::GetProcessingFlag(procFlags, PFLAG_KeepParams);
	if (m_bInParamDB && !bKeepParams) {
		dbEntry->WriteToProcessParams(m_imageProcParamsInitial, m_eProcFlagsInitial, m_nInitialRotation);
		dbEntry->GetColorCorrectionAmounts(m_fColorCorrectionFactors);

		dbEntry->WriteToGeometricParams(m_dInitialZoom, m_initialOffsets, SizeAfterRotation(m_nInitialRotation),
			targetSize);
	} else {
		m_nInitialRotation = nRotation;
		m_eProcFlagsInitial = bKeepParams ? procFlags : GetProcFlagsIncludeExcludeFolders(sFileName, procFlags);
		procFlags = m_eProcFlagsInitial;
		m_imageProcParamsInitial = imageProcParams;
		m_imageProcParamsInitial.LightenShadows *= m_fLightenShadowFactor;
	}
}

void CJPEGImage::GetFileParams(LPCTSTR sFileName, EProcessingFlags& eFlags, CImageProcessingParams& params) const {
	if (IsClipboardImage()) {
		return;
	}
	CParameterDBEntry* dbEntry = CParameterDB::This().FindEntry(GetPixelHash());
	if (m_bInParamDB) {
		int nDummy;
		if (!::GetProcessingFlag(eFlags, PFLAG_KeepParams)) {
			dbEntry->WriteToProcessParams(params, eFlags, nDummy);
		}
	} else {
		params.LightenShadows *= m_fLightenShadowFactor;
		if (!::GetProcessingFlag(eFlags, PFLAG_KeepParams)) {
			eFlags = GetProcFlagsIncludeExcludeFolders(sFileName, eFlags);
		}
	}
}

void CJPEGImage::SetFileDependentProcessParams(LPCTSTR sFileName, CProcessParams* pParams) {
	CParameterDBEntry* dbEntry = CParameterDB::This().FindEntry(GetPixelHash());
	m_bInParamDB = dbEntry != NULL;
	m_bHasZoomStoredInParamDB = m_bInParamDB && dbEntry->HasZoomOffsetStored();
	if (m_bInParamDB) {
		if (!::GetProcessingFlag(pParams->ProcFlags, PFLAG_KeepParams)) {
			dbEntry->WriteToProcessParams(pParams->ImageProcParams, pParams->ProcFlags, pParams->Rotation);
			dbEntry->GetColorCorrectionAmounts(m_fColorCorrectionFactors);
			dbEntry->WriteToGeometricParams(pParams->Zoom, pParams->Offsets, SizeAfterRotation(pParams->Rotation),
				CSize(pParams->TargetWidth, pParams->TargetHeight));
		}
	} else {
		pParams->ImageProcParams.LightenShadows *= m_fLightenShadowFactor;
		if (!::GetProcessingFlag(pParams->ProcFlags, PFLAG_KeepParams)) {
			pParams->ProcFlags = GetProcFlagsIncludeExcludeFolders(sFileName, pParams->ProcFlags);
		}
	}

	m_nInitialRotation = pParams->Rotation;
	m_dInitialZoom = pParams->Zoom;
	m_initialOffsets = pParams->Offsets;
	m_eProcFlagsInitial = pParams->ProcFlags;
	m_imageProcParamsInitial = pParams->ImageProcParams;
}

void CJPEGImage::DIBToOrig(float & fX, float & fY) {
	float fXo = m_TargetOffset.x + fX;
	float fYo = m_TargetOffset.y + fY;
	fX = fXo/m_FullTargetSize.cx*m_nOrigWidth;
	fY = fYo/m_FullTargetSize.cy*m_nOrigHeight;
}

void CJPEGImage::OrigToDIB(float & fX, float & fY) {
	float fXo = fX/m_nOrigWidth*m_FullTargetSize.cx;
	float fYo = fY/m_nOrigHeight*m_FullTargetSize.cy;
	fX = fXo - m_TargetOffset.x;
	fY = fYo - m_TargetOffset.y;
}

__int64 CJPEGImage::GetUncompressedPixelHash() const { 
	return (m_pLDC == NULL) ? 0 : m_pLDC->GetPixelHash(); 
}

///////////////////////////////////////////////////////////////////////////////////
// Private
///////////////////////////////////////////////////////////////////////////////////

void* CJPEGImage::ApplyCorrectionLUTandLDC(const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags,
										   void * & pCachedTargetDIB, CSize fullTargetSize, CPoint targetOffset, 
										   void * pSourceDIB, CSize dibSize,
										   bool bGeometryChanged, bool bOnlyCheck) {
	bool bAutoContrast = GetProcessingFlag(eProcFlags, PFLAG_AutoContrast);
	bool bAutoContrastOld = GetProcessingFlag(m_eProcFlags, PFLAG_AutoContrast);
	bool bAutoContrastSection = GetProcessingFlag(eProcFlags, PFLAG_AutoContrastSection) && bAutoContrast;
	bool bAutoContrastSectionOld = GetProcessingFlag(m_eProcFlags, PFLAG_AutoContrastSection);
	bool bLDC = GetProcessingFlag(eProcFlags, PFLAG_LDC);
	bool bLDCOld = GetProcessingFlag(m_eProcFlags, PFLAG_LDC);

	bool bNullLUTProcessings = fabs(imageProcParams.Contrast) < 1e-4 && fabs(imageProcParams.Gamma - 1) < 1e-4;
	bool bNoColorCastCorrection = fabs(imageProcParams.CyanRed) < 1e-4 && fabs(imageProcParams.MagentaGreen) < 1e-4 &&
		fabs(imageProcParams.YellowBlue) < 1e-4;
	bool bNoLUTApplied = bNullLUTProcessings && bNoColorCastCorrection && !bAutoContrast;
	bool bGlobalLUTChanged = fabs(imageProcParams.Contrast - m_imageProcParams.Contrast) > 1e-4 ||
		fabs(imageProcParams.Gamma - m_imageProcParams.Gamma) > 1e-4;
	bool bColorCastCorrChanged = fabs(imageProcParams.CyanRed - m_imageProcParams.CyanRed) > 1e-4 ||
		fabs(imageProcParams.MagentaGreen - m_imageProcParams.MagentaGreen) > 1e-4 ||
		fabs(imageProcParams.YellowBlue - m_imageProcParams.YellowBlue) > 1e-4;
	bool bCorrectionFactorChanged = fabs(imageProcParams.ColorCorrectionFactor-m_imageProcParams.ColorCorrectionFactor) > 1e-4 || 
		fabs(imageProcParams.ContrastCorrectionFactor-m_imageProcParams.ContrastCorrectionFactor) > 1e-4;
	bool bMustReapplyLUT = bAutoContrast != bAutoContrastOld || bAutoContrastSection != bAutoContrastSectionOld || bLDC != bLDCOld || 
		bCorrectionFactorChanged || bGlobalLUTChanged || bColorCastCorrChanged;
	bool bLDCParametersChanged = fabs(imageProcParams.LightenShadows - m_imageProcParams.LightenShadows) > 1e-4 ||
		fabs(imageProcParams.DarkenHighlights - m_imageProcParams.DarkenHighlights) > 1e-4 ||
		fabs(imageProcParams.LightenShadowSteepness - m_imageProcParams.LightenShadowSteepness) > 1e-4 ;
	bool bMustReapplyLDC = bLDC && (!bLDCOld || bGeometryChanged || bLDCParametersChanged);
	bool bMustUse3ChannelLUT = bAutoContrast || !bNoColorCastCorrection;
	bool bUseDimming = m_pDimRects != NULL && m_bEnableDimming;

	if (!bMustReapplyLUT && !bMustReapplyLDC && pCachedTargetDIB != NULL) {
		// consider special case that nothing is dimmed and no LUT and LDC is applied but
		// processed pixel is here, we do not want to use it - in this case we just continue.
		if (!(bNoLUTApplied && !bUseDimming && !bLDC)) {
			return pCachedTargetDIB;
		}
	}

	// If it shall only be checked if this method would be able to reuse the existing pCachedTargetDIB, return
	if (bOnlyCheck) {
		return NULL;
	}

	if (pSourceDIB == NULL) {
		return NULL;
	}

	// Recalculate LUTs if needed
	if (bGlobalLUTChanged || m_pLUTAllChannels == NULL) {
		delete[] m_pLUTAllChannels;
		m_pLUTAllChannels = bNullLUTProcessings ? NULL : CBasicProcessing::CreateSingleChannelLUT(imageProcParams.Contrast, imageProcParams.Gamma);
	}

	// Calculate LDC if needed
	if (m_pLDC == NULL) {
		m_pLDC = new CLocalDensityCorr(*this, true);
		bLDCParametersChanged = true;
	}
	if (bLDC) {
		// maybe only partially constructed, we need fully constructed object here
		m_pLDC->VerifyFullyConstructed();
	}
	if (bLDCParametersChanged && m_pLDC->IsMaskAvailable()) {
		m_pLDC->SetLDCAmount(imageProcParams.LightenShadows, imageProcParams.DarkenHighlights);
	}

	// Recalculate special histogram if needed
	const CHistogram* pHistogram = m_pLDC->GetHistogram();
	bool bSpecialHistogram = false;
	if (bMustUse3ChannelLUT) {
		if (bAutoContrast && bAutoContrastSection && m_bLDCOwned && (!bAutoContrastSectionOld || bCorrectionFactorChanged || bColorCastCorrChanged)) {
			pHistogram = new CHistogram(*this, false);
			bSpecialHistogram = true;
			delete[] m_pLUTRGB;
			m_pLUTRGB = NULL;
		}
	}
	if (bMustUse3ChannelLUT && (m_pLUTRGB == NULL || bCorrectionFactorChanged || bColorCastCorrChanged ||
		bAutoContrast != bAutoContrastOld)) {
		delete[] m_pLUTRGB;
		float fColorCastCorrs[3];
		fColorCastCorrs[0] = (float) imageProcParams.CyanRed;
		fColorCastCorrs[1] = (float) imageProcParams.MagentaGreen;
		fColorCastCorrs[2] = (float) imageProcParams.YellowBlue;
		float fColorCorrFactor = bAutoContrast ? (float) imageProcParams.ColorCorrectionFactor : 0.0f;
		float fBrightnessCorrFactor = bAutoContrast ? 1.0f : 0.0f;
		float fContrastCorrFactor = bAutoContrast ? (float) imageProcParams.ContrastCorrectionFactor : 0.0f;
		m_pLUTRGB = CHistogramCorr::CalculateCorrectionLUT(*pHistogram, fColorCorrFactor, fBrightnessCorrFactor,
			fColorCastCorrs, bAutoContrast ? m_fColorCorrectionFactors : m_fColorCorrectionFactorsNull, fContrastCorrFactor);
	} else if (!bMustUse3ChannelLUT) {
		delete[] m_pLUTRGB;
		m_pLUTRGB = NULL;
	}
	
	delete[] pCachedTargetDIB;
	pCachedTargetDIB = NULL;
	if (bSpecialHistogram) {
		delete pHistogram;
	}

	if (!bNoLUTApplied || bLDC) {
		// LUT or/and LDC --> apply correction
		uint8* pLUT = CHistogramCorr::CombineLUTs(m_pLUTAllChannels, m_pLUTRGB);
		if (bLDC) {
			pCachedTargetDIB = CBasicProcessing::ApplyLDC32bpp(fullTargetSize, targetOffset, dibSize, m_pLDC->GetLDCMapSize(),
				pSourceDIB, pLUT, m_pLDC->GetLDCMap(), m_pLDC->GetBlackPt(), m_pLDC->GetWhitePt(), (float)imageProcParams.LightenShadowSteepness);
		} else {
			pCachedTargetDIB = CBasicProcessing::Apply3ChannelLUT32bpp(dibSize.cx, dibSize.cy, pSourceDIB, pLUT);
		}
		delete[] pLUT;
	} else if (!bUseDimming) {
		// no LUT, no LDC, no dimming --> return original pixels
		return pSourceDIB;
	} else {
		// no LUT, no LDC but dimming --> make copy of original pixels
		pCachedTargetDIB = new uint32[dibSize.cx*dibSize.cy];
		memcpy(pCachedTargetDIB, pSourceDIB, dibSize.cx*dibSize.cy*4);
	}

	if (bUseDimming) {
		for (int i = 0; i < m_nNumDimRects; i++) {
			CBasicProcessing::DimRectangle32bpp(dibSize.cx, dibSize.cy, pCachedTargetDIB, 
				m_pDimRects[i].Rect, m_pDimRects[i].Factor);
		}
	}

	return pCachedTargetDIB;
}

void CJPEGImage::ConvertSrcTo4Channels() {
	if (m_nIJLChannels == 3) {
		void* pNewIJL = CBasicProcessing::Convert3To4Channels(m_nOrigWidth, m_nOrigHeight, m_pIJLPixels);
		delete[] m_pIJLPixels;
		m_pIJLPixels = pNewIJL;
		m_nIJLChannels = 4;
	}
}

EProcessingFlags CJPEGImage::GetProcFlagsIncludeExcludeFolders(LPCTSTR sFileName, EProcessingFlags procFlags) const {
	EProcessingFlags eFlags = procFlags;
	CSettingsProvider& sp = CSettingsProvider::This();
	LPCTSTR sPatternInclude;
	LPCTSTR sPatternExclude;

	bool bIncludeACCMatch = Helpers::PatternMatch(sPatternInclude, sFileName, sp.ACCInclude());
	bool bExcludeACCMatch = Helpers::PatternMatch(sPatternExclude, sFileName, sp.ACCExclude());
	if (bIncludeACCMatch && bExcludeACCMatch) {
		// Take more specific pattern if both match
		int nSpec = Helpers::FindMoreSpecificPattern(sPatternInclude, sPatternExclude);
		if (nSpec == -1) {
			bIncludeACCMatch = false; // Exclude is more specific
		} else {
			bExcludeACCMatch = false; // Include is more specific (or no difference)
		}
	}
	if (bIncludeACCMatch || bExcludeACCMatch) {
		eFlags = SetProcessingFlag(eFlags, PFLAG_AutoContrast, bIncludeACCMatch);
	}

	bool bIncludeLDCMatch = Helpers::PatternMatch(sPatternInclude, sFileName, sp.LDCInclude());
	bool bExcludeLDCMatch = Helpers::PatternMatch(sPatternExclude, sFileName, sp.LDCExclude());
	if (bIncludeLDCMatch && bExcludeLDCMatch) {
		// Take more specific pattern if both match
		int nSpec = Helpers::FindMoreSpecificPattern(sPatternInclude, sPatternExclude);
		if (nSpec == -1) {
			bIncludeLDCMatch = false; // Exclude is more specific
		} else {
			bExcludeLDCMatch = false; // Include is more specific (or no difference)
		}
	}
	if (bIncludeLDCMatch || bExcludeLDCMatch) {
		eFlags = SetProcessingFlag(eFlags, PFLAG_LDC, bIncludeLDCMatch);
	}

	return eFlags;
}

CSize CJPEGImage::SizeAfterRotation(int nRotation) {
	int nDiff = ((nRotation - m_nRotation) + 360) % 360;
	if (nDiff == 90 || nDiff == 270) {
		return CSize(m_nOrigHeight, m_nOrigWidth);
	} else {
		return CSize(m_nOrigWidth, m_nOrigHeight);
	}
}

CJPEGImage::EResizeType CJPEGImage::GetResizeType(CSize targetSize, CSize sourceSize) {
	if (targetSize.cx == sourceSize.cx && targetSize.cy == sourceSize.cy) {
		return NoResize;
	} else if (targetSize.cx <= sourceSize.cx && targetSize.cy <= sourceSize.cy) {
		return DownSample;
	} else {
		return UpSample;
	}
}
