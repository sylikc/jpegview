#include "StdAfx.h"
#include "EXIFReader.h"
#include "ImageProcessingTypes.h"

double CEXIFReader::UNKNOWN_DOUBLE_VALUE = 283740261.192864;

static uint32 ReadUInt(void * ptr, bool bLittleEndian) {
	uint32 nValue = *((uint32*)ptr);
	if (!bLittleEndian) {
		return _byteswap_ulong(nValue);
	} else {
		return nValue;
	}
}

static uint16 ReadUShort(void * ptr, bool bLittleEndian) {
	uint16 nValue = *((uint16*)ptr);
	if (!bLittleEndian) {
		return _byteswap_ushort(nValue);
	} else {
		return nValue;
	}
}

static uint8* FindTag(uint8* ptr, uint8* ptrLast, uint16 nTag, bool bLittleEndian) {
	while (ptr < ptrLast) {
		if (ReadUShort(ptr, bLittleEndian) == nTag) {
			return ptr;
		}
		ptr += 12;
	}
	return NULL;
}

static void ReadStringTag(CString & strOut, uint8* ptr, uint8* pTIFFHeader, bool bLittleEndian) {
	try {
		if (ptr != NULL && ReadUShort(ptr + 2, bLittleEndian) == 2) {
			int nSize = ReadUInt(ptr + 4, bLittleEndian);
			if (nSize <= 4) {
				strOut = CString(ptr + 8);
			} else {
				strOut = CString(pTIFFHeader + ReadUInt(ptr + 8, bLittleEndian));
			}
		} else {
			strOut.Empty();
		}
	} catch (...) {
		// EXIF corrupt?
		strOut.Empty();
	}
}

static uint32 ReadLongTag(uint8* ptr, bool bLittleEndian) {
	if (ptr != NULL && ReadUShort(ptr + 2, bLittleEndian) == 4) {
		return ReadUInt(ptr + 8, bLittleEndian);
	} else {
		return 0;
	}
}

static uint16 ReadShortTag(uint8* ptr, bool bLittleEndian) {
	if (ptr != NULL && ReadUShort(ptr + 2, bLittleEndian) == 3) {
		return ReadUShort(ptr + 8, bLittleEndian);
	} else {
		return 0;
	}
}

static int ReadRationalTag(Rational & rational, uint8* ptr, uint8* pTIFFHeader, bool bLittleEndian) {
	rational.Numerator = 0;
	rational.Denumerator = 0;
	if (ptr != NULL) {
		uint16 nType = ReadUShort(ptr + 2, bLittleEndian);
		if (nType == 5 || nType == 10) {
			int nOffset = ReadUInt(ptr + 8, bLittleEndian);
			rational.Numerator = ReadUInt(pTIFFHeader + nOffset, bLittleEndian);
			rational.Denumerator = ReadUInt(pTIFFHeader + nOffset + 4, bLittleEndian);
			if (rational.Numerator != 0 && rational.Denumerator != 0) {
				// Calculate the ggT
				uint32 nModulo;
				uint32 nA = (nType == 10) ? abs((int)rational.Numerator) : rational.Numerator;
				uint32 nB = (nType == 10) ? abs((int)rational.Denumerator) : rational.Denumerator;
				do {
				  nModulo = nA % nB;
				  nA = nB;
				  nB = nModulo;
				} while (nB != 0);
				// normalize
				if (nType == 10) {
					rational.Numerator = (int)rational.Numerator/(int)nA;
					rational.Denumerator = (int)rational.Denumerator/(int)nA;
				} else {
					rational.Numerator /= nA;
					rational.Denumerator /= nA;
				}
			}
		}
		return nType;
	}
	return 0;
}

static bool ReadSignedRationalTag(SignedRational & rational, uint8* ptr, uint8* pTIFFHeader, bool bLittleEndian) {
	ReadRationalTag((Rational &)rational, ptr, pTIFFHeader, bLittleEndian);
}

static double ReadDoubleTag(uint8* ptr, uint8* pTIFFHeader, bool bLittleEndian) {
	Rational rational(0, 0);
	int nType = ReadRationalTag(rational, ptr, pTIFFHeader, bLittleEndian);
	if (rational.Denumerator != 0) {
		if (nType == 5) {
			return (double)rational.Numerator/rational.Denumerator;
		} else {
			int nNum = rational.Numerator;
			int nDenum = rational.Denumerator;
			return (double)nNum/nDenum;
		}
	}
	return CEXIFReader::UNKNOWN_DOUBLE_VALUE;
}

