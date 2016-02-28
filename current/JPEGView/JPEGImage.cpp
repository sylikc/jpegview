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
#include "RawMetadata.h"
#include "MaxImageDef.h"
#include "libjpeg-turbo\include\turbojpeg.h"
#include <math.h>
#include <assert.h>

///////////////////////////////////////////////////////////////////////////////////
// Static helpers
///////////////////////////////////////////////////////////////////////////////////

static void RotateInplace(const CSize& imageSize, double& dX, double& dY, double dAngle) {
	dX -= (imageSize.cx - 1) * 0.5;
	dY -= (imageSize.cy - 1) * 0.5;
	double dXr = cos(dAngle) * dX - sin(dAngle) * dY;
	double dYr = sin(dAngle) * dX + cos(dAngle) * dY;
	dX = dXr;
	dY = dYr;
}

///////////////////////////////////////////////////////////////////////////////////
// Public interface
///////////////////////////////////////////////////////////////////////////////////

CJPEGImage::CJPEGImage(int nWidth, int nHeight, void* pIJLPixels, void* pEXIFData, int nChannels, __int64 nJPEGHash, 
					   EImageFormat eImageFormat, bool bIsAnimation, int nFrameIndex, int nNumberOfFrames, int nFrameTimeMs,
					   CLocalDensityCorr* pLDC, bool bIsThumbnailImage, CRawMetadata* pRawMetadata) : m_rotationParams(0) {
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

	m_pRawMetadata = pRawMetadata;

	m_nPixelHash = nJPEGHash;
	m_eImageFormat = eImageFormat;
	m_bIsAnimation = bIsAnimation;
	m_nFrameIndex = nFrameIndex;
	m_nNumberOfFrames = nNumberOfFrames;
	m_nFrameTimeMs = nFrameTimeMs;
	m_eJPEGChromoSampling = TJSAMP_420;

	m_nOrigWidth = m_nInitOrigWidth = nWidth;
	m_nOrigHeight = m_nInitOrigHeight = nHeight;
	m_pDIBPixels = NULL;
	m_pDIBPixelsLUTProcessed = NULL;
	m_pLastDIB = NULL;
	m_pThumbnail = NULL;
	m_pHistogramThumbnail = NULL;
	m_pGrayImage = NULL;
	m_pSmoothGrayImage = NULL;
	
	m_pLUTAllChannels = NULL;
	m_pLUTRGB = NULL;
	m_pSaturationLUTs = NULL;
	m_eProcFlags = PFLAG_None;
	m_eProcFlagsInitial = PFLAG_None;
	m_nInitialRotation = 0;
	m_dInitialZoom = -1;
	m_initialOffsets = CPoint(0, 0);
	m_pDimRects = 0;
	m_nNumDimRects = 0;
	m_bEnableDimming = true;
	m_bShowGrid = false;
	m_bInParamDB = false;
	m_bHasZoomStoredInParamDB = false;
	m_bUnsharpMaskParamsValid = false;
	m_bIsThumbnailImage = bIsThumbnailImage;
	m_pCachedProcessedHistogram = NULL;

	m_bCropped = false;
	m_bIsDestructivlyProcessed = false;
	m_bIsProcessedNoParamDB = false;
	m_bRotationByEXIF = false;
	m_bFirstReprocessing = true;
	m_dLastOpTickCount = 0;
	m_dLoadTickCount = 0;
	m_dUnsharpMaskTickCount = 0;
	m_FullTargetSize = CSize(0, 0);
	m_ClippingSize = CSize(0, 0);
	m_TargetOffset = CPoint(0, 0);
	m_dRotationLQ = 0.0;
	m_bTrapezoidValid = false;

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
	delete[] m_pGrayImage;
	m_pGrayImage = NULL;
	delete[] m_pSmoothGrayImage;
	m_pSmoothGrayImage = NULL;
	delete[] m_pLUTAllChannels;
	m_pLUTAllChannels = NULL;
	delete[] m_pLUTRGB;
	m_pLUTRGB = NULL;
	delete[] m_pSaturationLUTs;
	m_pSaturationLUTs = NULL;
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
	delete m_pHistogramThumbnail;
	m_pHistogramThumbnail = NULL;
	delete m_pCachedProcessedHistogram;
	m_pCachedProcessedHistogram = NULL;
	delete m_pRawMetadata;
	m_pRawMetadata = NULL;
}

bool CJPEGImage::CanUseLosslessJPEGTransformations() {
	return m_eImageFormat == IF_JPEG && (m_nOrigWidth % tjMCUWidth[m_eJPEGChromoSampling]) == 0 &&
		(m_nOrigHeight % tjMCUHeight[m_eJPEGChromoSampling]) == 0;
}

void CJPEGImage::TrimRectToMCUBlockSize(CRect& rect) {
	int nBlockWidth = tjMCUWidth[m_eJPEGChromoSampling];
	rect.left = rect.left & ~(nBlockWidth - 1);
	rect.right = (rect.right + (nBlockWidth - 1)) & ~(nBlockWidth - 1);
	if (rect.right > m_nOrigWidth) {
		rect.right -= nBlockWidth; 
	}

	int nBlockHeight = tjMCUHeight[m_eJPEGChromoSampling];
	rect.top = rect.top & ~(nBlockHeight - 1);
	rect.bottom = (rect.bottom + (nBlockHeight - 1)) & ~(nBlockHeight - 1);
	if (rect.bottom > m_nOrigHeight) {
		rect.bottom -= nBlockHeight; 
	}
}

void* CJPEGImage::GetThumbnailDIB(CSize size, const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags) {
	return GetThumbnailDIB(size, size, CPoint(0, 0), imageProcParams, eProcFlags);
}

void* CJPEGImage::GetThumbnailDIB(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset, const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags) {
	assert(!m_bIsThumbnailImage);
	if (m_pThumbnail == NULL) {
		m_pThumbnail = CreateThumbnailImage();
	}
	return m_pThumbnail->GetDIB(fullTargetSize, clippingSize, targetOffset, imageProcParams, eProcFlags);
}

void* CJPEGImage::GetThumbnailDIBRotated(CSize size, const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags, double dRotation) {
	assert(!m_bIsThumbnailImage);
	if (m_pThumbnail == NULL) {
		m_pThumbnail = CreateThumbnailImage();
	}
	return m_pThumbnail->GetDIBRotated(size, size, CPoint(0, 0), imageProcParams, eProcFlags, dRotation, false);
}

void* CJPEGImage::GetThumbnailDIBTrapezoid(CSize size, const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags, const CTrapezoid& trapezoid) {
	assert(!m_bIsThumbnailImage);
	if (m_pThumbnail == NULL) {
		m_pThumbnail = CreateThumbnailImage();
	}
	return m_pThumbnail->GetDIBTrapezoid(size, size, CPoint(0, 0), imageProcParams, eProcFlags, &trapezoid, false);
}

void* CJPEGImage::GetDIBUnsharpMasked(CSize clippingSize, CPoint targetOffset,
									  const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags, 
									  const CUnsharpMaskParams & unsharpMaskParams) {
	assert(!m_bIsThumbnailImage);
	bool bUseUnsharpMask = unsharpMaskParams.Amount > 0 && unsharpMaskParams.Radius > 0;
	bool bParametersChanged;
	return GetDIBInternal(CSize(m_nOrigWidth, m_nOrigHeight), clippingSize, targetOffset, imageProcParams, 
		eProcFlags, bUseUnsharpMask ? &unsharpMaskParams : NULL, NULL, 0.0, false, bParametersChanged);
}

