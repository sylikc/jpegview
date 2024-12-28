#include "stdafx.h"
#include "DDSWrapper.h"

#include "BcnDecode.h"
#include "SettingsProvider.h"

// Implementation adapted from https://github.com/python-pillow/Pillow/blob/main/src/PIL/DdsImagePlugin.py
// Tested with same .dds files as Pillow to replicate functionality
CJPEGImage* DDSWrapper::ReadImage(LPCTSTR strFileName, bool& bOutOfMemory)
{
    FILE* file = _tfopen(strFileName, _T("rb"));
    if (file == NULL) {
        return NULL;
    }

    // Skip magic number
    fseek(file, 4, SEEK_SET);

    // Read header
    DDS_HEADER header;
    fread(&header, sizeof(DDS_HEADER), 1, file);
    if (header.dwSize != 124) {
        fclose(file);
        return NULL;
    }

    uint8_t *pixelBuf = NULL;

    // Check pixel format flags and handle accordingly
    DWORD pfFlags = header.pixelFormat.dwFlags;
    if (pfFlags & DDS_PIXEL_FORMAT_FLAGS::PF_RGB || pfFlags & DDS_PIXEL_FORMAT_FLAGS::PF_YUV) {
        pixelBuf = HandleDDSRGB(file, &header, bOutOfMemory);

    } else if (pfFlags & DDS_PIXEL_FORMAT_FLAGS::PF_LUMINANCE) {
        pixelBuf = HandleLuminance(file, &header, bOutOfMemory);

    } else if (pfFlags & DDS_PIXEL_FORMAT_FLAGS::PF_PALETTEINDEXED8) {
        pixelBuf = HandlePaletteIndexed(file, &header, bOutOfMemory);

    } else if (pfFlags & DDS_PIXEL_FORMAT_FLAGS::PF_FOURCC) {
        if (header.pixelFormat.dwFourCC == DDS_FOURCC::FOURCC_DX10) {
            pixelBuf = HandleDXT10(file, &header, bOutOfMemory);
        } else {
            pixelBuf = HandleFourCC(file, &header, bOutOfMemory);
        }
    } else {
        fclose(file);
        return NULL;
    }

    if (pixelBuf == NULL) {
        fclose(file);
        return NULL;
    }

    fclose(file);

    // Fake alpha channel
    uint32* pImage32 = (uint32*)pixelBuf;
    for (int i = 0; i < header.dwWidth * header.dwHeight; i++)
        *pImage32++ = Helpers::AlphaBlendBackground(*pImage32, CSettingsProvider::This().ColorTransparency());

    return new CJPEGImage(
        header.dwWidth, header.dwHeight,
        (void*)pixelBuf, NULL, 4, 0, IF_DDS, false, 0, 1, 0, NULL, 0, 0);
}


// Takes a dw(RGBA)BitMask and determines
// - how many bits of padding are on the right, used for shifting pixel data
// - the maximum value possible with the mask, used for normalizing pixel data
void GetMaskPadding(uint32_t mask, uint32_t* offset, uint32_t* maxVal) {
    if (mask == 0) {
        return;
    }

    *offset = 0;
    *maxVal = 0;

    while (true) {
        // Shift the mask right then back left to remove 1 bit of padding
        uint32_t shifted_mask = mask >> (*offset + 1);
        uint32_t reconstructed_mask = shifted_mask << (*offset + 1);
        
        if (reconstructed_mask != mask) {
            *maxVal = mask >> *offset;
            break;
        }
        *offset += 1;
    }
}


