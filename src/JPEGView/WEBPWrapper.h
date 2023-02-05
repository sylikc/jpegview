
#pragma once

class WebpReaderWriter
{
public:
	// Returns data in 4 byte BGRA
	static void* ReadImage(int& width,   // width of the image loaded.
		int& height,  // height of the image loaded.
		int& bpp,     // BYTES (not bits) PER PIXEL.
		bool& has_animation,     // if the image is animated
		int& frame_count, // number of frames
		int& frame_time, // frame duration in milliseconds
		bool& outOfMemory, // set to true when no memory to read image
		const void* buffer, // memory address containing webp compressed data.
		int sizebytes); // size of webp compressed data

	static void DeleteCache();

	// Compress image data into WEBP stream, returns compressed data.
	static void* Compress(const void* buffer, // address of image in memory, format must be 3 bytes per pixel BRGBGR with padding to 4 byte boundary
		int width, // width of image in pixels
		int height, // height of image in pixels.
		size_t& len, // returns length of compressed data
		int quality, // image quality as a percentage (ignored if lossless)
		bool lossless); // use lossless compression if true

	static void FreeMemory(void* pointer);

private:
	struct webp_cache;
	static webp_cache cache;
};
