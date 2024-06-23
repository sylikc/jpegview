
#pragma once

class PngReader
{
public:
#ifndef WINXP
	// Returns data in 4 byte BGRA
	static void* ReadImage(int& width,   // width of the image loaded.
		int& height,  // height of the image loaded.
		int& bpp,     // BYTES (not bits) PER PIXEL.
		bool& has_animation,     // if the image is animated
		int& frame_count, // number of frames
		int& frame_time, // frame duration in milliseconds
		void*& exif_chunk, // Pointer to Exif data (must be freed by caller)
		bool& outOfMemory, // set to true when no memory to read image
		void* buffer, // memory address containing png compressed data.
		size_t sizebytes); // size of png compressed data

	static void DeleteCache();

	// Returns true if PNG is unsupported by GDI+
	static bool MustUseLibpng(const void* buffer, size_t sizebytes);
#endif
	// Get EXIF Block
	static void* GetEXIFBlock(void* buffer, size_t sizebytes);

#ifndef WINXP
private:
	struct png_cache;
	static png_cache cache;
	static bool BeginReading(void* buffer, size_t sizebytes, bool& outOfMemory);
	static void* ReadNextFrame(void** exif_chunk, unsigned int* exif_size);
	static void DeleteCacheInternal(bool free_buffer);
#endif
};
