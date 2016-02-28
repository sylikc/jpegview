#pragma once

class CProcessingThread;

// Basic image processing methods processing the image pixel data
class CBasicProcessing
{
public:

	friend class CProcessingThread;

	// Note for all methods: The caller gets ownership of the returned image and is responsible to delete 
	// this pointer when no longer used.
	
	// Note for all methods: If there is not enough memory to allocate a new image, all methods return a null pointer
	// No exception is thrown in this case.

	// Note on DIBs: The rows of DIBs are padded to the next 4 byte boundary. For 32 bpp DIBs this is
	// implicitely true (no padding needed), for other types the padding must be considered.
	
	// Note: The output format of the JPEG lib is 24 bit BGR format. Padding is to 4 byte boundaries, as DIBs.

	// Convert a 8 bpp DIB with color palette to a 32 bpp BGRA DIB
	// The palette must contain 256 entries with format BGR0 (32 bits)
	static void* Convert8bppTo32bppDIB(int nWidth, int nHeight, const void* pDIBPixels, const uint8* pPalette);

	// Convert a 32 bpp BGRA DIB to a 24 bpp BGR DIB, flip vertically if requested.
	// Note that in contrast to the other methods, the target memory must be preallocated by the caller.
	// Consider row padding of the DIB to 4 byte boundary when allocating memory for the target DIB
	static void Convert32bppTo24bppDIB(int nWidth, int nHeight, void* pDIBTarget, const void* pDIBSource, bool bFlip);

	// Convert from a single channel image (8 bpp) to a 4 channel image (32 bpp DIB, BGRA).
	// The source image is row-padded to 4 byte boundary
	// No palette is given, the resulting image is black/white.
	static void* Convert1To4Channels(int nWidth, int nHeight, const void* pPixels);

	// Convert a 16 bpp single channel grayscale image to a 32 bpp BGRA DIB
	static void* Convert16bppGrayTo32bppDIB(int nWidth, int nHeight, const int16* pPixels);

	// Convert from a 3 channel image (24 bpp, BGR) to a 4 channel image (32 bpp DIB, BGRA)
	static void* Convert3To4Channels(int nWidth, int nHeight, const void* pIJLPixels);

	// Convert from GDI+ 32 bpp RGBA format to 32 bpp BGRA DIB format
	static void* ConvertGdiplus32bppRGB(int nWidth, int nHeight, int nStride, const void* pGdiplusPixels);

	// Copy rectangular pixel block from source to target 32 bpp bitmap. The target bitmap is allocated
	// if the 'pTarget' parameter is NULL. Note that size of source and target rect must match.
	static void* CopyRect32bpp(void* pTarget, const void* pSource,  CSize targetSize, CRect targetRect,
		CSize sourceSize, CRect sourceRect);

	// Clockwise rotation of a 32 bit DIB. The rotation angle must be 90, 180 or 270 degrees, in all other
	// cases the return value is NULL
	static void* Rotate32bpp(int nWidth, int nHeight, const void* pDIBPixels, int nRotationAngleCW);

	// Mirror 32 bit DIB horizontally
	static void* MirrorH32bpp(int nWidth, int nHeight, const void* pDIBPixels);

	// Mirror 32 bit DIB vertically
	static void* MirrorV32bpp(int nWidth, int nHeight, const void* pDIBPixels);

	// Mirror 32 bit DIB vertically inplace
	static void MirrorVInplace(int nWidth, int nHeight, int nStride, void* pDIBPixels);

	// Crop specified 32 bpp image. Returns NULL if cropRect is not fully inside source image.
	static void* Crop32bpp(int nWidth, int nHeight, const void* pDIBPixels, CRect cropRect);

	// Create a lookup-table for brightness and contrast correction.
	// dContrastEnh is in [-0.5 .. 0.5], dGamma in [0.1 .. 10]
	// The lookup table contains 256 uint8 entries (single channel LUT)
	static uint8* CreateSingleChannelLUT(double dContrastEnh, double dGamma);

	// Creates lookup tables for color saturation correction.
	// dSaturation must be between 0 and 2
	//  0: Grayscale image
	//  1: Image unmodified
	//  2: Image strongly staturated
	// Creates 6 * 256 int32 entries for the matrix elements of the saturation matrix. The elements are in 8.24 fixed point format.
	static int32* CreateColorSaturationLUTs(double dSaturation);

