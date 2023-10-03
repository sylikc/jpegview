#pragma once

#include "EXIFReader.h"

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
	GPSCoordinate* GetGPSLatitude() { return m_pLatitude; }
	GPSCoordinate* GetGPSLongitude() { return m_pLongitude; }
	double GetGPSAltitude() { return m_altitude; }
	bool IsGPSInformationPresent() { return m_pLatitude != NULL && m_pLongitude != NULL; }
	bool IsGPSAltitudePresent() { return m_altitude != CEXIFReader::UNKNOWN_DOUBLE_VALUE; }


	// Note: Orientation is in cdraw format ('flip' global variable in cdraw_mod.cpp)
	CRawMetadata(char* manufacturer, char* model, time_t acquisitionTime, bool flashFired, double isoSpeed, double exposureTime, double focalLength,
		double aperture, int orientation, int width, int height, float* latitude = NULL, char latref = 0, float* longitude = NULL, char longref = 0,
		double altitude = CEXIFReader::UNKNOWN_DOUBLE_VALUE, char altref = 0)
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
		char zeros[3 * sizeof(float)] = { 0 };
		if (latitude != NULL && longitude != NULL && !(memcmp(latitude, zeros, 3 * sizeof(float)) == 0 && memcmp(longitude, zeros, 3 * sizeof(float)) == 0)) {
			wchar_t c[2] = { 0 };
			c[0] = btowc(latref);
			m_pLatitude = new GPSCoordinate(c, latitude[0], latitude[1], latitude[2]);
			c[0] = btowc(longref);
			m_pLongitude = new GPSCoordinate(c, longitude[0], longitude[1], longitude[2]);
		} else {
			m_pLatitude = m_pLongitude = NULL;
		}
		m_altitude = altref == 1 ? -altitude : altitude;

		LONGLONG time = (LONGLONG)acquisitionTime * 10000000 + 116444736000000000;
		FILETIME fileTime;
		fileTime.dwLowDateTime = (DWORD)time;
		fileTime.dwHighDateTime = time >> 32;
		::FileTimeToSystemTime(&fileTime, &m_acquisitionTime);
	}

	~CRawMetadata(void) {
		delete m_pLatitude;
		delete m_pLongitude;
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
	GPSCoordinate* m_pLatitude;
	GPSCoordinate* m_pLongitude;
	double m_altitude;
};

