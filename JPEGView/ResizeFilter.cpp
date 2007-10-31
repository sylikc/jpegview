#include "StdAfx.h"
#include "ResizeFilter.h"
#include <math.h>
#include <stdlib.h>

// We cannot enlarge to a factor above the number of kernels without artefacts
#define NUM_KERNELS_RESIZE 32
#define NUM_KERNELS_RESIZE_LOG2 5


//////////////////////////////////////////////////////////////////////////////////////
// Helpers
//////////////////////////////////////////////////////////////////////////////////////

inline static int16 round(double d) {
	return (d < 0) ? (int16)(d - 0.5) : (int16)(d + 0.5);
}

// normalization means: sum of filter elements equals FP_ONE
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
// Public
//////////////////////////////////////////////////////////////////////////////////////

CResizeFilter::CResizeFilter(int nSourceSize, int nTargetSize, double dSharpen, EFilterType eFilter, bool bXMM) {
	m_nSourceSize = nSourceSize;
	m_nTargetSize = nTargetSize;
	m_dSharpen = min(0.5, max(0.0, dSharpen));
	m_eFilter = eFilter;
	m_bCalculated = false;
	m_bXMMCalculated = false;
	m_nRefCnt = 0;
	memset(&m_kernels, 0, sizeof(m_kernels));
	memset(&m_kernelsXMM, 0, sizeof(m_kernelsXMM));

	if (bXMM) {
		CalculateXMMFilterKernels();
	} else {
		CalculateFilterKernels();
	}
}

CResizeFilter::~CResizeFilter(void) {
	if (m_bCalculated) {
		delete[] m_kernels.Indices;
		delete[] m_kernels.Kernels;
	}
	if (m_bXMMCalculated) {
		delete[] m_kernelsXMM.Indices;
		delete[] m_kernelsXMM.UnalignedMemory;
	}
}

