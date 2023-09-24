#pragma once

#include "JPEGImage.h"

class PsdReader
{
public:
	// Returns data in the form 4-byte BGRA
	static CJPEGImage* ReadImage(LPCTSTR strFileName, bool& bOutOfMemory);

	static CJPEGImage* ReadThumb(LPCTSTR strFileName, bool& bOutOfMemory);

private:
	enum ColorMode {
		MODE_Bitmap = 0,
		MODE_Grayscale = 1,
		MODE_Indexed = 2,
		MODE_RGB = 3,
		MODE_CMYK = 4,
		MODE_Multichannel = 7,
		MODE_Duotone = 8,
		MODE_Lab = 9,
	};
};
