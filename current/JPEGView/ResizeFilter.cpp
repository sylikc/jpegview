#include "StdAfx.h"
#include "ResizeFilter.h"
#include "Helpers.h"
#include <math.h>
#include <stdlib.h>

// We cannot enlarge to a factor above the number of kernels without artefacts
#define NUM_KERNELS_RESIZE 128
#define NUM_KERNELS_RESIZE_LOG2 7


//////////////////////////////////////////////////////////////////////////////////////
// Helpers
//////////////////////////////////////////////////////////////////////////////////////

inline static int16 roundToInt16(double d) {
	return (d < 0) ? (int16)(d - 0.5) : (int16)(d + 0.5);
}

// Normalization means: sum of filter elements equals CResizeFilter::FP_ONE
static void NormalizeFilter(int16* pFilter, int nLen) {
	int nSum = 0;
	for (int i = 0; i < nLen; i++) {
		nSum += pFilter[i];
	}
	if (nSum > 0) {
		int nTotal = 0;
		for (int i = 0; i < nLen; i++) {
			pFilter[i] = (int16)((pFilter[i] * CResizeFilter::FP_ONE) / nSum);
			nTotal += pFilter[i];
		}
		pFilter[0] += (CResizeFilter::FP_ONE - nTotal);
	}
}

//////////////////////////////////////////////////////////////////////////////////////
// Filter kernel evaluation
//////////////////////////////////////////////////////////////////////////////////////

// dSharpen = 0.07 means no sharpening
// Piecewise quadratic sinc like filter, support is [-1.5, 1.5], 3 taps
static inline double EvaluateCore_Narrow(double dX, double dSharpen) {
	if (dX < -1.5 || dX > 1.5) {
		return 0.0;
	}
	else if (dX < -0.5) {
		double dTemp = 2 * dX + 2;
		return -dSharpen*(1 - dTemp*dTemp);
	}
	else if (dX < 0.5) {
		double dTemp = 2 * dX;
		return 1 - dTemp*dTemp;
	}
	else {
		double dTemp = 2 * dX - 2;
		return -dSharpen*(1 - dTemp*dTemp);
	}
}

// dSharpen = 0.15 means no sharpening
// Piecewise quadratic sinc like filter, support is [-2.0, 2.0], 3 taps
static inline double EvaluateCore_BestQuality(double dX, double dSharpen) {
	if (dX < -2 || dX > 2) {
		return 0.0;
	}
	else if (dX < -1) {
		double dTemp = 2 * dX + 3;
		return -dSharpen*(1 - dTemp*dTemp);
	}
	else if (dX < 1) {
		return 1 - dX*dX;
	}
	else {
		double dTemp = 2 * dX - 3;
		return -dSharpen*(1 - dTemp*dTemp);
	}
}

#define PI 3.14159265358979323846
#define PI_DIV_2 (PI/2)
#define PI_SQR (PI*PI)

// dSharpen = 1.0 means no sharpening
// Lanczos filter, support is [-2.0, 2.0]
static inline double EvaluateCore_NoAliasing(double dX, double dSharpen) {
	// this is a Lanczos filter
	if (dX < -2 || dX > 2) {
		return 0.0;
	}
	else if (abs(dX) < 1e-6) {
		return 1.0;
	}
	else if (dX > -1 && dX < 1) {
		return (2 * sin(PI*dX)*sin(PI_DIV_2*dX)) / (PI_SQR*dX*dX);
	}
	else {
		return dSharpen*(2 * sin(PI*dX)*sin(PI_DIV_2*dX)) / (PI_SQR*dX*dX);
	}
}

