#pragma once

#include "ProcessParams.h"

class CHistogram;
class CLocalDensityCorr;
class CEXIFReader;
class CRawMetadata;
enum TJSAMP;

// Represents a rectangle to dim out in the image
struct CDimRect {
	CDimRect() {}
	CDimRect(float fFactor, const CRect& rect) { Factor = fFactor; Rect = rect; }
	float Factor; // between 0.0 and 1.0
	CRect Rect;
};

// Class holding a decoded image (not just JPEG - any supported format) and its meta data (if available).
class CJPEGImage {
public:
	// Ownership of memory in pPixels goes to class, accessing this pointer after the constructor has been called
	// may causes access violations (use OriginalPixels() instead).
	// nChannels can be 1 (greyscale image), 3 (BGR color image) or 4 (BGRA color image, A ignored)
	// pEXIFData can be a pointer to the APP1 block containing the EXIF data. If this pointer is null
	// no EXIF data is available.
	// The nJPEGHash hash value gives a hash over the compressed JPEG pixels that uniquely identifies the
	// JPEG image. It can be zero in which case a pixel based hash value is internally created.
	// The image format is a hint about the original image format this image was created from.
	// The bIsAnimation, nFrameIndex, nNumberOfFrames and nFrameTimeMs are used for multiframe images, e.g. animated GIFs.
	// Frame index is zero based and the frame time is given in milliseconds.
	// The pLDC object is used internally only for thumbnail image creation to avoid duplication. In all other situations,
	// its value must be NULL.
	// If RAW metadata is specified, ownership of this memory is transferred to this class.
	CJPEGImage(int nWidth, int nHeight, void* pPixels, void* pEXIFData, int nChannels, 
		__int64 nJPEGHash, EImageFormat eImageFormat, bool bIsAnimation, int nFrameIndex, int nNumberOfFrames, int nFrameTimeMs,
		CLocalDensityCorr* pLDC = NULL, bool bIsThumbnailImage = false, CRawMetadata* pRawMetadata = NULL);
	~CJPEGImage(void);

	// Gets resampled and processed 32 bpp DIB image (up or downsampled).
	// Parameters:
	// fullTargetSize: Full target size of resized image to render (without any clipping to actual clipping size)
	// clippingSize: Sub-rectangle in fullTargetSize to render (the returned DIB has this size)
	// targetOffset: Offset of the clipping rectangle in full target size
	// imageProcParams: Processing parameters, such as contrast, brightness and sharpen
	// eProcFlags: Processing flags, e.g. apply LDC
	// Return value:
	// The returned DIB is a 32 bpp DIB (BGRA). Note that the returned pointer must not
	// be deleted by the caller and is only valid until another call to GetDIB() is done, the CJPEGImage
	// object is disposed or the Rotate(), Crop() or any other method affecting original pixels is called!
	// Note: The method tries to satisfy consecuting requests as efficient as possible. Calling the GetDIB()
	// method multiple times with the same set of parameters will return the same image starting from the second
	// call without doing image processing anymore.
	void* GetDIB(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset,
		const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags) {
		bool bNotUsed;
		return GetDIBInternal(fullTargetSize, clippingSize, targetOffset, imageProcParams, eProcFlags, NULL, NULL, 0.0, false, bNotUsed);
	}

	// Gets resampled and processed DIB image (up or downsampled), also including a low quality rotation.
	// The rotation angle is specified in radians.
	// PFLAG_HighQualityResampling must not be set when calling this method 
	void* GetDIBRotated(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset,
		const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags, double dRotation, bool bShowGrid) {
		bool bNotUsed;
		return GetDIBInternal(fullTargetSize, clippingSize, targetOffset, imageProcParams, eProcFlags, NULL, NULL, dRotation, bShowGrid, bNotUsed);
	}

	// Gets resampled and processed DIB image (up or downsampled), including a low quality trapezoid transformation.
	// PFLAG_HighQualityResampling must not be set when calling this method 
	void* GetDIBTrapezoid(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset,
		const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags, const CTrapezoid* pTrapezoid, bool bShowGrid) {
		bool bNotUsed;
		return GetDIBInternal(fullTargetSize, clippingSize, targetOffset, imageProcParams, eProcFlags, NULL, pTrapezoid, 0.0, bShowGrid, bNotUsed);
	}

	// Gets a thumbnail of the original image.
	// The returned thumbnail (32 bpp DIB) has the specified size. 'Size' should not be larger than 400 x 300 pixels
	void* GetThumbnailDIB(CSize size, const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags);

