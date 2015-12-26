
#include "StdAfx.h"
#include "ImageLoadThread.h"
#include <gdiplus.h>
#include "JPEGImage.h"
#include "MessageDef.h"
#include "Helpers.h"
#include "SettingsProvider.h"
#include "ReaderBMP.h"
#include "ReaderTGA.h"
#include "BasicProcessing.h"
#include "dcraw_mod.h"
#include "TJPEGWrapper.h"
#include "MaxImageDef.h"

using namespace Gdiplus;

// static intializers
volatile LONG CImageLoadThread::m_curHandle = 0;

/////////////////////////////////////////////////////////////////////////////////////////////
// static helpers
/////////////////////////////////////////////////////////////////////////////////////////////

// find image format of this image by reading some header bytes
static EImageFormat GetImageFormat(LPCTSTR sFileName) {
	FILE *fptr;
	if ((fptr = _tfopen(sFileName, _T("rb"))) == NULL) {
		return IF_Unknown;
	}
	unsigned char header[16];
	int nSize = (int)fread((void*)header, 1, 16, fptr);
	fclose(fptr);
	if (nSize < 2) {
		return IF_Unknown;
	}
	if (header[0] == 0x42 && header[1] == 0x4d) {
		return IF_WindowsBMP;
	} else if (header[0] == 0xff && header[1] == 0xd8) {
		return IF_JPEG;
	} else if (header[0] == 'R' && header[1] == 'I' && header[2] == 'F' && header[3] == 'F' &&
		header[8] == 'W' && header[9] == 'E' && header[10] == 'B' && header[11] == 'P') {
		return IF_WEBP;
	} else {
        return Helpers::GetImageFormat(sFileName);
	}
}

static EImageFormat GetBitmapFormat(Gdiplus::Bitmap * pBitmap) {
	GUID guid;
	memset(&guid, 0, sizeof(GUID));
	pBitmap->GetRawFormat(&guid);
	if (guid == Gdiplus::ImageFormatBMP) {
		return IF_WindowsBMP;
	} else if (guid == Gdiplus::ImageFormatPNG) {
		return IF_PNG;
	} else if (guid == Gdiplus::ImageFormatGIF) {
		return IF_GIF;
	} else if (guid == Gdiplus::ImageFormatTIFF) {
		return IF_TIFF;
	} else if (guid == Gdiplus::ImageFormatJPEG || guid == Gdiplus::ImageFormatEXIF) {
		return IF_JPEG;
	} else {
		return IF_Unknown;
	}
}