// Evaluate a filter kernel at position dX. The filter kernel is assumed to have zero solutions at
// integer values and is centered around zero.
// dSharpen is a parameter that is used to control scaling of the negative pads of the filter kernel,
// resulting in increased sharpening effect.
static double inline EvaluateKernel(double dX, double dSharpen, EFilterType eFilter) {
	switch (eFilter) {
	case Filter_Downsampling_Best_Quality:
		return EvaluateCore_BestQuality(dX, dSharpen);
		break;
	case Filter_Downsampling_No_Aliasing:
		return EvaluateCore_NoAliasing(dX, dSharpen);
		break;
	case Filter_Downsampling_Narrow:
		return EvaluateCore_Narrow(dX, dSharpen);
		break;
	}
	return 0.0;
}

// Evaluation of filter kernel using an integration over the source pixel width.
// This implements a convolution of a box filter with the filter kernel.
// Note that dX is given in the source pixel space
static double EvaluateKernelIntegrated(double dX, EFilterType eFilter, double dMultX, double dSharpen) {
	double dXScaled = dX*dMultX;

	// take integral of target function from [dX - 0.5, dX + 0.5]
	const double NUM_STEPS = 32;
	double dStartX = dXScaled - dMultX*0.5;
	double dStepX = dMultX*(1.0 / (NUM_STEPS - 1));
	double dSum = 0.0;
	for (int i = 0; i < NUM_STEPS; i++) {
		dSum += EvaluateKernel(dStartX, dSharpen, eFilter);
		dStartX += dStepX;
	}
	return dSum;
}

static double EvaluateCubicFilterKernel(double dFrac, int nKernelElement) {
	const double cdFactorA = -0.5;
	double dAbsDiff;
	switch (nKernelElement) {
	case 0:
		dAbsDiff = 1.0 + dFrac;
		return cdFactorA*dAbsDiff*dAbsDiff*dAbsDiff - 5 * cdFactorA*dAbsDiff*dAbsDiff +
			8 * cdFactorA*dAbsDiff - 4 * cdFactorA;
	case 1:
		dAbsDiff = dFrac;
		return (cdFactorA + 2)*dAbsDiff*dAbsDiff*dAbsDiff -
			(cdFactorA + 3)*dAbsDiff*dAbsDiff + 1;
	case 2:
		dAbsDiff = (1.0 - dFrac);
		return (cdFactorA + 2)*dAbsDiff*dAbsDiff*dAbsDiff -
			(cdFactorA + 3)*dAbsDiff*dAbsDiff + 1;
	case 3:
		dAbsDiff = 1.0 + (1.0 - dFrac);
		return cdFactorA*dAbsDiff*dAbsDiff*dAbsDiff - 5 * cdFactorA*dAbsDiff*dAbsDiff +
			8 * cdFactorA*dAbsDiff - 4 * cdFactorA;
	}
	return -1;
}

// Filter is normalized in fixed point format, sum of elements is FP_ONE
// nFrac is fractional part (sub-pixel offset), coded in [0..65535] --> [0...1]
// Returned filter in pFilterOut has length BICUBIC_FILTER_LEN
static void GetBicubicFilter(uint16 nFrac, int16* pFilterOut) {
	const int BICUBIC_FILTER_LEN = 4;
	double dFrac = nFrac*(1.0/65535.0);
	double dFilter[BICUBIC_FILTER_LEN];
	double dSum = 0.0;
	for (int i = 0; i < BICUBIC_FILTER_LEN; i++) {
		dFilter[i] = EvaluateCubicFilterKernel(dFrac, i);
		dSum += dFilter[i];
	}
	for (int i = 0; i < BICUBIC_FILTER_LEN; i++) {
		pFilterOut[i] = roundToInt16(CResizeFilter::FP_ONE * dFilter[i] / dSum);
	}

	NormalizeFilter(pFilterOut, BICUBIC_FILTER_LEN);
}


//////////////////////////////////////////////////////////////////////////////////////
// Public
//////////////////////////////////////////////////////////////////////////////////////

