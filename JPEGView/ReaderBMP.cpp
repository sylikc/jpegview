#include "StdAfx.h"
#include "ReaderBMP.h"
#include "JPEGImage.h"
#include "Helpers.h"
#include "BasicProcessing.h"

//////////////////////////////////////////////////////////////////////////////////
// BITMAP reading

struct BMHEADER {
	unsigned short int type;                 /* Magic identifier            */
	unsigned int size;                       /* File size in bytes          */
	unsigned short int reserved1, reserved2;
	unsigned int offset;                     /* Offset to image data, bytes */
};

struct BMINFOHEADER {
	unsigned int size;               /* Header size in bytes      */
	int width,height;                /* Width and height of image */
	unsigned short int planes;       /* Number of colour planes   */
	unsigned short int bits;         /* Bits per pixel            */
	unsigned int compression;        /* Compression type          */
	unsigned int imagesize;          /* Image size in bytes       */
	int xresolution,yresolution;     /* Pixels per meter          */
	unsigned int ncolours;           /* Number of colours         */
	unsigned int importantcolours;   /* Important colours         */
};

static void ReadUShort(FILE* file, uint16* pUShort) {
	fread(pUShort, sizeof(uint16), 1, file);
}

static void ReadUInt(FILE* file, uint32* pUInt) {
	fread(pUInt, sizeof(uint32), 1, file);
}

CJPEGImage* CReaderBMP::ReadBmpImage(LPCTSTR strFileName, bool& bOutOfMemory) {
	BMHEADER header;
	BMINFOHEADER infoheader;
	FILE *fptr;

	const int MAX_FILE_SIZE_BYTES = 100 * 1024 * 1024; // 100 MB

	bOutOfMemory = false;

	/* Open file */
	if ((fptr = _tfopen(strFileName,_T("rb"))) == NULL) {
		return NULL;
	}

	/* Read the header */
	ReadUShort(fptr,&header.type);
	ReadUInt(fptr,&header.size);
	ReadUShort(fptr,&header.reserved1);
	ReadUShort(fptr,&header.reserved2);
	ReadUInt(fptr,&header.offset);

	/* Read and check the information header */
	if (fread(&infoheader,sizeof(BMINFOHEADER),1,fptr) != 1) {
		fclose(fptr);
		return NULL;
	}
	/* Only 24 and 32 bpp */
	if (infoheader.bits != 24 && infoheader.bits != 32 && infoheader.bits != 8) {
		fclose(fptr);
		return NULL;
	}

	// read palette for 8 bpp DIBs
	uint8 palette[4*256];
	if (infoheader.bits == 8) {
		fseek(fptr, infoheader.size + 14, SEEK_SET);
		if (fread(palette, 256*4, 1, fptr) != 1) {
			fclose(fptr);
			return NULL;
		}
	}

	/* Seek to the start of the image data */
	fseek(fptr,header.offset,SEEK_SET);

	// DIBs are normally stored flipped vertically (meaning they are stored bottom-up)
	bool bFlipped;
	if (infoheader.height < 0) {
		infoheader.height = -infoheader.height;
		bFlipped = false;
	} else {
		bFlipped = true;
	}

	int bytesPerPixel = infoheader.bits/8;
	int paddedWidth = Helpers::DoPadding(infoheader.width*bytesPerPixel, 4);
	int fileSizeBytes = infoheader.height*paddedWidth;
	if (fileSizeBytes <= 0 || fileSizeBytes > MAX_FILE_SIZE_BYTES) {
		fclose(fptr);
		bOutOfMemory = fileSizeBytes > MAX_FILE_SIZE_BYTES;
		return NULL; // corrupt or manipulated header
	}

	uint8* pDest = new(std::nothrow) uint8[fileSizeBytes];
	if (pDest == NULL) {
		fclose(fptr);
		bOutOfMemory = true;
		return NULL;
	}
	uint8* pStart = bFlipped ? pDest + paddedWidth*(infoheader.height-1) : pDest;
	for (int nLine = 0; nLine < infoheader.height; nLine++) {
		if (paddedWidth != fread(pStart, 1, paddedWidth, fptr)) {
			delete[] pDest;
			fclose(fptr);
			return NULL;
		}
		pStart = pStart + (bFlipped ? -paddedWidth : paddedWidth);
	}

	// Convert 8 bpp DIBs
	if (infoheader.bits == 8) {
		uint8* pTemp = (uint8*)CBasicProcessing::Convert8bppTo32bppDIB(infoheader.width, infoheader.height, pDest, palette);
		delete[] pDest;
		pDest = pTemp;
		infoheader.bits = 32;
	}

	// The CJPEGImage object gets ownership of the memory in pDest
	CJPEGImage* pImage = (pDest == NULL) ? NULL : new CJPEGImage(infoheader.width, infoheader.height, pDest, NULL, infoheader.bits/8, 
		0, IF_WindowsBMP, false, 0, 1, 0);

	fclose(fptr);

	bOutOfMemory = pImage == NULL;

	return pImage;
}