static CJPEGImage* ConvertGDIPlusBitmapToJPEGImage(Gdiplus::Bitmap* pBitmap, int nFrameIndex, void* pEXIFData, 
    __int64 nJPEGHash, bool &isOutOfMemory, bool &isAnimatedGIF) {

	isOutOfMemory = false;
    isAnimatedGIF = false;
    Gdiplus::Status lastStatus = pBitmap->GetLastStatus();
    if (lastStatus != Gdiplus::Ok) {
        isOutOfMemory = lastStatus == Gdiplus::OutOfMemory;
		return NULL;
	}

    if (pBitmap->GetWidth() > MAX_IMAGE_DIMENSION || pBitmap->GetHeight() > MAX_IMAGE_DIMENSION)
    {
        return NULL;
    }
	if ((double)pBitmap->GetWidth() * pBitmap->GetHeight() > MAX_IMAGE_PIXELS)
	{
		isOutOfMemory = true;
		return NULL;
	}

    EImageFormat eImageFormat = GetBitmapFormat(pBitmap);

    // Handle multiframe images.
    // Note that only the first frame dimension is looked at, it is unclear if GDI+ anyway supports image formats with more dimensions
    UINT nDimensions = pBitmap->GetFrameDimensionsCount();
    GUID* pDimensionIDs = new GUID[nDimensions];
    pBitmap->GetFrameDimensionsList(pDimensionIDs, nDimensions);
    int nFrameCount = (nDimensions == 0) ? 1 : pBitmap->GetFrameCount(&pDimensionIDs[0]);
    nFrameIndex = max(0, min(nFrameCount - 1, nFrameIndex));
    int nFrameTimeMs = 100;
    if (nFrameCount > 1) {
        isAnimatedGIF = eImageFormat == IF_GIF;
        int nTagFrameDelaySize = pBitmap->GetPropertyItemSize(PropertyTagFrameDelay);
        if (nTagFrameDelaySize > 0) {
            PropertyItem* pPropertyItem = (PropertyItem*)new char[nTagFrameDelaySize];
            if (pBitmap->GetPropertyItem(PropertyTagFrameDelay, nTagFrameDelaySize, pPropertyItem) == Gdiplus::Ok) {
                nFrameTimeMs = ((long*)pPropertyItem->value)[nFrameIndex] * 10;
                if (nFrameTimeMs <= 0) nFrameTimeMs = 100;
            }
            delete[] pPropertyItem;
        }
        GUID pageGuid = (eImageFormat == IF_TIFF) ? FrameDimensionPage : FrameDimensionTime;
        pBitmap->SelectActiveFrame(&pageGuid, nFrameIndex);
    }
    delete[] pDimensionIDs;

	// If there is an alpha channel in the original file we must blit the image onto a background color offscreen
	// bitmap first to archieve proper rendering.
	CJPEGImage* pJPEGImage = NULL;
	Gdiplus::PixelFormat pixelFormat = pBitmap->GetPixelFormat();
	bool bHasAlphaChannel = (pixelFormat & (PixelFormatAlpha | PixelFormatPAlpha));
	Gdiplus::Bitmap* pBmTarget = NULL;
	Gdiplus::Graphics* pBmGraphics = NULL;
	Gdiplus::Bitmap* pBitmapToUse;
	if (bHasAlphaChannel) {
		pBmTarget = new Gdiplus::Bitmap(pBitmap->GetWidth(), pBitmap->GetHeight(), PixelFormat32bppRGB);
		pBmGraphics = new Gdiplus::Graphics(pBmTarget);
		COLORREF bkColor = CSettingsProvider::This().ColorTransparency();
		Gdiplus::SolidBrush bkBrush(Gdiplus::Color(GetRValue(bkColor), GetGValue(bkColor), GetBValue(bkColor)));
		pBmGraphics->FillRectangle(&bkBrush, 0, 0, pBmTarget->GetWidth(), pBmTarget->GetHeight());
		pBmGraphics->DrawImage(pBitmap, 0, 0, pBmTarget->GetWidth(), pBmTarget->GetHeight());
		pBitmapToUse = pBmTarget;
		if (pBmGraphics->GetLastStatus() == Gdiplus::OutOfMemory) {
            isOutOfMemory = true;
			delete pBmGraphics; delete pBmTarget;
			return NULL;
		}
	} else {
		pBitmapToUse = pBitmap;
	}

	Gdiplus::Rect bmRect(0, 0, pBitmap->GetWidth(), pBitmap->GetHeight());
	Gdiplus::BitmapData bmData;
	if (pBitmapToUse->LockBits(&bmRect, Gdiplus::ImageLockModeRead, PixelFormat32bppRGB, &bmData) == Gdiplus::Ok) {
		assert(bmData.PixelFormat == PixelFormat32bppRGB);
		void* pDIB = CBasicProcessing::ConvertGdiplus32bppRGB(bmRect.Width, bmRect.Height, bmData.Stride, bmData.Scan0);
		if (pDIB != NULL) {
			pJPEGImage = new CJPEGImage(bmRect.Width, bmRect.Height, pDIB, pEXIFData, 4, nJPEGHash, eImageFormat,
                eImageFormat == IF_GIF && nFrameCount > 1, nFrameIndex, nFrameCount, nFrameTimeMs);
		}
		pBitmapToUse->UnlockBits(&bmData);
	}

	if (pBmGraphics != NULL && pBmTarget != NULL) {
		delete pBmGraphics;
		delete pBmTarget;
	}

    pBitmap->GetLastStatus(); // reset status

	return pJPEGImage;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Public
/////////////////////////////////////////////////////////////////////////////////////////////

CImageLoadThread::CImageLoadThread(void) : CWorkThread(true) {
    m_pLastBitmap = NULL;
}

CImageLoadThread::~CImageLoadThread(void) {
    DeleteCachedGDIBitmap();
}

int CImageLoadThread::AsyncLoad(LPCTSTR strFileName, int nFrameIndex, const CProcessParams & processParams, HWND targetWnd, HANDLE eventFinished) {
	CRequest* pRequest = new CRequest(strFileName, nFrameIndex, targetWnd, processParams, eventFinished);

	ProcessAsync(pRequest);

	return pRequest->RequestHandle;
}

CImageData CImageLoadThread::GetLoadedImage(int nHandle) {
	// do not delete anything here, only mark as deleted
	::EnterCriticalSection(&m_csList);
	CJPEGImage* imageFound = NULL;
    bool bFailedMemory = false;
	std::list<CRequestBase*>::iterator iter;
	for (iter = m_requestList.begin( ); iter != m_requestList.end( ); iter++ ) {
		CRequest* pRequest = (CRequest*)(*iter);
		if (pRequest->Processed && pRequest->Deleted == false && pRequest->RequestHandle == nHandle) {
			imageFound = pRequest->Image;
            bFailedMemory = pRequest->OutOfMemory;
			pRequest->Deleted = true;
			break;
		}
	}
	::LeaveCriticalSection(&m_csList);
	return CImageData(imageFound, bFailedMemory);
}

void CImageLoadThread::ReleaseFile(LPCTSTR strFileName) {
    CReleaseFileRequest* pRequest = new CReleaseFileRequest(strFileName);
    ProcessAndWait(pRequest);
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Protected
/////////////////////////////////////////////////////////////////////////////////////////////

void CImageLoadThread::ProcessRequest(CRequestBase& request) {
    if (request.Type == CReleaseFileRequest::ReleaseFileRequest) {
        CReleaseFileRequest& rq = (CReleaseFileRequest&)request;
        if (rq.FileName == m_sLastFileName) {
            DeleteCachedGDIBitmap();
        }
        return;
    }

	CRequest& rq = (CRequest&)request;
	double dStartTime = Helpers::GetExactTickCount(); 
	// Get image format and read the image
	switch (GetImageFormat(rq.FileName)) {
		case IF_JPEG :
            DeleteCachedGDIBitmap();
			ProcessReadJPEGRequest(&rq);
			break;
		case IF_WindowsBMP :
            DeleteCachedGDIBitmap();
			ProcessReadBMPRequest(&rq);
			break;
        case IF_TGA :
            DeleteCachedGDIBitmap();
			ProcessReadTGARequest(&rq);
			break;
		case IF_WEBP:
            DeleteCachedGDIBitmap();
			ProcessReadWEBPRequest(&rq);
			break;
		case IF_CameraRAW:
            DeleteCachedGDIBitmap();
			ProcessReadRAWRequest(&rq);
			break;
        case IF_WIC:
            DeleteCachedGDIBitmap();
            ProcessReadWICRequest(&rq);
            break;
		default:
			// try with GDI+
			ProcessReadGDIPlusRequest(&rq);
			break;
	}
	// then process the image if read was successful
	if (rq.Image != NULL) {
		rq.Image->SetLoadTickCount(Helpers::GetExactTickCount() - dStartTime); 
		if (!ProcessImageAfterLoad(&rq)) {
			delete rq.Image;
			rq.Image = NULL;
			rq.OutOfMemory = true;
		}
	}
}

void CImageLoadThread::AfterFinishProcess(CRequestBase& request) {
    if (request.Type == CReleaseFileRequest::ReleaseFileRequest) {
        return;
    }

	CRequest& rq = (CRequest&)request;
	if (rq.TargetWnd != NULL) {
		// post message to window that request has been processed
		::PostMessage(rq.TargetWnd, WM_JPEG_LOAD_COMPLETED, 0, rq.RequestHandle);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Private
/////////////////////////////////////////////////////////////////////////////////////////////

static void LimitOffsets(CPoint& offsets, CSize clippingSize, const CSize & imageSize) {
	int nMaxOffsetX = (imageSize.cx - clippingSize.cx)/2;
	nMaxOffsetX = max(0, nMaxOffsetX);
	int nMaxOffsetY = (imageSize.cy - clippingSize.cy)/2;
	nMaxOffsetY = max(0, nMaxOffsetY);
	offsets.x = max(-nMaxOffsetX, min(+nMaxOffsetX, offsets.x));
	offsets.y = max(-nMaxOffsetY, min(+nMaxOffsetY, offsets.y));
}

void CImageLoadThread::DeleteCachedGDIBitmap() {
    if (m_pLastBitmap != NULL) {
        delete m_pLastBitmap;
    }
    m_pLastBitmap = NULL;
    m_sLastFileName.Empty();
}

void CImageLoadThread::ProcessReadJPEGRequest(CRequest * request) {
	HANDLE hFile = ::CreateFile(request->FileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		return;
	}

	HGLOBAL hFileBuffer = NULL;
	void* pBuffer = NULL;
	try {
		// Don't read too huge files...
		unsigned int nFileSize = ::GetFileSize(hFile, NULL);
		if (nFileSize > MAX_JPEG_FILE_SIZE) {
			request->OutOfMemory = true;
			::CloseHandle(hFile);
			return;
		}
		hFileBuffer  = ::GlobalAlloc(GMEM_MOVEABLE, nFileSize);
		pBuffer = (hFileBuffer == NULL) ? NULL : ::GlobalLock(hFileBuffer);
		if (pBuffer == NULL) {
			if (hFileBuffer) ::GlobalFree(hFileBuffer);
			request->OutOfMemory = true;
			::CloseHandle(hFile);
			return;
		}
		unsigned int nNumBytesRead;
		if (::ReadFile(hFile, pBuffer, nFileSize, (LPDWORD) &nNumBytesRead, NULL) && nNumBytesRead == nFileSize) {
			if (CSettingsProvider::This().ForceGDIPlus() || CSettingsProvider::This().UseEmbeddedColorProfiles()) {
				IStream* pStream = NULL;
				if (::CreateStreamOnHGlobal(hFileBuffer, FALSE, &pStream) == S_OK) {
					Gdiplus::Bitmap* pBitmap = Gdiplus::Bitmap::FromStream(pStream, CSettingsProvider::This().UseEmbeddedColorProfiles());
                    bool isOutOfMemory, isAnimatedGIF;
					request->Image = ConvertGDIPlusBitmapToJPEGImage(pBitmap, 0, Helpers::FindEXIFBlock(pBuffer, nFileSize),
						Helpers::CalculateJPEGFileHash(pBuffer, nFileSize), isOutOfMemory, isAnimatedGIF);
					request->OutOfMemory = request->Image == NULL && isOutOfMemory;
					if (request->Image != NULL) {
						request->Image->SetJPEGComment(Helpers::GetJPEGComment(pBuffer, nFileSize));
					}
					pStream->Release();
					delete pBitmap;
				} else {
					request->OutOfMemory = true;
				}
			} else {
				int nWidth, nHeight, nBPP;
                TJSAMP eChromoSubSampling;
				bool bOutOfMemory;
				// int nTicks = ::GetTickCount();

                void* pPixelData = TurboJpeg::ReadImage(nWidth, nHeight, nBPP, eChromoSubSampling, bOutOfMemory, pBuffer, nFileSize);
				
				/*
				TCHAR buffer[20];
				_stprintf_s(buffer, 20, _T("%d"), ::GetTickCount() - nTicks);
				::MessageBox(NULL, CString(_T("Elapsed ticks: ")) + buffer, _T("Time"), MB_OK);
				*/

				// Color and b/w JPEG is supported
				if (pPixelData != NULL && (nBPP == 3 || nBPP == 1)) {
					request->Image = new CJPEGImage(nWidth, nHeight, pPixelData, 
						Helpers::FindEXIFBlock(pBuffer, nFileSize), nBPP, 
						Helpers::CalculateJPEGFileHash(pBuffer, nFileSize), IF_JPEG, false, 0, 1, 0);
					request->Image->SetJPEGComment(Helpers::GetJPEGComment(pBuffer, nFileSize));
                    request->Image->SetJPEGChromoSampling(eChromoSubSampling);
				} else if (bOutOfMemory) {
					request->OutOfMemory = true;
				} else {
					// failed, try GDI+
					delete[] pPixelData;
					ProcessReadGDIPlusRequest(request);
				}
			}
		}
	} catch (...) {
		delete request->Image;
		request->Image = NULL;
	}
	::CloseHandle(hFile);
	if (pBuffer) ::GlobalUnlock(hFileBuffer);
	if (hFileBuffer) ::GlobalFree(hFileBuffer);
}

void CImageLoadThread::ProcessReadBMPRequest(CRequest * request) {
	bool bOutOfMemory;
	request->Image = CReaderBMP::ReadBmpImage(request->FileName, bOutOfMemory);
	if (bOutOfMemory) {
		request->OutOfMemory = true;
	} else if (request->Image == NULL) {
		// probabely one of the bitmap formats that can not be read directly, try with GDI+
		ProcessReadGDIPlusRequest(request);
	}
}

void CImageLoadThread::ProcessReadTGARequest(CRequest * request) {
    bool bOutOfMemory;
	request->Image = CReaderTGA::ReadTgaImage(request->FileName, CSettingsProvider::This().ColorBackground(), bOutOfMemory);
	if (bOutOfMemory) {
		request->OutOfMemory = true;
	}
}

__declspec(dllimport) int Webp_Dll_GetInfo(const uint8* data, size_t data_size, int* width, int* height);
__declspec(dllimport) uint8* Webp_Dll_DecodeBGRInto(const uint8* data, uint32 data_size, uint8* output_buffer, int output_buffer_size, int output_stride);

void CImageLoadThread::ProcessReadWEBPRequest(CRequest * request) {
	HANDLE hFile = ::CreateFile(request->FileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		return;
	}

	char* pBuffer = NULL;
	try {
		// Don't read too huge files...
		unsigned int nFileSize = ::GetFileSize(hFile, NULL);
		if (nFileSize > MAX_WEBP_FILE_SIZE) {
			request->OutOfMemory = true;
			::CloseHandle(hFile);
			return;
		}
		unsigned int nNumBytesRead;
		pBuffer = new(std::nothrow) char[nFileSize];
		if (pBuffer == NULL) {
			request->OutOfMemory = true;
			::CloseHandle(hFile);
			return;
		}
		if (::ReadFile(hFile, pBuffer, nFileSize, (LPDWORD) &nNumBytesRead, NULL) && nNumBytesRead == nFileSize) {
			int nWidth, nHeight;
			if (Webp_Dll_GetInfo((uint8*)pBuffer, nFileSize, &nWidth, &nHeight)) {
                if (nWidth <= MAX_IMAGE_DIMENSION && nHeight <= MAX_IMAGE_DIMENSION) {
				    if ((double)nWidth * nHeight <= MAX_IMAGE_PIXELS) {
					    int nStride = Helpers::DoPadding(nWidth*3, 4);
					    uint8* pPixelData = new(std::nothrow) unsigned char[nStride * nHeight];
					    if (pPixelData != NULL) {
						    if (Webp_Dll_DecodeBGRInto((uint8*)pBuffer, nFileSize, pPixelData, nStride * nHeight, nStride)) {
							    request->Image = new CJPEGImage(nWidth, nHeight, pPixelData, NULL, 3, 0, IF_WEBP, false, 0, 1, 0);
						    } else {
							    delete[] pPixelData;
						    }
					    } else {
						    request->OutOfMemory = true;
					    }
				    } else {
					    request->OutOfMemory = true;
				    }
                }
			}
		}
	} catch (...) {
		delete request->Image;
		request->Image = NULL;
	}
	::CloseHandle(hFile);
	delete[] pBuffer;
}

void CImageLoadThread::ProcessReadRAWRequest(CRequest * request) {
	bool bOutOfMemory = false;
	try {
		request->Image = CReaderRAW::ReadRawImage(request->FileName, bOutOfMemory);
	} catch (...) {
		delete request->Image;
		request->Image = NULL;
	}
	if (bOutOfMemory) {
		request->OutOfMemory = true;
	}
}

void CImageLoadThread::ProcessReadGDIPlusRequest(CRequest * request) {
	const wchar_t* sFileName;
#ifdef _UNICODE
	sFileName = (const wchar_t*)request->FileName;
#else
	wchar_t buff[MAX_PATH];
	size_t nDummy;
	mbstowcs_s(&nDummy, buff, MAX_PATH, (const char*)request->FileName, request->FileName.GetLength());
	sFileName = (const wchar_t*)buff;
#endif

    Gdiplus::Bitmap* pBitmap = NULL;
    if (sFileName == m_sLastFileName) {
        pBitmap = m_pLastBitmap;
    } else {
        DeleteCachedGDIBitmap();
        m_pLastBitmap = pBitmap = new Gdiplus::Bitmap(sFileName, CSettingsProvider::This().UseEmbeddedColorProfiles());
        m_sLastFileName = sFileName;
    }
    bool isOutOfMemory, isAnimatedGIF;
	request->Image = ConvertGDIPlusBitmapToJPEGImage(pBitmap, request->FrameIndex, NULL, 0, isOutOfMemory, isAnimatedGIF);
	request->OutOfMemory = request->Image == NULL && isOutOfMemory;
    if (!isAnimatedGIF) {
        DeleteCachedGDIBitmap();
    }
}

static unsigned char* alloc(int sizeInBytes) {
    return new(std::nothrow) unsigned char[sizeInBytes];
}

static void dealloc(unsigned char* buffer) {
    delete[] buffer;
}

typedef unsigned char* Allocator(int sizeInBytes);
typedef void Deallocator(unsigned char* buffer);

__declspec(dllimport) unsigned char* __stdcall LoadImageWithWIC(LPCWSTR fileName, Allocator* allocator, Deallocator* deallocator,
    unsigned int* width, unsigned int* height);

void CImageLoadThread::ProcessReadWICRequest(CRequest* request) {
	const wchar_t* sFileName;
#ifdef _UNICODE
	sFileName = (const wchar_t*)request->FileName;
#else
	wchar_t buff[MAX_PATH];
	size_t nDummy;
	mbstowcs_s(&nDummy, buff, MAX_PATH, (const char*)request->FileName, request->FileName.GetLength());
	sFileName = (const wchar_t*)buff;
#endif

    try {
        uint32 nWidth, nHeight;
        unsigned char* pDIB = LoadImageWithWIC(sFileName, &alloc, &dealloc, &nWidth, &nHeight);
        if (pDIB != NULL) {
            request->Image = new CJPEGImage(nWidth, nHeight, pDIB, NULL, 4, 0, IF_WIC, false, 0, 1, 0);
        }
    } catch (...) {
        // fatal error in WIC
    }
}

bool CImageLoadThread::ProcessImageAfterLoad(CRequest * request) {
	// set process parameters depending on filename
	request->Image->SetFileDependentProcessParams(request->FileName, &(request->ProcessParams));

	// First do rotation, this maybe modifies the width and height
	if (!request->Image->VerifyRotation(request->ProcessParams.RotationParams)) {
		return false;
	}

	// Do nothing if processing after load turned off
	if (GetProcessingFlag(request->ProcessParams.ProcFlags, PFLAG_NoProcessingAfterLoad)) {
		return true;
	}

	int nWidth = request->Image->OrigWidth();
	int nHeight = request->Image->OrigHeight();

	double dZoom = request->ProcessParams.Zoom;
	CSize newSize;
	if (dZoom < 0.0) {
		newSize = Helpers::GetImageRect(nWidth, nHeight, 
			request->ProcessParams.TargetWidth, request->ProcessParams.TargetHeight, request->ProcessParams.AutoZoomMode, dZoom);
	} else {
		newSize = CSize((int)(nWidth*dZoom + 0.5), (int)(nHeight*dZoom + 0.5));
	}

	newSize.cx = max(1, min(65535, newSize.cx));
	newSize.cy = max(1, min(65535, newSize.cy)); // max size must not be bigger than this after zoom

	// clip to target rectangle
	CSize clippedSize(min(request->ProcessParams.TargetWidth, newSize.cx), 
		min(request->ProcessParams.TargetHeight, newSize.cy));

	LimitOffsets(request->ProcessParams.Offsets, CSize(request->ProcessParams.TargetWidth, request->ProcessParams.TargetHeight), newSize);

	// this will process the image
	CPoint offsetInImage = request->Image->ConvertOffset(newSize, clippedSize, request->ProcessParams.Offsets);
	return NULL != request->Image->GetDIB(newSize, clippedSize, offsetInImage,
		request->ProcessParams.ImageProcParams, request->ProcessParams.ProcFlags);
}