CResizeFilter::CResizeFilter(int nSourceSize, int nTargetSize, double dSharpen, EFilterType eFilter, FilterSIMDType filterSIMDType) {
	m_nSourceSize = nSourceSize;
	m_nTargetSize = nTargetSize;
	m_dSharpen = min(0.5, max(0.0, dSharpen));
	m_eFilter = eFilter;
	m_filterSIMDType = filterSIMDType;
	m_nRefCnt = 0;
	memset(&m_kernels, 0, sizeof(m_kernels));
	memset(&m_kernelsXMM, 0, sizeof(m_kernelsXMM));
	memset(&m_kernelsAVX, 0, sizeof(m_kernelsAVX));

	if (filterSIMDType == FilterSIMDType_AVX) {
		CalculateAVXFilterKernels();
	} else if (filterSIMDType == FilterSIMDType_SSE) {
		CalculateXMMFilterKernels();
	} else {
		CalculateFilterKernels();
	}
}

CResizeFilter::~CResizeFilter(void) {
	delete[] m_kernels.Indices;
	delete[] m_kernels.Kernels;
	delete[] m_kernelsXMM.Indices;
	delete[] m_kernelsXMM.UnalignedMemory;
	delete[] m_kernelsAVX.Indices;
	delete[] m_kernelsAVX.UnalignedMemory;
}

bool CResizeFilter::ParametersMatch(int nSourceSize, int nTargetSize, double dSharpen, EFilterType eFilter, FilterSIMDType filterSIMDType) {
	if (nSourceSize == m_nSourceSize && nTargetSize == m_nTargetSize && abs(dSharpen - m_dSharpen) < 1e-6 &&
		eFilter == m_eFilter && m_filterSIMDType == filterSIMDType) {
			return true;
	} else {
		return false;
	}
}

void CResizeFilter::GetBicubicFilterKernels(int nNumKernels, int16* pKernels) {
	uint32 nIncFrac = 65535/(nNumKernels - 1);
	uint32 nKFrac = 0;
	for (int i = 0; i < nNumKernels; i++) {
		int16* pThisKernel = &(pKernels[i * 4]);
		GetBicubicFilter((uint16)nKFrac, pThisKernel);
		nKFrac += nIncFrac;
	}
}

//////////////////////////////////////////////////////////////////////////////////////
// Private
//////////////////////////////////////////////////////////////////////////////////////

