#pragma once

#include "JPEGImage.h"

class PsdReader
{
public:
	// Returns image from PSD file
	static CJPEGImage* ReadImage(LPCTSTR strFileName, bool& bOutOfMemory);

	// Returns embedded JPEG thumbnail from PSD file
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

	enum CompressionMode {
		COMPRESSION_None = 0,
		COMPRESSION_RLE = 1,
		COMPRESSION_ZipWithoutPrediction = 2,
		COMPRESSION_ZipWithPrediction = 3,
	};
};
