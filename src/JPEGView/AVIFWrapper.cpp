#include "stdafx.h"

#include "AVIFWrapper.h"
#include "avif/avif.h"
#include "MaxImageDef.h"

struct AvifReader::avif_cache {
	avifDecoder* decoder;
	avifRGBImage rgb;
	uint8_t* data;
	size_t data_size;
};

AvifReader::avif_cache AvifReader::cache = { 0 };

void* AvifReader::ReadImage(int& width,
	int& height,
	int& nchannels,
	bool& has_animation,
	int frame_index,
	int& frame_count,
	int& frame_time,
	bool& outOfMemory,
	const void* buffer,
	int sizebytes)
{
	outOfMemory = false;
	width = height = 0;
	nchannels = 4;
	has_animation = false;
	unsigned char* pPixelData = NULL;

	avifResult result;

	// Cache animations
	if (cache.decoder == NULL) {
		cache.data = (uint8_t*)malloc(sizebytes);
		if (cache.data == NULL)
			return NULL;
		memcpy(cache.data, buffer, sizebytes);
		cache.decoder = avifDecoderCreate();
		result = avifDecoderSetIOMemory(cache.decoder, cache.data, sizebytes);
		if (result != AVIF_RESULT_OK) {
			DeleteCache();
			return NULL;
		}
		result = avifDecoderParse(cache.decoder);
		if (result != AVIF_RESULT_OK) {
			DeleteCache();
			return NULL;
		}
		cache.rgb = { 0 };
		
		cache.data_size = sizebytes;
	}
	
	// Decode a frame
	result = avifDecoderNthImage(cache.decoder, frame_index);
	if (result != AVIF_RESULT_OK) {
		DeleteCache();
		return NULL;
	}
	avifRGBImageSetDefaults(&cache.rgb, cache.decoder->image);
	cache.rgb.depth = 8;
	cache.rgb.format = AVIF_RGB_FORMAT_BGRA;
	avifRGBImageAllocatePixels(&cache.rgb);
	result = avifImageYUVToRGB(cache.decoder->image, &cache.rgb);
	if (result != AVIF_RESULT_OK) {
		DeleteCache();
		return NULL;
	}

	width = cache.rgb.width;
	height = cache.rgb.height;
	has_animation = cache.decoder->imageCount > 1;
	frame_count = cache.decoder->imageCount;
	frame_time = cache.decoder->imageTiming.duration * 1000;

	uint8_t* pixels = cache.rgb.pixels;
	cache.rgb.pixels = NULL; // must be set to NULL to avoid double free
	cache.rgb.rowBytes = 0;
	return pixels;
}

void AvifReader::DeleteCache() {
	avifRGBImageFreePixels(&cache.rgb);
	if (cache.decoder)
		avifDecoderDestroy(cache.decoder);
	free(cache.data);
	cache = { 0 };
}