	// Create a one channel 16 bpp gray scale image from a 32 or 24 bpp BGR(A) DIB image (nChannels must be 3 or 4)
	// In the resulting image, 14 bits are used, thus white is 2^14 
	static int16* Create1Channel16bppGrayscaleImage(int nWidth, int nHeight, const void* pDIBPixels, int nChannels);

	// Apply a three channel LUT (memory layout: 256 uint8 values for B channel, followed by 256 uint8 values for G channel,
	// followed by 256 uint8 values for R channel, totally 3*256 bytes) to a 32 bpp BGRA DIB.
	// Notice that the A channel is not processed and set to fixed value 0xFF.
	static void* Apply3ChannelLUT32bpp(int nWidth, int nHeight, const void* pDIBPixels, const uint8* pLUT);

	// Apply the specified saturation LUTs (as returned by CreateColorSaturationLUTs() method) and then a 
	// three channel LUT (256*B, 256*G, 256*R, see above for details) to a 32 bpp BGRA DIB
	// Notice that the A channel is not processed and set to fixed value 0xFF.
	static void* ApplySaturationAnd3ChannelLUT32bpp(int nWidth, int nHeight, const void* pDIBPixels, const int32* pSatLUTs, const uint8* pLUT);

	// Dim out a rectangle in the given 32 bpp BGRA DIB.
	// Notice that dimming is done by modifying the BGR values, the A channel is set to fixed value 0xFF.
	// fDimValue is the value to multiply with the B, G and R values (between 0.0 and 1.0)
	// The method is inplace and changes the input DIB
	static void DimRectangle32bpp(int nWidth, int nHeight, void* pDIBPixels, CRect rect, float fDimValue);

	// Fills a rectangle in the 32 bpp BGRA DIB with the given color
	// The method is inplace and changes the input DIB. Clipping of rect against DIB size is done.
	// The A value in 'color' is ignored and set to fixed value 0xFF.
	static void FillRectangle32bpp(int nWidth, int nHeight, void* pDIBPixels, CRect rect, COLORREF color);

	// The following methods take into account that the original image with size (w, h) - denoted as 'sourceSize' -
	// is zoomed by a factor x, thus resulting in a (virtual) image size of (w * x, h * x) - denoted as 'fullTargetSize'.
	// Actually displayed is only a part of this virtual image, using a cropping rectangle with top, left
	// coordinates (xo, yo) - denoted as 'fullTargetOffset' - and a cropping rectangle size (wc, hc) - denoted
	// as 'clippedTargetSize'.

	// Applies the LDC (local density correction) map and a three channel LUT (256*B, 256*G, 256*R, see above for details)
	// to a 32 bpp BGRA DIB.
	// Notice that the A channel is not processed and set to fixed value 0xFF.
	// This method is used to lighten shadows and darken highlights.
	// fullTargetSize: Virtual size of zoomed image.
	// fullTargetOffset: Offset for start of clipping window (in the region given by fullTargetSize)
	// clippedTargetSize: Actual clipped window size, this is also the size of the input DIB
	// ldcMapSize: Size of LDC map in pixels
	// pDIBPixels: 32 bpp input DIB, size is clippedTargetSize
	// pSatLUTs: saturation LUTs - can be NULL. Format as returned by CreateColorSaturationLUTs() method.
	// pLUT: three channel LUT (BBBBB..., GGGGG..., RRRRRR..., 256 entries per channel)
	// pLDCMap: LDC map, 8 bits per pixel, grayscale
	// fBlackPt, fWhitePt: Black and white point of original unprocessed, unclipped image
	// fBlackPtSteepness: Stepness of black point correction (0..1)
	static void* ApplyLDC32bpp(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize,
		CSize ldcMapSize, const void* pDIBPixels, const int32* pSatLUTs, const uint8* pLUT, const uint8* pLDCMap, 
		float fBlackPt, float fWhitePt, float fBlackPtSteepness);

