#include "StdAfx.h"
#include "JPEGImage.h"
#include "BasicProcessing.h"
#include "XMMImage.h"
#include "Helpers.h"
#include "SettingsProvider.h"
#include "HistogramCorr.h"
#include "LocalDensityCorr.h"
#include "ParameterDB.h"
#include <math.h>
#include <assert.h>

// Dim factor (0..1) for SetDimBitmapRegion(), 0 means black, 1 means no dimming
static const float cfDimFactor = 0.6f;

// Gets rectangle in DIB to be dimmed out, taking the flipped flag into account
static CRect GetDimRect(bool bFlipped, int nDIBHeight, const CRect& rect) {
	if (bFlipped) {
		return CRect(rect.left, nDIBHeight - rect.bottom, rect.right, nDIBHeight - rect.top);
	} else {
		return rect;
	}
}

///////////////////////////////////////////////////////////////////////////////////
// Public interface
///////////////////////////////////////////////////////////////////////////////////

CJPEGImage::CJPEGImage(int nWidth, int nHeight, void* pIJLPixels, void* pEXIFData, int nChannels, 
					   __int64 nJPEGHash, EImageFormat eImageFormat) {
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
	} else {
		m_nEXIFSize = 0;
		m_pEXIFData = NULL;
	}

	m_nPixelHash = nJPEGHash;
	m_eImageFormat = eImageFormat;

	m_nOrigWidth = nWidth;
	m_nOrigHeight = nHeight;
	m_pDIBPixels = NULL;
	m_pDIBPixelsLUTProcessed = NULL;
	m_pLastDIB = NULL;
	
	m_pLUTAllChannels = NULL;
	m_pLUTRGB = NULL;
	m_eProcFlags = PFLAG_None;
	m_eProcFlagsInitial = PFLAG_None;
	m_nInitialRotation = 0;
	m_dInitialZoom = -1;
	m_initialOffsets = CPoint(0, 0);
	m_nDimRegion = 0;
	m_bInParamDB = false;

	m_bFlipped = false;
	m_nRotation = 0;
	m_bFirstReprocessing = true;
	m_nLastOpTickCount = 0;
	m_FullTargetSize = CSize(0, 0);
	m_ClippingSize = CSize(0, 0);
	m_TargetOffset = CPoint(0, 0);

	// Create the LDC object on the image
	m_pLDC = new CLocalDensityCorr(*this, true);
	m_pHistogram = m_pLDC->GetHistogram();
	if (nJPEGHash == 0) {
		// Use the decompressed pixel hash in this case
		m_nPixelHash = m_pLDC->GetPixelHash();
	}
	m_fLightenShadowFactor = (1.0f - m_pHistogram->IsNightShot())*(1.0f - m_pLDC->IsSunset());

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
	delete m_pHistogram;
	m_pHistogram = NULL;
	delete m_pLDC;
	m_pLDC = NULL;
	m_pLastDIB = NULL;
	delete[] m_pEXIFData;
	m_pEXIFData = NULL;
}

