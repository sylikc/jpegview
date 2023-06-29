#include "stdafx.h"

#include "WEBPWrapper.h"
#include "webp/decode.h"
#include "webp/encode.h"
#include "webp/demux.h"
#include "MaxImageDef.h"
#include "Helpers.h"
#include "ICCProfileTransform.h"

struct WebpReaderWriter::webp_cache {
	WebPAnimDecoder* decoder;
	WebPData data;
	int prev_frame_timestamp;
	int width;
	int height;
	void* transform;
};

WebpReaderWriter::webp_cache WebpReaderWriter::cache = { 0 };

void* WebpReaderWriter::ReadImage(int& width,
	int& height,
	int& nchannels,
	bool& has_animation,
	int& frame_count,
	int& frame_time,
	void*& exif_chunk,
	bool& outOfMemory,
	const void* buffer,
	int sizebytes)
{
	uint8* pPixelData = NULL;
	WebPBitstreamFeatures features;
	width = height = 0;
	nchannels = 4;
	outOfMemory = false;
	exif_chunk = NULL;

	if (!cache.decoder || !cache.data.bytes) {
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

		// Check for ICCP chunk
		WebPData data;
		data.bytes = (const uint8_t*)buffer;
		data.size = sizebytes;
		WebPDemuxer* demuxer = WebPDemux(&data);
		WebPChunkIterator chunk_iter;
		void* transform = NULL;
		
		if (WebPDemuxGetChunk(demuxer, "ICCP", 1, &chunk_iter))
			transform = ICCProfileTransform::CreateTransform(chunk_iter.chunk.bytes, chunk_iter.chunk.size, ICCProfileTransform::FORMAT_BGRA);
		WebPDemuxReleaseChunkIterator(&chunk_iter);
		if (WebPDemuxGetChunk(demuxer, "EXIF", 1, &chunk_iter)) {
			WebPData exif = chunk_iter.chunk;
			if (exif.size > 8 && exif.size < 65528 && exif.bytes != NULL) {
				exif_chunk = malloc(exif.size + 10);
				if (exif_chunk != NULL) {
					memcpy(exif_chunk, "\xFF\xE1\0\0Exif\0\0", 10);
					*((unsigned short*)exif_chunk + 1) = _byteswap_ushort(exif.size + 8);
					memcpy((uint8_t*)exif_chunk + 10, exif.bytes, exif.size);
				}
			}
		}
		WebPDemuxReleaseChunkIterator(&chunk_iter);
		WebPDemuxDelete(demuxer);

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

			// ICCP transform in place
			ICCProfileTransform::DoTransform(transform, pPixelData, pPixelData, width, height);
			ICCProfileTransform::DeleteTransform(transform);

			return pPixelData;
		}
	
		// Cache WebP data and decoder to keep track of where we are in the file
		DeleteCache();
		WebPAnimDecoderOptions anim_config;
		WebPAnimDecoderOptionsInit(&anim_config);
		anim_config.color_mode = MODE_BGRA;
		uint8_t* cached_webp_bytes = new uint8_t[sizebytes];
		memcpy(cached_webp_bytes, buffer, sizebytes);
		cache.data.bytes = cached_webp_bytes;
		cache.data.size = sizebytes;
		cache.decoder = WebPAnimDecoderNew(&cache.data, &anim_config);
		cache.width = width;
		cache.height = height;
		cache.transform = transform;
	}
	WebPAnimDecoder* decoder = cache.decoder;
	WebPData webp_data = cache.data;
	width = cache.width;
	height = cache.height;

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
	if (timestamp < cache.prev_frame_timestamp)
		cache.prev_frame_timestamp = 0;
	frame_time = timestamp - cache.prev_frame_timestamp;
	cache.prev_frame_timestamp = timestamp;

	pPixelData = new(std::nothrow) unsigned char[width * height * nchannels];
	if (pPixelData == NULL) {
		outOfMemory = true;
		return NULL;
	}

	// Try copying with ICCP transform
	if (!ICCProfileTransform::DoTransform(cache.transform, buf, pPixelData, width, height)) {
		// Copy frame to output buffer directly otherwise
		memcpy(pPixelData, buf, width * height * nchannels);
	}

	return pPixelData;

}

void WebpReaderWriter::DeleteCache() {
	WebPAnimDecoderDelete(cache.decoder);
	WebPDataClear(&cache.data);
	ICCProfileTransform::DeleteTransform(cache.transform);
	cache = { 0 };
}

void* WebpReaderWriter::Compress(const void* source,
	int width,
	int height,
	size_t& len,
	int quality,
	bool lossless) {

	uint8* pOutput = NULL;
	if (lossless)
		len = WebPEncodeLosslessBGR((uint8*)source, width, height, Helpers::DoPadding(width * 3, 4), &pOutput);
	else
		len = WebPEncodeBGR((uint8*)source, width, height, Helpers::DoPadding(width * 3, 4), (float)quality, &pOutput);
	return pOutput;
}

void WebpReaderWriter::FreeMemory(void* pointer) {
	free(pointer);
}
