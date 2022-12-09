
#pragma once

class J40JxlReader
{
public:
	// Returns data in the form BGRBGR**********BGR000 where the zeros are padding to 4 byte boundary
	static void* ReadImage(int& width,   // width of the image loaded.
		int& height,  // height of the image loaded.
		int& bpp,     // BYTES (not bits) PER PIXEL.
		bool& outOfMemory, // set to true when no memory to read image
		const void* buffer, // memory address containing jpeg compressed data.
		int sizebytes); // size of jpeg compressed data
};
