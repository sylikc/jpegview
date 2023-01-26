#include "stdafx.h"

#include "WEBPWrapper.h"
#include "webp/decode.h"
#include "webp/encode.h"
#include "webp/demux.h"
#include "MaxImageDef.h"

// These are necessary to properly animate frames without playing the entire file for each one
WebPAnimDecoder* cached_webp_decoder = NULL;
WebPData cached_webp_data = { 0 };
int cached_webp_prev_frame_timestamp = 0;
int cached_webp_width = 0;
int cached_webp_height = 0;

void* WebpReader::ReadImage(int& width,
	int& height,
	int& nchannels,
	bool& has_animation,
	int& frame_count,
	int& frame_time,
	bool& outOfMemory,
	const void* buffer,
	int sizebytes)
{
	uint8* pPixelData = NULL;
	WebPBitstreamFeatures features;
	width = height = 0;
	nchannels = 4;
	outOfMemory = false;
	if (!cached_webp_decoder || !cached_webp_data.bytes) {
		if (!WebPGetInfo((const uint8_t*)buffer, sizebytes, &width, &height))
			return NULL;
		if (width > MAX_IMAGE_DIMENSION || height > MAX_IMAGE_DIMENSION)
			return NULL;
		if (WebPGetFeatures((const uint8_t*)buffer, sizebytes, &features) != VP8_STATUS_OK)
			return NULL;
		if ((double)width * height > MAX_IMAGE_PIXELS) {
			outOfMemory = true;
			return NULL;
		}
		has_animation = features.has_animation;
		if (!has_animation) {
			int nStride = width * nchannels;
			int size = height * nStride;
			pPixelData = new(std::nothrow) unsigned char[size];
			if (pPixelData == NULL) {
				outOfMemory = true;
				return NULL;
			}
			WebPDecodeBGRAInto((const uint8_t*)buffer, sizebytes, pPixelData, size, nStride);
			return pPixelData;
		}
	
		// Cache WebP data and decoder to keep track of where we are in the file
		DeleteCache();
		WebPAnimDecoderOptions anim_config;
		WebPAnimDecoderOptionsInit(&anim_config);
		anim_config.color_mode = MODE_BGRA;
		uint8_t* cached_webp_bytes = new uint8_t[sizebytes];
		memcpy(cached_webp_bytes, buffer, sizebytes);
		cached_webp_data.bytes = cached_webp_bytes;
		cached_webp_data.size = sizebytes;
		cached_webp_decoder = WebPAnimDecoderNew(&cached_webp_data, &anim_config);
		cached_webp_width = width;
		cached_webp_height = height;
	}
	WebPAnimDecoder* decoder = cached_webp_decoder;
	WebPData webp_data = cached_webp_data;
	width = cached_webp_width;
	height = cached_webp_height;

	if (decoder == NULL)
		return NULL;

	// Decode frame
	int timestamp;
	uint8_t* buf;
	if (!WebPAnimDecoderHasMoreFrames(decoder))
		WebPAnimDecoderReset(decoder);
	if (!WebPAnimDecoderGetNext(decoder, &buf, &timestamp))
		return NULL;

	// Set frametime and frame count
	WebPAnimInfo anim_info;
	WebPAnimDecoderGetInfo(decoder, &anim_info);
	frame_count = max(anim_info.frame_count, 1);
	timestamp = max(timestamp, 0);
	if (timestamp < cached_webp_prev_frame_timestamp)
		cached_webp_prev_frame_timestamp = 0;
	frame_time = timestamp - cached_webp_prev_frame_timestamp;
	cached_webp_prev_frame_timestamp = timestamp;

	pPixelData = new(std::nothrow) unsigned char[width * height * nchannels];
	if (pPixelData == NULL) {
		outOfMemory = true;
		return NULL;
	}
	// Copy frame to output buffer
	memcpy(pPixelData, buf, width * height * nchannels);
	return pPixelData;

}

void WebpReader::DeleteCache() {
	WebPAnimDecoderDelete(cached_webp_decoder);
	cached_webp_decoder = NULL;
	WebPDataClear(&cached_webp_data);
	cached_webp_prev_frame_timestamp = 0;
	cached_webp_width = 0;
	cached_webp_height = 0;
}
