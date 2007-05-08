#include "StdAfx.h"
#include "SaveImage.h"
#include "JPEGImage.h"
#include "BasicProcessing.h"
#include "Helpers.h"
#include "IJLWrapper.h"
#include "SettingsProvider.h"
#include "ParameterDB.h"

// Returns the compressed JPEG stream that must be freed by the caller. NULL in case of error.
static void* CompressAndSave(LPCTSTR sFileName, CJPEGImage * pImage, 
							 void* pData, int nWidth, int nHeight, int nQuality, int& nJPEGStreamLen) {
	nJPEGStreamLen = 0;
	unsigned char* pTargetStream = (unsigned char*) Jpeg::Compress(pData, nWidth, nHeight, 3, 
		nJPEGStreamLen, nQuality);
	if (pTargetStream == NULL) {
		return NULL;
	}

	FILE *fptr = _tfopen(sFileName, _T("wb"));
	if (fptr == NULL) {
		delete[] pTargetStream;
		return NULL;
	}

	// If EXIF data is present, replace any JFIF block by this EXIF block to preserve the EXIF information
	if (pImage->GetEXIFData() != NULL) {
		unsigned char* pNewStream = new unsigned char[nJPEGStreamLen + pImage->GetEXIFDataLength()];
		memcpy(pNewStream, pTargetStream, 2); // copy SOI block
		memcpy(pNewStream + 2, pImage->GetEXIFData(), pImage->GetEXIFDataLength()); // copy EXIF block
		int nJFIFLength = 0;
		if (pTargetStream[2] == 0xFF && pTargetStream[3] == 0xE0) {
			// don't copy JFIF block
			nJFIFLength = pTargetStream[4]*256 + pTargetStream[5] + 2;
		}
		memcpy(pNewStream + 2 + pImage->GetEXIFDataLength(), pTargetStream + 2 + nJFIFLength, nJPEGStreamLen - 2 - nJFIFLength);
		delete[] pTargetStream;
		pTargetStream = pNewStream;
		nJPEGStreamLen = nJPEGStreamLen - nJFIFLength + pImage->GetEXIFDataLength();
	}

	bool bSuccess = fwrite(pTargetStream, 1, nJPEGStreamLen, fptr) == nJPEGStreamLen;
	fclose(fptr);

	// delete partial file if no success
	if (!bSuccess) {
		delete[] pTargetStream;
		_tunlink(sFileName);
		return NULL;
	}

	// Success, return compressed JPEG stream
	return pTargetStream;
}

bool CSaveImage::SaveImage(LPCTSTR sFileName, CJPEGImage * pImage, const CImageProcessingParams& procParams,
						   EProcessingFlags eFlags) {
	int nOldRegion = pImage->GetDimBitmapRegion();
	pImage->SetDimBitmapRegion(0);

	CSize fullImageSize = CSize(pImage->OrigWidth(), pImage->OrigHeight());
	void* pDIB32bpp = pImage->GetDIB(fullImageSize, fullImageSize, CPoint(0, 0), procParams, eFlags);

	uint32 nSizeLinePadded = Helpers::DoPadding(fullImageSize.cx*3, 4);
	uint32 nSizeBytes = nSizeLinePadded*fullImageSize.cy;
	char* pDIB24bpp = new char[nSizeBytes];
	CBasicProcessing::Convert32bppTo24bppDIB(fullImageSize.cx, fullImageSize.cy, pDIB24bpp, pDIB32bpp, pImage->GetFlagFlipped());

	int nJPEGStreamLen;
	void* pCompressedJPEG = CompressAndSave(sFileName, pImage, pDIB24bpp, fullImageSize.cx, fullImageSize.cy, 
		CSettingsProvider::This().JPEGSaveQuality(), nJPEGStreamLen);
	bool bSuccess = pCompressedJPEG != NULL;

	delete[] pDIB24bpp;
	pDIB24bpp = NULL;

	// Create database entry to avoid processing image again
	if (bSuccess) {
		__int64 nJPEGHash = Helpers::CalculateJPEGFileHash(pCompressedJPEG, nJPEGStreamLen);
		delete[] pCompressedJPEG;
		pCompressedJPEG = NULL;
		if (nJPEGHash != 0) {
			CParameterDBEntry newEntry;
			CImageProcessingParams ippNone(0.0, 1.0, CSettingsProvider::This().Sharpen(), 0.0, 0.5, 0.5, 0.25, 0.5, 0.0, 0.0, 0.0);
			EProcessingFlags procFlagsNone = GetProcessingFlag(eFlags, PFLAG_HighQualityResampling) ? PFLAG_HighQualityResampling : PFLAG_None;
			newEntry.InitFromProcessParams(ippNone, procFlagsNone, 0);
			newEntry.SetHash(nJPEGHash);
			CParameterDB::This().AddEntry(newEntry);
		}
	}
	pImage->SetDimBitmapRegion(nOldRegion);

	return bSuccess;
}
