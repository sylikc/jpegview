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

	// Note: Orientation is in cdraw format ('flip' global variable in cdraw_mod.cpp)
	CRawMetadata(char* manufacturer, char* model, time_t acquisitionTime, bool flashFired, double isoSpeed, double exposureTime, double focalLength,
		double aperture, int orientation, int width, int height)
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
};