void CResizeFilter::CalculateFilterKernels() {
	CalculateFilterParams(m_eFilter);

	if ((m_nTargetSize > m_nSourceSize && m_eFilter != Filter_Upsampling_Bicubic) || 
		m_nTargetSize == 0 || m_nSourceSize > 65535 || m_nTargetSize > 65535) {
		return;
	}

	// Calculate the increment here as the number of increments in one pixel
	// gives the number of border kernels we need.
	// The exact increment depends on the filter
	uint32 nIncrementX;
	uint32 nX;
	if (m_eFilter == Filter_Upsampling_Bicubic) {
		if (m_nSourceSize == 1 || m_nTargetSize == 1) {
			nIncrementX = (uint32)(m_nSourceSize << 16)/m_nTargetSize;
		} else {
			nIncrementX = (uint32)((m_nSourceSize - 1) << 16)/(m_nTargetSize - 1);
		}
		nIncrementX = max(1, nIncrementX);
		nX = 0;
	} else {
		nIncrementX = (uint32)(m_nSourceSize << 16)/m_nTargetSize + 1;
		nX = (nIncrementX - 65536) >> 1;
	}

	int nBorderKernelsPerPixel = (int)(max(1.0f, 65536.0f/nIncrementX) + 0.999999f);
	int nTotalKernels = NUM_KERNELS_RESIZE + nBorderKernelsPerPixel*(2*m_nFilterOffset + 1);
	m_kernels.Indices = new FilterKernel*[m_nTargetSize];
	m_kernels.Kernels = new FilterKernel[nTotalKernels];
	memset(m_kernels.Kernels, 0, sizeof(FilterKernel)*nTotalKernels);

	// calculate kernels for different fractional values (no border kernels yet)
	uint32 nIncFrac = 65535/(NUM_KERNELS_RESIZE - 1);
	uint32 nKFrac = 0;
	for (int i = 0; i < NUM_KERNELS_RESIZE; i++) {
		FilterKernel* pThisKernel = &(m_kernels.Kernels[i]);
		pThisKernel->FilterLen = m_nFilterLen;
		pThisKernel->FilterOffset = m_nFilterOffset;
		int16* pKernel = GetFilter((uint16)nKFrac, m_eFilter);
		memcpy(&(pThisKernel->Kernel), pKernel, m_nFilterLen*sizeof(int16));
		nKFrac += nIncFrac;
	}

	int nIdxBorderKernel = NUM_KERNELS_RESIZE;
	for (int i = 0; i < m_nTargetSize; i++) {
		uint32 nXInt = nX >> 16;
		uint32 nXFrac = nX & 0xFFFF;
		if ((int)nXInt < m_nFilterOffset) {
			// left border handling, the (m_nFilterOffset - nXInt) left elements are cut from the filter
			int16* pBorderFilter = GetFilter((uint16)nXFrac, m_eFilter) + (m_nFilterOffset - nXInt);
			int nFilterLen = m_nFilterLen - (m_nFilterOffset - nXInt);
			nFilterLen = min(nFilterLen, m_nSourceSize);
			NormalizeFilter(pBorderFilter, nFilterLen);
			FilterKernel* pThisKernel = &(m_kernels.Kernels[nIdxBorderKernel]);
			pThisKernel->FilterLen = nFilterLen;
			pThisKernel->FilterOffset = nXInt;
			memcpy(&(pThisKernel->Kernel), pBorderFilter, nFilterLen * sizeof(int16));
			m_kernels.Indices[i] = pThisKernel;
			nIdxBorderKernel++;
		} else if ((int)nXInt - m_nFilterOffset + m_nFilterLen > m_nSourceSize) {
			// right border handling
			int16* pBorderFilter = GetFilter((uint16)nXFrac, m_eFilter);
			int nFilterLen =  m_nSourceSize - nXInt + m_nFilterOffset;
			nFilterLen = min(nFilterLen, m_nSourceSize);
			NormalizeFilter(pBorderFilter, nFilterLen);
			FilterKernel* pThisKernel = &(m_kernels.Kernels[nIdxBorderKernel]);
			pThisKernel->FilterLen = nFilterLen;
			pThisKernel->FilterOffset = m_nFilterOffset;
			memcpy(&(pThisKernel->Kernel), pBorderFilter, nFilterLen * sizeof(int16));
			m_kernels.Indices[i] = pThisKernel;
			nIdxBorderKernel++;
		} else {
			// normal kernels
			uint32 nFilterIdx = nXFrac >> (16 - NUM_KERNELS_RESIZE_LOG2);
			m_kernels.Indices[i] = &(m_kernels.Kernels[nFilterIdx]);
		}
		nX += nIncrementX;
	}
	m_kernels.NumKernels = nIdxBorderKernel;
}

