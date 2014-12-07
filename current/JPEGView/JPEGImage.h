#pragma once

#include "ProcessParams.h"

class CHistogram;
class CLocalDensityCorr;
class CEXIFReader;
enum TJSAMP;

// Represents a rectangle to dim out in the image
struct CDimRect {
	CDimRect() {}
	CDimRect(float fFactor, const CRect& rect) { Factor = fFactor; Rect = rect; }
	float Factor; // between 0.0 and 1.0
	CRect Rect;
};

// Class holding a decoded image and allowing to get a processed section of the raw
// image as DIB.
class CJPEGImage {
public:
	// Ownership of memory in pIJLPixels goes to class, accessing this pointer after the constructor
	// has been called is illegal (use IJLPixels() instead)
	// nChannels can be 1 (greyscale image), 3 (BGR color image) or 4 (BGRA color image, A ignored)
	// pEXIFData can be a pointer to the APP1 block containing the EXIF data. If this pointer is null
	// no EXIF data is available.
	// The nJPEGHash hash value gives a hash over the compressed JPEG pixels that uniquely identifies the
	// JPEG image. It can be zero in which case a pixel based hash value is internally created.
	// The image format is a hint about the original image format this image was created from.
    // The bIsAnimation, nFrameIndex, nNumberOfFrames and nFrameTimeMs are used for multiframe images, e.g. animated GIFs.
    // Frame index is zero based and the frame time is given in milliseconds.
	// The pLDC object is used internally for thumbnail image creation to avoid duplication. From external,
	// its value must be NULL.
	CJPEGImage(int nWidth, int nHeight, void* pIJLPixels, void* pEXIFData, int nChannels, 
		__int64 nJPEGHash, EImageFormat eImageFormat, bool bIsAnimation, int nFrameIndex, int nNumberOfFrames, int nFrameTimeMs,
        CLocalDensityCorr* pLDC = NULL, bool bIsThumbnailImage = false);
	~CJPEGImage(void);

	// Converts the target offset from 'center of image' based format to pixel coordinate format 
	static CPoint ConvertOffset(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset);

	// Gets resampled and processed DIB image (up or downsampled). 
	// fullTargetSize: Full target size of image to render (without any clipping to actual screen size)
	// clippingSize: Sub-rectangle in fullTargetSize to render (the returned DIB has this size)
	// targetOffset: Offset of the clipping rectangle in full target size
	// imageProcParams: Processing parameters, such as contrast, brightness and sharpen
	// eProcFlags: Processing flags, e.g. to apply LDC
	// The returned DIB is a 32 bpp DIB (BGRA). Note that the returned pointer must not
	// be deleted by the caller and is only valid until another call to GetDIB() is done, the CJPEGImage
	// object is disposed or the Rotate() method is called!
	// Note: The method tries to satisfy consecuting requests as efficient as possible. Two calls
	// to GetDIB() with the same set of parameters will return the same image in the second call without
	// doing any processing then.
	void* GetDIB(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset,
		const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags) {
		bool bNotUsed;
		return GetDIBInternal(fullTargetSize, clippingSize, targetOffset, imageProcParams, eProcFlags, NULL, NULL, 0.0, false, bNotUsed);
	}

	// Gets resampled and processed DIB image (up or downsampled), also including a low quality rotation.
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

	// Gets a DIB for a thumbnail of the given size. The thumbnail should not be smaller than 400 x 300 pixels
	void* GetThumbnailDIB(CSize size, const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags);

	// Same as above but with low quality rotation
	void* GetThumbnailDIBRotated(CSize size, const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags, double dRotation);

	// Same as GetThumbnailDIB but with trapezoid correction
	void* GetThumbnailDIBTrapezoid(CSize size, const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags, const CTrapezoid& trapezoid);

	// Performs unsharp masking to a section in the original image, then apply image processing as requested and
	// return resulting DIB. The returned DIB is always in original image resolution. The original
	// image pixels are not touched by this operation.
	// clippingSize: Sub-rectangle in original size image to render (the returned DIB has this size)
	// targetOffset: Offset in original size image
	// See GetDIB() for more information.
	void* GetDIBUnsharpMasked(CSize clippingSize, CPoint targetOffset,
		const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags, const CUnsharpMaskParams & unsharpMaskParams);

	// Gets histogram of the original, unprocessed image
	const CHistogram* GetOriginalHistogram();

	// Gets the histogram of the processed image - histogram is over the whole image, not only the visible section
	const CHistogram* GetProcessedHistogram();

	// Frees any resources hold to speed up GetDIBUnsharpMasked(). Subsequent calls to GetDIBUnsharpMasked() will work but may be slower.
	void FreeUnsharpMaskResources();