	// Gets a thumbnail of a section of the original image.
	// The returned thumbnail (32 bpp DIB) has the size 'clippingSize'. 'ClippingSize' should not be larger than 400 x 300 pixels
	// The parameters are the same as with GetDIB() - see there.
	void* GetThumbnailDIB(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset,
		const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags);

	// Same as GetThumbnailDIB() but with low quality rotation. The rotation angle is specified in radians.
	void* GetThumbnailDIBRotated(CSize size, const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags, double dRotation);

	// Same as GetThumbnailDIB() but with trapezoid correction.
	void* GetThumbnailDIBTrapezoid(CSize size, const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags, const CTrapezoid& trapezoid);

	// Performs unsharp masking to a section in the original image, then applies image processing as requested and
	// return resulting 32 bpp DIB. The returned DIB has the same resolution as the original image.
	// The original image pixels are not touched by this operation.
	// clippingSize: Sub-rectangle in original size image to render (the returned DIB has this size)
	// targetOffset: Offset in original size image
	// See GetDIB() for more information.
	void* GetDIBUnsharpMasked(CSize clippingSize, CPoint targetOffset,
		const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags,
		const CUnsharpMaskParams & unsharpMaskParams);

	// Frees any resources hold to speed up GetDIBUnsharpMasked(). Subsequent calls to GetDIBUnsharpMasked() will work but may be slower.
	void FreeUnsharpMaskResources();

	// Operations on original pixels of the image

	// Apply unsharp masking to the original image pixels. Cannot be undone except by reloading the image from disk.
	// Returns false if not enough memory is available to perform the operation.
	bool ApplyUnsharpMaskToOriginalPixels(const CUnsharpMaskParams & unsharpMaskParams);

	// Mirrors the image horizontally or vertically.
	// Applies to original pixels!
	bool Mirror(bool bHorizontally);

	// Crops the image. 
	// Applies to original pixels!
	bool Crop(CRect cropRect);

	// Rotate the image clockwise by 90, 180 or 270 degrees. All other angles are invalid.
	// Applies to original pixels!
	bool Rotate(int nRotation);

	// Rotate original pixels by given angle (in radians). The original pixels are replaced by this operation.
	// If autocrop is enabled, the maximum rectangular area is cropped, else black borders are added to the image.
	// If keep aspect ratio is true, the aspect ratio of the cropped area is the same as the aspect ratio of the original image.
	// In all cases the size of the image in pixels is changed by the rotation.
	// Returns false if not enough memory is available to perform the operation.
	bool RotateOriginalPixels(double dRotation, bool bAutoCrop, bool bKeepAspectRatio);

	// Transform original pixels into horizontal trapezoid. The original pixels are replaced by this operation.
	// See RotateOriginalPixels() for auto crop parameter and keep aspect ratio parameter.
	// In all cases the size of the image in pixels is changed by this operation.
	// Returns false if not enough memory is available to perform the operation.
	bool TrapezoidOriginalPixels(const CTrapezoid& trapezoid, bool bAutoCrop, bool bKeepAspectRatio);

	// Resizes the original pixels to the given target size.
	// Returns false if not enough memory is available to perform the operation or if specified size is not valid.
	bool ResizeOriginalPixels(EResizeFilter filter, CSize newSize);

	// Gets histogram of the original, unprocessed image
	const CHistogram* GetOriginalHistogram();

	// Gets the histogram of the processed image - histogram is over the whole image, not only the visible section
	const CHistogram* GetProcessedHistogram();

	// Gets the hash value of the pixels, for JPEGs the hash is on the compressed pixels
	__int64 GetPixelHash() const { return m_nPixelHash; }

	// Gets the pixel hash over the de-compressed pixels
	__int64 GetUncompressedPixelHash() const;

	// Original image size. Operations on the original pixels will change this size!
	int OrigWidth() const { return m_nOrigWidth; }
	int OrigHeight() const { return m_nOrigHeight; }
	CSize OrigSize() const { return CSize(m_nOrigWidth, m_nOrigHeight); }

	// Original image size at the time the CJPEGImage was constructed.
	// Operations on the original pixels will not change this size!
	int InitOrigWidth() const { return m_nInitOrigWidth; }
	int InitOrigHeight() const { return m_nInitOrigHeight; }

	// Size of DIB - size of resampled section of the original image. If zero, no DIB is currently available.
	int DIBWidth() const { return m_ClippingSize.cx; }
	int DIBHeight() const { return m_ClippingSize.cy; }