void CResizeFilter::CalculateXMMFilterKernels() {
	CalculateFilterKernels();
	if (m_nTargetSize == 0) {
		return;
	}

	// Get size of kernel array - this is not trivial as the kernels have different sizes and
	// are packed
	int nTotalKernelElements = 0;
	for (int i = 0; i < m_kernels.NumKernels; i++) {
		nTotalKernelElements += m_kernels.Kernels[i].FilterLen;
	}
	uint32 nSizeOfKernels = m_kernels.NumKernels * 16 + sizeof(XMMKernelElement) * nTotalKernelElements;

	m_kernelsXMM.NumKernels = m_kernels.NumKernels;
	m_kernelsXMM.Indices = new XMMFilterKernel*[m_nTargetSize];
	m_kernelsXMM.UnalignedMemory = new uint8[nSizeOfKernels + 15];
	m_kernelsXMM.Kernels = (XMMFilterKernel*)(((PTR_INTEGRAL_TYPE)m_kernelsXMM.UnalignedMemory + 15) & ~15);
	memset(m_kernelsXMM.Kernels, 0, nSizeOfKernels);

	// create an array of the start address of the filter kernels
	XMMFilterKernel** pKernelStartAddress = new XMMFilterKernel*[m_kernelsXMM.NumKernels];
	// create the XMM kernels, pack the kernels
	XMMFilterKernel* pCurKernelXMM = m_kernelsXMM.Kernels;
	for (int i = 0; i < m_kernelsXMM.NumKernels; i++) {
		int nCurFilterLen = m_kernels.Kernels[i].FilterLen;
		pKernelStartAddress[i] = pCurKernelXMM;
		pCurKernelXMM->FilterLen = nCurFilterLen;
		pCurKernelXMM->FilterOffset = m_kernels.Kernels[i].FilterOffset;
		for (int j = 0; j < nCurFilterLen; j++) {
			for (int k = 0; k < 8; k++) {
				pCurKernelXMM->Kernel[j].valueRepeated[k] = m_kernels.Kernels[i].Kernel[j];
			}
		}
		pCurKernelXMM = (XMMFilterKernel*) ((PTR_INTEGRAL_TYPE)pCurKernelXMM + 16 + sizeof(XMMKernelElement)*nCurFilterLen);
	}

	for (int i = 0; i < m_nTargetSize; i++) {
		int nIndex = (int)(m_kernels.Indices[i] - m_kernels.Kernels);
		m_kernelsXMM.Indices[i] = pKernelStartAddress[nIndex];
	}

	delete[] pKernelStartAddress;
}

void CResizeFilter::CalculateAVXFilterKernels() {
	CalculateFilterKernels();
	if (m_nTargetSize == 0) {
		return;
	}

	// Get size of kernel array - this is not trivial as the kernels have different sizes and
	// are packed
	int nTotalKernelElements = 0;
	for (int i = 0; i < m_kernels.NumKernels; i++) {
		nTotalKernelElements += m_kernels.Kernels[i].FilterLen;
	}
	uint32 nSizeOfKernels = m_kernels.NumKernels * 32 + sizeof(AVXKernelElement)* nTotalKernelElements;

	m_kernelsAVX.NumKernels = m_kernels.NumKernels;
	m_kernelsAVX.Indices = new AVXFilterKernel*[m_nTargetSize];
	m_kernelsAVX.UnalignedMemory = new uint8[nSizeOfKernels + 31];
	m_kernelsAVX.Kernels = (AVXFilterKernel*)(((PTR_INTEGRAL_TYPE)m_kernelsAVX.UnalignedMemory + 31) & ~31);
	memset(m_kernelsAVX.Kernels, 0, nSizeOfKernels);

	// create an array of the start address of the filter kernels
	AVXFilterKernel** pKernelStartAddress = new AVXFilterKernel*[m_kernelsAVX.NumKernels];
	// create the AVX kernels, pack the kernels
	AVXFilterKernel* pCurKernelAVX = m_kernelsAVX.Kernels;
	for (int i = 0; i < m_kernelsAVX.NumKernels; i++) {
		int nCurFilterLen = m_kernels.Kernels[i].FilterLen;
		pKernelStartAddress[i] = pCurKernelAVX;
		pCurKernelAVX->FilterLen = nCurFilterLen;
		pCurKernelAVX->FilterOffset = m_kernels.Kernels[i].FilterOffset;
		for (int j = 0; j < nCurFilterLen; j++) {
			for (int k = 0; k < 16; k++) {
				pCurKernelAVX->Kernel[j].valueRepeated[k] = m_kernels.Kernels[i].Kernel[j];
			}
		}
		pCurKernelAVX = (AVXFilterKernel*)((PTR_INTEGRAL_TYPE)pCurKernelAVX + 32 + sizeof(AVXKernelElement)*nCurFilterLen);
	}

	for (int i = 0; i < m_nTargetSize; i++) {
		int nIndex = (int)(m_kernels.Indices[i] - m_kernels.Kernels);
		m_kernelsAVX.Indices[i] = pKernelStartAddress[nIndex];
	}

	delete[] pKernelStartAddress;
}