	// Apply unsharp masking to the original image pixels. Cannot be undone except by reloading the image from disc.
	// Returns false if not enough memory to perform the operation
	bool ApplyUnsharpMaskToOriginalPixels(const CUnsharpMaskParams & unsharpMaskParams);

	// Rotate original pixels by given angle (in radians). The original pixels are replaced by this operation.
	// If autocrop is enabled, the maximum defined rectangular area is cropped, else black borders are added to the image.
	// If keep aspect ratio is true, the aspect ratio of the cropped area is the same as the aspect ratio of the original image.
	// In all cases the size of the image in pixels is changed by rotation
	// Returns false if not enough memory to perform the operation
	bool RotateOriginalPixels(double dRotation, bool bAutoCrop, bool bKeepAspectRatio);

	// Transform into horizontal trapezoid. The original pixels are replaced by this operation.
	// See RotateOriginalPixels() for auto crop parameter and keep aspect ratio parameter.
	// In all cases the size of the image in pixels is changed by this operation
	// Returns false if not enough memory to perform the operation
	bool TrapezoidOriginalPixels(const CTrapezoid& trapezoid, bool bAutoCrop, bool bKeepAspectRatio);

	// Resizes the original pixels to the given target size
	bool ResizeOriginalPixels(EResizeFilter filter, CSize newSize);

	// Gets the hash value of the pixels, for JPEGs it on the compressed pixels
	__int64 GetPixelHash() const { return m_nPixelHash; }

	// Gets the pixel hash over the de-compressed pixels
	__int64 GetUncompressedPixelHash() const;

	// Original image size (of the unprocessed raw image, however the raw image may was rotated or cropped)
	int OrigWidth() const { return m_nOrigWidth; }
	int OrigHeight() const { return m_nOrigHeight; }
	CSize OrigSize() const { return CSize(m_nOrigWidth, m_nOrigHeight); }

	// Original image size at the time the object was constructed. The image may has been rotated or cropped in the meantime.
	int InitOrigWidth() const { return m_nInitOrigWidth; }
	int InitOrigHeight() const { return m_nInitOrigHeight; }

	// Size of DIB - size of resampled section of the original image. If zero, no DIB is currently processed.
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
    // Checks if this is a JPEG image and if the dimension of this image is dividable by the JPEG block size used
    bool CanUseLosslessJPEGTransformations();

    // Trims the given rectangle to MCU block size (allowing lossless JPEG transformations)
    void TrimRectToMCUBlockSize(CRect& rect);

	// Declare the generated DIB as invalid - forcing it to be regenerated on next access
	void SetDIBInvalid() { m_ClippingSize = CSize(0, 0); }

	// Verify that the image is currently rotated by the specified paramters and rotate the image if not.
	bool VerifyRotation(const CRotationParams& rotationParams);

	// Rotate the image clockwise by 90, 180 or 270 degrees. All other angles are invalid.
	// Applies to original image!
	bool Rotate(int nRotation);

	// Mirrors the image horizontally or vertically.
	// Applies to original image!
	bool Mirror(bool bHorizontally);

	// Crops the image. 
	// Applies to original image!
	bool Crop(CRect cropRect);

	// Returns if this image has been cropped or not
	bool IsCropped() { return m_bCropped; }

	// Returns if this image's original pixels have been processed destructively (e.g. cropped or rotated by non-90 degrees steps)
	bool IsDestructivlyProcessed() { return m_bIsDestructivlyProcessed; }

	// Returns if this image has been processed in a way not supported to be stored in the parameter DB.
	bool IsProcessedNoParamDB() { return m_bIsProcessedNoParamDB; }

	// raw access to input pixels - do not delete or store the pointer returned
	void* IJLPixels() { return  m_pIJLPixels; }
	const void* IJLPixels() const { return m_pIJLPixels; }
	// remove IJL pixels form class - will be NULL afterwards
	void DetachIJLPixels() { m_pIJLPixels = NULL; }

	// returns the number of channels in the IJLPixels (3 or 4, corresponding to 24 bpp and 32 bpp)
	int IJLChannels() const { return m_nIJLChannels; }

	// raw access to DIB pixels with no LUT applied - do not delete or store the pointer returned
	// note that this DIB can be NULL due to optimization if currently only the processed DIB is maintained
	void* DIBPixels() { return m_pDIBPixels; }
	const void* DIBPixels() const { return m_pDIBPixels; }

	// Verifies that the original DIB pixels (in m_pDIBPixels) are available
	void VerifyDIBPixelsCreated();

	// Gets the DIB last processed. If none, the last used parameters are taken to generate the DIB
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

	// Gets the parameters according to param DB or file path dependend.
	void GetFileParams(LPCTSTR sFileName, EProcessingFlags& eFlags, CImageProcessingParams& params) const;