const CHistogram* CJPEGImage::GetOriginalHistogram() {
	assert(!m_bIsThumbnailImage);
	if (m_pLDC == NULL) {
		m_pLDC = new CLocalDensityCorr(*this, true);
	}
	return m_pLDC->GetHistogram();
}

const CHistogram* CJPEGImage::GetProcessedHistogram() {
	assert(!m_bIsThumbnailImage);
	if (m_pHistogramThumbnail == NULL) {
		m_pHistogramThumbnail = CreateThumbnailImage();
	}
	return (m_imageProcParams.Contrast == -1) ? NULL : 
		m_pHistogramThumbnail->GetHistogramOfProcessedDIB(
		CImageProcessingParams(m_imageProcParams.Contrast, m_imageProcParams.Gamma, m_imageProcParams.Saturation,
		0.0, m_imageProcParams.ColorCorrectionFactor, m_imageProcParams.ContrastCorrectionFactor, 
		m_imageProcParams.LightenShadows, m_imageProcParams.DarkenHighlights, m_imageProcParams.LightenShadowSteepness, 
		m_imageProcParams.CyanRed, m_imageProcParams.MagentaGreen, m_imageProcParams.YellowBlue), 
		SetProcessingFlag(m_eProcFlags, PFLAG_HighQualityResampling, false));
}

const CHistogram* CJPEGImage::GetHistogramOfProcessedDIB(const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags) {
	assert(m_bIsThumbnailImage);
	CSize origSize(m_nOrigWidth, m_nOrigHeight);
	bool bParametersChanged;
	void* pDIBPixels = GetDIBInternal(origSize, origSize, CPoint(0, 0), imageProcParams, eProcFlags, NULL, NULL, 0.0, false, bParametersChanged);
	if (bParametersChanged || m_pCachedProcessedHistogram == NULL) {
		delete m_pCachedProcessedHistogram;
		m_pCachedProcessedHistogram = NULL;
		if (pDIBPixels == NULL) {
			return NULL;
		}
		m_pCachedProcessedHistogram = new CHistogram(pDIBPixels, origSize);
	}
	return m_pCachedProcessedHistogram;
}

void CJPEGImage::FreeUnsharpMaskResources() {
	delete[] m_pGrayImage;
	m_pGrayImage = NULL;
	delete[] m_pSmoothGrayImage;
	m_pSmoothGrayImage = NULL;
}

bool CJPEGImage::ApplyUnsharpMaskToOriginalPixels(const CUnsharpMaskParams & unsharpMaskParams) {
	InvalidateAllCachedPixelData();

	double dStartTime = Helpers::GetExactTickCount();

	bool bSuccess = false;
	int16* pGray = CBasicProcessing::Create1Channel16bppGrayscaleImage(m_nOrigWidth, m_nOrigHeight, m_pIJLPixels, m_nIJLChannels);
	if (pGray != NULL) {
		int16* pSmoothed = CBasicProcessing::GaussFilter16bpp1Channel(CSize(m_nOrigWidth, m_nOrigHeight), CPoint(0, 0), 
			CSize(m_nOrigWidth, m_nOrigHeight), unsharpMaskParams.Radius, pGray);
		if (pSmoothed != NULL) {
			bSuccess = NULL != CBasicProcessing::UnsharpMask(CSize(m_nOrigWidth, m_nOrigHeight), CPoint(0,0), CSize(m_nOrigWidth, m_nOrigHeight), 
				unsharpMaskParams.Amount, unsharpMaskParams.Threshold, pGray, pSmoothed, m_pIJLPixels, m_pIJLPixels, m_nIJLChannels);
		}
		delete[] pSmoothed;
	}
	delete[] pGray;

	m_dUnsharpMaskTickCount = Helpers::GetExactTickCount() - dStartTime;

	MarkAsDestructivelyProcessed();
	m_bIsProcessedNoParamDB = true;

	return bSuccess;
}

bool CJPEGImage::RotateOriginalPixels(double dRotation, bool bAutoCrop, bool bKeepAspectRatio) {
	InvalidateAllCachedPixelData();

	CPoint offset;
	CSize newSize = GetSizeAfterFreeRotation(CSize(m_nOrigWidth, m_nOrigHeight), dRotation, bAutoCrop, bKeepAspectRatio, offset);
	void* pRotatedPixels = CBasicProcessing::RotateHQ(offset, newSize, dRotation,
		CSize(m_nOrigWidth, m_nOrigHeight), m_pIJLPixels, m_nIJLChannels, CSettingsProvider::This().ColorBackground());
	if (pRotatedPixels == NULL) return false;
	delete[] m_pIJLPixels;

	m_nOrigWidth = newSize.cx;
	m_nOrigHeight = newSize.cy;
	m_nIJLChannels = 4;
	m_pIJLPixels = pRotatedPixels;
	MarkAsDestructivelyProcessed();

	m_rotationParams.FreeRotation = fmod(360 * dRotation / (2 * 3.141592653), 360);
	m_rotationParams.Flags = SetRotationFlag(m_rotationParams.Flags, RFLAG_AutoCrop, bAutoCrop);
	m_rotationParams.Flags = SetRotationFlag(m_rotationParams.Flags, RFLAG_KeepAspectRatio, bKeepAspectRatio);

	return true;
}

bool CJPEGImage::TrapezoidOriginalPixels(const CTrapezoid& trapezoid, bool bAutoCrop, bool bKeepAspectRatio) {
	InvalidateAllCachedPixelData();

	int nXStart, nXEnd;
	int nYStart = trapezoid.y1, nYEnd = trapezoid.y2;

	if (bAutoCrop) {
		if (bKeepAspectRatio) {
			CRect rect = Helpers::CalculateMaxIncludedRectKeepAR(trapezoid, (double)m_nOrigWidth / m_nOrigHeight);
			nXStart = rect.left; nXEnd = rect.right;
			nYStart = rect.top; nYEnd = rect.bottom;
		} else {
			// Calculate the maximum included rectangle
			nXStart = max(trapezoid.x1s, trapezoid.x2s);
			nXEnd = min(trapezoid.x1e, trapezoid.x2e);
		}
	} else {
		nXStart = min(trapezoid.x1s, trapezoid.x2s);
		nXEnd = max(trapezoid.x1e, trapezoid.x2e);
	}

	if (nXStart >= nXEnd) {
		return false;
	}

	CSize newSize(nXEnd - nXStart + 1, nYEnd - nYStart + 1);
	void* pTransformedPixels = CBasicProcessing::TrapezoidHQ(CPoint(nXStart, nYStart), newSize, trapezoid, 
		CSize(m_nOrigWidth, m_nOrigHeight), m_pIJLPixels, m_nIJLChannels, CSettingsProvider::This().ColorBackground());
	if (pTransformedPixels == NULL) return false;
	delete[] m_pIJLPixels;

	m_nOrigWidth = newSize.cx;
	m_nOrigHeight = newSize.cy;
	m_nIJLChannels = 4;
	m_pIJLPixels = pTransformedPixels;
	MarkAsDestructivelyProcessed();
	m_bIsProcessedNoParamDB = true;

	return true;
}

