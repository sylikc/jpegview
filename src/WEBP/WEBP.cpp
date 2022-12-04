// WEBP.cpp

#include "stdafx.h"
#include "decode.h"
#include "encode.h"
#include "demux.h"
#include <stdlib.h>

__declspec(dllexport) int Webp_Dll_GetInfo(const uint8_t* data, size_t data_size, int* width, int* height)
{
	return WebPGetInfo(data, data_size, width, height);
}

__declspec(dllexport) bool Webp_Dll_HasAnimation(const uint8_t* data, uint32_t data_size)
{
	WebPBitstreamFeatures features;
	if (WebPGetFeatures(data, data_size, &features) != VP8_STATUS_OK)
		return false;
	return features.has_animation;
}

// These are necessary to properly animate frames without playing the entire file for each one
WebPAnimDecoder* cached_webp_decoder = NULL;
WebPData cached_webp_data = {0};

__declspec(dllexport) void Webp_Dll_AnimDecoderDelete()
{
	WebPAnimDecoderDelete(cached_webp_decoder);
	cached_webp_decoder = NULL;
	WebPDataClear(&cached_webp_data);
}
__declspec(dllexport) uint8_t* Webp_Dll_AnimDecodeBGRAInto(const uint8_t* data, uint32_t data_size, uint8_t* output_buffer, int output_buffer_size, int* nFrameCount, int* nFrameTimeMs)
{
	if (cached_webp_decoder && cached_webp_data.bytes) {
		;
	}
	else {
		Webp_Dll_AnimDecoderDelete();
		WebPAnimDecoderOptions anim_config;
		WebPAnimDecoderOptionsInit(&anim_config);
		anim_config.color_mode = MODE_BGRA;
		uint8_t* cached_webp_bytes = new uint8_t[data_size];
		memcpy(cached_webp_bytes, data, data_size);
		cached_webp_data.bytes = cached_webp_bytes;
		cached_webp_data.size = data_size;
		cached_webp_decoder = WebPAnimDecoderNew(&cached_webp_data, &anim_config);
	}
	WebPAnimDecoder* decoder = cached_webp_decoder;
	WebPData webp_data = cached_webp_data;
	
	if (decoder == NULL)
		return NULL;
	
	// Get animation info
	WebPIterator iter;
	WebPDemuxer* demux = WebPDemux(&webp_data); // Add to cache?
	if (WebPDemuxGetFrame(demux, 1, &iter)) {
		*nFrameCount = max(iter.num_frames, 1);
		*nFrameTimeMs = max(iter.duration, 0);
	}
	else
		return NULL;
	WebPDemuxReleaseIterator(&iter); 
	WebPDemuxDelete(demux);

	// Decode frame
	int timestamp;
	uint8_t* buf;
	if (!WebPAnimDecoderHasMoreFrames(decoder))
		WebPAnimDecoderReset(decoder);
	if (!WebPAnimDecoderHasMoreFrames(decoder) || !WebPAnimDecoderGetNext(decoder, &buf, &timestamp))
		return NULL;
	memcpy(output_buffer, buf, output_buffer_size);
	return output_buffer;
}

__declspec(dllexport) uint8_t* Webp_Dll_DecodeBGRInto(const uint8_t* data, uint32_t data_size, uint8_t* output_buffer, int output_buffer_size, int output_stride)
{
	return WebPDecodeBGRInto(data, data_size, output_buffer, output_buffer_size, output_stride);
}

__declspec(dllexport) uint8_t* Webp_Dll_DecodeBGRAInto(const uint8_t* data, uint32_t data_size, uint8_t* output_buffer, int output_buffer_size, int output_stride)
{
	return WebPDecodeBGRAInto(data, data_size, output_buffer, output_buffer_size, output_stride);
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