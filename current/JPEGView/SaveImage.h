#pragma once

#include "ProcessParams.h"

class CJPEGImage;

// Class that enables to save a processed image to a JPEG file
class CSaveImage
{
public:
	// Save processed image to file. Returns if saving was successful or not.
	static bool SaveImage(LPCTSTR sFileName, CJPEGImage * pImage, const CImageProcessingParams& procParams,
		EProcessingFlags eFlags, bool bFullSize, bool bUseLosslessWEBP, bool bCreateParameterDBEntry = true);

	// Saves processed image in current size. Creates no parameter DB entry for the saved image.
	static bool SaveImage(LPCTSTR sFileName, CJPEGImage * pImage, bool bUseLosslessWEBP);

private:
	CSaveImage(void);
};
