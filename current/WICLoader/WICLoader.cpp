// Exported methods of the WICLoader DLL

#include "stdafx.h"

#pragma comment(lib, "WindowsCodecs.lib")


#define IFS(fn)                \
{                              \
    if (SUCCEEDED(hr)) {       \
        hr = (fn);             \
    }                          \
}

#define RELEASE_INTERFACE(pi)  \
{                              \
    if (pi) {                  \
        pi->Release();         \
        pi = NULL;             \
    }                          \
}


// forward declaration
static HRESULT CopyWICBitmapToBuffer(IWICBitmapSource *piBitmapSource, byte* pBuffer);

typedef byte* Allocator(int sizeInBytes);
typedef void Deallocator(byte* buffer);

// Checks if WIC is installed by asking for the WIC imaging factory.
// WIC is integral part of Windows Vista and Windows 7 and can be installed for XP
__declspec(dllexport) bool __stdcall WICPresent(void) {
    bool wicPresent = false;
    IWICImagingFactory *piImagingFactory = NULL;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (LPVOID*) &piImagingFactory);
    if (SUCCEEDED(hr)) {
        wicPresent = true;
    }
    RELEASE_INTERFACE(piImagingFactory);

    return wicPresent;
}

// Loads the given image file with WIC. Returns NULL if loading fails. Memory is allocated and deallocated by the
// given function pointers be decouple from CRT version
__declspec(dllexport) byte* __stdcall LoadImageWithWIC(LPCWSTR fileName, Allocator* allocator, Deallocator* deallocator,
    unsigned int* width, unsigned int* height) {

    IWICImagingFactory *piImagingFactory = NULL;
    IWICBitmapDecoder *piDecoder = NULL;
    IWICBitmapFrameDecode *piBitmapFrame = NULL;
    UINT frameCount = 0;
    HRESULT hr = S_OK;

    IFS(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (LPVOID*) &piImagingFactory));
    IFS(piImagingFactory->CreateDecoderFromFilename(fileName, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &piDecoder));
    IFS(piDecoder->GetFrameCount(&frameCount));

    byte* bitmapBuffer = NULL;

    if (frameCount > 0) {
        IFS(piDecoder->GetFrame(0, &piBitmapFrame));
        if (SUCCEEDED(hr)) {
            piBitmapFrame->GetSize(width, height);
            bitmapBuffer = allocator(*width * *height * 4);
            if (bitmapBuffer != NULL) {
                hr = CopyWICBitmapToBuffer(piBitmapFrame, bitmapBuffer);
                if (!SUCCEEDED(hr)) {
                    deallocator(bitmapBuffer);
                    bitmapBuffer = NULL;
                }
            }
            piBitmapFrame->Release();
            piBitmapFrame = NULL;
        }
    }

    RELEASE_INTERFACE(piDecoder);
    RELEASE_INTERFACE(piImagingFactory);

    return bitmapBuffer;
}

static HRESULT CopyWICBitmapToBuffer(IWICBitmapSource *piBitmapSource, byte* targetBuffer) {
    HRESULT hr = S_OK;
    UINT width = 0;
    UINT height = 0;
    IWICImagingFactory *piImagingFactory = NULL;
    IWICFormatConverter *piFormatConverter = NULL;

    IFS(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (LPVOID*) &piImagingFactory));
    IFS(piImagingFactory->CreateFormatConverter(&piFormatConverter));
    IFS(piFormatConverter->Initialize(piBitmapSource, GUID_WICPixelFormat32bppBGR, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom));
    IFS(piFormatConverter->GetSize(&width, &height));

    if (SUCCEEDED(hr)) {
        UINT stride = width * 4;
        WICRect rc = { 0, 0, width, height };
        IFS(piFormatConverter->CopyPixels(&rc, stride, stride * height, targetBuffer));
    }

    RELEASE_INTERFACE(piFormatConverter);
    RELEASE_INTERFACE(piImagingFactory);

    return hr;
}


 
