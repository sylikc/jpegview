#pragma once

class CProcessingThread;

// Basic image processing methods processing the pixel data
class CBasicProcessing
{
public:

	friend class CProcessingThread;

	// Note for all methods: The caller gets ownership of the returned image and is responsible to delete 
	// this pointer if when no longer used.

	// Note on DIBs: The rows of DIBs are padded to the next 4 byte boundary. For 32 bpp DIBs this is
	// implicitely true (no padding needed), for other types the padding must be considered.
	
	// Note on IJL images: The output format of the IJL is either a 24 bit BGR format or a 8 bit grey level
	// format. Padding is to 4 byte boundaries, as DIBs.

	// Convert a 8 bpp DIB to a 32 bpp DIB
	// The palette must contain 256 entries with format BGR0 (32 bits)
	static void* Convert8bppTo32bppDIB(int nWidth, int nHeight, const void* pDIBPixels, const uint8* pPalette);

	// Convert a 32 bpp DIB to a 24 bpp DIB, flip vertically if needed.
	// Note that in contrast to the other methods, the target memory must be preallocated by the caller.
	static void Convert32bppTo24bppDIB(int nWidth, int nHeight, void* pDIBTarget, const void* pDIBSource, bool bFlip);

	// Convert from a single channel image to a 4 channel image (BGRA). No palette is given, the resulting
	// image is black/white.
	static void* Convert1To4Channels(int nWidth, int nHeight, const void* pIJLPixels);

	// Convert from a 3 channel image (BGR) to a 4 channel image (BGRA)
	static void* Convert3To4Channels(int nWidth, int nHeight, const void* pIJLPixels);

	// Convert from GDI+ 32 bpp RGBA format to 32 bpp BGRA format
	static void* ConvertGdiplus32bppRGB(int nWidth, int nHeight, int nStride, const void* pGdiplusPixels);

	// Copy rectangular pixel block from source to target 32 bpp bitmap. The target bitmap is allocated
	// if the parameter is NULL. Note that size of source and target rect must match.
	static void* CopyRect32bpp(void* pTarget, const void* pSource,  CSize targetSize, CRect targetRect,
		CSize sourceSize, CRect sourceRect);

	// Clockwise rotation of a 32 bit DIB. The rotation angle must be 90, 180 or 270 degrees, in all other
	// cases the return value is NULL
	static void* Rotate32bpp(int nWidth, int nHeight, const void* pDIBPixels, int nRotationAngleCW);

	// Crop given image
	static void* Crop32bpp(int nWidth, int nHeight, const void* pDIBPixels, CRect cropRect);

	// Create a lookuptable for brightness and contrast adaption.
	// dContrastEnh is in [-0.5 .. 0.5], dGamma in [0.1 .. 10]
	// The lookup table contains 256 entries (single channel LUT)
	static uint8* CreateSingleChannelLUT(double dContrastEnh, double dGamma);

	// Creates lookup tables for color saturation correction.
	// dSaturation must be between 0 and 2
	//  0: Grayscale image
	//  1: Image unmodified
	//  2: Image strongly staturated
	// Creates 6 * 256 entries for the matrix elements of the saturation matrix. The elements are scaled with 2^24
	static int32* CreateColorSaturationLUTs(double dSaturation);

	// Apply a three channel LUT (all B values, then all G, then all R) to a 32 bpp DIB
	static void* Apply3ChannelLUT32bpp(int nWidth, int nHeight, const void* pDIBPixels, const uint8* pLUT);

	// Apply saturation and then a three channel LUT (all B values, then all G, then all R) to a 32 bpp DIB
	static void* ApplySaturationAnd3ChannelLUT32bpp(int nWidth, int nHeight, const void* pDIBPixels, const int32* pSatLUTs, const uint8* pLUT);