	// Resize 32 or 24 bpp BGR(A) image using point sampling (i.e. no interpolation).
	// Point sampling is fast but produces a lot of aliasing artefacts.
	// Notice that the A channel is kept unchanged for 32 bpp images.
	// Notice that the returned image is always 32 bpp!
	// fullTargetSize: Virtual size of target image (unclipped).
	// fullTargetOffset: Offset for start of clipping window (in the region given by fullTargetSize)
	// clippedTargetSize: Size of clipped window - returned DIB has this size
	// sourceSize: Size of source image
	// pPixels: Source image
	// nChannels: Number of channels (bytes) in source image, must be 3 or 4
	// Returns a 32 bpp BGRA DIB of size 'clippedTargetSize'
	static void* PointSample(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize, 
		CSize sourceSize, const void* pPixels, int nChannels);

	// Rotate 32 or 24 bpp BGR(A) image and resample using point sampling (i.e. no interpolation). Rotation is around image center.
	// Notice that the A channel is kept unchanged for 32 bpp images.
	// Notice that the returned image is always 32 bpp!
	// dRotation: Rotation angle in radians
	// backColor: Color to fill areas in rotated target image that are outside of source image. The A value is ignored and set to 0xFF.
	// See PointSample() for other parameters
	// Returns a 32 bpp BGRA DIB of size 'clippedTargetSize'
	static void* PointSampleWithRotation(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize, 
		CSize sourceSize, double dRotation, const void* pPixels, int nChannels, COLORREF backColor);

	// Resample 32 or 24 bpp BGR(A) image using point sampling (i.e. no interpolation) and map to trapezoid.
	// Notice that the A channel is kept unchanged for 32 bpp images.
	// Notice that the returned image is always 32 bpp!
	// fullTargetTrapezoid: Trapezoid, height must be fullTargetSize.cy.
	// fullTargetOffset: Offset in fullTargetSize image, before mapping to trapezoid
	// backColor: Color to fill areas in transformed target image that are outside of source image. The A value is ignored and set to 0xFF.
	// See PointSample() for other parameters
	// Returns a 32 bpp BGRA DIB of size 'clippedTargetSize'
	static void* PointSampleTrapezoid(CSize fullTargetSize, const CTrapezoid& fullTargetTrapezoid, CPoint fullTargetOffset, CSize clippedTargetSize, 
		CSize sourceSize, const void* pPixels, int nChannels, COLORREF backColor);

	// High quality downsampling of 32 or 24 bpp BGR(A) image to target size, using a set of down-sampling kernels that
	// do some sharpening during down-sampling if desired. 
	// Notice that the A channel is not processed and set to fixed value 0xFF.
	// Notice that the returned image is always 32 bpp!
	// dSharpen: Amount of sharping to apply in [0, 1].
	// eFilter: Filter to apply. Note that the filter type can only be one of the downsampling filter types.
	// See PointSample() for other parameters
	// Returns a 32 bpp BGRA DIB of size 'clippedTargetSize'
	static void* SampleDown_HQ(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize,
		CSize sourceSize, const void* pPixels, int nChannels, double dSharpen, EFilterType eFilter);

	// Same as above, SSE/MMX implementation.
	// Notice that the A channel is not processed and set to fixed value 0xFF.
	// Notice that the returned image is always 32 bpp!
	static void* SampleDown_HQ_SSE_MMX(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize,
		CSize sourceSize, const void* pPixels, int nChannels, double dSharpen, EFilterType eFilter, bool bSSE);

	// High quality upsampling of 32 or 24 bpp BGR(A) image using bicubic interpolation.
	// Notice that the A channel is not processed and set to fixed value 0xFF.
	// Notice that the returned image is always 32 bpp!
	// See PointSample() for parameters
	static void* SampleUp_HQ(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize,
		CSize sourceSize, const void* pPixels, int nChannels);

	// Same as above, SSE/MMX implementation
	// Notice that the A channel is not processed and set to fixed value 0xFF.
	// Notice that the returned image is always 32 bpp!
	static void* SampleUp_HQ_SSE_MMX(CSize fullTargetSize, CPoint fullTargetOffset, CSize clippedTargetSize,
		CSize sourceSize, const void* pPixels, int nChannels, bool bSSE);

