#pragma once

#include "ProcessParams.h"

class CJPEGImage;

// Class that enables to save a processed image to a JPEG file
class CSaveImage
{
public:
	// Save processed image to JPEG file. Returns if saving was successful or not.
	static bool SaveImage(LPCTSTR sFileName, CJPEGImage * pImage, const CImageProcessingParams& procParams,
		EProcessingFlags eFlags);

private:
	CSaveImage(void);
};
