#pragma once

#include "JPEGImage.h"

class RawReader
{
public:
	static CJPEGImage* ReadImage(LPCTSTR strFileName, bool& bOutOfMemory, bool bGetThumb);
};
