#pragma once

#include "ImageProcessingTypes.h"

// Represents an image with line interleaving and padding rows to 2^x bytes (16 for SSE, 32 for AVX) optimal for
// SIMD processing. Each pixel has 16 bits per channel, channel order is B, G, R, x stands for padding:
// BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBxxx
// GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGxxx
// RRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRxxx
// BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBxxx
// GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGxxx
// RRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRxxx
class CXMMImage
{
public:
	// padding is in pixels (not bytes)
	CXMMImage(int nWidth, int nHeight, int padding);
	CXMMImage(int nWidth, int nHeight, bool bPadHeight, int padding); // padding is in pixels (not bytes), width is always padded, height only when bPadHeight is true
	// convert from section of 24 or 32 bpp DIB, from first to (and including) last column and row
	// padding is in pixels(not bytes)
	CXMMImage(int nWidth, int nHeight, int nFirstX, int nLastX, int nFirstY, int nLastY, const void* pDIB, int nChannels, int padding);
	~CXMMImage(void);

	// Pointer to aligned memory of 16 bpp image
	void * AlignedPtr() { return m_pMemory; }
	void * AlignedPtr() const { return m_pMemory; }

	// Geometry
	int GetWidth() const { return m_nWidth; }
	int GetHeight() const { return m_nHeight; }
	int GetPaddedWidth() const { return m_nPaddedWidth; }
	int GetPaddedHeight() const { return m_nPaddedHeight; }

	// Generate a BGRA (32 bit) DIB and return it, caller gets ownership of returned object
	void* ConvertToDIBRGBA() const;

private:
	int GetLineSize() const { return m_nPaddedWidth*2; }
	int GetMemSize() const { return GetLineSize()*3*m_nPaddedHeight; }
	void Init(int nWidth, int nHeight, bool bPadHeight, int padding);

	void* m_pMemory;
	int m_nWidth, m_nHeight;
	int m_nPaddedWidth; // in pixels
	int m_nPaddedHeight; // in pixels
};
