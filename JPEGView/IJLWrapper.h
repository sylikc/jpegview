//*** The JPEG.H wrapper layer header file.

#ifndef JPEG_H
#define JPEG_H

//############################################################################
//##                                                                        ##
//##  JPEG.H                                                                ##
//##                                                                        ##
//##  Wrapper class to compress or decompress a jpeg from a block of memory ##
//##  using the Intel Jpeg Library.                                         ##
//##  OpenSourced 2/4/2000 by John W. Ratcliff                              ##
//##                                                                        ##
//##  No warranty expressed or implied.  Released as part of the triangle   ##
//##  throughput testbed project.                                           ##
//############################################################################
//##                                                                        ##
//##  Contact John W. Ratcliff at jratcliff@verant.com                      ##
//############################################################################

class Jpeg
{
public:
	// Returns data in the form BGRBGR**********BGR000 where the zeros are padding to 4 byte boundary
	static void * ReadImage(int &width,   // width of the image loaded.
                         int &height,  // height of the image loaded.
                         int &bpp,     // BYTES (not bits) PER PIXEL.
						 bool &outOfMemory, // set to true when no memory to read image
                         const void *buffer, // memory address containing jpeg compressed data.
                         int sizebytes); // size of jpeg compressed data.

	// Currently not used in JPEG view
	static void * Compress(const void *buffer, // address of image in memory
                         int width, // width of image in pixels
                         int height, // height of image in pixels.
                         int bpp, // *BYTES* per pixel of image 1 or 3
                         int &len, // returns length of compressed data
						 bool &outOfMemory, // returns if out of memory
                         int quality=75); // image quality as a percentage

};

#endif