bool CJPEGImage::ResizeOriginalPixels(EResizeFilter filter, CSize newSize) {
	int newWidth = newSize.cx, newHeight = newSize.cy;
	if (newWidth <= 0 || newHeight <= 0) {
		return false;
	}
	if (((long long)newWidth) * newHeight > MAX_IMAGE_PIXELS) {
		return false;
	}
	if (newWidth > MAX_IMAGE_DIMENSION || newHeight > MAX_IMAGE_DIMENSION) {
		return false;
	}
	if (newWidth == m_nOrigWidth && newHeight == m_nOrigHeight) {
		return true;
	}

	InvalidateAllCachedPixelData();

	void* pResizedPixels = m_pIJLPixels;
	int currentWidth = m_nOrigWidth;
	int currentHeight = m_nOrigHeight;
	int channels = m_nIJLChannels;
	double totalFactor = (double)currentWidth / newWidth;
	int steps = (totalFactor > 5) ? (int)ceil(log(totalFactor) / log(5.0)) : 1;
	double factor = (steps > 1) ? pow(totalFactor, 1.0 / steps) : 1.0;
	for (int i = 0; i < steps; i++) {
		EResizeFilter usedFilter;
		int oldWidth = currentWidth, oldHeight = currentHeight;
		if (i != steps - 1 && filter != Resize_PointFilter) {
			currentWidth = (int)(currentWidth / factor);
			currentHeight = (int)(currentHeight / factor);
			usedFilter = (filter == Resize_SharpenMedian) ? Resize_SharpenLow : Resize_NoAliasing;
		} else {
			currentWidth = newWidth;
			currentHeight = newHeight;
			usedFilter = filter;
		}
		void* pOldPixels = pResizedPixels;
		pResizedPixels = InternalResize(pResizedPixels, channels, usedFilter, CSize(currentWidth, currentHeight), CSize(oldWidth, oldHeight));
		delete[] pOldPixels;
		channels = 4;
	}

	m_nOrigWidth = newWidth;
	m_nOrigHeight = newHeight;
	m_nIJLChannels = 4;
	m_pIJLPixels = pResizedPixels;
	MarkAsDestructivelyProcessed();
	m_bIsProcessedNoParamDB = true;

	return true;
}