void* CJPEGImage::GetDIB(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset,
						 const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags) {

	// Check if resampling due to bHighQualityResampling parameter change is needed
	bool bMustResampleQuality = GetProcessingFlag(eProcFlags, PFLAG_HighQualityResampling) != GetProcessingFlag(m_eProcFlags, PFLAG_HighQualityResampling);
	// Check if resampling due to change of geometric parameters is needed
	bool bMustResampleGeometry = fullTargetSize != m_FullTargetSize || clippingSize != m_ClippingSize ||
		targetOffset != m_TargetOffset;
	// Check if resampling due to change of processing parameters is needed
	bool bMustResampleProcessings = fabs(imageProcParams.Sharpen - m_imageProcParams.Sharpen) > 1e-2;

	EResizeType eResizeType;
	if (fullTargetSize.cx == m_nOrigWidth && fullTargetSize.cy == m_nOrigHeight) {
		eResizeType = NoResize;
	} else if (fullTargetSize.cx <= m_nOrigWidth && fullTargetSize.cy <= m_nOrigHeight) {
		eResizeType = DownSample;
	} else {
		eResizeType = UpSample;
	}

	// the geometrical parameters must be set before calling ApplyCorrectionLUT()
	m_FullTargetSize = fullTargetSize;
	m_ClippingSize = clippingSize;
	m_TargetOffset = targetOffset;

	LONG nStartTickCount = ::GetTickCount();

	// Check if only the LUT must be reapplied but no resampling (resampling is much slower than the LUTs)
	void * pDIB = NULL;
	if (!bMustResampleQuality && !bMustResampleGeometry && !bMustResampleProcessings && m_pDIBPixels != NULL) {
		// only LUT must be reapplied (or even nothing must be done)
		pDIB = ApplyCorrectionLUTandLDC(imageProcParams, eProcFlags,
			fullTargetSize, targetOffset, clippingSize, bMustResampleGeometry);
	} else {
		// if the image is reprocessed more than once, it is worth to convert the original to 4 channels
		// as this is faster for further processing
		if (!m_bFirstReprocessing) {
			ConvertSrcTo4Channels();
		}
		m_bFirstReprocessing = false;

		// needs to recalculate whole image, get rid of old DIBs
		delete[] m_pDIBPixels; m_pDIBPixels = NULL;
		delete[] m_pDIBPixelsLUTProcessed; m_pDIBPixelsLUTProcessed = NULL;

		Helpers::CPUType cpu = CSettingsProvider::This().AlgorithmImplementation();
		EFilterType filter = CSettingsProvider::This().DownsamplingFilter();

		if (GetProcessingFlag(eProcFlags, PFLAG_HighQualityResampling) && 
			!(eResizeType == NoResize && filter == Filter_Downsampling_Best_Quality)) {
			if (cpu == Helpers::CPU_SSE || cpu == Helpers::CPU_MMX) {
				if (eResizeType == UpSample) {
					m_pDIBPixels = CBasicProcessing::SampleUp_HQ_SSE_MMX(fullTargetSize, targetOffset, clippingSize, 
						CSize(m_nOrigWidth, m_nOrigHeight), m_pIJLPixels, m_nIJLChannels, cpu == Helpers::CPU_SSE);
				} else {
					m_pDIBPixels = CBasicProcessing::SampleDown_HQ_SSE_MMX(fullTargetSize, targetOffset, clippingSize,
						CSize(m_nOrigWidth, m_nOrigHeight), m_pIJLPixels, m_nIJLChannels, imageProcParams.Sharpen, filter, cpu == Helpers::CPU_SSE);
				}
			} else {
				if (eResizeType == UpSample) {
					m_pDIBPixels = CBasicProcessing::SampleUp_HQ(fullTargetSize, targetOffset, clippingSize, 
						CSize(m_nOrigWidth, m_nOrigHeight), m_pIJLPixels, m_nIJLChannels);
				} else {
					m_pDIBPixels = CBasicProcessing::SampleDown_HQ(fullTargetSize, targetOffset, clippingSize, 
						CSize(m_nOrigWidth, m_nOrigHeight), m_pIJLPixels, m_nIJLChannels, imageProcParams.Sharpen, filter);
				}
			}
		} else {
			if (eResizeType == UpSample) {
				m_pDIBPixels = CBasicProcessing::SampleUpFast(fullTargetSize, targetOffset, clippingSize, 
					CSize(m_nOrigWidth, m_nOrigHeight), m_pIJLPixels, m_nIJLChannels);
			} else {
				m_pDIBPixels = CBasicProcessing::SampleDownFast(fullTargetSize, targetOffset, clippingSize, 
					CSize(m_nOrigWidth, m_nOrigHeight), m_pIJLPixels, m_nIJLChannels);
			}
		}

		pDIB = ApplyCorrectionLUTandLDC(imageProcParams, eProcFlags, fullTargetSize, 
			targetOffset, clippingSize, bMustResampleGeometry);
	}

	int nLastOpTickCount = ::GetTickCount() - nStartTickCount; 
	if (nLastOpTickCount > 0) {
		m_nLastOpTickCount = nLastOpTickCount;
	}

	// set these parameters after ApplyCorrectionLUT() - else it cannot be detected that the parameters changed
	m_imageProcParams = imageProcParams;
	m_eProcFlags = eProcFlags;

	m_pLastDIB = pDIB;

	return pDIB;
}

