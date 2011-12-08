#pragma once
#include "IJLWrapper.h"
#include "Helpers.h"
#include "JPEGImage.h"

#define NO_JPEG
#define RGBWIDTH(Width) ((Width * 3 + 3) & -4)

class CReaderRAW
{
public:
	static CJPEGImage* ReadRawImage(LPCTSTR strFileName, bool& bOutOfMemory);
private:
	CReaderRAW(void);
};

