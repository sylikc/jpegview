#pragma once

#include "ProcessParams.h"

class CHistogram;
class CLocalDensityCorr;
class CEXIFReader;

// Represents a rectangle to dim out in the image
struct CDimRect {
	CDimRect() {}
	CDimRect(float fFactor, const CRect& rect) { Factor = fFactor; Rect = rect; }
	float Factor; // between 0.0 and 1.0
	CRect Rect;
};

// Class holding a decoded image and allowing to get a processed section of the raw
// image as DIB.
class CJPEGImage
{
public:
	// Image formats (can be other than JPEG...)
	enum EImageFormat {
		IF_JPEG,
		IF_WindowsBMP,
		IF_PNG,
		IF_GIF,
		IF_TIFF,
		IF_CLIPBOARD,
		IF_Unknown
	};

public:
	// Ownership of memory in pIJLPixels goes to class, accessing this pointer after the constructor
	// has been called is illegal (use IJLPixels() instead)
	// nChannels can be 1 (greyscale image), 3 (BGR color image) or 4 (BGRA color image, A ignored)
	// pEXIFData can be a pointer to the APP1 block containing the EXIF data. If this pointer is null
	// no EXIF data is available.
	// The nJPEGHash hash value gives a hash over the compressed JPEG pixels that uniquely identifies the
	// JPEG image. It can be zero in which case a pixel based hash value is internally created.
	// The image format is a hint about the original image format this image was created from.
	// The pLDC object is used internally for thumbnail image creation to avoid duplication. From external,
	// its value must be NULL.
	CJPEGImage(int nWidth, int nHeight, void* pIJLPixels, void* pEXIFData, int nChannels, 
		__int64 nJPEGHash, EImageFormat eImageFormat, CLocalDensityCorr* pLDC = NULL);
	~CJPEGImage(void);

	// Converts the target offset from 'center of image' based format to pixel coordinate format 
	static CPoint ConvertOffset(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset);

