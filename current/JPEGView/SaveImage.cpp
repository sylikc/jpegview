#include "StdAfx.h"
#include "SaveImage.h"
#include "JPEGImage.h"
#include "BasicProcessing.h"
#include "Helpers.h"
#include "IJLWrapper.h"
#include "SettingsProvider.h"
#include "ParameterDB.h"
#include "EXIFReader.h"
#include <gdiplus.h>

//////////////////////////////////////////////////////////////////////////////////////////////
// Helpers
//////////////////////////////////////////////////////////////////////////////////////////////

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

// Gets the thumbnail DIB, returns size of thumbnail in sizeThumb
static void* GetThumbnailDIB(CJPEGImage * pImage, CSize& sizeThumb) {
	EProcessingFlags eFlags = pImage->GetLastProcessFlags();
	eFlags = (EProcessingFlags)(eFlags | PFLAG_HighQualityResampling);
	eFlags = (EProcessingFlags)(eFlags & ~PFLAG_AutoContrastSection);
	eFlags = (EProcessingFlags)(eFlags & ~PFLAG_KeepParams);
	CImageProcessingParams params = pImage->GetLastProcessParams();
	params.Sharpen = 0.0;
	double dZoom;
	sizeThumb = Helpers::GetImageRect(pImage->OrigWidth(), pImage->OrigHeight(), 160, 160, Helpers::ZM_FitToScreenNoZoom, dZoom);
	void* pDIB32bpp = pImage->GetThumbnailDIB(sizeThumb, params, eFlags);
	if (pDIB32bpp == NULL) {
		return NULL;
	}
	uint32 nSizeLinePadded = Helpers::DoPadding(sizeThumb.cx*3, 4);
	uint32 nSizeBytes = nSizeLinePadded*sizeThumb.cy;
	char* pDIB24bpp = new char[nSizeBytes];
	CBasicProcessing::Convert32bppTo24bppDIB(sizeThumb.cx, sizeThumb.cy, pDIB24bpp, pDIB32bpp, false);
	return pDIB24bpp;
}

static int GetJFIFBlockLength(unsigned char* pJPEGStream) {
	int nJFIFLength = 0;
	if (pJPEGStream[2] == 0xFF && pJPEGStream[3] == 0xE0) {
		nJFIFLength = pJPEGStream[4]*256 + pJPEGStream[5] + 2;
	}
	return nJFIFLength;
}