void CJPEGImage::ResampleWithPan(void* & pDIBPixels, void* & pDIBPixelsLUTProcessed, CSize fullTargetSize, 
								 CSize clippingSize, CPoint targetOffset, CRect oldClippingRect,
								 EProcessingFlags eProcFlags, const CImageProcessingParams & imageProcParams, 
								 double dRotation, EResizeType eResizeType) {
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
			targetOffset, pDIBPixels, clippingSize, false, true, false) != NULL && (m_pDimRects == NULL || !m_bEnableDimming);

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
			void* pTop = Resample(fullTargetSize, clipSize, targetOffset, eProcFlags, imageProcParams.Sharpen, dRotation, eResizeType);
			
			if (!bCanUseLUTProcDIB) {
				CBasicProcessing::CopyRect32bpp(pPannedPixels, pTop,
					clippingSize, CRect(CPoint(0, 0), clipSize),
					clipSize, CRect(CPoint(0, 0), clipSize));
			} else {
				void* pTopProc = NULL;
				ApplyCorrectionLUTandLDC(imageProcParams, eProcFlags, pTopProc, fullTargetSize, targetOffset, pTop, clipSize, false, false, false);
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
			void* pBottom = Resample(fullTargetSize, clipSize, offset, eProcFlags, imageProcParams.Sharpen, dRotation, eResizeType);
			
			if (!bCanUseLUTProcDIB) {
				CBasicProcessing::CopyRect32bpp(pPannedPixels, pBottom,
					clippingSize, CRect(CPoint(0, targetRect.bottom), clipSize),
					clipSize, CRect(CPoint(0, 0), clipSize));
			} else {
				void* pBottomProc = NULL;
				ApplyCorrectionLUTandLDC(imageProcParams, eProcFlags, pBottomProc, fullTargetSize, offset, pBottom, clipSize, false, false, false);
				CBasicProcessing::CopyRect32bpp(pPannedPixelsLUTProcessed, pBottomProc,
					clippingSize, CRect(CPoint(0, targetRect.bottom), clipSize),
					clipSize, CRect(CPoint(0, 0), clipSize));
				delete[] pBottomProc;
			}

			delete[] pBottom;
		}
		if (targetRect.left > 0) {
			CSize clipSize(targetRect.left, clippingSize.cy);
			void* pLeft = Resample(fullTargetSize, clipSize, targetOffset, eProcFlags, imageProcParams.Sharpen, dRotation, eResizeType);
			
			if (!bCanUseLUTProcDIB) {
				CBasicProcessing::CopyRect32bpp(pPannedPixels, pLeft,
					clippingSize, CRect(CPoint(0, 0), clipSize),
					clipSize, CRect(CPoint(0, 0), clipSize));
			} else {
				void* pLeftProc = NULL;
				ApplyCorrectionLUTandLDC(imageProcParams, eProcFlags, pLeftProc, fullTargetSize, targetOffset, pLeft, clipSize, false, false, false);
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
			void* pRight = Resample(fullTargetSize, clipSize, offset, eProcFlags, imageProcParams.Sharpen, dRotation, eResizeType);
			
			if (!bCanUseLUTProcDIB) {
				CBasicProcessing::CopyRect32bpp(pPannedPixels, pRight,
					clippingSize, CRect(CPoint(targetRect.right, 0), clipSize),
					clipSize, CRect(CPoint(0, 0), clipSize));
			} else {
				void* pRigthProc = NULL;
				ApplyCorrectionLUTandLDC(imageProcParams, eProcFlags, pRigthProc, fullTargetSize, offset, pRight, clipSize, false, false, false);
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
						  EProcessingFlags eProcFlags, double dSharpen, double dRotation, EResizeType eResizeType) {
	Helpers::CPUType cpu = CSettingsProvider::This().AlgorithmImplementation();
	EFilterType filter = CSettingsProvider::This().DownsamplingFilter();

	if (fullTargetSize.cx > 65535 || fullTargetSize.cy > 65535) return NULL;

	if (GetProcessingFlag(eProcFlags, PFLAG_HighQualityResampling) && 
		!(eResizeType == NoResize && (filter == Filter_Downsampling_Best_Quality || filter == Filter_Downsampling_No_Aliasing))) {
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
		bool bHasRotation = fabs(dRotation) > 1e-3;
		if (bHasRotation) {
			return CBasicProcessing::PointSampleWithRotation(fullTargetSize, targetOffset, clippingSize, 
				CSize(m_nOrigWidth, m_nOrigHeight), dRotation, m_pIJLPixels, m_nIJLChannels, CSettingsProvider::This().ColorBackground());
		} else {
			return CBasicProcessing::PointSample(fullTargetSize, targetOffset, clippingSize, 
				CSize(m_nOrigWidth, m_nOrigHeight), m_pIJLPixels, m_nIJLChannels);
		}
	}
}

void* CJPEGImage::InternalResize(void* pixels, int channels, EResizeFilter filter, CSize targetSize, CSize sourceSize) {
	EResizeType eResizeType = GetResizeType(targetSize, sourceSize);
	Helpers::CPUType cpu = CSettingsProvider::This().AlgorithmImplementation();

	if (filter == Resize_PointFilter) {
		return CBasicProcessing::PointSample(targetSize, CPoint(0, 0), targetSize, sourceSize, pixels, channels);
	}

	EFilterType downSamplingFilter = (filter == Resize_NoAliasing) ? Filter_Downsampling_No_Aliasing : Filter_Downsampling_Best_Quality;
	double dSharpen = (filter == Resize_SharpenLow) ? 0.15 : (filter == Resize_SharpenMedian) ? 0.3 : 0.0;

	if (cpu == Helpers::CPU_SSE || cpu == Helpers::CPU_MMX) {
		if (eResizeType == UpSample) {
			return CBasicProcessing::SampleUp_HQ_SSE_MMX(targetSize, CPoint(0, 0), targetSize,
				sourceSize, pixels, channels, cpu == Helpers::CPU_SSE);
		} else {
			return CBasicProcessing::SampleDown_HQ_SSE_MMX(targetSize, CPoint(0, 0), targetSize,
				sourceSize, pixels, channels, dSharpen, downSamplingFilter, cpu == Helpers::CPU_SSE);
		}
	} else {
		if (eResizeType == UpSample) {
			return CBasicProcessing::SampleUp_HQ(targetSize, CPoint(0, 0), targetSize,
				sourceSize, pixels, channels);
		} else {
			return CBasicProcessing::SampleDown_HQ(targetSize, CPoint(0, 0), targetSize,
				sourceSize, pixels, channels, dSharpen, downSamplingFilter);
		}
	}
}

CPoint CJPEGImage::ConvertOffset(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset) {
	int nStartX = (fullTargetSize.cx - clippingSize.cx)/2 - targetOffset.x;
	int nStartY = (fullTargetSize.cy - clippingSize.cy)/2 - targetOffset.y;
	return CSize(nStartX, nStartY);
}

bool CJPEGImage::VerifyRotation(const CRotationParams& rotationParams) {
	// First integer rotation (fast)
	int nDiff = ((rotationParams.Rotation - m_rotationParams.Rotation) + 360) % 360;
	if (nDiff != 0) {
		if (!Rotate(nDiff)) return false;
	}
	if (fabs(rotationParams.FreeRotation - m_rotationParams.FreeRotation) >= 0.009)
	{
		return RotateOriginalPixels(2 * 3.141592653  * rotationParams.FreeRotation / 360, 
			GetRotationFlag(rotationParams.Flags, RFLAG_AutoCrop), GetRotationFlag(rotationParams.Flags, RFLAG_KeepAspectRatio));
	}
	return true;
}

bool CJPEGImage::Rotate(int nRotation) {
	double dStartTickCount = Helpers::GetExactTickCount();

	// Rotation can only be done in 32 bpp
	if (!ConvertSrcTo4Channels()) {
		return false;
	}

	InvalidateAllCachedPixelData();
	void* pNewIJL = CBasicProcessing::Rotate32bpp(m_nOrigWidth, m_nOrigHeight, m_pIJLPixels, nRotation);
	if (pNewIJL == NULL) return false;
	delete[] m_pIJLPixels;
	m_pIJLPixels = pNewIJL;
	if (nRotation != 180) {
		// swap width and height
		int nTemp = m_nOrigWidth;
		m_nOrigWidth = m_nOrigHeight;
		m_nOrigHeight = nTemp;
	}
	m_rotationParams.Rotation = (m_rotationParams.Rotation + nRotation) % 360;

	m_dLastOpTickCount = Helpers::GetExactTickCount() - dStartTickCount;
	return true;
}

bool CJPEGImage::Mirror(bool bHorizontally) {
	double dStartTickCount = Helpers::GetExactTickCount();

	// Rotation can only be done in 32 bpp
	if (!ConvertSrcTo4Channels()) {
		return false;
	}

	InvalidateAllCachedPixelData();
	void* pNewIJL = bHorizontally ? CBasicProcessing::MirrorH32bpp(m_nOrigWidth, m_nOrigHeight, m_pIJLPixels) :
		CBasicProcessing::MirrorV32bpp(m_nOrigWidth, m_nOrigHeight, m_pIJLPixels);
	if (pNewIJL == NULL) return false;
	delete[] m_pIJLPixels;
	m_pIJLPixels = pNewIJL;
	MarkAsDestructivelyProcessed();
	m_bIsProcessedNoParamDB = true;

	m_dLastOpTickCount = Helpers::GetExactTickCount() - dStartTickCount;
	return true;
}

bool CJPEGImage::Crop(CRect cropRect) {
	// Cropping can only be done in 32 bpp
	if (!ConvertSrcTo4Channels()) {
		return false;
	}

	InvalidateAllCachedPixelData();
	void* pNewIJL = CBasicProcessing::Crop32bpp(m_nOrigWidth, m_nOrigHeight, m_pIJLPixels, cropRect);
	if (pNewIJL == NULL) {
		return false;
	}
	delete[] m_pIJLPixels;
	m_pIJLPixels = pNewIJL;
	m_nOrigWidth = cropRect.Width();
	m_nOrigHeight = cropRect.Height();
	m_bCropped = true;
	MarkAsDestructivelyProcessed();
	m_bIsProcessedNoParamDB = true;

	return true;
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

void* CJPEGImage::DIBPixelsLastProcessed(bool bGenerateDIBIfNeeded) {
	if (bGenerateDIBIfNeeded && m_pLastDIB == NULL) {
		m_pLastDIB = GetDIB(m_FullTargetSize, m_ClippingSize, m_TargetOffset, m_imageProcParams, m_eProcFlags);
	}
	return m_pLastDIB;
}

void CJPEGImage::VerifyDIBPixelsCreated() {
	if (m_pDIBPixels == NULL) {
		EResizeType eResizeType = GetResizeType(m_FullTargetSize, CSize(m_nOrigWidth, m_nOrigHeight));
		m_pDIBPixels = Resample(m_FullTargetSize, m_ClippingSize, m_TargetOffset, m_eProcFlags, m_imageProcParams.Sharpen, m_dRotationLQ, eResizeType);
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
										  CPoint offsets, CSize targetSize, CSize monitorSize) {
	// zoom and offsets must be initialized in all cases as they may be not in param DB even when
	// other parameters are
	m_dInitialZoom = dZoom;
	m_initialOffsets = offsets;

	CParameterDBEntry* dbEntry = CParameterDB::This().FindEntry(GetPixelHash());
	m_bInParamDB = dbEntry != NULL;
	m_bHasZoomStoredInParamDB = m_bInParamDB && dbEntry->HasZoomOffsetStored();
	bool bKeepParams = ::GetProcessingFlag(procFlags, PFLAG_KeepParams);
	if (m_bInParamDB && !bKeepParams) {
		dbEntry->WriteToProcessParams(m_imageProcParamsInitial, m_eProcFlagsInitial, CRotationParams(m_nInitialRotation));
		dbEntry->GetColorCorrectionAmounts(m_fColorCorrectionFactors);

		dbEntry->WriteToGeometricParams(m_dInitialZoom, m_initialOffsets, SizeAfterRotation(CRotationParams(m_rotationParams, m_nInitialRotation)),
			dbEntry->IsStoredRelativeToScreenSize() ? monitorSize : targetSize);
	} else {
		m_nInitialRotation = GetRotationFromEXIF(nRotation);
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
		CRotationParams notUsed(0);
		if (!::GetProcessingFlag(eFlags, PFLAG_KeepParams)) {
			dbEntry->WriteToProcessParams(params, eFlags, notUsed);
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
			dbEntry->WriteToProcessParams(pParams->ImageProcParams, pParams->ProcFlags, pParams->RotationParams);
			dbEntry->GetColorCorrectionAmounts(m_fColorCorrectionFactors);
			dbEntry->WriteToGeometricParams(pParams->Zoom, pParams->Offsets, SizeAfterRotation(pParams->RotationParams),
				dbEntry->IsStoredRelativeToScreenSize() ? pParams->MonitorSize : CSize(pParams->TargetWidth, pParams->TargetHeight));
		}
	} else {
		pParams->RotationParams.Rotation = GetRotationFromEXIF(pParams->RotationParams.Rotation);
		pParams->ImageProcParams.LightenShadows *= m_fLightenShadowFactor;
		if (!::GetProcessingFlag(pParams->ProcFlags, PFLAG_KeepParams)) {
			pParams->ProcFlags = GetProcFlagsIncludeExcludeFolders(sFileName, pParams->ProcFlags);
		}
	}

	m_nInitialRotation = pParams->RotationParams.Rotation;
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

void* CJPEGImage::GetDIBInternal(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset,
						 const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags,
						 const CUnsharpMaskParams * pUnsharpMaskParams, const CTrapezoid * pTrapezoid,
						 double dRotation, bool bShowGrid, bool &bParametersChanged) {

	if (fabs(dRotation) > 1e-6 && GetProcessingFlag(eProcFlags, PFLAG_HighQualityResampling)) {
		assert(false);
	}
	if (pTrapezoid != NULL && GetProcessingFlag(eProcFlags, PFLAG_HighQualityResampling)) {
		assert(false);
	}
	if (fabs(dRotation) > 1e-6 || pTrapezoid != NULL) {
		eProcFlags = SetProcessingFlag(eProcFlags, PFLAG_LDC, false); // not supported during rotation or trapezoid processing with low quality
	}

	// Check if resampling due to bHighQualityResampling parameter change is needed
	bool bMustResampleQuality = GetProcessingFlag(eProcFlags, PFLAG_HighQualityResampling) != GetProcessingFlag(m_eProcFlags, PFLAG_HighQualityResampling);
	bool bTargetSizeChanged = fullTargetSize != m_FullTargetSize;
	bool bMustResampleRotation = fabs(dRotation - m_dRotationLQ) > 1e-6;
	bool bMustResampleTrapezoid = (m_bTrapezoidValid != (pTrapezoid != NULL)) || ((pTrapezoid != NULL) && *pTrapezoid != m_trapezoid);
	// Check if resampling due to change of geometric parameters is needed
	bool bMustResampleGeometry = bTargetSizeChanged || clippingSize != m_ClippingSize || targetOffset != m_TargetOffset || bMustResampleRotation || bMustResampleTrapezoid;
	// Check if resampling due to change of processing parameters is needed
	bool bMustResampleProcessings = fabs(imageProcParams.Sharpen - m_imageProcParams.Sharpen) > 1e-2 && GetProcessingFlag(eProcFlags, PFLAG_HighQualityResampling);
	bool bShowGridChanged = m_bShowGrid != bShowGrid;

	EResizeType eResizeType = GetResizeType(fullTargetSize, CSize(m_nOrigWidth, m_nOrigHeight));

	// the geometrical parameters must be set before calling ApplyCorrectionLUT()
	CRect oldClippingRect = CRect(m_TargetOffset, m_ClippingSize);
	m_FullTargetSize = fullTargetSize;
	m_ClippingSize = clippingSize;
	m_TargetOffset = targetOffset;
	m_dRotationLQ = dRotation;
	m_bShowGrid = bShowGrid;
	m_bTrapezoidValid = pTrapezoid != NULL;
	m_trapezoid = (pTrapezoid != NULL) ? *pTrapezoid : CTrapezoid();

	double dStartTickCount = Helpers::GetExactTickCount();

	if (bShowGridChanged) {
		delete[] m_pDIBPixels; m_pDIBPixels = NULL;
		delete[] m_pDIBPixelsLUTProcessed; m_pDIBPixelsLUTProcessed = NULL;
		m_pLastDIB = NULL;
	}

	// Check if only the LUT must be reapplied but no resampling (resampling is much slower than the LUTs)
	void * pDIB = NULL;
	void * pDIBUnsharpMasked = NULL;
	if (!bMustResampleQuality && !bMustResampleGeometry && !bMustResampleProcessings) {
		// no resizing needed (maybe even nothing must be done)
		bool bNoChangesLDCandLUTs = ApplyCorrectionLUTandLDC(imageProcParams, eProcFlags, m_pDIBPixelsLUTProcessed, 
			fullTargetSize, targetOffset, m_pDIBPixels, clippingSize, bMustResampleGeometry, true, false) != NULL;
		pDIBUnsharpMasked = ApplyUnsharpMask(pUnsharpMaskParams, bNoChangesLDCandLUTs);
		pDIB = ApplyCorrectionLUTandLDC(imageProcParams, eProcFlags, m_pDIBPixelsLUTProcessed, 
			fullTargetSize, targetOffset, (pDIBUnsharpMasked != NULL) ? pDIBUnsharpMasked : m_pDIBPixels, clippingSize, 
			bMustResampleGeometry, false, pDIBUnsharpMasked != NULL, bParametersChanged);
	}
	// ApplyCorrectionLUTandLDC() could have failed, then recreate the DIBs
	if (pDIB == NULL) {
		// if the image is reprocessed more than once, it is worth to convert the original to 4 channels
		// as this is faster for further processing
		if (!m_bFirstReprocessing) {
			ConvertSrcTo4Channels();
		}

		bParametersChanged = true;

		assert(pDIBUnsharpMasked == NULL);

		// If we only pan, we can resample far more efficiently by only calculating the newly visible areas
		bool bPanningOnly = !m_bFirstReprocessing && !bMustResampleProcessings && !bTargetSizeChanged && !bMustResampleQuality && 
			!bMustResampleRotation && !bShowGrid && pTrapezoid == NULL;
		m_bFirstReprocessing = false;
		if (bPanningOnly && pUnsharpMaskParams == NULL) {
			ResampleWithPan(m_pDIBPixels, m_pDIBPixelsLUTProcessed, fullTargetSize, clippingSize, targetOffset, 
				oldClippingRect, eProcFlags, imageProcParams, dRotation, eResizeType);
			delete[] m_pGrayImage; m_pGrayImage = NULL;
			delete[] m_pSmoothGrayImage; m_pSmoothGrayImage = NULL;
		} else {
			delete[] m_pDIBPixelsLUTProcessed; m_pDIBPixelsLUTProcessed = NULL;
			delete[] m_pDIBPixels; m_pDIBPixels = NULL;
			delete[] m_pGrayImage; m_pGrayImage = NULL;
			delete[] m_pSmoothGrayImage; m_pSmoothGrayImage = NULL;
		}

		// both DIBs are NULL, do normal resampling
		if (m_pDIBPixels == NULL && m_pDIBPixelsLUTProcessed == NULL) {
			if (pTrapezoid == NULL) {
				m_pDIBPixels = Resample(fullTargetSize, clippingSize, targetOffset, eProcFlags, imageProcParams.Sharpen, dRotation, eResizeType);
			} else {
				m_pDIBPixels = CBasicProcessing::PointSampleTrapezoid(fullTargetSize, *pTrapezoid, targetOffset, clippingSize, 
					CSize(m_nOrigWidth, m_nOrigHeight), m_pIJLPixels, m_nIJLChannels, CSettingsProvider::This().ColorBackground());
			}
		}

		// if ResampleWithPan() has preseved this DIB, we can reuse it
		if (m_pDIBPixelsLUTProcessed == NULL) {
			pDIBUnsharpMasked = ApplyUnsharpMask(pUnsharpMaskParams, false);
			pDIB = ApplyCorrectionLUTandLDC(imageProcParams, eProcFlags, m_pDIBPixelsLUTProcessed, fullTargetSize, 
				targetOffset, (pDIBUnsharpMasked != NULL) ? pDIBUnsharpMasked : m_pDIBPixels, clippingSize, 
				bMustResampleGeometry, false, pDIBUnsharpMasked != NULL);
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
	if (pUnsharpMaskParams != NULL) {
		m_unsharpMaskParams = *pUnsharpMaskParams;
		m_bUnsharpMaskParamsValid = true;
	} else {
		m_bUnsharpMaskParamsValid = false;
	}
	// do not touch sharpen parameter if no resampling done - avoids cummulative error propagation
	if (!bMustResampleProcessings) {
		m_imageProcParams.Sharpen = dOldSharpen;
	}
	m_eProcFlags = eProcFlags;

	m_pLastDIB = pDIB;
	if (m_pDIBPixelsLUTProcessed != pDIBUnsharpMasked) {
		delete[] pDIBUnsharpMasked;
	}

	return pDIB;
}

void* CJPEGImage::ApplyUnsharpMask(const CUnsharpMaskParams * pUnsharpMaskParams, bool bNoChangesLDCandLUT) {
	bool bThisUnsharpMaskValid = pUnsharpMaskParams != NULL;
	if (bThisUnsharpMaskValid != m_bUnsharpMaskParamsValid) {
		delete[] m_pDIBPixelsLUTProcessed;
		m_pDIBPixelsLUTProcessed = NULL;
	}
	bool bAmountChanged = true;
	bool bRadiusChanged = true;
	bool bThresholdChanged = true;
	if (bThisUnsharpMaskValid && m_bUnsharpMaskParamsValid) {
		bAmountChanged = fabs(pUnsharpMaskParams->Amount - m_unsharpMaskParams.Amount) > 1e-4;
		bRadiusChanged = fabs(pUnsharpMaskParams->Radius - m_unsharpMaskParams.Radius) > 1e-4;
		bThresholdChanged = fabs(pUnsharpMaskParams->Threshold - m_unsharpMaskParams.Threshold) > 1e-4;
		if (bAmountChanged || bRadiusChanged || bThresholdChanged) {
			delete[] m_pDIBPixelsLUTProcessed;
			m_pDIBPixelsLUTProcessed = NULL;
		}
	}
	if (m_pDIBPixels == NULL || pUnsharpMaskParams == NULL) {
		return NULL;
	}
	if (!(bAmountChanged || bRadiusChanged || bThresholdChanged) && bNoChangesLDCandLUT) {
		return NULL; // nothing changed, we can reuse m_pDIBPixelsLUTProcessed later on
	}
	if (bRadiusChanged) {
		delete[] m_pSmoothGrayImage;
		m_pSmoothGrayImage = NULL;
	}
	if (m_pGrayImage == NULL) {
		m_pGrayImage = CBasicProcessing::Create1Channel16bppGrayscaleImage(m_ClippingSize.cx, m_ClippingSize.cy, m_pDIBPixels, 4);
	}
	if (m_pSmoothGrayImage == NULL) {
		m_pSmoothGrayImage = CBasicProcessing::GaussFilter16bpp1Channel(m_ClippingSize, CPoint(0, 0), 
			m_ClippingSize, pUnsharpMaskParams->Radius, m_pGrayImage);
	}
	if (m_pGrayImage == NULL || m_pSmoothGrayImage == NULL) {
		return NULL;
	}

	uint32* pNewImage = new(std::nothrow) uint32[m_ClippingSize.cx * m_ClippingSize.cy];
	return (pNewImage == NULL) ? NULL : CBasicProcessing::UnsharpMask(m_ClippingSize, CPoint(0,0), m_ClippingSize, 
		pUnsharpMaskParams->Amount, pUnsharpMaskParams->Threshold, m_pGrayImage, m_pSmoothGrayImage, m_pDIBPixels, pNewImage, 4);
}

void* CJPEGImage::ApplyCorrectionLUTandLDC(const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags,
										   void * & pCachedTargetDIB, CSize fullTargetSize, CPoint targetOffset, 
										   void * pSourceDIB, CSize dibSize,
										   bool bGeometryChanged, bool bOnlyCheck, bool bCanTakeOwnershipOfSourceDIB, bool &bParametersChanged) {

	bool bAutoContrast = GetProcessingFlag(eProcFlags, PFLAG_AutoContrast);
	bool bAutoContrastOld = GetProcessingFlag(m_eProcFlags, PFLAG_AutoContrast);
	bool bAutoContrastSection = GetProcessingFlag(eProcFlags, PFLAG_AutoContrastSection) && bAutoContrast;
	bool bAutoContrastSectionOld = GetProcessingFlag(m_eProcFlags, PFLAG_AutoContrastSection);
	bool bLDC = GetProcessingFlag(eProcFlags, PFLAG_LDC);
	bool bLDCOld = GetProcessingFlag(m_eProcFlags, PFLAG_LDC);

	bool bNoContrastAndGammaLUT = fabs(imageProcParams.Contrast) < 1e-4 && fabs(imageProcParams.Gamma - 1) < 1e-4;
	bool bMustUseSaturationLUTs = fabs(imageProcParams.Saturation - 1.0) > 1e-4;
	bool bNoColorCastCorrection = fabs(imageProcParams.CyanRed) < 1e-4 && fabs(imageProcParams.MagentaGreen) < 1e-4 &&
		fabs(imageProcParams.YellowBlue) < 1e-4;
	bool bNoLUTsApplied = bNoContrastAndGammaLUT && bNoColorCastCorrection && !bAutoContrast && !bMustUseSaturationLUTs;
	bool bContrastOrGammaChanged = fabs(imageProcParams.Contrast - m_imageProcParams.Contrast) > 1e-4 ||
		fabs(imageProcParams.Gamma - m_imageProcParams.Gamma) > 1e-4;
	bool bColorCastCorrChanged = fabs(imageProcParams.CyanRed - m_imageProcParams.CyanRed) > 1e-4 ||
		fabs(imageProcParams.MagentaGreen - m_imageProcParams.MagentaGreen) > 1e-4 ||
		fabs(imageProcParams.YellowBlue - m_imageProcParams.YellowBlue) > 1e-4;
	bool bCorrectionFactorChanged = fabs(imageProcParams.ColorCorrectionFactor-m_imageProcParams.ColorCorrectionFactor) > 1e-4 || 
		fabs(imageProcParams.ContrastCorrectionFactor-m_imageProcParams.ContrastCorrectionFactor) > 1e-4;
	bool bSaturationChanged = fabs(imageProcParams.Saturation - m_imageProcParams.Saturation) > 1e-4;
	bool bMustReapplyLUTs = bAutoContrast != bAutoContrastOld || bAutoContrastSection != bAutoContrastSectionOld || bLDC != bLDCOld || 
		bCorrectionFactorChanged || bContrastOrGammaChanged || bColorCastCorrChanged || bSaturationChanged;
	bool bLDCParametersChanged = fabs(imageProcParams.LightenShadows - m_imageProcParams.LightenShadows) > 1e-4 ||
		fabs(imageProcParams.DarkenHighlights - m_imageProcParams.DarkenHighlights) > 1e-4 ||
		fabs(imageProcParams.LightenShadowSteepness - m_imageProcParams.LightenShadowSteepness) > 1e-4 ;
	bool bMustReapplyLDC = bLDC && (!bLDCOld || bGeometryChanged || bLDCParametersChanged);
	bool bMustUse3ChannelLUT = bAutoContrast || !bNoColorCastCorrection;
	bool bUseDimming = m_bShowGrid || (m_pDimRects != NULL && m_bEnableDimming);

	bParametersChanged = bMustReapplyLUTs || bMustReapplyLDC;

	if (!bMustReapplyLUTs && !bMustReapplyLDC && pCachedTargetDIB != NULL) {
		// consider special case that nothing is dimmed and no LUT and LDC is applied but
		// processed pixel is here, we do not want to use it - in this case we just continue.
		if (!(bNoLUTsApplied && !bUseDimming && !bLDC)) {
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
	if (bContrastOrGammaChanged || m_pLUTAllChannels == NULL) {
		delete[] m_pLUTAllChannels;
		m_pLUTAllChannels = bNoContrastAndGammaLUT ? NULL : CBasicProcessing::CreateSingleChannelLUT(imageProcParams.Contrast, imageProcParams.Gamma);
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
	if (bMustUseSaturationLUTs && (m_pSaturationLUTs == NULL || bSaturationChanged)) {
		delete[] m_pSaturationLUTs;
		m_pSaturationLUTs = CBasicProcessing::CreateColorSaturationLUTs(imageProcParams.Saturation);
	} else if (!bMustUseSaturationLUTs) {
		delete[] m_pSaturationLUTs;
		m_pSaturationLUTs = NULL;
	}
	
	delete[] pCachedTargetDIB;
	pCachedTargetDIB = NULL;
	if (bSpecialHistogram) {
		delete pHistogram;
	}

	if (!bNoLUTsApplied || bLDC) {
		// LUT or/and LDC --> apply correction
		uint8* pLUT = CHistogramCorr::CombineLUTs(m_pLUTAllChannels, m_pLUTRGB);
		if (bLDC) {
			pCachedTargetDIB = CBasicProcessing::ApplyLDC32bpp(fullTargetSize, targetOffset, dibSize, m_pLDC->GetLDCMapSize(),
				pSourceDIB, bMustUseSaturationLUTs ? m_pSaturationLUTs : NULL, pLUT, m_pLDC->GetLDCMap(),
				m_pLDC->GetBlackPt(), m_pLDC->GetWhitePt(), (float)imageProcParams.LightenShadowSteepness);
		} else {
			if (bMustUseSaturationLUTs) {
				pCachedTargetDIB = CBasicProcessing::ApplySaturationAnd3ChannelLUT32bpp(dibSize.cx, dibSize.cy, pSourceDIB, m_pSaturationLUTs, pLUT);
			} else {
				pCachedTargetDIB = CBasicProcessing::Apply3ChannelLUT32bpp(dibSize.cx, dibSize.cy, pSourceDIB, pLUT);
			}
		}
		delete[] pLUT;
	} else if (bCanTakeOwnershipOfSourceDIB) {
		// no LUTs, no LDC just take over ownership of source DIB if we are allowed
		pCachedTargetDIB = pSourceDIB;
	} else if (!bUseDimming) {
		// no LUTs, no LDC, no dimming --> return original pixels
		return pSourceDIB;
	} else {
		// no LUTs, no LDC but dimming --> make copy of original pixels
		pCachedTargetDIB = new(std::nothrow) uint32[dibSize.cx*dibSize.cy];
		if (pCachedTargetDIB != NULL) {
			memcpy(pCachedTargetDIB, pSourceDIB, dibSize.cx*dibSize.cy*4);
		}
	}

	if (bUseDimming && pCachedTargetDIB != NULL) {
		for (int i = 0; i < m_nNumDimRects; i++) {
			CBasicProcessing::DimRectangle32bpp(dibSize.cx, dibSize.cy, pCachedTargetDIB, 
				m_pDimRects[i].Rect, m_pDimRects[i].Factor);
		}
	}
	if (m_bShowGrid && pCachedTargetDIB != NULL) {
		DrawGridLines(pCachedTargetDIB, dibSize);
	}

	return pCachedTargetDIB;
}

bool CJPEGImage::ConvertSrcTo4Channels() {
	if (m_nIJLChannels == 3) {
		void* pNewIJL = CBasicProcessing::Convert3To4Channels(m_nOrigWidth, m_nOrigHeight, m_pIJLPixels);
		if (pNewIJL != NULL) {
			delete[] m_pIJLPixels;
			m_pIJLPixels = pNewIJL;
			m_nIJLChannels = 4;
		}
		return pNewIJL != NULL;
	}
	return true;
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

CSize CJPEGImage::SizeAfterRotation(const CRotationParams& rotationParams) {
	int nDiff = ((rotationParams.Rotation - m_rotationParams.Rotation) + 360) % 360;
	CSize size = (nDiff == 90 || nDiff == 270) ? CSize(m_nOrigHeight, m_nOrigWidth) : CSize(m_nOrigWidth, m_nOrigHeight);
	double dDiff = fabs(rotationParams.FreeRotation - m_rotationParams.FreeRotation);
	if (dDiff >= 0.009) {
		CPoint offset;
		return GetSizeAfterFreeRotation(size, 2 * 3.141592653 * rotationParams.FreeRotation / 360, GetRotationFlag(rotationParams.Flags, RFLAG_AutoCrop),
			GetRotationFlag(rotationParams.Flags, RFLAG_KeepAspectRatio), offset);
	}
	return size;
}

CSize CJPEGImage::GetSizeAfterFreeRotation(const CSize& sourceSize, double dRotation, bool bAutoCrop, bool bKeepAspectRatio, CPoint & offset) {
	double dCoords[] = { 0, 0, sourceSize.cx - 1, 0, sourceSize.cx - 1, sourceSize.cy - 1, 0, sourceSize.cy - 1 };
	double dXMin = HUGE_VAL, dXMax = -HUGE_VAL;
	double dYMin = HUGE_VAL, dYMax = -HUGE_VAL;
	for (int i = 0; i < 4; i++) {
		RotateInplace(sourceSize, dCoords[i * 2], dCoords[i * 2 + 1], dRotation);
		dXMin = min(dXMin, dCoords[i * 2]);
		dXMax = max(dXMax, dCoords[i * 2]);
		dYMin = min(dYMin, dCoords[i * 2 + 1]);
		dYMax = max(dYMax, dCoords[i * 2 + 1]);
	}

	double dCenterX = (sourceSize.cx - 1) * 0.5;
	double dCenterY = (sourceSize.cy - 1) * 0.5;

	int nXStart, nXEnd;
	int nYStart, nYEnd;

	if (bAutoCrop) {
		// Calculate the maximum enclosed rectangle by intersecting the diagonal lines of the bounding box with the rotated rectangle sides
		if (bKeepAspectRatio) {
			double dNeededX = ((double)sourceSize.cx / sourceSize.cy) * (dYMax - dYMin);
			double dCenterX = (dXMax + dXMin) * 0.5;
			dXMin = dCenterX - 0.5 * dNeededX;
			dXMax = dCenterX + 0.5 * dNeededX;
		}
		double dBestX = (dXMax - dXMin) * 0.5;
		double dBestY = (dYMax - dYMin) * 0.5;
		double dBestDistance = dBestX*dBestX + dBestY*dBestY;
		for (int nSign = -1; nSign < 2; nSign += 2) {
			for (int i = 0; i < 4; i++) {
				int j = (i + 1) % 4;
				double dV1x = dCoords[j * 2] - dCoords[i * 2];
				double dV1y = dCoords[j * 2 + 1] - dCoords[i * 2 + 1];
				double dNumerator = dCoords[i * 2] * dYMin - dCoords[i * 2 + 1] * nSign * dXMax;
				double dDenumerator = dV1y * nSign * dXMax - dV1x * dYMin;
				double dT = dNumerator / dDenumerator;
				if (dT > -1e-8 && dT - 1 < 1e-8) {
					double dX = dCoords[i * 2] + dT * dV1x;
					double dY = dCoords[i * 2 + 1] + dT * dV1y;
					double dDist = dX*dX + dY*dY;
					if (dDist < dBestDistance) {
						dBestDistance = dDist;
						dBestX = dX;
						dBestY = dY;
					}
				}
			}
		}
		nXStart = Helpers::RoundToInt(-fabs(dBestX) + dCenterX);
		nXEnd = Helpers::RoundToInt(fabs(dBestX) + dCenterX);
		nYStart = Helpers::RoundToInt(-fabs(dBestY) + dCenterY);
		nYEnd = Helpers::RoundToInt(fabs(dBestY) + dCenterY);
	} else {
		nXStart = (int)(dXMin + dCenterX - 0.999);
		nXEnd = (int)(dXMax + dCenterX + 0.999);
		nYStart = (int)(dYMin + dCenterY - 0.999);
		nYEnd = (int)(dYMax + dCenterY + 0.999);
	}

	offset = CPoint(nXStart, nYStart);

	return CSize(nXEnd - nXStart + 1, nYEnd - nYStart + 1);
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

int CJPEGImage::GetRotationFromEXIF(int nOrigRotation) {
	if (m_pEXIFReader != NULL && m_pEXIFReader->ImageOrientationPresent() && CSettingsProvider::This().AutoRotateEXIF()) {

		// Some tools rotate the pixel data but do not reset the EXIF orientation flag.
		// In this case the EXIF thumbnail is normally also not rotated.
		// So check if the thumbnail orientation is the same as the image orientation.
		// If not, it can be assumed that someone touched the pixels and we ignore the EXIF
		// orientation.
		if (m_pEXIFReader->GetThumbnailWidth() > 0 && m_pEXIFReader->GetThumbnailHeight() > 0) {
			bool bWHOrig = m_nInitOrigWidth > m_nInitOrigHeight;
			bool bWHThumb = m_pEXIFReader->GetThumbnailWidth() > m_pEXIFReader->GetThumbnailHeight();
			if (bWHOrig != bWHThumb) {
				return nOrigRotation;
			}
		}

		switch (m_pEXIFReader->GetImageOrientation()) {
			case 1:
				m_bRotationByEXIF = true;
				return 0;
			case 3:
				m_bRotationByEXIF = true;
				return 180;
			case 6:
				m_bRotationByEXIF = true;
				return 90;
			case 8:
				m_bRotationByEXIF = true;
				return 270;
		}
	}

	if (m_pRawMetadata != NULL && CSettingsProvider::This().AutoRotateEXIF()) {
		// Only rotate by 90 or 270 deg if not already rotated by camera
		if (m_pRawMetadata->GetWidth() >= m_pRawMetadata->GetHeight()) {
			int orientation = m_pRawMetadata->GetOrientation();
			if ((orientation & 4) != 0) {
				m_bRotationByEXIF = true;
				return ((orientation & 1) != 0) ? 270 : 90;
			}
		}
	}
	return nOrigRotation;
}

void CJPEGImage::MarkAsDestructivelyProcessed() {
	m_bIsDestructivlyProcessed = true;
	m_rotationParams.FreeRotation = 0.0;
	m_rotationParams.Flags = RFLAG_None;
}

void CJPEGImage::InvalidateAllCachedPixelData() {
	m_pLastDIB = NULL;
	if (m_bLDCOwned) delete m_pLDC; // LDC mask must be recalculated!
	m_pLDC = NULL;
	delete[] m_pDIBPixels; 
	m_pDIBPixels = NULL;
	delete[] m_pDIBPixelsLUTProcessed; 
	m_pDIBPixelsLUTProcessed = NULL;
	delete[] m_pGrayImage;
	m_pGrayImage = NULL;
	delete[] m_pSmoothGrayImage;
	m_pSmoothGrayImage = NULL;
	delete m_pThumbnail;
	m_pThumbnail = NULL;
	delete m_pHistogramThumbnail;
	m_pHistogramThumbnail = NULL;
	m_ClippingSize = CSize(0, 0);
}

CJPEGImage* CJPEGImage::CreateThumbnailImage() {
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
	return new CJPEGImage(nWidth, nHeight, pPixels, NULL, 4, -1, IF_CLIPBOARD, false, 0, 1, 0, m_pLDC, true);
}

void CJPEGImage::DrawGridLines(void * pDIB, const CSize& dibSize) {
	const int cnNumGridLines = 8;
	const float cfDimFactor = 0.8f;
	int nGridLinesX = min(cnNumGridLines, dibSize.cx / 40);
	int nGridLinesY = min(cnNumGridLines, dibSize.cy / 40);
	if (nGridLinesX > 0) {
		for (int i = 0; i <= nGridLinesX; i++) {
			int nX = i * (dibSize.cx - 1) / nGridLinesX;
			CBasicProcessing::FillRectangle32bpp(dibSize.cx, dibSize.cy, pDIB, CRect(nX, 0, nX + 1, dibSize.cy), RGB(192, 192, 192));
		}
	}
	if (nGridLinesY > 0) {
		for (int i = 0; i <= nGridLinesY; i++) {
			int nY = i * (dibSize.cy - 1) / nGridLinesY;
			CBasicProcessing::FillRectangle32bpp(dibSize.cx, dibSize.cy, pDIB, CRect(0, nY, dibSize.cx, nY + 1), RGB(192, 192, 192));
		}
	}
}

