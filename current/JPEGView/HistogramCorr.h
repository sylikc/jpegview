#pragma once

class CJPEGImage;

// Histogram (R,G,B,Grey) of an image
class CHistogram {
public:
	// If bUseOrigPixels is set to true, the original pixels (uncropped and not resized) are used.
	// If bUseOrigPixels is set to false, the cropped and resized subrectangle is used.
	CHistogram(const CJPEGImage & image, bool bUseOrigPixels);
	// Creating histogram with already known channel histograms
	CHistogram(const int* pChannelB, const int* pChannelG, const int* pChannelR, const int* pChannelGrey);

	// Gets the number of pixels used to build the histogram
	int GetTotalValues() const { return m_nTotalValues; }

	// Access to the histogram channels (256 values each)
	const int* GetChannelR() const { return &(m_ChannelR[0]); }
	const int* GetChannelG() const { return &(m_ChannelG[0]); }
	const int* GetChannelB() const { return &(m_ChannelB[0]); }
	const int* GetChannelGrey() const { return &(m_ChannelGrey[0]); }

	// Are the original, unprocessed pixels used for this histogram
	bool UsingOrigPixels() const { return m_bUseOrigPixels; }

	// Mean values of the channels
	int GetBMean() const { return m_nBMean; }
	int GetGMean() const { return m_nGMean; }
	int GetRMean() const { return m_nRMean; }

	// Examines the histogram to detect if this could be a night shot
	// The returned number is between 0 (no night shot) and 1 (night shot)
	float IsNightShot() const;

private:
	int m_nTotalValues;
	int m_ChannelR[256];
	int m_ChannelG[256];
	int m_ChannelB[256];
	int m_ChannelGrey[256];
	int m_nRMean, m_nGMean, m_nBMean;
	bool m_bUseOrigPixels;

	float m_fNightshot;
};

// Automatic contrast correction by histogram analysis
class CHistogramCorr
{
public:
	// strength of histogram correction (black-white points), in 0 .. 1
	static void SetContrastCorrectionStrength(float fStrength) { sm_ContrastCorrectionStrength = fStrength; }

	// strenght of brightness correction in 0 .. 1
	static void SetBrightnessCorrectionStrength(float fStrength) { sm_BrightnessCorrStrength = fStrength; }

	// Calculate a three channel correction LUT for color and contrast correction given a histogram
	// In fColorCastCorrection the order is C/R, M/G, Y/B
	static uint8* CalculateCorrectionLUT(const CHistogram & histogram, float fColorCorrectionFactor,
		float fBrightnessCorrectionFactor, const float fColorCastCorrection[3],
		const float fColorCorrectionStrength[6], float fContrastCorrectionFactor);

	// Combine the given two LUTs into a three channel LUT. Result(value) = 3CLUT(1CLUT(value))
	// If both LUTs are NULL, an identity LUT is returned.
	static uint8* CombineLUTs(const uint8* pSingleChannelLUT, const uint8* pThreeChannelLUT);

private:
	CHistogramCorr(void);

	static float CalcColorCastCorr(int nChannel, float fCast, const float fColorCorrectionStrength[6]);

	static float sm_ContrastCorrectionStrength;
	static float sm_BrightnessCorrStrength;
};
