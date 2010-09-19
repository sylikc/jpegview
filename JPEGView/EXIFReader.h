#pragma once

class SignedRational {
public:
	SignedRational(int num, int denum) { Numerator = num; Denumerator = denum; }
	int Numerator;
	int Denumerator;
};

class Rational {
public:
	Rational(unsigned int num, unsigned int denum) { Numerator = num; Denumerator = denum; }
	unsigned int Numerator;
	unsigned int Denumerator;
};

// Reads and parses the EXIF data of JPEG images
class CEXIFReader {
public:
	// The pApp1Block must point to the APP1 block of the EXIF data, including the APP1 block marker
	CEXIFReader(void* pApp1Block);
	~CEXIFReader(void);

public:
	// Camera model
	LPCTSTR GetCameraModel() { return m_sModel; }
	bool GetCameraModelPresent() { return !m_sModel.IsEmpty(); }
	// Date-time the picture was taken
	const SYSTEMTIME& GetAcquisitionTime() { return m_acqDate; }
	bool GetAcquisitionTimePresent() { return m_acqDate.wYear > 1600; }
	// Exposure time
	const Rational& GetExposureTime() { return m_exposureTime; }
	bool GetExposureTimePresent() { return m_exposureTime.Denumerator != 0; }
	// Exposure bias
	double GetExposureBias() { return m_dExposureBias; }
	bool GetExposureBiasPresent() { return m_dExposureBias != UNKNOWN_DOUBLE_VALUE; }
	// Flag if flash fired
	bool GetFlashFired() { return m_bFlashFired; }
	bool GetFlashFiredPresent() { return m_bFlashFlagPresent; }
	// Focal length (mm)
	double GetFocalLength() { return m_dFocalLength; }
	bool GetFocalLengthPresent() { return m_dFocalLength != UNKNOWN_DOUBLE_VALUE; }
	// F-Number
	double GetFNumber() { return m_dFNumber; }
	bool GetFNumberPresent() { return m_dFNumber != UNKNOWN_DOUBLE_VALUE; }
	// ISO speed value
	int GetISOSpeed() { return m_nISOSpeed; }
	bool GetISOSpeedPresent() { return m_nISOSpeed > 0; }
	// Image orientation as detected by sensor
	int GetImageOrientation() { return m_nImageOrientation; }
	bool ImageOrientationPresent() { return m_nImageOrientation > 0; }
	// Thumbnail image information
	bool HasJPEGCompressedThumbnail() { return m_bHasJPEGCompressedThumbnail; }
	int GetJPEGThumbStreamLen() { return m_nJPEGThumbStreamLen; }
	int GetThumbnailWidth() { return m_nThumbWidth; }
	int GetThumbnailHeight() { return m_nThumbHeight; }

	// Sets the image orientation to given value (if tag was present in input stream)
	void WriteImageOrientation(int nOrientation);
	
	// Updates an existing JPEG compressed thumbnail image by given JPEG stream (SOI stripped)
	void UpdateJPEGThumbnail(unsigned char* pJPEGStream, int nStreamLen, int nEXIFBlockLenCorrection, CSize sizeThumb);

	// Delete the thumbnail image
	void DeleteThumbnail();
public:
	// unknown double value
	static double UNKNOWN_DOUBLE_VALUE;

private:
	CString m_sModel;
	SYSTEMTIME m_acqDate;
	Rational m_exposureTime;
	double m_dExposureBias;
	bool m_bFlashFired;
	bool m_bFlashFlagPresent;
	double m_dFocalLength;
	double m_dFNumber;
	int m_nISOSpeed;
	int m_nImageOrientation;
	bool m_bHasJPEGCompressedThumbnail;
	int m_nThumbWidth;
	int m_nThumbHeight;
	int m_nJPEGThumbStreamLen;

	bool m_bLittleEndian;
	uint8* m_pApp1;
	uint8* m_pTagOrientation;
	uint8* m_pLastIFD0;
	uint8* m_pIFD1;
	uint8* m_pLastIFD1;
};