	// To be called after creation of the object to intialize the initial processing parameters.
	// Input are the global defaults for the processing parameters, output (in pParams) are the
	// processing parameters for this file (maybe different from the global ones)
	void SetFileDependentProcessParams(LPCTSTR sFileName, CProcessParams* pParams);

	// Sets the regions of the returned DIB that are dimmed out (NULL for no dimming)
	void SetDimRects(const CDimRect* dimRects, int nSize);
	// Allows to disable/enable dimming (default is enabled)
	void EnableDimming(bool bEnable);

	// Gets if the image was found in parameter DB
	bool IsInParamDB() const { return m_bInParamDB; }

	// Sets if the image is in the paramter DB (called after the user saves/deletes the image from param DB)
	void SetIsInParamDB(bool bSet) { m_bInParamDB = bSet; }

	// Gets if zoom and offset values are stored for this image in the parameter DB
	bool HasZoomStoredInParamDB() const { return m_bHasZoomStoredInParamDB; }

	// Gets the factor to lighten shadows based on sunset and nightshot detection
	float GetLightenShadowFactor() { return m_fLightenShadowFactor; }

	// Gets the EXIF data block (including the APP1 marker as first two bytes) of the image if any
	void* GetEXIFData() { return m_pEXIFData; }

	// Gets the size of the EXIF data block in bytes, including the APP1 marker (2 bytes)
	int GetEXIFDataLength() { return m_nEXIFSize; }

	CEXIFReader* GetEXIFReader() { return m_pEXIFReader; }

	// Gets the image format this image was originally created from
	EImageFormat GetImageFormat() const { return m_eImageFormat; }

	// Gets if the image format is one of the formats supported by GDI+
	bool IsGDIPlusFormat() const {
		return m_eImageFormat == IF_JPEG || m_eImageFormat == IF_WindowsBMP || m_eImageFormat == IF_PNG || m_eImageFormat == IF_TIFF || m_eImageFormat == IF_GIF;
	}

    // Gets if this image is part of an animation (GIF)
    bool IsAnimation() const { return m_bIsAnimation; }

    // Gets the frame index if this is a multiframe image, 0 otherwise
    int FrameIndex() const { return m_nFrameIndex; }

    // Gets the number of frames for multiframe images, 1 otherwise
    int NumberOfFrames() const { return m_nNumberOfFrames; }

    // Gets the frame time in milliseconds for animations
    int FrameTimeMs() const { return m_nFrameTimeMs; }

	// Gets if this image was created from the clipboard
	bool IsClipboardImage() const { return m_eImageFormat == IF_CLIPBOARD; }

	// Gets if the rotation is given by EXIF
	bool IsRotationByEXIF() const { return m_bRotationByEXIF; }

	// Debug: Ticks (millseconds) of the last operation
	double LastOpTickCount() const { return m_dLastOpTickCount; }

	// Debug: Loading time of image in ms
	void SetLoadTickCount(double tc) { m_dLoadTickCount = tc; }
	double GetLoadTickCount() { return m_dLoadTickCount; }

	// Debug: Unsharp mask time of image in ms
	double GetUnsharpMaskTickCount() { return m_dUnsharpMaskTickCount; }

	// Sets and gets the JPEG comment of this image (COM marker)
	void SetJPEGComment(LPCTSTR sComment) { m_sJPEGComment = CString(sComment); }
	LPCTSTR GetJPEGComment() { return m_sJPEGComment; }

	// Debug: Returns if this could be a night shot (heuristic, between 0 and 1)
	float IsNightShot() const;

	// Debug: Returns if this could be a sun set (heuristic, between 0 and 1)
	float IsSunset() const;

private:

	// used internally for re-sampling type
	enum EResizeType {
		NoResize,
		DownSample,
		UpSample
	};

	// Original pixel data - only rotations and crop are done directly on this data because this is non-destructive
	// The data is not modified in all other cases
	void* m_pIJLPixels;
	void* m_pEXIFData;
	int m_nEXIFSize;
	CEXIFReader* m_pEXIFReader;
	CString m_sJPEGComment;
	int m_nOrigWidth, m_nOrigHeight; // these may changes by rotation
	int m_nInitOrigWidth, m_nInitOrigHeight; // original width of image when constructed (before any rotation and crop)
	int m_nIJLChannels;
	__int64 m_nPixelHash;
	EImageFormat m_eImageFormat;
    TJSAMP m_eJPEGChromoSampling;

    // multiframe data
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
	double m_dRotationLQ; // low quality rotation angle
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

	// makes sure that the input image (m_pIJLPixels) is a 4 channel BGRA image (converts if necessary)
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
