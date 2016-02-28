#pragma once

#include "ImageProcessingTypes.h"

// Represents an image with line interleaving and padding to 16 bytes (8 pixels) usable for
// XMM processing. Each pixel has 16 bits per channel, channel order is B, G, R
class CXMMImage
{
public:
	CXMMImage(int nWidth, int nHeight);
	CXMMImage(int nWidth, int nHeight, bool bPadHeight);
	// convert from section of 24 or 32 bpp DIB, from first to (and including) last column and row
	CXMMImage(int nWidth, int nHeight, int nFirstX, int nLastX, int nFirstY, int nLastY, const void* pDIB, int nChannels);
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
	void Init(int nWidth, int nHeight, bool bPadHeight);

	void * m_pMemory;
	int m_nWidth, m_nHeight;
	int m_nPaddedWidth; // in pixels
	int m_nPaddedHeight; // in pixels
};