	// Convert DIB coordinates into original image coordinates and vice versa
	void DIBToOrig(float & fX, float & fY);
	void OrigToDIB(float & fX, float & fY);

	// Gets the rotation applied to the pixels
	const CRotationParams& GetRotationParams() {return m_rotationParams; }

	// Gets or sets the JPEG chromo sampling. See turbojpeg.h for the TJSAMP enumeration.
	TJSAMP GetJPEGChromoSampling() { return m_eJPEGChromoSampling; }
	void SetJPEGChromoSampling(TJSAMP eSampling) { m_eJPEGChromoSampling = eSampling; }

	// Gets if lossless JPEG transformations can be applied to this image.
	// Checks if this is a JPEG image and if the dimension of this image is dividable by the JPEG block size used.
	bool CanUseLosslessJPEGTransformations();

	// Trims the given rectangle to MCU block size of this image (allowing lossless JPEG transformations)
	void TrimRectToMCUBlockSize(CRect& rect);

	// Declare the cached DIB as invalid - forcing it to be regenerated on next GetDIB() call
	void SetDIBInvalid() { m_ClippingSize = CSize(0, 0); }

	// Verify that the image is currently rotated by the specified parameters and rotate the original pixels of the image if not.
	bool VerifyRotation(const CRotationParams& rotationParams);

	// Returns if this image has been cropped or not
	bool IsCropped() { return m_bCropped; }

	// Returns if this image's original pixels have been processed destructively (e.g. cropped or rotated by non-90 degrees steps)
	bool IsDestructivlyProcessed() { return m_bIsDestructivlyProcessed; }

	// Returns if this image has been processed in a way not supported to be stored in the parameter DB.
	bool IsProcessedNoParamDB() { return m_bIsProcessedNoParamDB; }

	// raw access to original pixels - do not delete or store the returned pointer
	void* OriginalPixels() { return  m_pOrigPixels; }
	const void* OriginalPixels() const { return m_pOrigPixels; }
	// remove original pixels from class - OriginalPixels() will return NULL afterwards
	void DetachOriginalPixels() { m_pOrigPixels = NULL; }

	// returns the number of channels in the OriginalPixels (3 or 4, corresponding to 24 bpp and 32 bpp)
	int OriginalChannels() const { return m_nOriginalChannels; }

	// raw access to DIB pixels with no LUT applied - do not delete or store the returned pointer
	// note that this DIB can be NULL due to optimization if currently only the processed DIB is maintained
	void* DIBPixels() { return m_pDIBPixels; }
	const void* DIBPixels() const { return m_pDIBPixels; }

	// Verifies that the original DIB pixels (DIBPixels()) are available
	void VerifyDIBPixelsCreated();

	// Gets the DIB last processed. If none, the last used parameters are taken to generate the DIB - if bGenerateDIBIfNeeded is true 
	void* DIBPixelsLastProcessed(bool bGenerateDIBIfNeeded);

	// Gets the DIB pixels of the last processed thumbnail image, NULL if none or invalid
	void* DIBPixelsLastThumbnail() { return (m_pThumbnail == NULL) ? NULL : m_pThumbnail->DIBPixelsLastProcessed(false); }

	// Gets the image processing flags as set as default (may varies from file to file)
	EProcessingFlags GetInitialProcessFlags() const { return m_eProcFlagsInitial; }

	// Gets the image processing flags last used to process an image
	EProcessingFlags GetLastProcessFlags() const { return m_eProcFlags; }
	
	// Gets the image processing parameters as set as default (may varies from file to file)
	const CImageProcessingParams& GetInitialProcessParams() const { return m_imageProcParamsInitial; }

	// Gets the image processing parameters last used to process an image
	const CImageProcessingParams& GetLastProcessParams() const { return m_imageProcParams; } 

	// Gets the rotation as set as default (may varies from file to file)
	int GetInitialRotation() const { return m_nInitialRotation; }

	// Gets the zoom as set as default (can vary from file to file)
	double GetInititialZoom() const { return m_dInitialZoom; }

	// Gets the offsets as set as default (can vary from file to file)
	CPoint GetInitialOffsets() const { return m_initialOffsets; }

	// Sets the initial parameters to the given values.
	void SetInitialParameters(const CImageProcessingParams& imageProcParams, EProcessingFlags procFlags, int nRotation, double dZoom, CPoint offsets);