void CResizeFilter::CalculateFilterParams(EFilterType eFilter) {
	if (eFilter == Filter_Downsampling_Best_Quality) {
		int nStdFilterLen = 4;
		double dFactor = (double)m_nSourceSize/m_nTargetSize;
		m_dMultX = (dFactor < 2) ? 1.0/(dFactor - 0.5) : 1.0/((dFactor + 1)*0.5);
		m_nFilterLen = (int)(nStdFilterLen*((dFactor + 1)*0.5) + 0.99);
		m_nFilterLen = min(MAX_FILTER_LEN, m_nFilterLen);
		m_nFilterOffset = (m_nFilterLen - 1)/2;
		if (fabs(dFactor - 1) < 0.01) {
			m_dSharpen = 0.0; // avoid to sharpen when no resizing
		}
	} else if (eFilter == Filter_Downsampling_No_Aliasing) {
		double dFactor = (double)m_nSourceSize/m_nTargetSize;
		m_dMultX = 1.0/dFactor;
		m_nFilterLen = (int) (5*dFactor);
		m_nFilterLen = min(MAX_FILTER_LEN, m_nFilterLen);
		m_nFilterOffset = (m_nFilterLen - 1)/2;
		m_dSharpen = 1 + m_dSharpen*7;
	} else if (eFilter == Filter_Downsampling_Narrow) {
		int nStdFilterLen = 4;
		double dFactor = (double)m_nSourceSize/m_nTargetSize;
		m_dMultX = 1.0/dFactor;
		m_nFilterLen = (int)(nStdFilterLen*dFactor + 0.99);
		m_nFilterLen = min(MAX_FILTER_LEN, m_nFilterLen);
		m_nFilterOffset = (m_nFilterLen - 1)/2;
		m_dSharpen /= 2;
	} else if (eFilter == Filter_Upsampling_Bicubic) {
		m_dMultX = 1.0;
		m_nFilterLen = 4;
		m_nFilterOffset = 1;
	} else {
		m_dMultX = 0.0;
		m_nFilterLen = 0;
		m_nFilterOffset = 0;
	}
}


// Filter is normalized in fixed point format, sum of elements is FP_ONE
// nFrac is fractional part (sub-pixel offset), coded in [0..65535] --> [0...1]
int16* CResizeFilter::GetFilter(uint16 nFrac, EFilterType eFilter) {
	double dFrac = nFrac*(1.0/65535.0);
	double dFilter[MAX_FILTER_LEN];
	double dSum = 0.0;
	for (int i = 0; i < m_nFilterLen; i++) {
		if (eFilter == Filter_Upsampling_Bicubic) {
			dFilter[i] = EvaluateCubicFilterKernel(dFrac, i);
		} else if (eFilter == Filter_Downsampling_No_Aliasing) {
			dFilter[i] = EvaluateKernel(m_dMultX*(-m_nFilterOffset + i - dFrac), m_dSharpen, eFilter);
		} else {
			dFilter[i] = EvaluateKernelIntegrated(-m_nFilterOffset + i - dFrac, eFilter, m_dMultX, m_dSharpen);
		}
		dSum += dFilter[i];
	}
	int nSum = 0;
	for (int i = 0; i < m_nFilterLen; i++) {
		m_Filter[i] = roundToInt16(FP_ONE * dFilter[i] / dSum);
		nSum += m_Filter[i];
	}
	m_Filter[0] += (int16)(FP_ONE - nSum);
	for (int j = m_nFilterLen; j < MAX_FILTER_LEN; j++) {
		m_Filter[j] = 0;
	}
	return m_Filter;
}