static void ParseDateString(SYSTEMTIME & date, const CString& str) {
	int nYear, nMonth, nDay, nHour, nMin, nSec;
	if (_stscanf(str, _T("%d:%d:%d %d:%d:%d"), &nYear, &nMonth, &nDay, &nHour, &nMin, &nSec) == 6) {
		date.wYear = nYear;
		date.wMonth = nMonth;
		date.wDay = nDay;
		date.wHour = nHour;
		date.wMinute = nMin;
		date.wSecond = nSec;
		date.wMilliseconds = 0;
		date.wDayOfWeek = 0;
	}
}

CEXIFReader::CEXIFReader(void* pApp1Block) 
: m_exposureTime(0, 0) {

	memset(&m_acqDate, 0, sizeof(SYSTEMTIME));
	m_bFlashFired = false;
	m_bFlashFlagPresent = false;
	m_dFocalLength = m_dExposureBias = m_dFNumber = UNKNOWN_DOUBLE_VALUE;
	m_nISOSpeed = 0;

	uint8* pApp1 = (uint8*)pApp1Block;
	// APP1 marker
	if (pApp1[0] != 0xFF || pApp1[1] != 0xE1) {
		return;
	}
	int nApp1Size = pApp1[2]*256 + pApp1[3] + 2;

	// Read TIFF header
	uint8* pTIFFHeader = pApp1 + 10;
	bool bLittleEndian;
	if (*(short*)pTIFFHeader == 0x4949) {
		bLittleEndian = true;
	} else if (*(short*)pTIFFHeader == 0x4D4D) {
		bLittleEndian = false;
	} else {
		return;
	}
	uint8* pIFD0 = pTIFFHeader + ReadUInt(pTIFFHeader + 4, bLittleEndian);
	if (pIFD0 - pApp1 >= nApp1Size) {
		return;
	}
	// Read IFD0
	uint16 nNumTags = ReadUShort(pIFD0, bLittleEndian);
	pIFD0 += 2;
	uint8* pLastIFD0 = pIFD0 + nNumTags*12;
	if (pLastIFD0 - pApp1 >= nApp1Size) {
		return;
	}

	uint8* pTagModel = FindTag(pIFD0, pLastIFD0, 0x110, bLittleEndian);
	ReadStringTag(m_sModel, pTagModel, pTIFFHeader, bLittleEndian);

	uint8* pTagEXIFIFD = FindTag(pIFD0, pLastIFD0, 0x8769, bLittleEndian);
	if (pTagEXIFIFD == NULL) {
		return;
	}

	// Read EXIF IFD
	uint32 nOffsetEXIF = ReadLongTag(pTagEXIFIFD, bLittleEndian);
	if (nOffsetEXIF == 0) {
		return;
	}
	uint8* pEXIFIFD = pTIFFHeader + nOffsetEXIF;
	nNumTags = ReadUShort(pEXIFIFD, bLittleEndian);
	pEXIFIFD += 2;
	uint8* pLastEXIF = pEXIFIFD + nNumTags*12;
	if (pLastEXIF - pApp1 >= nApp1Size) {
		return;
	}
	uint8* pTagAcquisitionDate = FindTag(pEXIFIFD, pLastEXIF, 0x9003, bLittleEndian);
	CString sAcqDate;
	ReadStringTag(sAcqDate, pTagAcquisitionDate, pTIFFHeader, bLittleEndian);
	ParseDateString(m_acqDate, sAcqDate);

	uint8* pTagExposureTime = FindTag(pEXIFIFD, pLastEXIF, 0x829A, bLittleEndian);
	ReadRationalTag(m_exposureTime, pTagExposureTime, pTIFFHeader, bLittleEndian);

	uint8* pTagExposureBias = FindTag(pEXIFIFD, pLastEXIF, 0x9204, bLittleEndian);
	m_dExposureBias = ReadDoubleTag(pTagExposureBias, pTIFFHeader, bLittleEndian);

	uint8* pTagFlash = FindTag(pEXIFIFD, pLastEXIF, 0x9209, bLittleEndian);
	uint16 nFlash = ReadShortTag(pTagFlash, bLittleEndian);
	m_bFlashFired = (nFlash & 1) != 0;
	m_bFlashFlagPresent = pTagFlash != NULL;

	uint8* pTagFocalLength = FindTag(pEXIFIFD, pLastEXIF, 0x920A, bLittleEndian);
	m_dFocalLength = ReadDoubleTag(pTagFocalLength, pTIFFHeader, bLittleEndian);

	uint8* pTagFNumber = FindTag(pEXIFIFD, pLastEXIF, 0x829D, bLittleEndian);
	m_dFNumber = ReadDoubleTag(pTagFNumber, pTIFFHeader, bLittleEndian);

	uint8* pTagISOSpeed = FindTag(pEXIFIFD, pLastEXIF, 0x8827, bLittleEndian);
	m_nISOSpeed = ReadShortTag(pTagISOSpeed, bLittleEndian);
}

CEXIFReader::~CEXIFReader(void) {
}
