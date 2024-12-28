#pragma once

#include "JPEGImage.h"

// Struct/enum definitions taken from: https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dds-header

typedef enum DDS_PIXEL_FORMAT_FLAGS {
    PF_ALPHAPIXELS = 0x1,
    PF_ALPHA = 0x2,
    PF_FOURCC = 0x4,
    PF_PALETTEINDEXED8 = 0x20,
    PF_RGB = 0x40,
    PF_YUV = 0x200,
    PF_LUMINANCE = 0x20000
} DDS_PIXEL_FORMAT_FLAGS;


#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
                (static_cast<uint32_t>(static_cast<uint8_t>(ch0)) \
                | (static_cast<uint32_t>(static_cast<uint8_t>(ch1)) << 8) \
                | (static_cast<uint32_t>(static_cast<uint8_t>(ch2)) << 16) \
                | (static_cast<uint32_t>(static_cast<uint8_t>(ch3)) << 24))
#endif /* MAKEFOURCC */


// Taken from https://github.com/microsoft/DirectXTex/blob/main/DirectXTex/DDS.h
// Cross-referenced with https://github.com/python-pillow/Pillow/blob/main/src/PIL/DdsImagePlugin.py
typedef enum DDS_FOURCC {
    FOURCC_DXT1 = MAKEFOURCC('D', 'X', 'T', '1'),
    FOURCC_DXT2 = MAKEFOURCC('D', 'X', 'T', '2'),
    FOURCC_DXT3 = MAKEFOURCC('D', 'X', 'T', '3'),
    FOURCC_DXT4 = MAKEFOURCC('D', 'X', 'T', '4'),
    FOURCC_DXT5 = MAKEFOURCC('D', 'X', 'T', '5'),
    FOURCC_BC4U = MAKEFOURCC('B', 'C', '4', 'U'),
    FOURCC_BC4S = MAKEFOURCC('B', 'C', '4', 'S'),
    FOURCC_BC5U = MAKEFOURCC('B', 'C', '5', 'U'),
    FOURCC_BC5S = MAKEFOURCC('B', 'C', '5', 'S'),
    FOURCC_RGBG = MAKEFOURCC('R', 'G', 'B', 'G'),
    FOURCC_GRGB = MAKEFOURCC('G', 'R', 'G', 'B'),
    FOURCC_YUY2 = MAKEFOURCC('Y', 'U', 'Y', '2'),
    FOURCC_UYVY = MAKEFOURCC('U', 'Y', 'V', 'Y'),
    FOURCC_DX10 = MAKEFOURCC('D', 'X', '1', '0'),
    FOURCC_ATI1 = MAKEFOURCC('A', 'T', 'I', '1'),
    FOURCC_ATI2 = MAKEFOURCC('A', 'T', 'I', '2')
} DDS_FOURCC;


