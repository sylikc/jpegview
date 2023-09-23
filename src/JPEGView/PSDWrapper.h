#pragma once

#include "JPEGImage.h"

class PsdReader
{
public:
	// Returns data in the form 4-byte BGRA
	static CJPEGImage* ReadImage(LPCTSTR strFileName, bool& bOutOfMemory);

	static CJPEGImage* ReadThumb(LPCTSTR strFileName, bool& bOutOfMemory);

	//static inline ReadBytes()
};
