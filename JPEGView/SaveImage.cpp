#include "StdAfx.h"
#include "SaveImage.h"
#include "JPEGImage.h"
#include "BasicProcessing.h"
#include "Helpers.h"
#include "SettingsProvider.h"
#include "ParameterDB.h"
#include "EXIFReader.h"
#include "TJPEGWrapper.h"
#include "libjpeg-turbo\include\turbojpeg.h"
#include <gdiplus.h>

//////////////////////////////////////////////////////////////////////////////////////////////
// Helpers
//////////////////////////////////////////////////////////////////////////////////////////////

// Removes an existing comment segment (if any) and returns the new JPEG stream, NULL if no comment was found in stream
static uint8* RemoveExistingCommentSegment(void* pJPEGStream, int& nStreamLength) {
    uint8* pCommentSeg = (uint8*)Helpers::FindJPEGMarker(pJPEGStream, nStreamLength, 0xFE);
    if (pCommentSeg != NULL) {
        int nLenSegment = pCommentSeg[2] * 256 + pCommentSeg[3];
        int nHeader = (int)(pCommentSeg - (uint8*)pJPEGStream);
        uint8* pNewBuffer = new uint8[nStreamLength - nLenSegment];
        memcpy(pNewBuffer, pJPEGStream, nHeader);
        memcpy(&(pNewBuffer[nHeader]), pCommentSeg + nLenSegment, nStreamLength - nHeader - nLenSegment);
        nStreamLength = nStreamLength - nLenSegment;
        return pNewBuffer;
    }
    return NULL;
}

// Finds first JPEG marker, returns NULL if none found (invalid JPEG stream?)
static uint8* FindFirstJPEGMarker(void* pJPEGStream, int nStreamLength) {
    uint8* pStream = (uint8*)pJPEGStream;
    if (pStream == NULL || nStreamLength < 3 || pStream[0] != 0xFF || pStream[1] != 0xD8) {
        return NULL; // not a JPEG
    }
    int nIndex = 2;
    do {
        if (pStream[nIndex] == 0xFF) {
            // block header found, skip padding bytes
            while (pStream[nIndex] == 0xFF && nIndex < nStreamLength) nIndex++;
            break;
        }
        else {
            break; // block with pixel data found, start hashing from here
        }
    } while (nIndex < nStreamLength);

    if (pStream[nIndex] != 0) {
        return &(pStream[nIndex - 1]); // place on marker start
    }
    else {
        return NULL;
    }
}

