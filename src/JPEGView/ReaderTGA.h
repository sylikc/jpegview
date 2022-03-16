#pragma once

class CJPEGImage;

// Simple reader for .tga files
class CReaderTGA
{
public:
	// Returns NULL in case of errors. backgroundColor is used for blending transparent parts of the image.
	static CJPEGImage* ReadTgaImage(LPCTSTR strFileName, COLORREF backgroundColor, bool& bOutOfMemory);
private:
	CReaderTGA(void);
};