	// Applies the LDC map and a three channel LUT to a 32 bpp DIB.
	// fullTargetSize: Size of target image (unclipped)
	// fullTargetOffset: Offset for start of clipping window (in the region given by fullTargetSize)
	// dibSize: Actual clipped window size, this is also the size of the input DIB
	// ldcMapSize: Size of LDC map
	// pDIBPixels: 32 bpp DIB
	// pSatLUTs: saturation LUTs - can be NULL
	// pLUT: three channel LUT (BBBBB..., GGGGG..., RRRRRR...)
	// pLDCMap: LDC map, 8 bits per pixel
	// fBlackPt, fWhitePt: Black and white point of original unprocessed, unclipped image
	// fBlackPtSteepness: Stepness of black point correction (0..1)
	static void* ApplyLDC32bpp(CSize fullTargetSize, CPoint fullTargetOffset, CSize dibSize,
		CSize ldcMapSize, const void* pDIBPixels, const int32* pSatLUTs, const uint8* pLUT, const uint8* pLDCMap, 
		float fBlackPt, float fWhitePt, float fBlackPtSteepness);

	// Dim out a rectangle in the given 32 bpp DIB using alpha blending
	// fDimValue is the value to multiply (between 0.0 and 1.0)
	// The method is inplace and changes the input DIB
	static void DimRectangle32bpp(int nWidth, int nHeight, void* pDIBPixels, CRect rect, float fDimValue);

	// Downsampling using point sampling (line removal). Point sampling is fast but produces aliasing artefacts.
	// fullTargetSize: Size of target image (unclipped)
	// fullTargetOffset: Offset for start of clipping window (in the region given by fullTargetSize)
	// clippedTargetSize: Size of clipped window - returned DIB has this size
	// sourceSize: Size of source image
	// pIJLPixels: Source image
	// nChannels: number of channels (bytes) in source image, must be 3 or 4
	// Returns a 32 bpp DIB of size clippedTargetSize
	static void* SampleDownFast(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize, 
		CSize sourceSize, const void* pIJLPixels, int nChannels);

	// Upsampling using point sampling (line duplication).
	// See SampleDownFast() for parameters
	static void* SampleUpFast(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize, 
		CSize sourceSize, const void* pIJLPixels, int nChannels);

	// High quality downsampling to target size, using a set of down-sampling kernels that
	// do some sharpening during down-sampling if desired. Note that the filter type can only
	// be one of the downsampling filter types.
	// See SampleDownFast() for other parameters
	static void* SampleDown_HQ(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize,
		CSize sourceSize, const void* pIJLPixels, int nChannels, double dSharpen, EFilterType eFilter);

	// Same as above, SSE/MMX implementation
	// See SampleDownFast() for parameters
	static void* SampleDown_HQ_SSE_MMX(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize,
		CSize sourceSize, const void* pIJLPixels, int nChannels, double dSharpen, EFilterType eFilter, bool bSSE);

	// High quality upsampling using bicubic interpolation.
	// See SampleDownFast() for parameters
	static void* SampleUp_HQ(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize,
		CSize sourceSize, const void* pIJLPixels, int nChannels);

	// Same as above, SSE/MMX implementation
	// See SampleDownFast() for parameters
	static void* SampleUp_HQ_SSE_MMX(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize,
		CSize sourceSize, const void* pIJLPixels, int nChannels, bool bSSE);

	// Debug: Gives some timing info of the last resize operation
	static LPCTSTR TimingInfo();

private:
	CBasicProcessing(void);

	static void* ApplyLDC32bpp_Core(CSize fullTargetSize, CPoint fullTargetOffset, CSize dibSize,
										CSize ldcMapSize, const void* pDIBPixels, const int32* pSatLUTs, const uint8* pLUT, const uint8* pLDCMap,
										float fBlackPt, float fWhitePt, float fBlackPtSteepness, uint32* pTarget);


	static void* SampleDown_HQ_SSE_MMX_Core(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize,
										CSize sourceSize, const void* pIJLPixels, int nChannels, double dSharpen, 
										EFilterType eFilter, bool bSSE, uint8* pTarget);

	static void* SampleUp_HQ_SSE_MMX_Core(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize,
										CSize sourceSize, const void* pIJLPixels, int nChannels, bool bSSE, 
										uint8* pTarget);

};