// Handles uncompressed RGB(A) pixel data 
uint8_t* DDSWrapper::HandleDDSRGB(FILE* file, const DDS_HEADER* header, bool& bOutOfMemory) {
    uint32_t byteCount = header->pixelFormat.dwRGBBitCount / 8; // How many bits RGB(A) is total
    uint32_t inPixels = header->dwHeight * header->dwWidth;
    uint32_t outSize = header->dwHeight * header->dwWidth * 4; 
    uint8_t *dst = (uint8_t*)malloc(sizeof(uint8_t) * outSize);
    if (dst == NULL) {
        bOutOfMemory = true;
        return NULL;
    }

    // Remove padding from each mask
    uint32_t rOffset = 0, gOffset = 0, bOffset = 0, aOffset = 0;
    uint32_t rMaxVal = 0, gMaxVal = 0, bMaxVal = 0, aMaxVal = 0;
    GetMaskPadding(header->pixelFormat.dwRBitMask, &rOffset, &rMaxVal);
    GetMaskPadding(header->pixelFormat.dwGBitMask, &gOffset, &gMaxVal);
    GetMaskPadding(header->pixelFormat.dwBBitMask, &bOffset, &bMaxVal);
    GetMaskPadding(header->pixelFormat.dwABitMask, &aOffset, &aMaxVal);

    // Read each pixel data and apply masks
    for (uint32_t i = 0; i < inPixels; i++) {
        uint32_t value;
        fread(&value, byteCount, 1, file);

        uint32_t r = (value & header->pixelFormat.dwRBitMask) >> rOffset;
        uint32_t g = (value & header->pixelFormat.dwGBitMask) >> gOffset;
        uint32_t b = (value & header->pixelFormat.dwBBitMask) >> bOffset;
        uint32_t a = (value & header->pixelFormat.dwABitMask) >> aOffset;

        if (rMaxVal != 0) r = (r / (double)rMaxVal) * 255;
        if (gMaxVal != 0) g = (g / (double)gMaxVal) * 255;
        if (bMaxVal != 0) b = (b / (double)bMaxVal) * 255;
        if (aMaxVal != 0) a = (a / (double)aMaxVal) * 255;

        // Set BGR(A) in destination buffer
        dst[i * 4 + 0] = b;
        dst[i * 4 + 1] = g;
        dst[i * 4 + 2] = r;

        if (header->pixelFormat.dwFlags & DDS_PIXEL_FORMAT_FLAGS::PF_ALPHAPIXELS ||
            header->pixelFormat.dwFlags & DDS_PIXEL_FORMAT_FLAGS::PF_ALPHA
        ) {
            dst[i * 4 + 3] = a;
        } else {
            dst[i * 4 + 3] = 255;
        }
    }

    return dst;
}


// Handles uncompressed luminance pixel data
uint8_t* DDSWrapper::HandleLuminance(FILE* file, const DDS_HEADER* header, bool& bOutOfMemory) {
    uint32_t byteCount = header->pixelFormat.dwRGBBitCount / 8;
    // Handle unsupported pixel formats
    if (byteCount != 1 && byteCount != 2) {
        return NULL;
    }
    if (byteCount == 2 && !(header->pixelFormat.dwFlags & DDS_PIXEL_FORMAT_FLAGS::PF_ALPHAPIXELS || 
                            header->pixelFormat.dwFlags & DDS_PIXEL_FORMAT_FLAGS::PF_ALPHA)) {
        return NULL;
    }
    
    uint32_t inPixels = header->dwHeight * header->dwWidth;
    uint32_t outSize = header->dwHeight * header->dwWidth * 4; 
    uint8_t *dst = (uint8_t*)malloc(sizeof(uint8_t) * outSize);
    if (dst == NULL) {
        bOutOfMemory = true;
        return NULL;
    }
    
    // Remove padding from each mask
    uint32_t lOffset = 0, aOffset = 0;
    uint32_t lMaxVal = 0, aMaxVal = 0;
    GetMaskPadding(header->pixelFormat.dwRBitMask, &lOffset, &lMaxVal);
    GetMaskPadding(header->pixelFormat.dwABitMask, &aOffset, &aMaxVal);

    // Read each pixel data and apply masks
    for (uint32_t i = 0; i < inPixels; i++) {
        uint32_t value;
        fread(&value, byteCount, 1, file);
        
        uint32_t l;
        uint32_t a;
        if (header->pixelFormat.dwFlags & DDS_PIXEL_FORMAT_FLAGS::PF_ALPHAPIXELS ||
            header->pixelFormat.dwFlags & DDS_PIXEL_FORMAT_FLAGS::PF_ALPHA
        ) {
            l = (value & header->pixelFormat.dwRBitMask) >> lOffset;
            a = (value & header->pixelFormat.dwABitMask) >> aOffset;
            if (lMaxVal != 0) l = (l / (double)lMaxVal) * 255;
            if (aMaxVal != 0) a = (a / (double)aMaxVal) * 255;
        } else {
            l = value;
            a = 255;
        }
    
        // Set BGR(A) in destination buffer
        dst[i * 4 + 0] = l;
        dst[i * 4 + 1] = l;
        dst[i * 4 + 2] = l;
        dst[i * 4 + 3] = a;
    }

    return dst;
}


