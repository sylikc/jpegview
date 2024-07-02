
#pragma once

#include <vector>

class JxlReader
{
public:
	// Returns data in 4 byte BGRA
	static void* ReadImage(int& width,   // width of the image loaded.
		int& height,  // height of the image loaded.
		int& bpp,     // BYTES (not bits) PER PIXEL.
		bool& has_animation,     // if the image is animated
		int& frame_count, // number of frames
		int& frame_time, // frame duration in milliseconds
		void*& exif, // Pointer to Exif data (must be freed by caller)
		bool& outOfMemory, // set to true when no memory to read image
		const void* buffer, // memory address containing jxl compressed data.
		int sizebytes); // size of jxl compressed data

	static void DeleteCache();

	static void* Compress(const void* buffer, // address of image in memory, format must be 3 bytes per pixel BRGBGR with padding to 4 byte boundary
		int width, // width of image in pixels
		int height, // height of image in pixels.
		size_t& len, // returns length of compressed data
		int quality, // image quality as a percentage
		bool lossless); // use lossless compression if true

	static void* CompressJPEG(const void* buffer, // address of JPEG-encoded image in memory
		size_t input_len, // Length of input JPEG image
		size_t& output_len); // returns length of compressed data

	static void Free(void* buffer);

private:
	struct jxl_cache;
	static jxl_cache cache;
	static bool DecodeJpegXlOneShot(const uint8_t* jxl, size_t size, std::vector<uint8_t>* pixels, int& xsize,
		int& ysize, bool& have_animation, int& frame_count, int& frame_time, std::vector<uint8_t>* icc_profile, bool& outOfMemory);
};
