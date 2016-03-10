#pragma once

#include "ProcessParams.h"

class CJPEGImage;

// Class that enables to save a processed image to a one of the supported file formats
class CSaveImage
{
public:
	// Save processed image to file. Returns if saving was successful or not.
	// The file format is derived from the file ending of the specified file name.
	// If bFullSize is true, the image in its original size is processed and saved.
	// If bFullSize is false, the image section as shown in the window is saved.
	// Processing parameters and flags are ignored when bFullSize is false, the image is not reprocessed in this case.
	static bool SaveImage(LPCTSTR sFileName, CJPEGImage * pImage, const CImageProcessingParams& procParams,
		EProcessingFlags eFlags, bool bFullSize, bool bUseLosslessWEBP, bool bCreateParameterDBEntry = true);

	// Saves processed image in current window size as displayed on screen. Creates no parameter DB entry for the saved image.
	// The file format is derived from the file ending of the specified file name.
	static bool SaveImage(LPCTSTR sFileName, CJPEGImage * pImage, bool bUseLosslessWEBP);

private:
	CSaveImage(void);
};