CPoint CJPEGImage::ConvertOffset(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset) const {
	int nSign = m_bFlipped ? -1 : +1;
	int nStartX = (fullTargetSize.cx - clippingSize.cx)/2 - targetOffset.x;
	int nStartY = (fullTargetSize.cy - clippingSize.cy)/2 - nSign*targetOffset.y;
	return CSize(nStartX, nStartY);
}

void CJPEGImage::VerifyRotation(int nRotation) {
	int nDiff = ((nRotation - m_nRotation) + 360) % 360;
	if (nDiff != 0) {
		Rotate(nDiff);
	}
}

void CJPEGImage::Rotate(int nRotation) {
	LONG nStartTickCount = ::GetTickCount();

	// Rotation can only be done in 32 bpp
	ConvertSrcTo4Channels();

	m_pLastDIB = NULL;
	delete m_pLDC; // LDC mask must be recalculated!
	m_pLDC = NULL;
	delete[] m_pDIBPixels; 
	m_pDIBPixels = NULL;
	delete[] m_pDIBPixelsLUTProcessed; 
	m_pDIBPixelsLUTProcessed = NULL;
	void* pNewIJL = CBasicProcessing::Rotate32bpp(m_nOrigWidth, m_nOrigHeight, m_pIJLPixels, nRotation);
	delete[] m_pIJLPixels;
	m_pIJLPixels = pNewIJL;
	m_ClippingSize = CSize(0, 0);
	if (nRotation != 180) {
		// swap width and height
		int nTemp = m_nOrigWidth;
		m_nOrigWidth = m_nOrigHeight;
		m_nOrigHeight = nTemp;
	}
	m_nRotation = (m_nRotation + nRotation) % 360;

	m_nLastOpTickCount = ::GetTickCount() - nStartTickCount; 
}

void CJPEGImage::SetDimBitmapRegion(int nRegion) {
	if (m_pDIBPixelsLUTProcessed) {
		if (nRegion > m_nDimRegion) {
			// only dim delta area
			int nDelta = nRegion - m_nDimRegion;
			CBasicProcessing::DimRectangle32bpp(DIBWidth(), DIBHeight(), m_pDIBPixelsLUTProcessed, 
				GetDimRect(m_bFlipped, DIBHeight(), CRect(0, DIBHeight() - nRegion, DIBWidth(), DIBHeight() - nRegion + nDelta)), cfDimFactor);
		} else if (nRegion < m_nDimRegion) {
			// can't undim - force to recreate processed DIB on next access
			delete[] m_pDIBPixelsLUTProcessed;
			m_pDIBPixelsLUTProcessed = NULL;
			m_pLastDIB = NULL;
		}
	}
	m_nDimRegion = nRegion; 
}

void* CJPEGImage::DIBPixelsLastProcessed() {
	if (m_pLastDIB == NULL) {
		m_pLastDIB = GetDIB(m_FullTargetSize, m_ClippingSize, m_TargetOffset, m_imageProcParams, m_eProcFlags);
	}
	return m_pLastDIB;
}