bool CResizeFilter::ParametersMatch(int nSourceSize, int nTargetSize, double dSharpen, EFilterType eFilter, bool bXMM) {
	if (nSourceSize == m_nSourceSize && nTargetSize == m_nTargetSize && abs(dSharpen - m_dSharpen) < 1e-6 &&
		eFilter == m_eFilter && ((bXMM && m_bXMMCalculated) || !bXMM)) {
			return true;
	} else {
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////////////////
// Private
//////////////////////////////////////////////////////////////////////////////////////

void CResizeFilter::CalculateFilterKernels() {
	CalculateFilterParams(m_eFilter);

	m_bCalculated = true;
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
		nIncrementX = (uint32)((m_nSourceSize - 1) << 16)/(m_nTargetSize - 1);
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
			int nFilterLen = min(m_nTargetSize, m_nFilterLen - (m_nFilterOffset - nXInt));
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
			int nFilterLen =  min(m_nTargetSize, m_nSourceSize - nXInt + m_nFilterOffset);
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
	m_bXMMCalculated = true;
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
		int nIndex = m_kernels.Indices[i] - m_kernels.Kernels;
		m_kernelsXMM.Indices[i] = pKernelStartAddress[nIndex];
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
		m_dMultX = 1.0/((dFactor + 1)*0.5);
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

static inline double EvaluateCore_Narrow(double dX, double dSharpen) {
	if (dX < -1.5 || dX > 1.5) {
		return 0.0;
	} else if (dX < -0.5) {
		double dTemp = 2*dX + 2;
		return -dSharpen*(1 - dTemp*dTemp);
	} else if (dX < 0.5) {
		double dTemp = 2*dX;
		return 1 - dTemp*dTemp;
	} else {
		double dTemp = 2*dX - 2;
		return -dSharpen*(1 - dTemp*dTemp);
	}
}

static inline double EvaluateCore_BestQuality(double dX, double dSharpen) {
	// piecewise quadratic wave like filter
	if (dX < -2 || dX > 2) {
		return 0.0;
	} else if (dX < -1) {
		double dTemp = 2*dX + 3;
		return -dSharpen*(1 - dTemp*dTemp);
	} else if (dX < 1) {
		return 1 - dX*dX;
	} else {
		double dTemp = 2*dX - 3;
		return -dSharpen*(1 - dTemp*dTemp);
	}	
}

#define PI 3.141592653
#define PI2 (PI/2)
#define PISQR (PI*PI)

static inline double EvaluateCore_NoAliasing(double dX, double dSharpen) {
	// this is a Lanczos filter
	if (dX < -2 || dX > 2) {
		return 0.0;
	} else if (dX == 0.0) {
		return 1.0;
	} else if (dX > -1 && dX < 1) {
		return (2*sin(PI*dX)*sin(PI2*dX))/(PISQR*dX*dX);
	} else {
		return dSharpen*(2*sin(PI*dX)*sin(PI2*dX))/(PISQR*dX*dX);
	}
}

double CResizeFilter::EvaluateKernel(double dX, EFilterType eFilter) {
	double dXScaled = dX*m_dMultX;

	// take integral of target function from [dX - 0.5, dX + 0.5]
	const double NUM_STEPS = 32;
	double dStartX = dXScaled - m_dMultX*0.5;
	double dStepX = m_dMultX*(1.0/(NUM_STEPS - 1));
	double dSum = 0.0;
	for (int i = 0; i < NUM_STEPS; i++) {
		switch (eFilter) {
			case Filter_Downsampling_Best_Quality:
				dSum += EvaluateCore_BestQuality(dStartX, m_dSharpen);
				break;
			case Filter_Downsampling_No_Aliasing:
				dSum += EvaluateCore_NoAliasing(dStartX, m_dSharpen);
				break;
			case Filter_Downsampling_Narrow:
				dSum += EvaluateCore_Narrow(dStartX, m_dSharpen);
				break;
		}
		dStartX += dStepX;
	}
	return dSum;
}

double CResizeFilter::EvaluateCubicFilterKernel(double dFrac, int nKernelElement) {
	const double cdFactorA = -0.5;
	double dAbsDiff;
	switch (nKernelElement) {
		case 0:
			dAbsDiff = 1.0 + dFrac;
			return cdFactorA*dAbsDiff*dAbsDiff*dAbsDiff - 5*cdFactorA*dAbsDiff*dAbsDiff +
					8*cdFactorA*dAbsDiff - 4*cdFactorA;
		case 1:
			dAbsDiff = dFrac;
			return (cdFactorA+2)*dAbsDiff*dAbsDiff*dAbsDiff -
								(cdFactorA+3)*dAbsDiff*dAbsDiff + 1;
		case 2:
			dAbsDiff = (1.0 - dFrac);
			return (cdFactorA+2)*dAbsDiff*dAbsDiff*dAbsDiff -
								(cdFactorA+3)*dAbsDiff*dAbsDiff + 1;
		case 3:
			dAbsDiff = 1.0 + (1.0 - dFrac);
			return cdFactorA*dAbsDiff*dAbsDiff*dAbsDiff - 5*cdFactorA*dAbsDiff*dAbsDiff +
					8*cdFactorA*dAbsDiff - 4*cdFactorA;
	}
	return -1;
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
		} else {
			dFilter[i] = EvaluateKernel(-m_nFilterOffset + i - dFrac, eFilter);
		}
		dSum += dFilter[i];
	}
	int nSum = 0;
	for (int i = 0; i < m_nFilterLen; i++) {
		m_Filter[i] = round(FP_ONE * dFilter[i] / dSum);
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

const CResizeFilter& CResizeFilterCache::GetFilter(int nSourceSize, int nTargetSize, double dSharpen, EFilterType eFilter, bool bXMM) {
	CResizeFilter* pMatchingFilter = NULL;

	::EnterCriticalSection(&m_csList);
	std::list<CResizeFilter*>::iterator iter;
	for (iter = m_filterList.begin( ); iter != m_filterList.end( ); iter++ ) {
		if ((*iter)->ParametersMatch(nSourceSize, nTargetSize, dSharpen, eFilter, bXMM)) {
			pMatchingFilter = *iter;
			break;
		}
	}

	if (pMatchingFilter != NULL) {
		// found matching filter, return it
		pMatchingFilter->m_nRefCnt++;
		m_filterList.remove(pMatchingFilter);
		m_filterList.push_front(pMatchingFilter); // move to top in list
		::LeaveCriticalSection(&m_csList);
		return *pMatchingFilter;
	}

	// no matching filter found, create a new one
	CResizeFilter* pNewFilter = new CResizeFilter(nSourceSize, nTargetSize, dSharpen, eFilter, bXMM);
	pNewFilter->m_nRefCnt++;
	m_filterList.push_front(pNewFilter);

	::LeaveCriticalSection(&m_csList);

	return *pNewFilter;
}

void CResizeFilterCache::ReleaseFilter(const CResizeFilter& filter) {
	
	const int MAX_SIZE = 4;

	::EnterCriticalSection(&m_csList);
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
	::LeaveCriticalSection(&m_csList);
}