	// Gets resampled and processed DIB image (up or downsampled). 
	// fullTargetSize: Full target size of image (without any clipping to actual screen size)
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
		const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags);

	// Gets a DIB for a thumbnail of the given size. The thumbnail should not be smaller than 400 x 300 pixels
	void* GetThumbnailDIB(CSize size, const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags);

	// Gets the hash value of the pixels, for JPEGs it on the compressed pixels
	__int64 GetPixelHash() const { return m_nPixelHash; }

	// Gets the pixel hash over the de-compressed pixels
	__int64 GetUncompressedPixelHash() const;

	// Original image size (of the unprocessed raw image, however the raw image may was rotated or cropped)
	int OrigWidth() const { return m_nOrigWidth; }
	int OrigHeight() const { return m_nOrigHeight; }

	// Size of DIB - size of resampled section of the original image. If zero, no DIB is currently processed.
	int DIBWidth() const { return m_ClippingSize.cx; }
	int DIBHeight() const { return m_ClippingSize.cy; }

	// Convert DIB coordinates into original image coordinates and vice versa
	void DIBToOrig(float & fX, float & fY);
	void OrigToDIB(float & fX, float & fY);

	// Declare the generated DIB as invalid - forcing it to be regenerated on next access
	void SetDIBInvalid() { m_ClippingSize = CSize(0, 0); }

	// Verify that the image is currently rotated by the given angle and rotate the image if not
	void VerifyRotation(int nRotation);

	// Rotate the image clockwise by 90, 180 or 270 degrees. All other angles are invalid.
	// Applies to original image!
	void Rotate(int nRotation);

	// Crops the image
	void Crop(CRect cropRect);

	// Returns if this image has been cropped or not
	bool IsCropped() { return m_bCropped; }

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

	// Gets the image processing flags as set as default (may varies from file to file)
	EProcessingFlags GetInitialProcessFlags() const { return m_eProcFlagsInitial; }
	
	// Gets the image processing parameters as set as default (may varies from file to file)
	const CImageProcessingParams& GetInitialProcessParams() const { return m_imageProcParamsInitial; }

	// Gets the rotation as set as default (may varies from file to file)
	int GetInitialRotation() const { return m_nInitialRotation; }

	// Gets the zoom as set as default (can vary from file to file)
	double GetInititialZoom() const { return m_dInitialZoom; }

	// Gets the offsets as set as default (can vary from file to file)
	CPoint GetInitialOffsets() const { return m_initialOffsets; }

	// Sets the initial parameters to the given values.
	void SetInitialParameters(const CImageProcessingParams& imageProcParams, EProcessingFlags procFlags, int nRotation, double dZoom, CPoint offsets);

	// Restores the initial parameters to the parameters dependent from the directory (not file)
	// Outputs the processing flags set for this directory
	void RestoreInitialParameters(LPCTSTR sFileName, const CImageProcessingParams& imageProcParams, 
		EProcessingFlags & procFlags, int nRotation, double dZoom, CPoint offsets, CSize targetSize);

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

	// Gets if this image was created from the clipboard
	bool IsClipboardImage() const { return m_eImageFormat == IF_CLIPBOARD; }

	// Gets if the rotation is given by EXIF
	bool IsRotationByEXIF() const { return m_bRotationByEXIF; }

	// Debug: Ticks (millseconds) of the last operation
	double LastOpTickCount() const { return m_dLastOpTickCount; }

	// Debug: Loading time of image in ms
	void SetLoadTickCount(double tc) { m_dLoadTickCount = tc; }
	double GetLoadTickCount() { return m_dLoadTickCount; }

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
	int m_nOrigWidth, m_nOrigHeight; // these may changes by rotation
	int m_nInitOrigWidth, m_nInitOrigHeight; // original width of image when constructed (before any rotation and crop)
	int m_nIJLChannels;
	__int64 m_nPixelHash;
	EImageFormat m_eImageFormat;

	// cached thumbnail image, created on first request
	CJPEGImage* m_pThumbnail;

	// Processed data of size m_ClippingSize, with LUT/LDC applied and without
	// The version without LUT/LDC is used to efficiently reapply a different LUT/LDC
	void* m_pDIBPixelsLUTProcessed;
	void* m_pDIBPixels;
	void* m_pLastDIB; // one of the pointers above

	// Image processing parameters and flags during last call to GetDIB()
	CImageProcessingParams m_imageProcParams;
	EProcessingFlags m_eProcFlags;

	// Initial image processing parameters and flags, as set with SetFileDependentProcessParams()
	CImageProcessingParams m_imageProcParamsInitial;
	EProcessingFlags m_eProcFlagsInitial;
	int m_nInitialRotation;
	double m_dInitialZoom;
	CPoint m_initialOffsets;
	float m_fLightenShadowFactor;

	bool m_bCropped; // Image has been cropped
	uint32 m_nRotation; // current rotation angle
	bool m_bRotationByEXIF; // is the rotation given by EXIF

	// This is the geometry that was requested during last GetDIB() call
	CSize m_FullTargetSize; 
	CSize m_ClippingSize; // this is the size of the DIB
	CPoint m_TargetOffset;

	bool m_bInParamDB; // true if image found in param DB
	bool m_bHasZoomStoredInParamDB; // true if image in param DB and entry contains zoom and offset values
	bool m_bFirstReprocessing; // true if never reprocessed before, some optimizations may be not done initially
	CDimRect* m_pDimRects;
	int m_nNumDimRects;
	bool m_bEnableDimming;

	double m_dLastOpTickCount;
	double m_dLoadTickCount;

	// stuff needed to perform LUT and LDC processing
	uint8* m_pLUTAllChannels; // for global contrast and brightness correction
	uint8* m_pLUTRGB; // B,G,R three channel LUT
	CLocalDensityCorr* m_pLDC;
	bool m_bLDCOwned;
	float m_fColorCorrectionFactors[6];
	float m_fColorCorrectionFactorsNull[6];

	// Resample when panning was done, using existing data in DIBs. Old clipping rectangle is given in oldClippingRect
	void CJPEGImage::ResampleWithPan(void* & pDIBPixels, void* & pDIBPixelsLUTProcessed, CSize fullTargetSize, 
		CSize clippingSize, CPoint targetOffset, CRect oldClippingRect,
		EProcessingFlags eProcFlags, const CImageProcessingParams & imageProcParams, EResizeType eResizeType);

	// Resample to given target size. Returns resampled DIB
	void* Resample(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset, 
		EProcessingFlags eProcFlags, double dSharpen, EResizeType eResizeType);

	// pCachedTargetDIB is a pointer at the caller side holding the old processed DIB.
	// Returns a pointer to DIB to be used (either pCachedTargetDIB or pSourceDIB)
	// If bOnlyCheck is set to true, the method does nothing but only checks if the existing processed DIB
	// can be used (return != NULL) or not (return == NULL)
	void* ApplyCorrectionLUTandLDC(const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags,
		void * & pCachedTargetDIB, CSize fullTargetSize, CPoint targetOffset, 
		void * pSourceDIB, CSize dibSize, bool bGeometryChanged, bool bOnlyCheck);

	// makes sure that the input image (m_pIJLPixels) is a 4 channel BGRA image (converts if necessary)
	void ConvertSrcTo4Channels();

	// Gets the processing flags according to the inclusion/exclusion list in INI file
	EProcessingFlags GetProcFlagsIncludeExcludeFolders(LPCTSTR sFileName, EProcessingFlags procFlags) const;

	// Return size of original image if the image would be rotated the given amount
	CSize SizeAfterRotation(int nRotation);

	// Get if from source to target size it is down or upsampling
	EResizeType GetResizeType(CSize targetSize, CSize sourceSize);

	// Gets the rotation from EXIF if available
	int GetRotationFromEXIF(int nOrigRotation);
};