//////////////////////////////////////////////////////////////////////////////////////
// CResizeFilterCache
//////////////////////////////////////////////////////////////////////////////////////

CResizeFilterCache* CResizeFilterCache::sm_instance;

CResizeFilterCache& CResizeFilterCache::This() {
	if (sm_instance == NULL) {
		sm_instance = new CResizeFilterCache();
		atexit(&Delete);
	}
	return *sm_instance;
}

CResizeFilterCache::CResizeFilterCache() {
	memset(&m_csList, 0, sizeof(CRITICAL_SECTION));
	::InitializeCriticalSection(&m_csList);
}

CResizeFilterCache::~CResizeFilterCache() {
	::DeleteCriticalSection(&m_csList);
	std::list<CResizeFilter*>::iterator iter;
	for (iter = m_filterList.begin( ); iter != m_filterList.end( ); iter++ ) {
		delete (*iter);
	}
}

const CResizeFilter& CResizeFilterCache::GetFilter(int nSourceSize, int nTargetSize, double dSharpen, EFilterType eFilter, FilterSIMDType filterSIMDType) {
	CResizeFilter* pMatchingFilter = NULL;

	Helpers::CAutoCriticalSection autoCriticalSection(m_csList);
	std::list<CResizeFilter*>::iterator iter;
	for (iter = m_filterList.begin( ); iter != m_filterList.end( ); iter++ ) {
		if ((*iter)->ParametersMatch(nSourceSize, nTargetSize, dSharpen, eFilter, filterSIMDType)) {
			pMatchingFilter = *iter;
			break;
		}
	}

	if (pMatchingFilter != NULL) {
		// found matching filter, return it
		pMatchingFilter->m_nRefCnt++;
		m_filterList.remove(pMatchingFilter);
		m_filterList.push_front(pMatchingFilter); // move to top in list
		return *pMatchingFilter;
	}

	// no matching filter found, create a new one
	CResizeFilter* pNewFilter = new CResizeFilter(nSourceSize, nTargetSize, dSharpen, eFilter, filterSIMDType);
	pNewFilter->m_nRefCnt++;
	m_filterList.push_front(pNewFilter);

	return *pNewFilter;
}

