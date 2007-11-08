#include "StdAfx.h"
#include "SaveImage.h"
#include "JPEGImage.h"
#include "BasicProcessing.h"
#include "Helpers.h"
#include "IJLWrapper.h"
#include "SettingsProvider.h"
#include "ParameterDB.h"
#include <gdiplus.h>

// Gets the image format given a file name (uses the file extension)
static CJPEGImage::EImageFormat GetImageFormat(LPCTSTR sFileName) {
	LPCTSTR sEnding = _tcsrchr(sFileName, _T('.'));
	if (sEnding != NULL) {
		sEnding += 1;
		if (_tcsicmp(sEnding, _T("JPG")) == 0 || _tcsicmp(sEnding, _T("JPEG")) == 0) {
			return CJPEGImage::IF_JPEG;
		} else if (_tcsicmp(sEnding, _T("BMP")) == 0) {
			return CJPEGImage::IF_WindowsBMP;
		} else if (_tcsicmp(sEnding, _T("PNG")) == 0) {
			return CJPEGImage::IF_PNG;
		} else if (_tcsicmp(sEnding, _T("TIF")) == 0 || _tcsicmp(sEnding, _T("TIFF")) == 0) {
			return CJPEGImage::IF_TIFF;
		}
	}
	return CJPEGImage::IF_Unknown;
}

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

// Copied from MS sample
static int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
   UINT  num = 0;          // number of image encoders
   UINT  size = 0;         // size of the image encoder array in bytes

   Gdiplus::GetImageEncodersSize(&num, &size);
   if(size == 0)
      return -1;  // Failure

   Gdiplus::ImageCodecInfo* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));

   Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

   for(UINT j = 0; j < num; ++j) {
      if( wcscmp(pImageCodecInfo[j].MimeType, format) == 0 ) {
         *pClsid = pImageCodecInfo[j].Clsid;
         free(pImageCodecInfo);
         return j;  // Success
      }    
   }

   free(pImageCodecInfo);
   return -1;  // Failure
}

// Saves the given 24 bpp DIB data to the file namge given using GDI+
static bool SaveGDIPlus(LPCTSTR sFileName, CJPEGImage::EImageFormat eFileFormat, void* pData, int nWidth, int nHeight) {
	Gdiplus::Bitmap* pBitmap = new Gdiplus::Bitmap(nWidth, nHeight, Helpers::DoPadding(nWidth*3, 4), PixelFormat24bppRGB, (BYTE*)pData);
	if (pBitmap->GetLastStatus() != Gdiplus::Ok) {
		delete pBitmap;
		return false;
	}

	const wchar_t* sMIMEType = NULL;
	switch (eFileFormat) {
		case CJPEGImage::IF_WindowsBMP:
			sMIMEType = L"image/bmp";
			break;
		case CJPEGImage::IF_PNG:
			sMIMEType = L"image/png";
			break;
		case CJPEGImage::IF_TIFF:
			sMIMEType = L"image/tiff";
			break;
	}

	CLSID encoderClsid;
	int result = (sMIMEType == NULL) ? -1 : GetEncoderClsid(sMIMEType, &encoderClsid);
	if (result < 0) {
		delete pBitmap;
		return false;
	}

	const wchar_t* sFileNameUnicode;
#ifdef _UNICODE
	sFileNameUnicode = (const wchar_t*)sFileName;
#else
	wchar_t buff[MAX_PATH];
	size_t nDummy;
	mbstowcs_s(&nDummy, buff, MAX_PATH, (const char*)sFileName, strlen(sFileName));
	sFileNameUnicode = (const wchar_t*)buff;
#endif

	bool bOk = pBitmap->Save(sFileNameUnicode, &encoderClsid, NULL) == Gdiplus::Ok;

	delete pBitmap;
	return true;
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
	CBasicProcessing::Convert32bppTo24bppDIB(fullImageSize.cx, fullImageSize.cy, pDIB24bpp, pDIB32bpp, false);

	CJPEGImage::EImageFormat eFileFormat = GetImageFormat(sFileName);
	bool bSuccess = false;
	__int64 nPixelHash = 0;
	if (eFileFormat == CJPEGImage::IF_JPEG) {
		// Save JPEG not over GDI+ - we want to keep the meta-data if there is meta-data
		int nJPEGStreamLen;
		void* pCompressedJPEG = CompressAndSave(sFileName, pImage, pDIB24bpp, fullImageSize.cx, fullImageSize.cy, 
			CSettingsProvider::This().JPEGSaveQuality(), nJPEGStreamLen);
		bSuccess = pCompressedJPEG != NULL;
		if (bSuccess) {
			nPixelHash = Helpers::CalculateJPEGFileHash(pCompressedJPEG, nJPEGStreamLen);
			delete[] pCompressedJPEG;
		}
	} else {
		bSuccess = SaveGDIPlus(sFileName, eFileFormat, pDIB24bpp, fullImageSize.cx, fullImageSize.cy);
		if (bSuccess) {
			CJPEGImage tempImage(fullImageSize.cx, fullImageSize.cy, pDIB32bpp, NULL, 4, 0, CJPEGImage::IF_Unknown);
			nPixelHash = tempImage.GetUncompressedPixelHash();
			tempImage.DetachIJLPixels();
		}
	}

	delete[] pDIB24bpp;
	pDIB24bpp = NULL;

	// Create database entry to avoid processing image again
	if (bSuccess) {
		if (nPixelHash != 0) {
			CParameterDBEntry newEntry;
			CImageProcessingParams ippNone(0.0, 1.0, CSettingsProvider::This().Sharpen(), 0.0, 0.5, 0.5, 0.25, 0.5, 0.0, 0.0, 0.0);
			EProcessingFlags procFlagsNone = GetProcessingFlag(eFlags, PFLAG_HighQualityResampling) ? PFLAG_HighQualityResampling : PFLAG_None;
			newEntry.InitFromProcessParams(ippNone, procFlagsNone, 0);
			newEntry.SetHash(nPixelHash);
			CParameterDB::This().AddEntry(newEntry);
		}
	}
	pImage->SetDimBitmapRegion(nOldRegion);

	return bSuccess;
}
