#include "StdAfx.h"
#include "XMMImage.h"
#include "Helpers.h"

CXMMImage::CXMMImage(int nWidth, int nHeight) {
	Init(nWidth, nHeight, false);
}

CXMMImage::CXMMImage(int nWidth, int nHeight, bool bPadHeight) {
	Init(nWidth, nHeight, bPadHeight);
}

CXMMImage::CXMMImage(int nWidth, int nHeight, int nFirstX, int nLastX, int nFirstY, int nLastY, 
					 const void* pDIB, int nChannels) {
	int nSectionWidth = nLastX - nFirstX + 1;
	int nSectionHeight = nLastY - nFirstY + 1;
	Init(nSectionWidth, nSectionHeight, false);

	if (m_pMemory != NULL) {
		int nSrcLineWidthPadded = Helpers::DoPadding(nWidth * nChannels, 4);
		const uint8* pSrc = (uint8*)pDIB + nFirstY*nSrcLineWidthPadded + nFirstX*nChannels;
		uint16* pDst = (unsigned short*) m_pMemory;
		for (int j = 0; j < nSectionHeight; j++) {
			if (nChannels == 4) {
				for (int i = 0; i < nSectionWidth; i++) {
					uint32 sourcePixel = ((uint32*)pSrc)[i];
					int d = i;
					uint32 nBlue = sourcePixel & 0xFF;
					uint32 nGreen = (sourcePixel >> 8) & 0xFF;
					uint32 nRed = (sourcePixel >> 16) & 0xFF;
					pDst[d] = ((((uint16) nBlue) << 8) + nBlue) >> 2;
					d += m_nPaddedWidth;
					pDst[d] = ((((uint16) nGreen) << 8) + nGreen) >> 2;
					d += m_nPaddedWidth;
					pDst[d] = ((((uint16) nRed) << 8) + nRed) >> 2;
				}
			} else {
				for (int i = 0; i < nSectionWidth; i++) {
					int s = i*3;
					int d = i;
					pDst[d] = ((((uint16) pSrc[s]) << 8) + pSrc[s]) >> 2;
					d += m_nPaddedWidth;
					pDst[d] = ((((uint16) pSrc[s+1]) << 8) + pSrc[s+1]) >> 2;
					d += m_nPaddedWidth;
					pDst[d] = ((((uint16) pSrc[s+2]) << 8) + pSrc[s+2]) >> 2;
				}
				/*
				int nSW = nSectionWidth >> 2;
				uint32* pDst32 = (uint32*) pDst;
				uint32* pSrc32 = (uint32*) pSrc;
				uint32 nInc = m_nPaddedWidth >> 1;
				for (int i = 0; i < nSW; i++) {
					int d = i*2;
					uint32 s1 = pSrc32[i*3];
					uint32 s2 = pSrc32[i*3+1];
					uint32 s3 = pSrc32[i*3+2];
					pDst32[d]   = (((((uint16) (s1 & 0xFF)) << 8) + (s1 & 0xFF)) >> 2) + ((((((uint16) (s1 >> 24)) << 8) + (s1 >> 24)) >> 2) << 16);
					pDst32[d+1] = (((((uint16) ((s2 >> 16) & 0xFF)) << 8) + ((s2 >> 16) & 0xFF)) >> 2) + ((((((uint16) ((s3 >> 8) & 0xFF)) << 8) + ((s3 >> 8) & 0xFF)) >> 2) << 16);
					d += nInc;
					pDst32[d] = (((((uint16) ((s1 >> 8) & 0xFF)) << 8) + ((s1 >> 8) & 0xFF)) >> 2) + ((((((uint16) (s2 & 0xFF)) << 8) + (s2 & 0xFF)) >> 2) << 16);
					pDst32[d+1] = (((((uint16) ((s2 >> 24) & 0xFF)) << 8) + ((s2 >> 24) & 0xFF)) >> 2) + ((((((uint16) ((s3 >> 16) & 0xFF)) << 8) + ((s3 >> 16) & 0xFF)) >> 2) << 16);
					d += nInc;
					pDst32[d] = (((((uint16) ((s1 >> 16) & 0xFF)) << 8) + ((s1 >> 16) & 0xFF)) >> 2) + ((((((uint16) ((s2 >> 8) & 0xFF)) << 8) + ((s2 >> 8) & 0xFF)) >> 2) << 16);
					pDst32[d+1] = (((((uint16) (s3 & 0xFF)) << 8) + (s3 & 0xFF)) >> 2) + ((((((uint16) ((s3 >> 24) & 0xFF)) << 8) + ((s3 >> 24) & 0xFF)) >> 2) << 16);
				}
				*/
			}
			pDst += 3*m_nPaddedWidth;
			pSrc += nSrcLineWidthPadded;
		}
	}
}

CXMMImage::~CXMMImage(void) {
	if (m_pMemory != NULL) {
		// free the memory pages
		::VirtualFree(m_pMemory, 0, MEM_RELEASE);
		m_pMemory = NULL;
	}
}

void* CXMMImage::ConvertToDIBRGBA() const {
	if (m_pMemory == NULL) {
		return NULL;
	}
	uint8* pDIB = new uint8[m_nWidth*4 * m_nHeight];
	
	uint16* pSrc = (uint16*) m_pMemory;
	uint8* pDst = pDIB;
	for (int j = 0; j < m_nHeight; j++) {
		for (int i = 0; i < m_nWidth; i++) {
			int d = i*4;
			int s = i;
			pDst[d] = (uint8)(pSrc[s] >> 6);
			s += m_nPaddedWidth;
			pDst[d+1] = (uint8)(pSrc[s] >> 6);
			s += m_nPaddedWidth;
			pDst[d+2] = (uint8)(pSrc[s] >> 6);
			pDst[d+3] = 0;
		}
		pSrc += 3*m_nPaddedWidth;
		pDst += m_nWidth*4;
	}

	return pDIB;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Private
/////////////////////////////////////////////////////////////////////////////////////////

void CXMMImage::Init(int nWidth, int nHeight, bool bPadHeight) {
	// pad scanlines to 16 byte boundary
	m_nPaddedWidth = Helpers::DoPadding(nWidth, 8);
	if (bPadHeight) {
		m_nPaddedHeight = Helpers::DoPadding(nHeight, 8);
	} else {
		m_nPaddedHeight = nHeight;
	}
	m_nWidth = nWidth;
	m_nHeight = nHeight;
	int nMemSize = GetMemSize();

	m_pMemory = ::VirtualAlloc(
						NULL,		// let the call determine the start address
						nMemSize, // the size
						MEM_RESERVE | MEM_COMMIT,	// I want that memory, now
						PAGE_READWRITE);			// need both read and write
}