	// Rotate 32 or 24 bpp BGR(A) image around image center using bicubic interpolation.
	// Notice that the A channel is processed for 32 bpp images.
	// Notice that the returned image is always 32 bpp!
	// targetOffset: Offset for start of clipping window (in the region given by sourceSize)
	// targetSize: Size of target image - returned DIB has this size
	// dRotation: rotation angle in radians
	// sourceSize: Size of source image (pSourcePixels)
	// pSourcePixels: Source image
	// nChannels: number of channels (bytes) in source image, must be 3 or 4
	// backColor: color to fill background of rotated image
	// Returns a 32 bpp DIB of size targetSize
	static void* RotateHQ(CPoint targetOffset, CSize targetSize, double dRotation, CSize sourceSize, 
		const void* pSourcePixels, int nChannels, COLORREF backColor);

	// Trapezoid correction (used for perspective correction) using bicubic interpolation of 32 or 24 bpp BGR(A) image.
	// This method is used for perspective correction.
	// Notice that the A channel is not processed and set to fixed value 0xFF.
	// Notice that the returned image is always 32 bpp!
	// targetOffset: Offset for start of clipping window (in the region given by sourceSize)
	// targetSize: Size of target image - returned DIB has this size
	// trapezoid: Trapezoid end points, height + 1 must equal sourceSize.cy
	// sourceSize: Size of source image
	// pSourcePixels: Source image
	// nChannels: Number of channels (bytes) in source image, must be 3 or 4
	// backColor: Color to fill background of rotated image
	// Returns a 32 bpp DIB of size targetSize
	static void* TrapezoidHQ(CPoint targetOffset, CSize targetSize, const CTrapezoid& trapezoid, CSize sourceSize, 
		const void* pSourcePixels, int nChannels, COLORREF backColor);

	// Gauss filtering of a 16 bpp 1 channel image. In the image with size fullSize, the rectangle rect at position offset is filtered.
	// The returned image has size 'rect'.
	static int16* GaussFilter16bpp1Channel(CSize fullSize, CPoint offset, CSize rect, double dRadius, const int16* pPixels);

	// Apply unsharp masking to the given source 32 or 24 bpp BGR(A) image and store result in pTargetPixels.
	// Notice that the A channel is not processed and set to fixed value 0xFF.
	// fullSize: Size of source image, target image and grayscale images (all must have the same size)
	// offset, rect: Part of the source image to apply unsharp masking to. The area outside is not touched (also not copied to target image).
	// dAmount, dThreshold: Unsharp masking parameters
	// pGrayImage: Grayscale image, one channel, 16 bpp
	// pSmoothedGrayImage: Smoothed grayscale image (having a Gauss kernel applied), one channel, 16 bpp
	// pSourcePixels: Source image
	// pTargetPixels: Target image, can be same pointer as pSourcePixels
	// nChannels: Number of channels (3 or 4) of source and target image
	// Returns pTargetPixels.
	static void* UnsharpMask(CSize fullSize, CPoint offset, CSize rect, double dAmount, double dThreshold, 
		const int16* pGrayImage, const int16* pSmoothedGrayImage, const void* pSourcePixels, void* pTargetPixels, int nChannels);

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

	static int16* GaussFilter16bpp1Channel_Core(CSize fullSize, CPoint offset, CSize rect, int nTargetWidth, double dRadius, 
												const int16* pSourcePixels, int16* pTargetPixels);

	static void* UnsharpMask_Core(CSize fullSize, CPoint offset, CSize rect, double dAmount, const int16* pThresholdLUT,
								  const int16* pGrayImage, const int16* pSmoothedGrayImage, const void* pSourcePixels, void* pTargetPixels, int nChannels);

	static void* RotateHQ_Core(CPoint targetOffset, CSize targetSize, double dRotation, CSize sourceSize, 
							   const void* pSourcePixels, void* pTargetPixels, int nChannels, COLORREF backColor);

	static void* TrapezoidHQ_Core(CPoint targetOffset, CSize targetSize, const CTrapezoid& trapezoid, CSize sourceSize, 
						   const void* pSourcePixels, void* pTargetPixels, int nChannels, COLORREF backColor);
};
