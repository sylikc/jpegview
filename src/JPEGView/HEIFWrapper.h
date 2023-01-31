#pragma once

#include "libheif/heif_cxx.h"

class HeifReader
{
public:
	// Returns data in the form 4-byte BGRA
	static void * ReadImage(int &width,   // width of the image loaded.
						 int &height,  // height of the image loaded.
						 int &bpp,     // BYTES (not bits) PER PIXEL.
						 int &frame_count, // number of top-level images
						 bool &outOfMemory, // set to true when no memory to read image
						 int frame_index, // index of requested frame
						 const void *buffer, // memory address containing heic compressed data.
						 int sizebytes); // size of heic compressed data.
};