// Handles palette indexed pixel data
uint8_t* DDSWrapper::HandlePaletteIndexed(FILE* file, const DDS_HEADER* header, bool& bOutOfMemory) {
    // Read palette buf
    uint32_t paletteSize = 256 * 4;
    uint8_t *paletteBuf = (uint8_t*)malloc(sizeof(uint8_t) * paletteSize);
    if (paletteBuf == NULL) {
        bOutOfMemory = true;
        return NULL;
    }

    fread(paletteBuf, sizeof(uint8_t), paletteSize, file);

    // Read image data
    uint32_t inPixels = header->dwHeight * header->dwWidth;
    uint32_t outSize = header->dwHeight * header->dwWidth * 4;
    uint8_t *dst = (uint8_t*)malloc(sizeof(uint8_t) * outSize);
    if (dst == NULL) {
        free(paletteBuf);
        bOutOfMemory = true;
        return NULL;
    }

    for (uint32_t i = 0; i < inPixels; i++) {
        uint8_t index;
        fread(&index, sizeof(uint8_t), 1, file);

        dst[i * 4 + 0] = paletteBuf[index * 4 + 2];
        dst[i * 4 + 1] = paletteBuf[index * 4 + 1];
        dst[i * 4 + 2] = paletteBuf[index * 4 + 0];
        dst[i * 4 + 3] = paletteBuf[index * 4 + 3];
    }

    free(paletteBuf);
    return dst;
}


// Handles non-DXT10 compressed pixel data
uint8_t* DDSWrapper::HandleFourCC(FILE* file, const DDS_HEADER* header, bool& bOutOfMemory) {
    // Setup decoder state
    DecodeState state;
    state.outNumChannels = 4;
    state.xsize = header->dwWidth;
    state.ysize = header->dwHeight;
    state.xoff = 0;
    state.yoff = 0;
    state.x = 0;
    state.y = 0;

    DWORD fourCC = header->pixelFormat.dwFourCC;
    if (fourCC == DDS_FOURCC::FOURCC_DXT1) {
        state.n = 1;
        state.pixel_format = "DXT1";
        state.inNumChannels = 4;
        state.bytesPerBlock = 8;

    } else if (fourCC == DDS_FOURCC::FOURCC_DXT3) {
        state.n = 2;
        state.pixel_format = "DXT3";
        state.inNumChannels = 4;
        state.bytesPerBlock = 16;

    } else if (fourCC == DDS_FOURCC::FOURCC_DXT5) {
        state.n = 3;
        state.pixel_format = "DXT5";
        state.inNumChannels = 4;
        state.bytesPerBlock = 16;

    } else if (fourCC == DDS_FOURCC::FOURCC_BC4U || fourCC == DDS_FOURCC::FOURCC_ATI1) {
        state.n = 4;
        state.pixel_format = "BC4";
        state.inNumChannels = 1;
        state.bytesPerBlock = 8;

    } else if (fourCC == DDS_FOURCC::FOURCC_BC5S) {
        state.n = 5;
        state.pixel_format = "BC5S";
        state.inNumChannels = 2;
        state.bytesPerBlock = 16;

    } else if (fourCC == DDS_FOURCC::FOURCC_BC5U || fourCC == DDS_FOURCC::FOURCC_ATI2) {
        state.n = 5;
        state.pixel_format = "BC5";
        state.inNumChannels = 2;
        state.bytesPerBlock = 16;

    } else {
        return NULL;
    }

    // Setup buffers
    int compressedWidth = header->dwWidth / 4;
    int compressedHeight = header->dwHeight / 4;
    int compressedSize = compressedWidth * compressedHeight * state.bytesPerBlock;
    int outSize = header->dwHeight * header->dwWidth * state.outNumChannels;
    uint8_t *src = (uint8_t*)malloc(sizeof(uint8_t) * compressedSize);
    uint8_t *dst = (uint8_t*)malloc(sizeof(uint8_t) * outSize);
    
    if (src == NULL || dst == NULL) {
        bOutOfMemory = true;
        return NULL;
    }
    
    fread(src, sizeof(uint8_t), compressedSize, file);

    int ret = ImagingBcnDecode(src, dst, &state, compressedSize);
    if (ret != 0) {
        free(src);
        free(dst);
        return NULL;
    }
    
    free(src);
    return dst;
}


// Helper function for DXT10 uncompressed R8G8B8A8 pixel data
uint8_t* HandleDXT10_R8G8B8A(FILE* file, const DDS_HEADER* header, bool& bOutOfMemory) {
    // Raw loading
    int outSize = header->dwHeight * header->dwWidth * 4;
    uint8_t *dst = (uint8_t*)malloc(sizeof(uint8_t) * outSize);
    if (dst == NULL) {
        bOutOfMemory = true;
        return NULL;
    }
    
    fread(dst, sizeof(uint8_t), outSize, file);

    // Swap red and blue channels
    for (int i = 0; i < outSize; i += 4) {
        uint8_t temp = dst[i];
        dst[i] = dst[i + 2];
        dst[i + 2] = temp;
    }

    return dst;
}


