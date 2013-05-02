// WEBP.cpp : Definiert die exportierten Funktionen für die DLL-Anwendung.
//

#include "stdafx.h"
#include "decode.h"
#include "encode.h"
#include <stdlib.h>

__declspec(dllexport) int Webp_Dll_GetInfo(const uint8_t* data, size_t data_size, int* width, int* height)
{
    return WebPGetInfo(data, data_size, width, height);
}

__declspec(dllexport) uint8_t* Webp_Dll_DecodeBGRInto(const uint8_t* data, uint32_t data_size, uint8_t* output_buffer, int output_buffer_size, int output_stride)
{
    return WebPDecodeBGRInto(data, data_size, output_buffer, output_buffer_size, output_stride);
}

__declspec(dllexport) size_t Webp_Dll_EncodeBGRLossy(const uint8_t* bgr, int width, int height, int stride, float quality_factor, uint8_t** output)
{
    return WebPEncodeBGR(bgr, width, height, stride, quality_factor, output);
}

__declspec(dllexport) size_t Webp_Dll_EncodeBGRLossless(const uint8_t* bgr, int width, int height, int stride, uint8_t** output)
{
    return WebPEncodeLosslessBGR(bgr, width, height, stride, output);
}

__declspec(dllexport) void Webp_Dll_FreeMemory(void* pointer)
{
    free(pointer);
}