	// Restores the initial parameters to the parameters dependent on the directory (not the file)
	// Outputs the processing flags set for this directory
	void RestoreInitialParameters(LPCTSTR sFileName, const CImageProcessingParams& imageProcParams, 
		EProcessingFlags & procFlags, int nRotation, double dZoom, CPoint offsets, CSize targetSize, CSize monitorSize);

	// Gets the processing parameters and flags according to param DB or file path.
	void GetFileParams(LPCTSTR sFileName, EProcessingFlags& eFlags, CImageProcessingParams& params) const;

	// To be called after creation of the object to intialize the initial processing parameters.
	// Input are the global defaults for the processing parameters, output (in pParams) are the
	// processing parameters for this file (maybe different from the global ones)
	void SetFileDependentProcessParams(LPCTSTR sFileName, CProcessParams* pParams);

	// Sets the regions of the returned DIB that are dimmed out (set dimRects to NULL for no dimming)
	void SetDimRects(const CDimRect* dimRects, int numberOfRects);
	// Disable/enable dimming with the set dim rects (default is enabled)
	void EnableDimming(bool bEnable);

	// Gets if the image was found in parameter DB
	bool IsInParamDB() const { return m_bInParamDB; }

	// Sets that this image is now in the parameter DB or is removed from the parameter DB
	// (called after the user saves/deletes the image from param DB)
	void SetIsInParamDB(bool bSet) { m_bInParamDB = bSet; }

	// Gets if zoom and offset values are stored for this image in the parameter DB
	bool HasZoomStoredInParamDB() const { return m_bHasZoomStoredInParamDB; }

	// Gets the factor to lighten shadows based on sunset and nightshot detection
	float GetLightenShadowFactor() { return m_fLightenShadowFactor; }

	// Gets the EXIF data block (including the APP1 marker in the first two bytes) of the image. Returns NULL if none.
	void* GetEXIFData() { return m_pEXIFData; }

	// Gets the size of the EXIF data block in bytes, including the APP1 marker (2 bytes)
	int GetEXIFDataLength() { return m_nEXIFSize; }

	// Gets the EXIF reader for this image if the image contains EXIF data, NULL if not.
	CEXIFReader* GetEXIFReader() { return m_pEXIFReader; }

	// Gets the image format this image has originally
	EImageFormat GetImageFormat() const { return m_eImageFormat; }

	// Gets if the image format is one of the formats supported by GDI+
	bool IsGDIPlusFormat() const {
		return m_eImageFormat == IF_JPEG || m_eImageFormat == IF_WindowsBMP || m_eImageFormat == IF_PNG || m_eImageFormat == IF_TIFF || m_eImageFormat == IF_GIF;
	}

	// Gets if this image is part of an animation (animated GIF)
	bool IsAnimation() const { return m_bIsAnimation; }

	// Gets the frame index if this is a multiframe image, 0 otherwise
	int FrameIndex() const { return m_nFrameIndex; }

	// Gets the number of frames for multiframe images, 1 otherwise
	int NumberOfFrames() const { return m_nNumberOfFrames; }

	// Gets the frame time in milliseconds for animations (animated GIF)
	int FrameTimeMs() const { return m_nFrameTimeMs; }

	// Gets if this image was created by pasting from clipboard
	bool IsClipboardImage() const { return m_eImageFormat == IF_CLIPBOARD; }

	// Gets if the rotation is given by EXIF data
	bool IsRotationByEXIF() const { return m_bRotationByEXIF; }

	// Sets and gets the JPEG comment of this image (COM marker).
	void SetJPEGComment(LPCTSTR sComment) { m_sJPEGComment = CString(sComment); }
	LPCTSTR GetJPEGComment() { return m_sJPEGComment; }

	// Gets the metadata for RAW camera images, NULL if none
	CRawMetadata* GetRawMetadata() { return m_pRawMetadata; }

	// Converts the target offset from 'center of image' based format to pixel coordinate format 
	static CPoint ConvertOffset(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset);

	// Debug: Returns if this could be a night shot (heuristic, between 0 and 1)
	float IsNightShot() const;

	// Debug: Returns if this could be a sun set (heuristic, between 0 and 1)
	float IsSunset() const;

	// Debug: Ticks (millseconds) of the last operation
	double LastOpTickCount() const { return m_dLastOpTickCount; }

	// Debug: Loading time of image in ms
	void SetLoadTickCount(double tc) { m_dLoadTickCount = tc; }
	double GetLoadTickCount() { return m_dLoadTickCount; }

