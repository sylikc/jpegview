#include "stdafx.h"

#include "JXLWrapper.h"
#include "jxl/decode.h"
#include "jxl/decode_cxx.h"
#include "jxl/resizable_parallel_runner.h"
#include "jxl/resizable_parallel_runner_cxx.h"
#include "MaxImageDef.h"
#include "ICCProfileTransform.h"

struct JxlReader::jxl_cache {
	JxlDecoderPtr decoder;
	JxlResizableParallelRunnerPtr runner;
	JxlBasicInfo info;
	uint8_t* data;
	size_t data_size;
	int prev_frame_timestamp;
	int width;
	int height;
	void* transform;
	std::vector<uint8_t> exif;
};

JxlReader::jxl_cache JxlReader::cache = { 0 };

// based on https://github.com/libjxl/libjxl/blob/main/examples/decode_oneshot.cc
// and https://github.com/libjxl/libjxl/blob/main/examples/decode_exif_metadata.cc
bool JxlReader::DecodeJpegXlOneShot(const uint8_t* jxl, size_t size, std::vector<uint8_t>* pixels, int& xsize,
	int& ysize, bool& have_animation, int& frame_count, int& frame_time, std::vector<uint8_t>* icc_profile, bool& outOfMemory) {

	if (cache.decoder.get() == NULL) {
		cache.runner = JxlResizableParallelRunnerMake(nullptr);

		cache.decoder = JxlDecoderMake(nullptr);
		if (JXL_DEC_SUCCESS !=
			JxlDecoderSubscribeEvents(cache.decoder.get(), JXL_DEC_BASIC_INFO |
				JXL_DEC_COLOR_ENCODING |
				JXL_DEC_BOX |
				JXL_DEC_FRAME |
				JXL_DEC_FULL_IMAGE)) {
			return false;
		}

		if (JXL_DEC_SUCCESS != JxlDecoderSetDecompressBoxes(cache.decoder.get(), JXL_TRUE)) {
			return false;
		}

		if (JXL_DEC_SUCCESS != JxlDecoderSetParallelRunner(cache.decoder.get(),
			JxlResizableParallelRunner,
			cache.runner.get())) {
			return false;
		}

		JxlDecoderSetInput(cache.decoder.get(), jxl, size);
		JxlDecoderCloseInput(cache.decoder.get());
		cache.data = (uint8_t*)jxl;
		cache.data_size = size;
	}

	JxlBasicInfo info;
	JxlPixelFormat format = { 4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0 };
	const constexpr size_t kChunkSize = 65536;
	size_t output_pos = 0;

	bool loop_check = false;
	for (;;) {
		JxlDecoderStatus status = JxlDecoderProcessInput(cache.decoder.get());

		if (status == JXL_DEC_ERROR) {
			return false;
		} else if (status == JXL_DEC_NEED_MORE_INPUT) {
			return false;
		} else if (status == JXL_DEC_BASIC_INFO) {
			if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(cache.decoder.get(), &info)) {
				return false;
			}
			cache.info = info;
			if (cache.info.xsize > MAX_IMAGE_DIMENSION || cache.info.ysize > MAX_IMAGE_DIMENSION)
				return false;
			if (abs((double)cache.info.xsize * cache.info.ysize) > MAX_IMAGE_PIXELS) {
				outOfMemory = true;
				return false;
			}
			JxlResizableParallelRunnerSetThreads(
				cache.runner.get(),
				JxlResizableParallelRunnerSuggestThreads(info.xsize, info.ysize));
		} else if (status == JXL_DEC_COLOR_ENCODING) {
			// Get the ICC color profile of the pixel data
			size_t icc_size;
			if (JXL_DEC_SUCCESS !=
				JxlDecoderGetICCProfileSize(
					cache.decoder.get(), JXL_COLOR_PROFILE_TARGET_DATA, &icc_size)) {
				return false;
			}
			icc_profile->resize(icc_size);
			if (JXL_DEC_SUCCESS != JxlDecoderGetColorAsICCProfile(
				cache.decoder.get(),
				JXL_COLOR_PROFILE_TARGET_DATA,
				icc_profile->data(), icc_profile->size())) {
				return false;
			}
		} else if (status == JXL_DEC_FRAME) {
			JxlFrameHeader header;
			JxlDecoderGetFrameHeader(cache.decoder.get(), &header);
			JxlAnimationHeader animation = cache.info.animation;
			if (animation.tps_numerator)
				frame_time = (int)(1000.0 * header.duration * animation.tps_denominator / animation.tps_numerator);
			else
				frame_time = 0;
		} else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
			size_t buffer_size;
			if (JXL_DEC_SUCCESS !=
				JxlDecoderImageOutBufferSize(cache.decoder.get(), &format, &buffer_size)) {
				return false;
			}
			if (buffer_size != cache.info.xsize * cache.info.ysize * 4) {
				return false;
			}
			pixels->resize(cache.info.xsize * cache.info.ysize * 4);
			void* pixels_buffer = (void*)pixels->data();
			size_t pixels_buffer_size = pixels->size() * sizeof(uint8_t);
			if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(cache.decoder.get(), &format,
				pixels_buffer,
				pixels_buffer_size)) {
				return false;
			}
		} else if (status == JXL_DEC_FULL_IMAGE) {
			// Full frame has been decoded

			xsize = cache.info.xsize;
			ysize = cache.info.ysize;
			have_animation = cache.info.have_animation;
			if (have_animation) {
				// TODO: Find a better way to indicate unknown frame count. JPEG XL images do not store number of frames.
				frame_count = 2;
			} else {
				frame_count = 1;
			}
			return true;
		} else if (status == JXL_DEC_SUCCESS) {
			// All decoding successfully finished, we are at the end of the file.
			// We must rewind the decoder to get a new frame.
			if (loop_check)
				return false; // escape infinite loop
			loop_check = true;
			JxlDecoderRewind(cache.decoder.get());
			JxlDecoderSubscribeEvents(cache.decoder.get(), JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE);
			JxlDecoderSetInput(cache.decoder.get(), cache.data, cache.data_size);
			JxlDecoderCloseInput(cache.decoder.get());
		} else if (status == JXL_DEC_BOX) {
			if (!cache.exif.empty()) {
				size_t remaining = JxlDecoderReleaseBoxBuffer(cache.decoder.get());
				cache.exif.resize(cache.exif.size() - remaining);
			} else {
				JxlBoxType type;
				if (JXL_DEC_SUCCESS !=
					JxlDecoderGetBoxType(cache.decoder.get(), type, true)) {
					return false;
				}
				if (!memcmp(type, "Exif", 4)) {
					cache.exif.resize(kChunkSize);
					JxlDecoderSetBoxBuffer(cache.decoder.get(), cache.exif.data(), cache.exif.size());
				}
			}
		} else if (status == JXL_DEC_BOX_NEED_MORE_OUTPUT) {
			size_t remaining = JxlDecoderReleaseBoxBuffer(cache.decoder.get());
			output_pos += kChunkSize - remaining;
			cache.exif.resize(cache.exif.size() + kChunkSize);
			JxlDecoderSetBoxBuffer(cache.decoder.get(), cache.exif.data() + output_pos, cache.exif.size() - output_pos);
		} else {
			return false;
		}
	}
}

