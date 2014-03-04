#include "StdAfx.h"
#include "EXIFReader.h"
#include "ImageProcessingTypes.h"
#include "Helpers.h"

double CEXIFReader::UNKNOWN_DOUBLE_VALUE = 283740261.192864;

static uint32 ReadUInt(void * ptr, bool bLittleEndian) {
	uint32 nValue = *((uint32*)ptr);
	if (!bLittleEndian) {
		return _byteswap_ulong(nValue);
	} else {
		return nValue;
	}
}

static void WriteUInt(void * ptr, uint32 nValue, bool bLittleEndian) {
	uint32 nVal = nValue;
	if (!bLittleEndian) {
		nVal = _byteswap_ulong(nVal);
	}
	*((uint32*)ptr) = nVal;
}

static uint16 ReadUShort(void * ptr, bool bLittleEndian) {
	uint16 nValue = *((uint16*)ptr);
	if (!bLittleEndian) {
		return _byteswap_ushort(nValue);
	} else {
		return nValue;
	}
}

static void WriteUShort(void * ptr, uint16 nValue, bool bLittleEndian) {
	uint16 nVal = nValue;
	if (!bLittleEndian) {
		nVal = _byteswap_ushort(nVal);
	}
	*((uint16*)ptr) = nVal;
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

static void ReadStringTag(CString & strOut, uint8* ptr, uint8* pTIFFHeader, bool bLittleEndian, bool bTryReadAsUTF8 = false) {
	try {
		if (ptr != NULL && ReadUShort(ptr + 2, bLittleEndian) == 2) {
			int nSize = ReadUInt(ptr + 4, bLittleEndian);
			LPCSTR pString = (nSize <= 4) ? (LPCSTR)(ptr + 8) : (LPCSTR)(pTIFFHeader + ReadUInt(ptr + 8, bLittleEndian));
			int nMaxChars = min(nSize, (int)strlen(pString));
            if (bTryReadAsUTF8) {
                CString strUTF8 = Helpers::TryConvertFromUTF8((uint8*)pString, nMaxChars);
                if (!strUTF8.IsEmpty()) {
                    strOut = strUTF8;
                    return;
                }
            }
			strOut = CString(pString, nMaxChars);
		} else {
			strOut.Empty();
		}
	} catch (...) {
		// EXIF corrupt?
		strOut.Empty();
	}
}

static void ReadUserCommentTag(CString & strOut, uint8* ptr, uint8* pTIFFHeader, bool bLittleEndian) {
	try {
		if (ptr != NULL && ReadUShort(ptr + 2, bLittleEndian) == 7) {
			int nSize = ReadUInt(ptr + 4, bLittleEndian);
			if (nSize > 10 && nSize <= 4096) {
				LPCSTR sCodeDesc = (LPCSTR)(pTIFFHeader + ReadUInt(ptr + 8, bLittleEndian));
				if (strcmp(sCodeDesc, "ASCII") == 0) {
					strOut = CString((LPCSTR)(sCodeDesc + 8), nSize - 8);
				} else if (strcmp(sCodeDesc, "UNICODE") == 0) {
					bool bLE = bLittleEndian;
					nSize -= 8;
					sCodeDesc += 8;
					if (sCodeDesc[0] == 0xFF && sCodeDesc[1] == 0xFE) {
						bLE = true;
						nSize -= 2; sCodeDesc += 2;
					} else if (sCodeDesc[0] == 0xFE && sCodeDesc[1] == 0xFF) {
						bLE = false;
						nSize -= 2; sCodeDesc += 2;
					}
					if (bLE) {
						strOut = CString((LPCWSTR)sCodeDesc, nSize / 2);
					} else {
						// swap 16 bit characters to little endian
						char* pString = new char[nSize];
						char* pSwap = pString;
						memcpy(pString, sCodeDesc, nSize);
						for (int i = 0; i < nSize / 2; i++) {
							char t = *pSwap;
							*pSwap = pSwap[1];
							pSwap[1] = t;
							pSwap += 2;
						}
						strOut = CString((LPCWSTR)pString, nSize / 2);
						delete[] pString;
					}
				}
			} // else comment is corrupt or too big
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

static uint32 ReadShortOrLongTag(uint8* ptr, bool bLittleEndian) {
	if (ptr != NULL) {
		uint16 nType = ReadUShort(ptr + 2, bLittleEndian);
		if (nType == 3) {
			return ReadUShort(ptr + 8, bLittleEndian);
		} else if (nType == 4) {
			return ReadUInt(ptr + 8, bLittleEndian);
		}
	}
	return 0;
}

static void WriteShortTag(uint8* ptr, uint16 nValue, bool bLittleEndian) {
	if (ptr != NULL && ReadUShort(ptr + 2, bLittleEndian) == 3) {
		WriteUShort(ptr + 8, nValue, bLittleEndian);
	}
}

static void WriteLongTag(uint8* ptr, uint32 nValue, bool bLittleEndian) {
	if (ptr != NULL && ReadUShort(ptr + 2, bLittleEndian) == 4) {
		WriteUInt(ptr + 8, nValue, bLittleEndian);
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

bool CEXIFReader::ParseDateString(SYSTEMTIME & date, const CString& str) {
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
		return true;
	}
	return false;
}

CEXIFReader::CEXIFReader(void* pApp1Block) 
: m_exposureTime(0, 0) {

	memset(&m_acqDate, 0, sizeof(SYSTEMTIME));
	m_bFlashFired = false;
	m_bFlashFlagPresent = false;
	m_dFocalLength = m_dExposureBias = m_dFNumber = UNKNOWN_DOUBLE_VALUE;
	m_nISOSpeed = 0;
	m_nImageOrientation = 0;
	m_pTagOrientation = NULL;
	m_bLittleEndian = true;
	m_nThumbWidth = -1;
	m_nThumbHeight = -1;
	m_pLastIFD0 = NULL;
	m_pIFD1 = NULL;
	m_pLastIFD1 = NULL;
	m_bHasJPEGCompressedThumbnail = false;
	m_nJPEGThumbStreamLen = 0;
	
	m_pApp1 = (uint8*)pApp1Block;
	// APP1 marker
	if (m_pApp1[0] != 0xFF || m_pApp1[1] != 0xE1) {
		return;
	}
	int nApp1Size = m_pApp1[2]*256 + m_pApp1[3] + 2;

	// Read TIFF header
	uint8* pTIFFHeader = m_pApp1 + 10;
	bool bLittleEndian;
	if (*(short*)pTIFFHeader == 0x4949) {
		bLittleEndian = true;
	} else if (*(short*)pTIFFHeader == 0x4D4D) {
		bLittleEndian = false;
	} else {
		return;
	}
	m_bLittleEndian = bLittleEndian;

	uint8* pIFD0 = pTIFFHeader + ReadUInt(pTIFFHeader + 4, bLittleEndian);
	if (pIFD0 - m_pApp1 >= nApp1Size) {
		return;
	}
	// Read IFD0
	uint16 nNumTags = ReadUShort(pIFD0, bLittleEndian);
	pIFD0 += 2;
	uint8* pLastIFD0 = pIFD0 + nNumTags*12;
	if (pLastIFD0 - m_pApp1 + 4 >= nApp1Size) {
		return;
	}
	uint32 nOffsetIFD1 = ReadUInt(pLastIFD0, bLittleEndian);
	m_pLastIFD0 = pLastIFD0;

	// image orientation
	uint8* pTagOrientation = FindTag(pIFD0, pLastIFD0, 0x112, bLittleEndian);
	if (pTagOrientation != NULL) {
		m_nImageOrientation = ReadShortTag(pTagOrientation, bLittleEndian);
	}
	m_pTagOrientation = pTagOrientation;

	uint8* pTagModel = FindTag(pIFD0, pLastIFD0, 0x110, bLittleEndian);
	ReadStringTag(m_sModel, pTagModel, pTIFFHeader, bLittleEndian);

	uint8* pTagImageDesc = FindTag(pIFD0, pLastIFD0, 0x10E, bLittleEndian);
	ReadStringTag(m_sImageDescription, pTagImageDesc, pTIFFHeader, bLittleEndian, true);

	// Add the manufacturer name if not contained in model name
	if (!m_sModel.IsEmpty()) {
		CString sMake;
		uint8* pTagMake = FindTag(pIFD0, pLastIFD0, 0x10F, bLittleEndian);
		ReadStringTag(sMake, pTagMake, pTIFFHeader, bLittleEndian);
		if (!sMake.IsEmpty()) {
			int nSpace = sMake.Find(_T(' '));
			CString sMakeL(nSpace > 0 ? sMake.Left(nSpace) : sMake);
			if (m_sModel.Find(sMakeL) == -1) {
				m_sModel = sMakeL + _T(" ") + m_sModel;
			}
		}
	}

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
	if (pLastEXIF - m_pApp1 >= nApp1Size) {
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

	uint8* pTagUserComment = FindTag(pEXIFIFD, pLastEXIF, 0x9286, bLittleEndian);
	ReadUserCommentTag(m_sUserComment, pTagUserComment, pTIFFHeader, bLittleEndian);
	// Samsung Galaxy puts this useless comment into each JPEG, just ignore
	if (m_sUserComment == "User comments") {	
		m_sUserComment = "";
	}

	if (nOffsetIFD1 != 0) {
		m_pIFD1 = pTIFFHeader + nOffsetIFD1;
		if (m_pIFD1 - m_pApp1 >= nApp1Size || m_pIFD1 - m_pApp1 < 0) {
			return;
		}
		nNumTags = ReadUShort(m_pIFD1, bLittleEndian);
		m_pIFD1 += 2;
		uint8* pLastIFD1 = m_pIFD1 + nNumTags*12;
		if (pLastIFD1 - m_pApp1 >= nApp1Size) {
			return;
		}
		m_pLastIFD1 = pLastIFD1;
		uint8* pTagCompression = FindTag(m_pIFD1, pLastIFD1, 0x103, bLittleEndian);
		if (pTagCompression == NULL) {
			return;
		}
		if (ReadShortTag(pTagCompression, bLittleEndian) == 6) {
			// compressed
			uint8* pTagOffsetSOI = FindTag(m_pIFD1, pLastIFD1, 0x0201, bLittleEndian);
			uint8* pTagJPEGLen = FindTag(m_pIFD1, pLastIFD1, 0x0202, bLittleEndian);
			if (pTagOffsetSOI != NULL && pTagJPEGLen != NULL) {
				uint32 nOffsetSOI = ReadLongTag(pTagOffsetSOI, bLittleEndian);
				uint32 nJPEGBytes = ReadLongTag(pTagJPEGLen, bLittleEndian);
				uint8* pSOI = pTIFFHeader + nOffsetSOI;
				uint8* pSOF = (uint8*) Helpers::FindJPEGMarker(pSOI, nJPEGBytes, 0xC0);
				if (pSOF != NULL) {
					m_nThumbWidth = pSOF[7]*256 + pSOF[8];
					m_nThumbHeight = pSOF[5]*256 + pSOF[6];
					m_nJPEGThumbStreamLen = nJPEGBytes;
					m_bHasJPEGCompressedThumbnail = true;
				}
			}
		} else {
			// uncompressed
			uint8* pTagThumbWidth = FindTag(m_pIFD1, pLastIFD1, 0x0001, bLittleEndian);
			uint8* pTagThumbHeight = FindTag(m_pIFD1, pLastIFD1, 0x0101, bLittleEndian);
			if (pTagThumbWidth != NULL && pTagThumbHeight != NULL) {
				m_nThumbWidth = ReadShortOrLongTag(pTagThumbWidth, bLittleEndian);
				m_nThumbHeight = ReadShortOrLongTag(pTagThumbHeight, bLittleEndian);
			}
		}
	}
}

CEXIFReader::~CEXIFReader(void) {
}

void CEXIFReader::WriteImageOrientation(int nOrientation) {
	if (m_pTagOrientation != NULL && ImageOrientationPresent()) {
		WriteShortTag(m_pTagOrientation, nOrientation, m_bLittleEndian);
	}
}

void CEXIFReader::DeleteThumbnail() {
	if (m_pLastIFD0 != NULL) {
		*m_pLastIFD0 = 0;
	}
}

void CEXIFReader::UpdateJPEGThumbnail(unsigned char* pJPEGStream, int nStreamLen, int nEXIFBlockLenCorrection, CSize sizeThumb) {
	if (!m_bHasJPEGCompressedThumbnail) {
		return;
	}
	uint8* pTagOffsetSOI = FindTag(m_pIFD1, m_pLastIFD1, 0x0201, m_bLittleEndian);
	uint32 nOffsetSOI = ReadLongTag(pTagOffsetSOI, m_bLittleEndian);
	uint8* pTIFFHeader = m_pApp1 + 10;
	uint8* pSOI = pTIFFHeader + nOffsetSOI;
	memcpy(pSOI + 2, pJPEGStream, nStreamLen);

	uint8* pTagJPEGBytes = FindTag(m_pIFD1, m_pLastIFD1, 0x0202, m_bLittleEndian);
	WriteLongTag(pTagJPEGBytes, nStreamLen + 2, m_bLittleEndian);
	int nNewApp1Size = m_pApp1[2]*256 + m_pApp1[3] + nEXIFBlockLenCorrection;
	m_pApp1[2] = nNewApp1Size >> 8;
	m_pApp1[3] = nNewApp1Size & 0xFF;
}
