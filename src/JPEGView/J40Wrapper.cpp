#include "stdafx.h"

#define J40_CONFIRM_THAT_THIS_IS_EXPERIMENTAL_AND_POTENTIALLY_UNSAFE
#define J40_IMPLEMENTATION
#include "J40Wrapper.h"
#include "j40/j40.h"
#include "MaxImageDef.h"

void* J40JxlReader::ReadImage(int& width,
	int& height,
	int& nchannels,
	bool& outOfMemory,
	const void* buffer,
	int sizebytes)
{
	outOfMemory = false;
	width = height = 0;
	nchannels = 4;

	unsigned char* pPixelData = NULL;
	j40_image image;
	if (j40_from_memory(&image, (void*)buffer, sizebytes, free) == 0 &&
		j40_output_format(&image, J40_RGBA, J40_U8X4) == 0 &&
		j40_next_frame(&image)) {
		j40_frame frame = j40_current_frame(&image);
		j40_pixels_u8x4 pixels = j40_frame_pixels_u8x4(&frame, J40_RGBA);
		width = pixels.width;
		height = pixels.height;
		if (abs((double)width * height) > MAX_IMAGE_PIXELS) {
			outOfMemory = true;
		} else if (width <= MAX_IMAGE_DIMENSION && height <= MAX_IMAGE_DIMENSION) {
			int nStride = width * 4;
			pPixelData = new(std::nothrow) unsigned char[nStride * height];
			if (pPixelData != NULL) {
				// JPEG XL strides are slightly bigger than width * 4, so you have to copy row by row
				for (int y = 0; y < height; ++y)
					memcpy(pPixelData + y * nStride, (void*)j40_row_u8x4(pixels, y), nStride);

				// RGBA -> BGRA conversion (with little-endian integers)
				for (uint32_t* i = (uint32_t*)pPixelData; (uint8_t*)i < pPixelData + nStride * height; i++)
					*i = ((*i & 0x00FF0000) >> 16) | ((*i & 0x0000FF00)) | ((*i & 0x000000FF) << 16) | ((*i & 0xFF000000));
			} else {
				outOfMemory = true;
			}
		}
	}

	// This also frees the input buffer given to load the image
	j40_free(&image);

	return pPixelData;
}