// Inserts the specified string as a JPEG comment into the JPEG stream. Returns NULL in case of errors, a new JPEG stream otherwise
static uint8* InsertCommentBlock(void* pJPEGStream, int& nStreamLength, LPCTSTR comment) {
    const int MAX_COMMENT_LEN = 4096;
    uint8* pFirstJPGMarker = FindFirstJPEGMarker(pJPEGStream, nStreamLength);
    if (pFirstJPGMarker == NULL) {
        return NULL;
    }

    uint8* pNewStream = new uint8[nStreamLength + MAX_COMMENT_LEN];
    
    if (pFirstJPGMarker[1] == 0xE0) {
        int nApp0Len = pFirstJPGMarker[2] * 256 + pFirstJPGMarker[3];
        pFirstJPGMarker += 2;
        pFirstJPGMarker += nApp0Len;
    }

    int nSizeToMarker = (int)(pFirstJPGMarker - (uint8*)pJPEGStream);
    memcpy(pNewStream, pJPEGStream, nSizeToMarker);
    pNewStream[nSizeToMarker] = 0xFF;
    pNewStream[nSizeToMarker + 1] = 0xFE;

    char* pBuffer = new char[MAX_COMMENT_LEN];
    wcstombs(pBuffer, comment, MAX_COMMENT_LEN);
    int nCommentSegLen = 2 + (int)strlen(pBuffer) + 1;
    pNewStream[nSizeToMarker + 2] = nCommentSegLen >> 8;
    pNewStream[nSizeToMarker + 3] = nCommentSegLen & 0xFF;
    strcpy((char*)&(pNewStream[nSizeToMarker + 4]), pBuffer);
    memcpy(&(pNewStream[nSizeToMarker + 2 + nCommentSegLen]), &(((uint8*)pJPEGStream)[nSizeToMarker]), nStreamLength - nSizeToMarker);

    nStreamLength += nCommentSegLen + 2;

    delete[] pBuffer;

    return pNewStream;
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
							 void* pData, int nWidth, int nHeight, int nQuality, int& nJPEGStreamLen, 
                             bool& tjFreeNeeded, bool bCopyEXIF, bool bDeleteThumbnail) {
	nJPEGStreamLen = 0;
    tjFreeNeeded = true;
	bool bOutOfMemory;
	unsigned char* pTargetStream = (unsigned char*) TurboJpeg::Compress(pData, nWidth, nHeight, 
		nJPEGStreamLen, bOutOfMemory, nQuality);
	if (pTargetStream == NULL) {
		return NULL;
	}

	FILE *fptr = _tfopen(sFileName, _T("wb"));
	if (fptr == NULL) {
		tjFree(pTargetStream);
		return NULL;
	}

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
				unsigned char* pJPEGThumb = (unsigned char*) TurboJpeg::Compress(pDIBThumb, sizeThumb.cx, sizeThumb.cy, nJPEGThumbStreamLen, bOutOfMemory, 70);
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
		tjFree(pTargetStream);
		pTargetStream = pNewStream;
        tjFreeNeeded = false;
		nJPEGStreamLen = nJPEGStreamLen - nJFIFLength + pImage->GetEXIFDataLength() + nEXIFBlockLenCorrection;
	}

    // Take over existing JPEG comment from the old image
    LPCTSTR sComment = pImage->GetJPEGComment();
    if (sComment != NULL && sComment[0] != 0) {
        uint8* pNewStream = RemoveExistingCommentSegment(pTargetStream, nJPEGStreamLen);
        if (pNewStream != NULL) {
            if (tjFreeNeeded) tjFree(pTargetStream); else delete[] pTargetStream;
            pTargetStream = pNewStream;
            tjFreeNeeded = false;
        }
        pNewStream = InsertCommentBlock(pTargetStream, nJPEGStreamLen, sComment);
        if (pNewStream != NULL) {
            if (tjFreeNeeded) tjFree(pTargetStream); else delete[] pTargetStream;
            pTargetStream = pNewStream;
            tjFreeNeeded = false;
        }
    }

	bool bSuccess = fwrite(pTargetStream, 1, nJPEGStreamLen, fptr) == nJPEGStreamLen;
	fclose(fptr);

	// delete partial file if no success
	if (!bSuccess) {
        if (tjFreeNeeded) tjFree(pTargetStream); else delete[] pTargetStream;
		_tunlink(sFileName);
		return NULL;
	}

	// Success, return compressed JPEG stream
	return pTargetStream;
}

__declspec(dllimport) size_t Webp_Dll_EncodeBGRLossy(const uint8* bgr, int width, int height, int stride, float quality_factor, uint8** output);
__declspec(dllimport) size_t Webp_Dll_EncodeBGRLossless(const uint8* bgr, int width, int height, int stride, uint8** output);
__declspec(dllimport) void Webp_Dll_FreeMemory(void* pointer);