typedef enum DDS_DXGI_FORMAT {
  DF_UNKNOWN = 0,
  DF_R32G32B32A32_TYPELESS = 1,
  DF_R32G32B32A32_FLOAT = 2,
  DF_R32G32B32A32_UINT = 3,
  DF_R32G32B32A32_SINT = 4,
  DF_R32G32B32_TYPELESS = 5,
  DF_R32G32B32_FLOAT = 6,
  DF_R32G32B32_UINT = 7,
  DF_R32G32B32_SINT = 8,
  DF_R16G16B16A16_TYPELESS = 9,
  DF_R16G16B16A16_FLOAT = 10,
  DF_R16G16B16A16_UNORM = 11,
  DF_R16G16B16A16_UINT = 12,
  DF_R16G16B16A16_SNORM = 13,
  DF_R16G16B16A16_SINT = 14,
  DF_R32G32_TYPELESS = 15,
  DF_R32G32_FLOAT = 16,
  DF_R32G32_UINT = 17,
  DF_R32G32_SINT = 18,
  DF_R32G8X24_TYPELESS = 19,
  DF_D32_FLOAT_S8X24_UINT = 20,
  DF_R32_FLOAT_X8X24_TYPELESS = 21,
  DF_X32_TYPELESS_G8X24_UINT = 22,
  DF_R10G10B10A2_TYPELESS = 23,
  DF_R10G10B10A2_UNORM = 24,
  DF_R10G10B10A2_UINT = 25,
  DF_R11G11B10_FLOAT = 26,
  DF_R8G8B8A8_TYPELESS = 27,
  DF_R8G8B8A8_UNORM = 28,
  DF_R8G8B8A8_UNORM_SRGB = 29,
  DF_R8G8B8A8_UINT = 30,
  DF_R8G8B8A8_SNORM = 31,
  DF_R8G8B8A8_SINT = 32,
  DF_R16G16_TYPELESS = 33,
  DF_R16G16_FLOAT = 34,
  DF_R16G16_UNORM = 35,
  DF_R16G16_UINT = 36,
  DF_R16G16_SNORM = 37,
  DF_R16G16_SINT = 38,
  DF_R32_TYPELESS = 39,
  DF_D32_FLOAT = 40,
  DF_R32_FLOAT = 41,
  DF_R32_UINT = 42,
  DF_R32_SINT = 43,
  DF_R24G8_TYPELESS = 44,
  DF_D24_UNORM_S8_UINT = 45,
  DF_R24_UNORM_X8_TYPELESS = 46,
  DF_X24_TYPELESS_G8_UINT = 47,
  DF_R8G8_TYPELESS = 48,
  DF_R8G8_UNORM = 49,
  DF_R8G8_UINT = 50,
  DF_R8G8_SNORM = 51,
  DF_R8G8_SINT = 52,
  DF_R16_TYPELESS = 53,
  DF_R16_FLOAT = 54,
  DF_D16_UNORM = 55,
  DF_R16_UNORM = 56,
  DF_R16_UINT = 57,
  DF_R16_SNORM = 58,
  DF_R16_SINT = 59,
  DF_R8_TYPELESS = 60,
  DF_R8_UNORM = 61,
  DF_R8_UINT = 62,
  DF_R8_SNORM = 63,
  DF_R8_SINT = 64,
  DF_A8_UNORM = 65,
  DF_R1_UNORM = 66,
  DF_R9G9B9E5_SHAREDEXP = 67,
  DF_R8G8_B8G8_UNORM = 68,
  DF_G8R8_G8B8_UNORM = 69,
  DF_BC1_TYPELESS = 70,
  DF_BC1_UNORM = 71,
  DF_BC1_UNORM_SRGB = 72,
  DF_BC2_TYPELESS = 73,
  DF_BC2_UNORM = 74,
  DF_BC2_UNORM_SRGB = 75,
  DF_BC3_TYPELESS = 76,
  DF_BC3_UNORM = 77,
  DF_BC3_UNORM_SRGB = 78,
  DF_BC4_TYPELESS = 79,
  DF_BC4_UNORM = 80,
  DF_BC4_SNORM = 81,
  DF_BC5_TYPELESS = 82,
  DF_BC5_UNORM = 83,
  DF_BC5_SNORM = 84,
  DF_B5G6R5_UNORM = 85,
  DF_B5G5R5A1_UNORM = 86,
  DF_B8G8R8A8_UNORM = 87,
  DF_B8G8R8X8_UNORM = 88,
  DF_R10G10B10_XR_BIAS_A2_UNORM = 89,
  DF_B8G8R8A8_TYPELESS = 90,
  DF_B8G8R8A8_UNORM_SRGB = 91,
  DF_B8G8R8X8_TYPELESS = 92,
  DF_B8G8R8X8_UNORM_SRGB = 93,
  DF_BC6H_TYPELESS = 94,
  DF_BC6H_UF16 = 95,
  DF_BC6H_SF16 = 96,
  DF_BC7_TYPELESS = 97,
  DF_BC7_UNORM = 98,
  DF_BC7_UNORM_SRGB = 99,
  DF_AYUV = 100,
  DF_Y410 = 101,
  DF_Y416 = 102,
  DF_NV12 = 103,
  DF_P010 = 104,
  DF_P016 = 105,
  DF_420_OPAQUE = 106,
  DF_YUY2 = 107,
  DF_Y210 = 108,
  DF_Y216 = 109,
  DF_NV11 = 110,
  DF_AI44 = 111,
  DF_IA44 = 112,
  DF_P8 = 113,
  DF_A8P8 = 114,
  DF_B4G4R4A4_UNORM = 115,
  DF_P208 = 130,
  DF_V208 = 131,
  DF_V408 = 132,
  DF_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE,
  DF_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE,
  DF_FORCE_UINT = 0xffffffff
} DDS_DXGI_FORMAT;


typedef struct DDS_PIXELFORMAT {
  DWORD dwSize;
  DWORD dwFlags;
  DWORD dwFourCC;
  DWORD dwRGBBitCount;
  DWORD dwRBitMask;
  DWORD dwGBitMask;
  DWORD dwBBitMask;
  DWORD dwABitMask;
} DDS_PIXELFORMAT;


typedef struct DDS_HEADER {
  DWORD           dwSize;
  DWORD           dwFlags;
  DWORD           dwHeight;
  DWORD           dwWidth;
  DWORD           dwPitchOrLinearSize;
  DWORD           dwDepth;
  DWORD           dwMipMapCount;
  DWORD           dwReserved1[11];
  DDS_PIXELFORMAT pixelFormat;
  DWORD           dwCaps;
  DWORD           dwCaps2;
  DWORD           dwCaps3;
  DWORD           dwCaps4;
  DWORD           dwReserved2;
} DDS_HEADER;


typedef struct DDS_DXT10_HEADER {
  DWORD dxgiFormat;
  DWORD resourceDimension;
  UINT  miscFlag;
  UINT  arraySize;
  UINT  miscFlags2;
} DDS_DXT10_HEADER;


class DDSWrapper {
public:
    static CJPEGImage* ReadImage(LPCTSTR strFileName, bool& bOutOfMemory);

private:
    static uint8_t* DDSWrapper::HandleDDSRGB(FILE* file, const DDS_HEADER* header, bool& bOutOfMemory);
    static uint8_t* DDSWrapper::HandleLuminance(FILE* file, const DDS_HEADER* header, bool& bOutOfMemory);
    static uint8_t* DDSWrapper::HandlePaletteIndexed(FILE* file, const DDS_HEADER* header, bool& bOutOfMemory);
    static uint8_t* DDSWrapper::HandleFourCC(FILE* file, const DDS_HEADER* header, bool& bOutOfMemory);
    static uint8_t* HandleDXT10(FILE* file, const DDS_HEADER* header, bool& bOutOfMemory);
};

