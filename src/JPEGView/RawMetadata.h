#pragma once



// Metadata read from camera RAW images
class CRawMetadata
{
public:
	LPCTSTR GetManufacturer() { return m_manufacturer; }
	LPCTSTR GetModel() { return m_model; }
	const SYSTEMTIME& GetAcquisitionTime() { return m_acquisitionTime; }
	bool IsFlashFired() { return m_flashFired; }
	double GetIsoSpeed() { return m_isoSpeed; }
	double GetExposureTime() { return m_exposureTime; }
	double GetFocalLength() { return m_focalLength; }
	double GetAperture() { return m_aperture; }
	// Note: Orientation is in cdraw format ('flip' global variable in cdraw_mod.cpp)
	int GetOrientation() { return m_orientatation; }
	int GetWidth() { return m_width; }
	int GetHeight() { return m_height; }
	double GetGPSLatitude() { return m_latitude; }
	double GetGPSLongitude() { return m_longitude; }
	double GetGPSAltitude() { return m_altitude; }
	bool IsGPSInformationPresent() { return m_latitude != UNKNOWN_DOUBLE_VALUE && m_longitude != UNKNOWN_DOUBLE_VALUE; }
	bool IsGPSAltitudePresent() { return m_altitude != UNKNOWN_DOUBLE_VALUE; }

	constexpr static double UNKNOWN_DOUBLE_VALUE = 283740261.192864;

	// Note: Orientation is in cdraw format ('flip' global variable in cdraw_mod.cpp)
	CRawMetadata(char* manufacturer, char* model, time_t acquisitionTime, bool flashFired, double isoSpeed, double exposureTime, double focalLength,
		double aperture, int orientation, int width, int height, double latitude = UNKNOWN_DOUBLE_VALUE, double longitude = UNKNOWN_DOUBLE_VALUE, double altitude = UNKNOWN_DOUBLE_VALUE)
	{
		m_manufacturer = CString(manufacturer);
		m_model = CString(model);
		m_flashFired = flashFired;
		m_isoSpeed = isoSpeed;
		m_exposureTime = exposureTime;
		m_focalLength = focalLength;
		m_aperture = aperture;
		m_orientatation = orientation;
		m_width = width;
		m_height = height;
		m_latitude = latitude;
		m_longitude = longitude;
		m_altitude = altitude;

		LONGLONG time = (LONGLONG)acquisitionTime * 10000000 + 116444736000000000;
		FILETIME fileTime;
		fileTime.dwLowDateTime = (DWORD)time;
		fileTime.dwHighDateTime = time >> 32;
		::FileTimeToSystemTime(&fileTime, &m_acquisitionTime);
	}

private:
	CString m_manufacturer;
	CString m_model;
	bool m_flashFired;
	double m_isoSpeed;
	double m_exposureTime;
	double m_focalLength;
	double m_aperture;
	int m_orientatation;
	int m_width, m_height;
	SYSTEMTIME m_acquisitionTime;
	double m_latitude, m_longitude, m_altitude;
};