	// Debug: Unsharp mask time of image in ms
	double GetUnsharpMaskTickCount() { return m_dUnsharpMaskTickCount; }

private:

	// used internally for re-sampling type
	enum EResizeType {
		NoResize,
		DownSample,
		UpSample
	};

	// Original pixel data - only rotations and crop are done directly on this data because this is non-destructive
	// The data is not modified in all other cases
	void* m_pOrigPixels;
	void* m_pEXIFData;
	CRawMetadata* m_pRawMetadata;
	int m_nEXIFSize;
	CEXIFReader* m_pEXIFReader;
	CString m_sJPEGComment;
	int m_nOrigWidth, m_nOrigHeight; // these may changes by rotation
	int m_nInitOrigWidth, m_nInitOrigHeight; // original width of image when constructed (before any rotation and crop)
	int m_nOriginalChannels;
	__int64 m_nPixelHash;
	EImageFormat m_eImageFormat;
	TJSAMP m_eJPEGChromoSampling;

	// multiframe and GIF animation related data
	bool m_bIsAnimation;
	int m_nFrameIndex;
	int m_nNumberOfFrames;
	int m_nFrameTimeMs;

	// cached thumbnail image, created on first request
	CJPEGImage* m_pThumbnail;

	// thumbnail image for histogram of the processed image
	// this thumbnail is needed because not the whole image is processed, only the visible section,
	// however the histogram must be calculated over the whole processed image
	CJPEGImage* m_pHistogramThumbnail;

	// Thumbnail related stuff
	bool m_bIsThumbnailImage;
	CHistogram* m_pCachedProcessedHistogram;

	// Processed data of size m_ClippingSize, with LUT/LDC applied and without
	// The version without LUT/LDC is used to efficiently reapply a different LUT/LDC
	// Size of the DIBs is m_ClippingSize
	void* m_pDIBPixelsLUTProcessed;
	void* m_pDIBPixels;
	void* m_pLastDIB; // one of the pointers above

	// Cached gray and smoothed gray image for unsharp masking
	int16* m_pGrayImage;
	int16* m_pSmoothGrayImage;

	// Image processing parameters and flags during last call to GetDIB()
	CImageProcessingParams m_imageProcParams;
	EProcessingFlags m_eProcFlags;
	CUnsharpMaskParams m_unsharpMaskParams;
	bool m_bUnsharpMaskParamsValid;

	// Initial image processing parameters and flags, as set with SetFileDependentProcessParams()
	CImageProcessingParams m_imageProcParamsInitial;
	EProcessingFlags m_eProcFlagsInitial;
	int m_nInitialRotation;
	double m_dInitialZoom;
	CPoint m_initialOffsets;
	float m_fLightenShadowFactor;

	bool m_bCropped; // Image has been cropped
	bool m_bIsDestructivlyProcessed; // Original image pixels destructively processed (i.e. cropped or size changed)
	bool m_bIsProcessedNoParamDB;
	CRotationParams m_rotationParams; // current rotation
	bool m_bRotationByEXIF; // is the rotation given by EXIF

	// This is the geometry that was requested during last GetDIB() call
	CSize m_FullTargetSize; 
	CSize m_ClippingSize; // this is the size of the DIB
	CPoint m_TargetOffset;
	double m_dRotationLQ; // low quality rotation angle (radians)
	CTrapezoid m_trapezoid;
	bool m_bTrapezoidValid;

	bool m_bInParamDB; // true if image found in param DB
	bool m_bHasZoomStoredInParamDB; // true if image in param DB and entry contains zoom and offset values
	bool m_bFirstReprocessing; // true if never reprocessed before, some optimizations may be not done initially
	CDimRect* m_pDimRects;
	int m_nNumDimRects;
	bool m_bEnableDimming;
	bool m_bShowGrid;

	double m_dLastOpTickCount;
	double m_dLoadTickCount;
	double m_dUnsharpMaskTickCount;

	// stuff needed to perform LUT and LDC processing
	uint8* m_pLUTAllChannels; // for global contrast and brightness correction
	uint8* m_pLUTRGB; // B,G,R three channel LUT
	int32* m_pSaturationLUTs; // Saturation LUTs
	CLocalDensityCorr* m_pLDC;
	bool m_bLDCOwned;
	float m_fColorCorrectionFactors[6];
	float m_fColorCorrectionFactorsNull[6];

