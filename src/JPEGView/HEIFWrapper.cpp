#include "stdafx.h"

#include "HEIFWrapper.h"
#include "libheif/heif_cxx.h"
#include "MaxImageDef.h"

void * HeifReader::ReadImage(int &width,
					   int &height,
					   int &nchannels,
					   bool &outOfMemory,
					   const void *buffer,
					   int sizebytes)
{
	outOfMemory = false;
	width = height = 0;
	nchannels = 4;

	unsigned char* pPixelData = NULL;

	heif::Context context;
	context.read_from_memory_without_copy(buffer, sizebytes);
	heif::ImageHandle handle = context.get_primary_image_handle();
	// height = handle.get_height();
	// width = handle.get_width();
	heif::Image image = handle.decode_image(heif_colorspace_RGB, heif_chroma_interleaved_RGBA);
	int stride;
	const uint8_t* data = image.get_plane(heif_channel_interleaved, &stride);
	height = image.get_height(heif_channel_interleaved);

	// Not sure how strides work for HEIC, different viewers display different widths...
	if (stride % nchannels != 0)
		return NULL;
	width = stride / nchannels;

	int size = stride * height;
	if (pPixelData = new(std::nothrow) unsigned char[size]) {
		memcpy(pPixelData, data, size);
		// RGBA -> BGRA conversion (with little-endian integers)
		for (uint32_t* i = (uint32_t*)pPixelData; (uint8_t*)i < pPixelData + size; i++)
			*i = ((*i & 0x00FF0000) >> 16) | ((*i & 0x0000FF00)) | ((*i & 0x000000FF) << 16) | ((*i & 0xFF000000));
	}

	return (void*)pPixelData;
}
