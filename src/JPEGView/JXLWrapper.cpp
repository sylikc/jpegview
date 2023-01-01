#include "stdafx.h"

#include "JXLWrapper.h"
#include "jxl/decode.h"
#include "jxl/decode_cxx.h"
#include "jxl/resizable_parallel_runner.h"
#include "jxl/resizable_parallel_runner_cxx.h"
#include "MaxImageDef.h"
#include <vector>

JxlDecoderPtr cached_jxl_decoder = NULL;
JxlResizableParallelRunnerPtr cached_jxl_runner = NULL;
JxlBasicInfo cached_jxl_info = {0};
uint8_t* cached_jxl_data = NULL;
size_t cached_jxl_data_size = 0;
int cached_jxl_prev_frame_timestamp = 0;
int cached_jxl_width = 0;
int cached_jxl_height = 0;

// based on https://github.com/libjxl/libjxl/blob/main/examples/decode_oneshot.cc
bool DecodeJpegXlOneShot(const uint8_t* jxl, size_t size, std::vector<uint8_t>* pixels, int& xsize,
	int& ysize, bool& have_animation, int& frame_count, int& frame_time, std::vector<uint8_t>* icc_profile, bool& outOfMemory) {

	if (cached_jxl_decoder.get() == NULL) {
		cached_jxl_runner = JxlResizableParallelRunnerMake(nullptr);

		cached_jxl_decoder = JxlDecoderMake(nullptr);
		if (JXL_DEC_SUCCESS !=
			JxlDecoderSubscribeEvents(cached_jxl_decoder.get(), JXL_DEC_BASIC_INFO |
				JXL_DEC_COLOR_ENCODING |
				JXL_DEC_FRAME |
				JXL_DEC_FULL_IMAGE)) {
			return false;
		}


		if (JXL_DEC_SUCCESS != JxlDecoderSetParallelRunner(cached_jxl_decoder.get(),
			JxlResizableParallelRunner,
			cached_jxl_runner.get())) {
			return false;
		}

		JxlDecoderSetInput(cached_jxl_decoder.get(), jxl, size);
		JxlDecoderCloseInput(cached_jxl_decoder.get());
		cached_jxl_data = (uint8_t*)jxl;
		cached_jxl_data_size = size;
	}

	JxlBasicInfo info;
	JxlPixelFormat format = { 4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0 };

	bool loop_check = false;
	for (;;) {
		JxlDecoderStatus status = JxlDecoderProcessInput(cached_jxl_decoder.get());

		if (status == JXL_DEC_ERROR) {
			return false;
		} else if (status == JXL_DEC_NEED_MORE_INPUT) {
			return false;
		} else if (status == JXL_DEC_BASIC_INFO) {
			if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(cached_jxl_decoder.get(), &info)) {
				return false;
			}
			cached_jxl_info = info;
			if (abs((double)cached_jxl_info.xsize * cached_jxl_info.ysize) > MAX_IMAGE_PIXELS) {
				outOfMemory = true;
				return false;
			}
			JxlResizableParallelRunnerSetThreads(
				cached_jxl_runner.get(),
				JxlResizableParallelRunnerSuggestThreads(info.xsize, info.ysize));
		} else if (status == JXL_DEC_COLOR_ENCODING) {
			// Get the ICC color profile of the pixel data
			size_t icc_size;
			if (JXL_DEC_SUCCESS !=
				JxlDecoderGetICCProfileSize(
					cached_jxl_decoder.get(), &format, JXL_COLOR_PROFILE_TARGET_DATA, &icc_size)) {
				return false;
			}
			icc_profile->resize(icc_size);
			if (JXL_DEC_SUCCESS != JxlDecoderGetColorAsICCProfile(
				cached_jxl_decoder.get(), &format,
				JXL_COLOR_PROFILE_TARGET_DATA,
				icc_profile->data(), icc_profile->size())) {
				return false;
			}
		} else if (status == JXL_DEC_FRAME) {
			JxlFrameHeader header;
			JxlDecoderGetFrameHeader(cached_jxl_decoder.get(), &header);
			JxlAnimationHeader animation = cached_jxl_info.animation;
			if (animation.tps_numerator)
				frame_time = 1000.0 * header.duration * animation.tps_denominator / animation.tps_numerator;
			else
				frame_time = 0;
		} else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
			size_t buffer_size;
			if (JXL_DEC_SUCCESS !=
				JxlDecoderImageOutBufferSize(cached_jxl_decoder.get(), &format, &buffer_size)) {
				return false;
			}
			if (buffer_size != cached_jxl_info.xsize * cached_jxl_info.ysize * 4) {
				return false;
			}
			pixels->resize(cached_jxl_info.xsize * cached_jxl_info.ysize * 4);
			void* pixels_buffer = (void*)pixels->data();
			size_t pixels_buffer_size = pixels->size() * sizeof(uint8_t);
			if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(cached_jxl_decoder.get(), &format,
				pixels_buffer,
				pixels_buffer_size)) {
				return false;
			}
		} else if (status == JXL_DEC_FULL_IMAGE) {
			// Full frame has been decoded

			xsize = cached_jxl_info.xsize;
			ysize = cached_jxl_info.ysize;
			have_animation = cached_jxl_info.have_animation;
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
			JxlDecoderRewind(cached_jxl_decoder.get());
			JxlDecoderSubscribeEvents(cached_jxl_decoder.get(), JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE);
			JxlDecoderSetInput(cached_jxl_decoder.get(), cached_jxl_data, cached_jxl_data_size);
			JxlDecoderCloseInput(cached_jxl_decoder.get());
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
	bool& outOfMemory,
	const void* buffer,
	int sizebytes)
{
	outOfMemory = false;
	width = height = 0;
	nchannels = 4;
	has_animation = false;
	unsigned char* pPixelData = NULL;


	std::vector<uint8_t> pixels;
	std::vector<uint8_t> icc_profile;
	if (!DecodeJpegXlOneShot((const uint8_t*)buffer, sizebytes, &pixels, width, height,
		has_animation, frame_count, frame_time, &icc_profile, outOfMemory)) {
		return NULL;
	}
	int size = width * height * nchannels;
	if (pPixelData = new(std::nothrow) unsigned char[size]) {
		memcpy(pPixelData, pixels.data(), size);
		// RGBA -> BGRA conversion (with little-endian integers)
		for (uint32_t* i = (uint32_t*)pPixelData; (uint8_t*)i < pPixelData + size; i++)
			*i = ((*i & 0x00FF0000) >> 16) | ((*i & 0x0000FF00)) | ((*i & 0x000000FF) << 16) | ((*i & 0xFF000000));
	}
	return pPixelData;
}

void JxlReader::DeleteCache() {
	// JxlDecoderDestroy(cached_jxl_decoder.get());
	// JxlResizableParallelRunnerDestroy(cached_jxl_runner.get());
	cached_jxl_info = { 0 };
	free(cached_jxl_data);
	cached_jxl_data = NULL;
	cached_jxl_data_size = 0;
	// Setting these to NULL will automatically destroy them
	cached_jxl_decoder = NULL;
	cached_jxl_runner = NULL;
}
