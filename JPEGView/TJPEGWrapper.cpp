
#include "stdafx.h"
#include "TJPEGWrapper.h"
#include "libjpeg-turbo\include\turbojpeg.h"
#include "MaxImageDef.h"

void * TurboJpeg::ReadImage(int &width,
                       int &height,
                       int &nchannels,
                       TJSAMP &chromoSubsampling,
					   bool &outOfMemory,
                       const void *buffer,
                       int sizebytes)
{
    outOfMemory = false;
    width = height = 0;
    nchannels = 3;
    chromoSubsampling = TJSAMP_420;

    tjhandle hDecoder = tjInitDecompress();
    if (hDecoder == NULL) {
        return NULL;
    }

    unsigned char* pPixelData = NULL;
    int nSubSampling;
    int nResult = tjDecompressHeader2(hDecoder, (unsigned char*)buffer, sizebytes, &width, &height, &nSubSampling);
    if (nResult == 0){
        chromoSubsampling = (TJSAMP)nSubSampling;
		if (abs((double)width * height) > MAX_IMAGE_PIXELS) {
            outOfMemory = true;
        } else if (width <= MAX_IMAGE_DIMENSION && height <= MAX_IMAGE_DIMENSION) {
            pPixelData = new(std::nothrow) unsigned char[TJPAD(width * 3) * height];
            if (pPixelData != NULL) {
	            nResult = tjDecompress2(hDecoder, (unsigned char*)buffer, sizebytes, pPixelData, width, TJPAD(width * 3), height,
		            TJPF_BGR, 0);
                if (nResult != 0) {
                    delete[] pPixelData;
                    pPixelData = NULL;
                }
            } else {
                outOfMemory = true;
            }
        }
    }

    tjDestroy(hDecoder);

    return pPixelData;
}

void * TurboJpeg::Compress(const void *source,
                      int width,
                      int height,
                      int &len,
                      bool &outOfMemory,
                      int quality)
{
    outOfMemory = false;
    len = 0;
    tjhandle hEncoder = tjInitCompress();
    if (hEncoder == NULL) {
        return NULL;
    }

    unsigned char* pJPEGCompressed = NULL;
    unsigned long nCompressedLen = 0;
    int nResult = tjCompress2(hEncoder, (unsigned char*)source, width, TJPAD(width * 3), height, TJPF_BGR,
        &pJPEGCompressed, &nCompressedLen, TJSAMP_420, quality, 0);
    if (nResult != 0) {
        if (pJPEGCompressed != NULL) {
            tjFree(pJPEGCompressed);
            pJPEGCompressed = NULL;
        } else {
            outOfMemory = true;
        }
    }

    len = nCompressedLen;

    tjDestroy(hEncoder);

    return pJPEGCompressed;
}