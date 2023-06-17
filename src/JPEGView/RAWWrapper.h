#pragma once

#include "JPEGImage.h"

class RawReader
{
public:
	// Returns data in 4 byte BGRA
	static CJPEGImage* ReadImage(LPCTSTR strFileName, bool& bOutOfMemory);


};
