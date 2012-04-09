
#pragma once

enum TJSAMP;

class TurboJpeg
{
public:
	// Returns data in the form BGRBGR**********BGR000 where the zeros are padding to 4 byte boundary
	static void * ReadImage(int &width,   // width of the image loaded.
                         int &height,  // height of the image loaded.
                         int &bpp,     // BYTES (not bits) PER PIXEL.
                         TJSAMP &chromoSubsampling, // chromo subsampling of image
						 bool &outOfMemory, // set to true when no memory to read image
                         const void *buffer, // memory address containing jpeg compressed data.
                         int sizebytes); // size of jpeg compressed data.

	// Compress image data into JPEG stream, returns compressed data.
    // The returned buffer must be freed with tjFree()!
	static void * Compress(const void *buffer, // address of image in memory, format must be 3 bytes per pixel BRGBGR with padding to 4 byte boundary
                         int width, // width of image in pixels
                         int height, // height of image in pixels.
                         int &len, // returns length of compressed data
						 bool &outOfMemory, // returns if out of memory
                         int quality=75); // image quality as a percentage

};