// Handles compressed & uncompressed pixel data with DXT10 header
uint8_t* DDSWrapper::HandleDXT10(FILE* file, const DDS_HEADER* header, bool& bOutOfMemory) {
    DDS_DXT10_HEADER h10;
    fread(&h10, sizeof(DDS_DXT10_HEADER), 1, file);

    // Setup decoder state
    DecodeState state;
    state.outNumChannels = 4;
    state.xsize = header->dwWidth;
    state.ysize = header->dwHeight;
    state.xoff = 0;
    state.yoff = 0;
    state.x = 0;
    state.y = 0;

    if (h10.dxgiFormat == DDS_DXGI_FORMAT::DF_BC1_TYPELESS || h10.dxgiFormat == DDS_DXGI_FORMAT::DF_BC1_UNORM) {
        state.n = 1;
        state.pixel_format = "BC1";
        state.inNumChannels = 4;
        state.bytesPerBlock = 8;

    } else if (h10.dxgiFormat == DDS_DXGI_FORMAT::DF_BC4_TYPELESS || 
               h10.dxgiFormat == DDS_DXGI_FORMAT::DF_BC4_UNORM) {
        state.n = 4;
        state.pixel_format = "BC4";
        state.inNumChannels = 1;
        state.bytesPerBlock = 8;

    } else if (h10.dxgiFormat == DDS_DXGI_FORMAT::DF_BC5_TYPELESS || h10.dxgiFormat == DDS_DXGI_FORMAT::DF_BC5_UNORM) {
        state.n = 5;
        state.pixel_format = "BC5";
        state.inNumChannels = 2;
        state.bytesPerBlock = 16;

    } else if (h10.dxgiFormat == DDS_DXGI_FORMAT::DF_BC5_SNORM) {
        state.n = 5;
        state.pixel_format = "BC5S";
        state.inNumChannels = 2;
        state.bytesPerBlock = 16;

    } else if (h10.dxgiFormat == DDS_DXGI_FORMAT::DF_BC6H_UF16) {
        state.n = 6;
        state.pixel_format = "BC6H";
        state.inNumChannels = 3;
        state.bytesPerBlock = 16;

    } else if (h10.dxgiFormat == DDS_DXGI_FORMAT::DF_BC6H_SF16) {
        state.n = 6;
        state.pixel_format = "BC6HS";
        state.inNumChannels = 3;
        state.bytesPerBlock = 16;

    } else if (h10.dxgiFormat == DDS_DXGI_FORMAT::DF_BC7_TYPELESS ||
               h10.dxgiFormat == DDS_DXGI_FORMAT::DF_BC7_UNORM ||
               h10.dxgiFormat == DDS_DXGI_FORMAT::DF_BC7_UNORM_SRGB) {
        state.n = 7;
        state.pixel_format = "BC7";
        state.inNumChannels = 4;
        state.bytesPerBlock = 16;

    } else if (h10.dxgiFormat == DDS_DXGI_FORMAT::DF_R8G8B8A8_TYPELESS ||
               h10.dxgiFormat == DDS_DXGI_FORMAT::DF_R8G8B8A8_UNORM ||
               h10.dxgiFormat == DDS_DXGI_FORMAT::DF_R8G8B8A8_UNORM_SRGB) {
        return HandleDXT10_R8G8B8A(file, header, bOutOfMemory);

    } else {
        return NULL;
    }

    // Setup buffers
    int compressedWidth = header->dwWidth / 4;
    int compressedHeight = header->dwHeight / 4;
    int compressedSize = compressedWidth * compressedHeight * state.bytesPerBlock;
    int outSize = header->dwHeight * header->dwWidth * state.outNumChannels;
    uint8_t *src = (uint8_t*)malloc(sizeof(uint8_t) * compressedSize);
    uint8_t *dst = (uint8_t*)malloc(sizeof(uint8_t) * outSize);
    if (src == NULL || dst == NULL) {
        bOutOfMemory = true;
        return NULL;
    }
    
    fread(src, sizeof(uint8_t), compressedSize, file);

    int ret = ImagingBcnDecode(src, dst, &state, compressedSize);
    if (ret != 0) {
        free(src);
        free(dst);
        return NULL;
    }
    
    free(src);
    return dst;
}
