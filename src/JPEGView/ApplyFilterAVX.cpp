#include "StdAfx.h"
#include "XMMImage.h"
#include "ResizeFilter.h"
#include "ApplyFilterAVX.h"

// This macro allows for aligned definition of a 32 byte value with initialization of the 16 components
// to a single value
#define DECLARE_ALIGNED_QQWORD(name, initializer) \
	int16 _tempVal##name[32]; \
	int16* name = (int16*)((((PTR_INTEGRAL_TYPE)&(_tempVal##name) + 31) & ~31)); \
	name[0] = name[1] = name[2] = name[3] = name[4] = name[5] = name[6] = name[7] = name[8] = name[9] = name[10] = name[11] = name[12] = name[13] = name[14] = name[15] = initializer;

#ifdef _WIN64

CXMMImage* ApplyFilter_AVX(int nSourceHeight, int nTargetHeight, int nWidth,
	int nStartY_FP, int nStartX, int nIncrementY_FP,
	const AVXFilterKernelBlock& filter,
	int nFilterOffset, const CXMMImage* pSourceImg) {

	int nStartXAligned = nStartX & ~15;
	int nEndXAligned = (nStartX + nWidth + 15) & ~15;
	CXMMImage* tempImage = new CXMMImage(nEndXAligned - nStartXAligned, nTargetHeight, 16);
	if (tempImage->AlignedPtr() == NULL) {
		delete tempImage;
		return NULL;
	}

	int nCurY = nStartY_FP;
	int nChannelLenBytes = pSourceImg->GetPaddedWidth() * sizeof(short);
	int nRowLenBytes = nChannelLenBytes * 3;
	int nNumberOfBlocksX = (nEndXAligned - nStartXAligned) >> 4;
	const uint8* pSourceStart = (const uint8*)pSourceImg->AlignedPtr() + nStartXAligned * sizeof(short);
	AVXFilterKernel** pKernelIndexStart = filter.Indices;

	DECLARE_ALIGNED_QQWORD(ONE_XMM, 16383 - 42); // 1.0 in fixed point notation, minus rounding correction

	__m256i ymm0 = *((__m256i*)ONE_XMM);
	__m256i ymm1 = _mm256_setzero_si256();
	__m256i ymm2;
	__m256i ymm3;
	__m256i ymm4 = _mm256_setzero_si256();
	__m256i ymm5 = _mm256_setzero_si256();
	__m256i ymm6 = _mm256_setzero_si256();
	__m256i ymm7;

	__m256i* pDestination = (__m256i*)tempImage->AlignedPtr();

	for (int y = 0; y < nTargetHeight; y++) {
		uint32 nCurYInt = (uint32)nCurY >> 16; // integer part of Y
		int filterIndex = y + nFilterOffset;
		AVXFilterKernel* pKernel = pKernelIndexStart[filterIndex];
		int filterLen = pKernel->FilterLen;
		int filterOffset = pKernel->FilterOffset;
		const __m256i* pFilterStart = (__m256i*)&(pKernel->Kernel);
		const __m256i* pSourceRow = (const __m256i*)(pSourceStart + ((int)nCurYInt - filterOffset) * nRowLenBytes);

		for (int x = 0; x < nNumberOfBlocksX; x++) {
			const __m256i* pSource = pSourceRow;
			const __m256i* pFilter = pFilterStart;
			ymm4 = _mm256_setzero_si256();
			ymm5 = _mm256_setzero_si256();
			ymm6 = _mm256_setzero_si256();
			for (int i = 0; i < filterLen; i++) {
				ymm7 = *pFilter;

				// the pixel data RED channel
				ymm2 = *pSource;
				ymm2 = _mm256_add_epi16(ymm2, ymm2);
				ymm2 = _mm256_mulhi_epi16(ymm2, ymm7);
				ymm2 = _mm256_add_epi16(ymm2, ymm2);
				ymm4 = _mm256_adds_epi16(ymm4, ymm2);
				pSource = (__m256i*)((uint8*)pSource + nChannelLenBytes);

				// the pixel data GREEN channel
				ymm3 = *pSource;
				ymm3 = _mm256_add_epi16(ymm3, ymm3);
				ymm3 = _mm256_mulhi_epi16(ymm3, ymm7);
				ymm3 = _mm256_add_epi16(ymm3, ymm3);
				ymm5 = _mm256_adds_epi16(ymm5, ymm3);
				pSource = (__m256i*)((uint8*)pSource + nChannelLenBytes);

				// the pixel data BLUE channel
				ymm2 = *pSource;
				ymm2 = _mm256_add_epi16(ymm2, ymm2);
				ymm2 = _mm256_mulhi_epi16(ymm2, ymm7);
				ymm2 = _mm256_add_epi16(ymm2, ymm2);
				ymm6 = _mm256_adds_epi16(ymm6, ymm2);
				pSource = (__m256i*)((uint8*)pSource + nChannelLenBytes);

				pFilter++;
			}

			// limit to range 0 (in ymm1), 16383-42 (in ymm0)
			ymm4 = _mm256_min_epi16(ymm4, ymm0);
			ymm5 = _mm256_min_epi16(ymm5, ymm0);
			ymm6 = _mm256_min_epi16(ymm6, ymm0);

			ymm1 = _mm256_setzero_si256();

			ymm4 = _mm256_max_epi16(ymm4, ymm1);
			ymm5 = _mm256_max_epi16(ymm5, ymm1);
			ymm6 = _mm256_max_epi16(ymm6, ymm1);

			// store result in blocks
			*pDestination++ = ymm4;
			*pDestination++ = ymm5;
			*pDestination++ = ymm6;

			pSourceRow++;
		};

		nCurY += nIncrementY_FP;
	};

	return tempImage;
}

#endif