#pragma once

class CJPEGImage;
class CHistogram;

// Local density correction for an image, lighting shadows and darkening highlights
class CLocalDensityCorr
{
public:
	// Note: Partially constructed LDC object allows getting the histogram and the pixel hash but not much more
	CLocalDensityCorr(const CJPEGImage & image, bool bFullConstruct);
	~CLocalDensityCorr(void);
	// if only constructed partially this does the rest for creating a fully functional LDC object
	void VerifyFullyConstructed();

	// Gets the histogram.
	// The histogram is a side product of the LDC and can be retrieved if needed
	const CHistogram* GetHistogram() { return m_pHistogramm; }

	// Gets the LDC map. The entries in the map are one byte, 127 means no correction, 
	// 0 means darken max, 255 means brighten max.
	// The size of this map is given by GetLDCMapSize().
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

	// Gets the size of the point sampled image
	CSize GetPSISize() const { return CSize(m_nPSIWidth, m_nPSIHeight); }

	// Gets the point sampled image as 32 bpp DIB. The caller gets ownership of the returned DIB
	// and must delete it when no longer used.
	void* GetPSImageAsDIB();

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
	__int64 m_nPixelHash;

	uint16* m_pPointSampledImage;
	int m_nPSIWidth;
	int m_nPSIHeight;

	void SmoothLDCMask();
	uint8* MultiplyMap(double dLightenShadows, double dDarkenHighlights);
	float CheckIfSunset(uint32* pRowBGR, int nHeight);
	void CreateLDCMap();
};