void CResizeFilterCache::ReleaseFilter(const CResizeFilter& filter) {
	
	const int MAX_SIZE = 4;

	Helpers::CAutoCriticalSection autoCriticalSection(m_csList);
	const_cast<CResizeFilter&>(filter).m_nRefCnt--;
	if (m_filterList.size() > MAX_SIZE) {
		// cache too large - try to free one entry
		CResizeFilter* pElementTBRemoved = NULL;
		std::list<CResizeFilter*>::reverse_iterator iter;
		for (iter = m_filterList.rbegin( ); iter != m_filterList.rend( ); iter++ ) {
			if ((*iter)->m_nRefCnt <= 0) {
				pElementTBRemoved = *iter;
				break;
			}
		}
		if (pElementTBRemoved != NULL) {
			m_filterList.remove(pElementTBRemoved);
			delete pElementTBRemoved;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////
// CGaussFilter
//////////////////////////////////////////////////////////////////////////////////////

CGaussFilter::CGaussFilter(int nSourceSize, double dRadius) {
	m_nSourceSize = nSourceSize;
	m_dRadius = dRadius;
	memset(&m_kernels, 0, sizeof(m_kernels));

	CalculateKernels();
}

CGaussFilter::~CGaussFilter(void) {
	delete[] m_kernels.Indices;
	delete[] m_kernels.Kernels;
}

void CGaussFilter::CalculateKernels() {
	FilterKernel kernel = CalculateKernel(m_dRadius);
	int nBorderKernels = kernel.FilterLen / 2;
	m_kernels.NumKernels = 1 + 2 * nBorderKernels;
	m_kernels.Kernels = new FilterKernel[m_kernels.NumKernels];
	m_kernels.Indices = new FilterKernel*[m_nSourceSize];
	for (int i = 0; i < m_nSourceSize; i++) {
		if (i < nBorderKernels) {
			// Left border
			m_kernels.Indices[i] = &(m_kernels.Kernels[1 + i]);
		} else if (i >= m_nSourceSize - nBorderKernels) {
			// Right border
			m_kernels.Indices[i] = &(m_kernels.Kernels[1 + nBorderKernels + i - (m_nSourceSize - nBorderKernels)]);
		} else {
			m_kernels.Indices[i] = &(m_kernels.Kernels[0]);
		}
	}
	m_kernels.Kernels[0] = kernel;
	// Calculate border kernels - these are cut and renormalized Gauss kernels
	for (int i = 0; i < nBorderKernels; i++) {
		// Left border kernels
		int nStart = nBorderKernels - i;
		int nCutFilterLen = min(m_nSourceSize, kernel.FilterLen - nStart);
		m_kernels.Kernels[1 + i] = kernel;
		m_kernels.Kernels[1 + i].FilterLen = nCutFilterLen;
		m_kernels.Kernels[1 + i].FilterOffset = i;
		memmove(&(m_kernels.Kernels[1 + i].Kernel[0]), &(m_kernels.Kernels[1 + i].Kernel[nStart]), sizeof(int16) * nCutFilterLen);
		memset(&(m_kernels.Kernels[1 + i].Kernel[nCutFilterLen]), 0, sizeof(int16) * (MAX_FILTER_LEN - nCutFilterLen));
		NormalizeFilter(m_kernels.Kernels[1 + i].Kernel, nCutFilterLen);
	}
	for (int i = 0; i < nBorderKernels; i++) {
		// Right border kernels
		int j = i + nBorderKernels + 1;
		int nCutFilterLen = min(m_nSourceSize, kernel.FilterLen - i - 1);
		m_kernels.Kernels[j] = kernel;
		m_kernels.Kernels[j].FilterLen = nCutFilterLen;
		// Filter offset remains as in kernel
		memset(&(m_kernels.Kernels[j].Kernel[nCutFilterLen]), 0, sizeof(int16) * (i + 1));
		NormalizeFilter(m_kernels.Kernels[j].Kernel, nCutFilterLen);
	}
}

FilterKernel CGaussFilter::CalculateKernel(double dRadius) {
	// Gauss filter is symmetric, calculate the central element in kernel[0] and one half of the filter
	const int cnNumElems = 1 + ((MAX_FILTER_LEN - 1) >> 1);
	double kernel[cnNumElems];
	double dInnerFactor = 1.0/(2*dRadius*dRadius);
	double dSum = 0.0; // Sum of both halves of the kernel
	for (int i = 0; i < cnNumElems; i++) {
		kernel[i] = exp(-i*i*dInnerFactor);
		if (i == 0) {
			dSum += kernel[i];
		} else {
			dSum += 2 * kernel[i];
		}
	}
	// Normalize and remove small elements - these would not contribute significantly to the result
	int nFilterLen = 0;
	for (int i = 0; i < cnNumElems; i++) {
		kernel[i] /= dSum;
		if (kernel[i] > 0.002) {
			nFilterLen++;
		}
	}
	nFilterLen = 1 + (nFilterLen - 1) * 2; // will be an odd number, as always for symmetric filter kernels

	FilterKernel filterKernel;
	filterKernel.FilterLen = nFilterLen;
	filterKernel.FilterOffset = (nFilterLen - 1) >> 1; // center filter
	int j = 0;
	for (int i = filterKernel.FilterOffset; i >= 0; i--) {
		filterKernel.Kernel[i] = roundToInt16(kernel[j] * FP_ONE);
		filterKernel.Kernel[filterKernel.FilterOffset + j] = filterKernel.Kernel[i];
		j++;
	}
	NormalizeFilter(filterKernel.Kernel, nFilterLen); // again - because small elements have been removed
	return filterKernel;
}