#pragma once

// Reads and parses the EXIF data of JPEG images
class CEXIFReader {
public:
	// The pApp1Block must point to the APP1 block of the EXIF data, including the APP1 block marker
	CEXIFReader(void* pApp1Block);
	~CEXIFReader(void);

private:
	CString m_sModel;
};
