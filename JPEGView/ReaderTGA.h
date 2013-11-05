#pragma once

class CJPEGImage;

// Simple reader for .tga files
class CReaderTGA
{
public:
	static CJPEGImage* ReadTgaImage(LPCTSTR strFileName, COLORREF backgroundColor, bool& bOutOfMemory);
private:
	CReaderTGA(void);
};