// pData must point to 24 bit BGR DIB
static bool SaveWebP(LPCTSTR sFileName, void* pData, int nWidth, int nHeight, bool bUseLosslessWEBP) {
	FILE *fptr = _tfopen(sFileName, _T("wb"));
	if (fptr == NULL) {
		return false;
	}

	bool bSuccess = false;
	try {
		uint8* pOutput;
        int nQuality = CSettingsProvider::This().WEBPSaveQuality();
		size_t nSize = bUseLosslessWEBP ? 
            Webp_Dll_EncodeBGRLossless((uint8*)pData, nWidth, nHeight, Helpers::DoPadding(nWidth*3, 4), &pOutput) :
            Webp_Dll_EncodeBGRLossy((uint8*)pData, nWidth, nHeight, Helpers::DoPadding(nWidth*3, 4), (float)nQuality, &pOutput);
		bSuccess = fwrite(pOutput, 1, nSize, fptr) == nSize;
		fclose(fptr);
		Webp_Dll_FreeMemory(pOutput);
	} catch(...) {
		fclose(fptr);
	}

	// delete partial file if no success
	if (!bSuccess) {
		_tunlink(sFileName);
		return false;
	}

	return true;
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
static bool SaveGDIPlus(LPCTSTR sFileName, EImageFormat eFileFormat, void* pData, int nWidth, int nHeight) {
	Gdiplus::Bitmap* pBitmap = new Gdiplus::Bitmap(nWidth, nHeight, Helpers::DoPadding(nWidth*3, 4), PixelFormat24bppRGB, (BYTE*)pData);
	if (pBitmap->GetLastStatus() != Gdiplus::Ok) {
		delete pBitmap;
		return false;
	}

	const wchar_t* sMIMEType = NULL;
	switch (eFileFormat) {
		case IF_WindowsBMP:
			sMIMEType = L"image/bmp";
			break;
		case IF_PNG:
			sMIMEType = L"image/png";
			break;
		case IF_TIFF:
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
	         EProcessingFlags eFlags, bool bFullSize, bool bUseLosslessWEBP, bool bCreateParameterDBEntry) {
    pImage->EnableDimming(false);

	CSize imageSize;
	void* pDIB32bpp;

	if (bFullSize) {
		imageSize = pImage->OrigSize();
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

	EImageFormat eFileFormat = Helpers::GetImageFormat(sFileName);
	bool bSuccess = false;
	__int64 nPixelHash = 0;
	if (eFileFormat == IF_JPEG || eFileFormat == IF_JPEG_Embedded) {
		// Save JPEG not over GDI+ - we want to keep the meta-data if there is meta-data
		int nJPEGStreamLen;
        bool tjFreeNeeded;
		void* pCompressedJPEG = CompressAndSave(sFileName, pImage, pDIB24bpp, imageSize.cx, imageSize.cy, 
			CSettingsProvider::This().JPEGSaveQuality(), nJPEGStreamLen, tjFreeNeeded, true, !bFullSize);
		bSuccess = pCompressedJPEG != NULL;
		if (bSuccess) {
			nPixelHash = Helpers::CalculateJPEGFileHash(pCompressedJPEG, nJPEGStreamLen);
            if (tjFreeNeeded) {
                tjFree((unsigned char*)pCompressedJPEG);
            } else {
			    delete[] pCompressedJPEG;
            }
		}
	} else {
		if (eFileFormat == IF_WEBP) {
			bSuccess = SaveWebP(sFileName, pDIB24bpp, imageSize.cx, imageSize.cy, bUseLosslessWEBP);
		} else {
			bSuccess = SaveGDIPlus(sFileName, eFileFormat, pDIB24bpp, imageSize.cx, imageSize.cy);
		}
		if (bSuccess) {
			CJPEGImage tempImage(imageSize.cx, imageSize.cy, pDIB32bpp, NULL, 4, 0, IF_Unknown, false, 0, 1, 0);
			nPixelHash = tempImage.GetUncompressedPixelHash();
			tempImage.DetachIJLPixels();
		}
	}

	delete[] pDIB24bpp;
	pDIB24bpp = NULL;

	// Create database entry to avoid processing image again
	if (bSuccess && bCreateParameterDBEntry && CSettingsProvider::This().CreateParamDBEntryOnSave()) {
		if (nPixelHash != 0) {
			CParameterDBEntry newEntry;
			CImageProcessingParams ippNone(0.0, 1.0, 1.0, CSettingsProvider::This().Sharpen(), 0.0, 0.5, 0.5, 0.25, 0.5, 0.0, 0.0, 0.0);
			EProcessingFlags procFlagsNone = GetProcessingFlag(eFlags, PFLAG_HighQualityResampling) ? PFLAG_HighQualityResampling : PFLAG_None;
			newEntry.InitFromProcessParams(ippNone, procFlagsNone, CRotationParams(0));
			newEntry.SetHash(nPixelHash);
			CParameterDB::This().AddEntry(newEntry);
		}
	}
	pImage->EnableDimming(true);

	return bSuccess;
}

bool CSaveImage::SaveImage(LPCTSTR sFileName, CJPEGImage * pImage, bool bUseLosslessWEBP)
{
	CImageProcessingParams paramsNotUsed;
	return SaveImage(sFileName, pImage, paramsNotUsed, PFLAG_None, false, bUseLosslessWEBP, false);
}