void* JxlReader::ReadImage(int& width,
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
	outOfMemory = false;
	width = height = 0;
	nchannels = 4;
	has_animation = false;
	unsigned char* pPixelData = NULL;
	exif_chunk = NULL;


	std::vector<uint8_t> pixels;
	std::vector<uint8_t> icc_profile;
	if (!DecodeJpegXlOneShot((const uint8_t*)buffer, sizebytes, &pixels, width, height,
		has_animation, frame_count, frame_time, &icc_profile, outOfMemory)) {
		return NULL;
	}
	int size = width * height * nchannels;
	pPixelData = new(std::nothrow) unsigned char[size];
	if (pPixelData == NULL) {
		outOfMemory = true;
		return NULL;
	}
	if (cache.transform == NULL)
		cache.transform = ICCProfileTransform::CreateTransform(icc_profile.data(), icc_profile.size(), ICCProfileTransform::FORMAT_RGBA);
	if (!ICCProfileTransform::DoTransform(cache.transform, pixels.data(), pPixelData, width, height)) {
		// RGBA -> BGRA conversion (with little-endian integers)
		uint32_t* data = (uint32_t*)pixels.data();
		for (int i = 0; i * sizeof(uint32_t) < size; i++) {
			((uint32_t*)pPixelData)[i] = _rotr(_byteswap_ulong(data[i]), 8);
		}
	}

	// Copy Exif data into the format understood by CEXIFReader
	if (!cache.exif.empty() && cache.exif.size() > 8 && cache.exif.size() < 65532) {
		exif_chunk = malloc(cache.exif.size() + 6);
		if (exif_chunk != NULL) {
			memcpy(exif_chunk, "\xFF\xE1\0\0Exif\0\0", 10);
			*((unsigned short*)exif_chunk + 1) = _byteswap_ushort(cache.exif.size() + 4);
			memcpy((uint8_t*)exif_chunk + 10, cache.exif.data() + 4, cache.exif.size() - 4);
		}
	}

	if (!has_animation)
		DeleteCache();

	return pPixelData;
}

void JxlReader::DeleteCache() {
	free(cache.data);
	ICCProfileTransform::DeleteTransform(cache.transform);
	// Setting the decoder and runner to 0 (NULL) will automatically destroy them
	cache = { 0 };
}
