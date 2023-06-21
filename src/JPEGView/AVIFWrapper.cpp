#include "stdafx.h"

#include "AVIFWrapper.h"
#include "avif/avif.h"
#include "MaxImageDef.h"
#include "ICCProfileTransform.h"

struct AvifReader::avif_cache {
	avifDecoder* decoder;
	avifRGBImage rgb;
	uint8_t* data;
	size_t data_size;
	void* transform;
};

AvifReader::avif_cache AvifReader::cache = { 0 };

void* AvifReader::ReadImage(int& width,
	int& height,
	int& nchannels,
	bool& has_animation,
	int frame_index,
	int& frame_count,
	int& frame_time,
	void*& exif_chunk,
	bool& outOfMemory,
	const void* buffer,
	int sizebytes)
{
	outOfMemory = false;
	width = height = 0;
	nchannels = 4;
	has_animation = false;
	exif_chunk = NULL;

	avifResult result;
	int nthreads = 256; // sets maximum number of active threads allowed for libavif, default is 1

	// Cache animations
	if (cache.decoder == NULL) {
		cache.data = (uint8_t*)malloc(sizebytes);
		if (cache.data == NULL) {
			outOfMemory = true;
			return NULL;
		}
		memcpy(cache.data, buffer, sizebytes);
		cache.decoder = avifDecoderCreate();
		cache.decoder->maxThreads = nthreads;
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
	cache.rgb.maxThreads = nthreads;

	width = cache.rgb.width;
	height = cache.rgb.height;
	has_animation = cache.decoder->imageCount > 1;
	frame_count = cache.decoder->imageCount;
	frame_time = (int)(cache.decoder->imageTiming.duration * 1000.0);

	size_t size = width * nchannels * height;
	cache.rgb.pixels = new(std::nothrow) unsigned char[size];
	if (cache.rgb.pixels == NULL) {
		outOfMemory = true;
		return NULL;
	}
	cache.rgb.rowBytes = width * nchannels;
	result = avifImageYUVToRGB(cache.decoder->image, &cache.rgb);
	if (result != AVIF_RESULT_OK) {
		delete[] cache.rgb.pixels;
		DeleteCache();
		return NULL;
	}
	avifRWData icc = cache.decoder->image->icc;
	if (cache.transform == NULL)
		cache.transform = ICCProfileTransform::CreateTransform(icc.data, icc.size, ICCProfileTransform::FORMAT_BGRA);
	ICCProfileTransform::DoTransform(cache.transform, cache.rgb.pixels, cache.rgb.pixels, width, height);

	avifRWData exif = cache.decoder->image->exif;
	if (exif.size > 8 && exif.size < 65528 && exif.data != NULL) {
		exif_chunk = malloc(exif.size + 10);
		if (exif_chunk != NULL) {
			memcpy(exif_chunk, "\xFF\xE1\0\0Exif\0\0", 10);
			*((unsigned short*)exif_chunk + 1) = _byteswap_ushort(exif.size + 8);
			memcpy((uint8_t*)exif_chunk + 10, exif.data, exif.size);
		}
	}

	void* pPixelData = cache.rgb.pixels;
	if (!has_animation)
		DeleteCache();

	return pPixelData;
}

void AvifReader::DeleteCache() {
	if (cache.decoder)
		avifDecoderDestroy(cache.decoder);
	free(cache.data);
	ICCProfileTransform::DeleteTransform(cache.transform);
	cache = { 0 };
}
