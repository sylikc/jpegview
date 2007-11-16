#include "StdAfx.h"
#include "EXIFReader.h"
#include "ImageProcessingTypes.h"

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
	if (ptr != NULL && ReadUShort(ptr + 2, bLittleEndian) == 2) {
		strOut = CString(pTIFFHeader + ReadUInt(ptr + 8, bLittleEndian));
	} else {
		strOut.Empty();
	}
}

CEXIFReader::CEXIFReader(void* pApp1Block) {
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

	uint8* pModel = FindTag(pIFD0, pLastIFD0, 0x110, bLittleEndian);
	ReadStringTag(m_sModel, pModel, pTIFFHeader, bLittleEndian);

	uint8* pEXIFIFD = FindTag(pIFD0, pLastIFD0, 0x8769, bLittleEndian);
	if (pEXIFIFD == NULL) {
		return;
	}
}

CEXIFReader::~CEXIFReader(void) {
}
