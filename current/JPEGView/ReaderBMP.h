#pragma once

class CJPEGImage;

// Simple reader for windows bitmap files (.bmp)
class CReaderBMP
{
public:
	// Returns NULL in case of errors
	static CJPEGImage* ReadBmpImage(LPCTSTR strFileName, bool& bOutOfMemory);
private:
	CReaderBMP(void);
};