float CJPEGImage::IsNightShot() const {
	if (m_pHistogram != NULL) {
		return m_pHistogram->IsNightShot();
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
										  EProcessingFlags & procFlags, int nRotation, double dZoom, CPoint offsets) {
	m_nInitialRotation = nRotation;
	m_dInitialZoom = dZoom;
	m_initialOffsets = offsets;
	m_eProcFlagsInitial = GetProcFlagsIncludeExcludeFolders(sFileName, procFlags);
	procFlags = m_eProcFlagsInitial;
	m_imageProcParamsInitial = imageProcParams;
	m_bInParamDB = false;
	m_imageProcParamsInitial.LightenShadows *= m_fLightenShadowFactor;
}

void CJPEGImage::SetFileDependentProcessParams(LPCTSTR sFileName, CProcessParams* pParams) {
	CParameterDBEntry* dbEntry = CParameterDB::This().FindEntry(GetPixelHash());
	if (dbEntry != NULL) {
		dbEntry->WriteToProcessParams(pParams->ImageProcParams, pParams->ProcFlags, pParams->Rotation);
		dbEntry->GetColorCorrectionAmounts(m_fColorCorrectionFactors);
		m_bInParamDB = true;
	} else {
		pParams->ImageProcParams.LightenShadows *= m_fLightenShadowFactor;
		if (!::GetProcessingFlag(pParams->ProcFlags, PFLAG_KeepParams)) {
			pParams->ProcFlags = GetProcFlagsIncludeExcludeFolders(sFileName, pParams->ProcFlags);
		}
	}

	m_nInitialRotation = pParams->Rotation;
	m_eProcFlagsInitial = pParams->ProcFlags;
	m_imageProcParamsInitial = pParams->ImageProcParams;
}

void CJPEGImage::SetZoomPanFromParamDB(CProcessParams* pParams) {
	CParameterDBEntry* dbEntry = CParameterDB::This().FindEntry(GetPixelHash());
	if (dbEntry != NULL) {
		dbEntry->WriteToGeometricParams(pParams->Zoom, pParams->Offsets, CSize(m_nOrigWidth, m_nOrigHeight), 
			CSize(pParams->TargetWidth, pParams->TargetHeight));
	}

	m_dInitialZoom = pParams->Zoom;
	m_initialOffsets = pParams->Offsets;
}

///////////////////////////////////////////////////////////////////////////////////
// Private
///////////////////////////////////////////////////////////////////////////////////


void* CJPEGImage::ApplyCorrectionLUTandLDC(const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags,
										   CSize fullTargetSize, CPoint targetOffset, CSize dibSize, bool bGeometryChanged) {
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

	if (m_pDIBPixels == NULL) {
		return NULL;
	}

	if (!bMustReapplyLUT && !bMustReapplyLDC && m_pDIBPixelsLUTProcessed != NULL) {
		// consider special case that dim area is set to zero and no LUT is applied but
		// processed pixel is here, we do not want to use it - in this case we just continue.
		if (!(bNoLUTApplied && m_nDimRegion == 0)) {
			return m_pDIBPixelsLUTProcessed;
		}
	}

	// Recalculate LUTs if needed
	if (bGlobalLUTChanged || m_pLUTAllChannels == NULL) {
		delete[] m_pLUTAllChannels;
		m_pLUTAllChannels = bNullLUTProcessings ? NULL : CBasicProcessing::CreateSingleChannelLUT(imageProcParams.Contrast, imageProcParams.Gamma);
	}

	// Calculate LDC if needed
	if (bLDC && m_pLDC == NULL) {
		m_pLDC = new CLocalDensityCorr(*this, true);
		bLDCParametersChanged = true;
	}
	if (bLDC) {
		// maybe only partially constructed, we need fully constructed object here
		m_pLDC->VerifyFullyConstructed();
	}
	if (bLDCParametersChanged && m_pLDC != NULL && m_pLDC->IsMaskAvailable()) {
		m_pLDC->SetLDCAmount(imageProcParams.LightenShadows, imageProcParams.DarkenHighlights);
	}

	// Recalculate histogram if needed
	if (bMustUse3ChannelLUT) {
		if (m_pHistogram == NULL || (bAutoContrast && m_pHistogram->UsingOrigPixels() != !bAutoContrastSection)) {
			delete m_pHistogram;
			m_pHistogram = new CHistogram(*this, !bAutoContrastSection);
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
		m_pLUTRGB = CHistogramCorr::CalculateCorrectionLUT(*m_pHistogram, fColorCorrFactor, fBrightnessCorrFactor,
			fColorCastCorrs, bAutoContrast ? m_fColorCorrectionFactors : m_fColorCorrectionFactorsNull, fContrastCorrFactor);
	} else if (!bMustUse3ChannelLUT) {
		delete[] m_pLUTRGB;
		m_pLUTRGB = NULL;
	}
	
	delete[] m_pDIBPixelsLUTProcessed;
	m_pDIBPixelsLUTProcessed = NULL;

	if (!bNoLUTApplied || bLDC) {
		// LUT or/and LDC --> apply correction
		uint8* pLUT = CHistogramCorr::CombineLUTs(m_pLUTAllChannels, m_pLUTRGB);
		if (bLDC && m_pLDC != NULL) {
			m_pDIBPixelsLUTProcessed = CBasicProcessing::ApplyLDC32bpp(fullTargetSize, targetOffset, dibSize, m_pLDC->GetLDCMapSize(),
				m_pDIBPixels, pLUT, m_pLDC->GetLDCMap(), m_pLDC->GetBlackPt(), m_pLDC->GetWhitePt(), (float)imageProcParams.LightenShadowSteepness);
		} else {
			m_pDIBPixelsLUTProcessed = CBasicProcessing::Apply3ChannelLUT32bpp(dibSize.cx, dibSize.cy, m_pDIBPixels, pLUT);
		}
		delete[] pLUT;
	} else if (m_nDimRegion == 0) {
		// no LUT, no LDC, no dimming --> return original pixels
		return m_pDIBPixels;
	} else {
		// no LUT, no LDC but dimming --> make copy of original pixels
		m_pDIBPixelsLUTProcessed = new uint32[dibSize.cx*dibSize.cy];
		memcpy(m_pDIBPixelsLUTProcessed, m_pDIBPixels, dibSize.cx*dibSize.cy*4);
	}

	if (m_nDimRegion > 0) {
		CBasicProcessing::DimRectangle32bpp(dibSize.cx, dibSize.cy, m_pDIBPixelsLUTProcessed, 
			GetDimRect(m_bFlipped, dibSize.cy, CRect(0, dibSize.cy - m_nDimRegion, dibSize.cx, dibSize.cy)), cfDimFactor);
	}

	return m_pDIBPixelsLUTProcessed;
}

void CJPEGImage::ConvertSrcTo4Channels() {
	if (m_nIJLChannels == 3) {
		void* pNewIJL = CBasicProcessing::Convert3To4Channels(m_nOrigWidth, m_nOrigHeight, m_pIJLPixels);
		delete[] m_pIJLPixels;
		m_pIJLPixels = pNewIJL;
		m_nIJLChannels = 4;
	}
}

EProcessingFlags CJPEGImage::GetProcFlagsIncludeExcludeFolders(LPCTSTR sFileName, EProcessingFlags procFlags) {
	EProcessingFlags eFlags = procFlags;
	CSettingsProvider& sp = CSettingsProvider::This();
	if (Helpers::PatternMatch(sFileName, sp.ACCInclude())) {
		eFlags = SetProcessingFlag(eFlags, PFLAG_AutoContrast, true);
	} else if (Helpers::PatternMatch(sFileName, sp.ACCExclude())) {
		eFlags = SetProcessingFlag(eFlags, PFLAG_AutoContrast, false);
	}
	if (Helpers::PatternMatch(sFileName, sp.LDCInclude())) {
		eFlags = SetProcessingFlag(eFlags, PFLAG_LDC, true);
	} else if (Helpers::PatternMatch(sFileName, sp.LDCExclude())) {
		eFlags = SetProcessingFlag(eFlags, PFLAG_LDC, false);
	}
	return eFlags;
}