// Returns the compressed JPEG stream that must be freed by the caller. NULL in case of error.
static void* CompressAndSave(LPCTSTR sFileName, CJPEGImage * pImage, 
							 void* pData, int nWidth, int nHeight, int nQuality, int& nJPEGStreamLen, bool bCopyEXIF, bool bDeleteThumbnail) {
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

	// The Intel lib writes a useless comment into the JPEG stream - delete this
	Helpers::ClearJPEGComment(pTargetStream, nJPEGStreamLen);

	// If EXIF data is present, replace any JFIF block by this EXIF block to preserve the EXIF information
	if (pImage->GetEXIFData() != NULL && bCopyEXIF) {
		const int cnAdditionalThumbBytes = 32000;
		int nEXIFBlockLenCorrection = 0;
		unsigned char* pNewStream = new unsigned char[nJPEGStreamLen + pImage->GetEXIFDataLength() + cnAdditionalThumbBytes];
		memcpy(pNewStream, pTargetStream, 2); // copy SOI block
		memcpy(pNewStream + 2, pImage->GetEXIFData(), pImage->GetEXIFDataLength()); // copy EXIF block
		
		// Set image orientation back to normal orientation, we save the pixels as displayed
		CEXIFReader exifReader( pNewStream + 2);
		exifReader.WriteImageOrientation(1); // 1 means default orientation (unrotated)
		if (bDeleteThumbnail) {
			exifReader.DeleteThumbnail();
		} else if (exifReader.HasJPEGCompressedThumbnail()) {
			// recreate EXIF thumbnail image
			CSize sizeThumb;
			void* pDIBThumb = GetThumbnailDIB(pImage, sizeThumb);
			if (pDIBThumb != NULL) {
				int nJPEGThumbStreamLen;
				unsigned char* pJPEGThumb = (unsigned char*) Jpeg::Compress(pDIBThumb, sizeThumb.cx, sizeThumb.cy, 3, nJPEGThumbStreamLen, 70);
				if (pJPEGThumb != NULL) {
					int nThumbJFIFLen = GetJFIFBlockLength(pJPEGThumb);
					nEXIFBlockLenCorrection = nJPEGThumbStreamLen - nThumbJFIFLen - exifReader.GetJPEGThumbStreamLen();
					if (nEXIFBlockLenCorrection <= cnAdditionalThumbBytes && pImage->GetEXIFDataLength() + nEXIFBlockLenCorrection < 65536) {
						exifReader.UpdateJPEGThumbnail(pJPEGThumb + 2 + nThumbJFIFLen, nJPEGThumbStreamLen - 2 - nThumbJFIFLen, nEXIFBlockLenCorrection, sizeThumb);
					} else {
						nEXIFBlockLenCorrection = 0;
					}
					delete[] pJPEGThumb;
				}
				delete[] pDIBThumb;
			}
		}

		int nJFIFLength = GetJFIFBlockLength(pTargetStream);
		memcpy(pNewStream + 2 + pImage->GetEXIFDataLength() + nEXIFBlockLenCorrection, pTargetStream + 2 + nJFIFLength, nJPEGStreamLen - 2 - nJFIFLength);
		delete[] pTargetStream;
		pTargetStream = pNewStream;
		nJPEGStreamLen = nJPEGStreamLen - nJFIFLength + pImage->GetEXIFDataLength() + nEXIFBlockLenCorrection;
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
	return bOk;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// CSaveImage
//////////////////////////////////////////////////////////////////////////////////////////////

bool CSaveImage::SaveImage(LPCTSTR sFileName, CJPEGImage * pImage, const CImageProcessingParams& procParams,
						   EProcessingFlags eFlags, bool bFullSize) {
    pImage->EnableDimming(false);

	CSize imageSize;
	void* pDIB32bpp;

	if (bFullSize) {
		imageSize = CSize(pImage->OrigWidth(), pImage->OrigHeight());
		pDIB32bpp = pImage->GetDIB(imageSize, imageSize, CPoint(0, 0), procParams, eFlags);
	} else {
		imageSize = CSize(pImage->DIBWidth(), pImage->DIBHeight());
		pDIB32bpp = pImage->DIBPixelsLastProcessed(true);
	}

	if (pDIB32bpp == NULL) {
		pImage->EnableDimming(true);
		return false;
	}

	uint32 nSizeLinePadded = Helpers::DoPadding(imageSize.cx*3, 4);
	uint32 nSizeBytes = nSizeLinePadded*imageSize.cy;
	char* pDIB24bpp = new char[nSizeBytes];
	CBasicProcessing::Convert32bppTo24bppDIB(imageSize.cx, imageSize.cy, pDIB24bpp, pDIB32bpp, false);

	CJPEGImage::EImageFormat eFileFormat = GetImageFormat(sFileName);
	bool bSuccess = false;
	__int64 nPixelHash = 0;
	if (eFileFormat == CJPEGImage::IF_JPEG) {
		// Save JPEG not over GDI+ - we want to keep the meta-data if there is meta-data
		int nJPEGStreamLen;
		void* pCompressedJPEG = CompressAndSave(sFileName, pImage, pDIB24bpp, imageSize.cx, imageSize.cy, 
			CSettingsProvider::This().JPEGSaveQuality(), nJPEGStreamLen, true, !bFullSize);
		bSuccess = pCompressedJPEG != NULL;
		if (bSuccess) {
			nPixelHash = Helpers::CalculateJPEGFileHash(pCompressedJPEG, nJPEGStreamLen);
			delete[] pCompressedJPEG;
		}
	} else {
		bSuccess = SaveGDIPlus(sFileName, eFileFormat, pDIB24bpp, imageSize.cx, imageSize.cy);
		if (bSuccess) {
			CJPEGImage tempImage(imageSize.cx, imageSize.cy, pDIB32bpp, NULL, 4, 0, CJPEGImage::IF_Unknown);
			nPixelHash = tempImage.GetUncompressedPixelHash();
			tempImage.DetachIJLPixels();
		}
	}

	delete[] pDIB24bpp;
	pDIB24bpp = NULL;

	// Create database entry to avoid processing image again
	if (bSuccess && CSettingsProvider::This().CreateParamDBEntryOnSave()) {
		if (nPixelHash != 0) {
			CParameterDBEntry newEntry;
			CImageProcessingParams ippNone(0.0, 1.0, 1.0, CSettingsProvider::This().Sharpen(), 0.0, 0.5, 0.5, 0.25, 0.5, 0.0, 0.0, 0.0);
			EProcessingFlags procFlagsNone = GetProcessingFlag(eFlags, PFLAG_HighQualityResampling) ? PFLAG_HighQualityResampling : PFLAG_None;
			newEntry.InitFromProcessParams(ippNone, procFlagsNone, 0);
			newEntry.SetHash(nPixelHash);
			CParameterDB::This().AddEntry(newEntry);
		}
	}
	pImage->EnableDimming(true);

	return bSuccess;
}
