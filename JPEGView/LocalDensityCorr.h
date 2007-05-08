#pragma once

class CJPEGImage;
class CHistogram;

// Local density correction for an image
class CLocalDensityCorr
{
public:
	// Note: Partially constructed LDC object allow to get the histogram and the pixel hash but not much more
	CLocalDensityCorr(const CJPEGImage & image, bool bFullConstruct);
	~CLocalDensityCorr(void);
	// if only constructed partially does the rest for creating a fully functional LDC object
	void VerifyFullyConstructed();

	// Gets the histogram - caller gets ownership of the histogram.
	// Can only be called once.
	// The histogram is a side product of the LDC and can be retrieved if needed
	CHistogram* GetHistogram();

	// Gets the LDC map. The entries in the map are one byte, 127 means no correction, 
	// 0 means darken max, 255 means brighten max
	const uint8* GetLDCMap();

	// Returns the pixel hash value. Note that for compressed images, using the pixel hash is
	// not a good identifier because the value depends on the implementation of the decompression.
	// For JPEG files use the method Helpers::CalculateJPEGFileHash() instead.
	__int64 GetPixelHash() const { return m_nPixelHash; }

	// Gets the size of the LDC map
	CSize GetLDCMapSize() const { return CSize(m_nLDCWidth, m_nLDCHeight); }

	// Get black or white point of image (in grey channel)
	float GetBlackPt() const { return m_fBlackPt; }
	float GetWhitePt() const { return m_fWhitePt; }

	// Sets the amount of LDC for shadows and highlights. Both values must be between 0 and 1
	void SetLDCAmount(double dLightenShadows, double dDarkenHighlights);

	// Returns if this could be a sunset picture.
	// The returned number is between 0 (no sunset) and 1 (sunset)
	float IsSunset() const { return m_fIsSunset; }

	bool IsMaskAvailable() const { return m_pLDCMap != NULL; }

private:
	CHistogram* m_pHistogramm;
	int m_nLDCWidth;
	int m_nLDCHeight;
	uint8* m_pLDCMap;
	uint8* m_pLDCMapMultiplied;
	float m_fBlackPt, m_fWhitePt;
	float m_fIsSunset;
	float m_fMiddleGrey;
	float m_fSunsetPixels;
	bool m_bFlipped;
	__int64 m_nPixelHash;

	uint16* m_pPointSampledImage;
	int m_nPSIWidth;
	int m_nPSIHeight;

	void SmoothLDCMask();
	uint8* MultiplyMap(double dLightenShadows, double dDarkenHighlights);
	float CheckIfSunset(uint32* pRowBGR, int nHeight);
	void CreateLDCMap();
};
