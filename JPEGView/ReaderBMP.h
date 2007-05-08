#pragma once

class CJPEGImage;

// Simple reader for windows bitmap files (.bmp)
class CReaderBMP
{
public:
	static CJPEGImage* ReadBmpImage(LPCTSTR strFileName);
private:
	CReaderBMP(void);
};
