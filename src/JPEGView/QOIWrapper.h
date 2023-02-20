#pragma once

class QoiReaderWriter
{
public:
	// Returns data in 3-byte BGR (padded to 4-byte boundary) or 4-byte BGRA
	static void* ReadImage(int& width,   // width of the image loaded.
		int& height,  // height of the image loaded.
		int& bpp,     // BYTES (not bits) PER PIXEL.
		bool& outOfMemory, // set to true when no memory to read image
		const void* buffer, // memory address containing qoi compressed data.
		int sizebytes); // size of qoi compressed data.

	// Compress image data into QOI stream, returns compressed data.
	static void* Compress(const void* buffer, // address of image in memory, format must be 3 bytes per pixel BRGBGR with padding to 4 byte boundary
		int width, // width of image in pixels
		int height, // height of image in pixels.
		int& len); // returns length of compressed data

	static void FreeMemory(void* pointer);
};