	// Internal GetDIB() implementation combining unsharp mask and (low quality) rotation with GetDIB().
	// pUnsharpMaskParams and pTrapezoid can be null if not used.
	// bUsingOriginalDIB is output parameter and contains if the cached DIB could be used and no processing has been done
	// When dRotation is not 0.0, PFLAG_HighQualityResampling must not be set
	void* GetDIBInternal(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset,
						 const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags,
						 const CUnsharpMaskParams * pUnsharpMaskParams, const CTrapezoid * pTrapezoid, 
						 double dRotation, bool bShowGrid, bool& bParametersChanged);

	// Resample when panning was done, using existing data in DIBs. Old clipping rectangle is given in oldClippingRect
	void ResampleWithPan(void* & pDIBPixels, void* & pDIBPixelsLUTProcessed, CSize fullTargetSize, 
		CSize clippingSize, CPoint targetOffset, CRect oldClippingRect,
		EProcessingFlags eProcFlags, const CImageProcessingParams & imageProcParams, double dRotation, EResizeType eResizeType);

	// Resample to given target size. Returns resampled DIB
	void* Resample(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset, 
		EProcessingFlags eProcFlags, double dSharpen, double dRotation, EResizeType eResizeType);

	// Resize to given target size. Returns resampled DIB. Used when resizing original pixels.
	void* InternalResize(void* pixels, int channels, EResizeFilter filter, CSize targetSize, CSize sourceSize);

	// Apply the given unsharp mask to m_pDIBPixels (can be null to not apply an unsharp mask, then NULL is returned)
	void* ApplyUnsharpMask(const CUnsharpMaskParams * pUnsharpMaskParams, bool bNoChangesLDCandLUT);

	// pCachedTargetDIB is a pointer at the caller side holding the old processed DIB.
	// Returns a pointer to DIB to be used (either pCachedTargetDIB or pSourceDIB)
	// If bOnlyCheck is set to true, the method does nothing but only checks if the existing processed DIB
	// can be used (return != NULL) or not (return == NULL)
	// The out parameter bParametersChanged returns if one of the parameters relevant for image processing has been changed since the last call
	void* ApplyCorrectionLUTandLDC(const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags,
		void * & pCachedTargetDIB, CSize fullTargetSize, CPoint targetOffset, 
		void * pSourceDIB, CSize dibSize, bool bGeometryChanged, bool bOnlyCheck, bool bCanTakeOwnershipOfSourceDIB, bool &bParametersChanged);

	void* ApplyCorrectionLUTandLDC(const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags,
		void * & pCachedTargetDIB, CSize fullTargetSize, CPoint targetOffset, 
		void * pSourceDIB, CSize dibSize, bool bGeometryChanged, bool bOnlyCheck, bool bCanTakeOwnershipOfSourceDIB) {
		bool bNotUsed;
		return ApplyCorrectionLUTandLDC(imageProcParams, eProcFlags, pCachedTargetDIB, fullTargetSize, targetOffset, 
			pSourceDIB, dibSize, bGeometryChanged, bOnlyCheck, bCanTakeOwnershipOfSourceDIB, bNotUsed);
	}

	// makes sure that the input image (m_pOrigPixels) is a 4 channel BGRA image (converts if necessary)
	bool ConvertSrcTo4Channels();

	// Gets the processing flags according to the inclusion/exclusion list in INI file
	EProcessingFlags GetProcFlagsIncludeExcludeFolders(LPCTSTR sFileName, EProcessingFlags procFlags) const;

	// Return size of original image if the image would be rotated the given amount
	CSize SizeAfterRotation(const CRotationParams& rotationParams);

	// Gets the new size of the image after doing a free rotation
	static CSize GetSizeAfterFreeRotation(const CSize& sourceSize, double dRotation, bool bAutoCrop, bool bKeepAspectRatio, CPoint & offset);

	// Get if from source to target size it is down or upsampling
	EResizeType GetResizeType(CSize targetSize, CSize sourceSize);

	// Gets the rotation from EXIF if available
	int GetRotationFromEXIF(int nOrigRotation);

	// Sets the m_bIsDestructivlyProcessed flag to true and resets rotation
	void MarkAsDestructivelyProcessed();

	// Called when the original pixels have changed (rotate, crop, unsharp mask), all cached pixel data gets invalid
	void InvalidateAllCachedPixelData();

	// Create a thumbnail image of this image
	CJPEGImage* CreateThumbnailImage();

	// Create histogram of the processed DIB (in original size) using the given image processing parameters
	const CHistogram* GetHistogramOfProcessedDIB(const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags);

	void DrawGridLines(void * pDIB, const CSize& dibSize);
};
