#include "stdafx.h"

#include "HEIFWrapper.h"
#include "MaxImageDef.h"

void * HeifReader::ReadImage(int &width,
					   int &height,
					   int &nchannels,
					   int &frame_count,
					   bool &outOfMemory,
					   int frame_index,
					   const void *buffer,
					   int sizebytes)
{
	outOfMemory = false;
	width = height = 0;
	nchannels = 4;

	unsigned char* pPixelData = NULL;

	heif::Context context;
	context.read_from_memory_without_copy(buffer, sizebytes);
	frame_count = context.get_number_of_top_level_images();
	heif_item_id item_id = context.get_list_of_top_level_image_IDs().at(frame_index);
	heif::ImageHandle handle = context.get_image_handle(item_id);
	// height = handle.get_height();
	// width = handle.get_width();
	heif::Image image = handle.decode_image(heif_colorspace_RGB, heif_chroma_interleaved_RGBA);
	int stride;
	uint8_t* data = image.get_plane(heif_channel_interleaved, &stride);
	width = image.get_width(heif_channel_interleaved);
	height = image.get_height(heif_channel_interleaved);

	if (width > MAX_IMAGE_DIMENSION || height > MAX_IMAGE_DIMENSION)
		return NULL;
	if (abs((double)width * height) > MAX_IMAGE_PIXELS) {
		outOfMemory = true;
		return NULL;
	}
	if (width < 1 || height < 1 || width * nchannels > stride)
		return NULL;

	int size = width * nchannels * height;
	uint8_t* p;
	size_t i, j;
	if (pPixelData = new(std::nothrow) unsigned char[size]) {
		memcpy(pPixelData, data, size);
		unsigned char* o = pPixelData;
		for (i = 0; i < height; i++) {
			p = data + i * stride;
			for (j = 0; j < width; j++) {
				// RGBA -> BGRA conversion
				o[0] = p[2];
				o[1] = p[1];
				o[2] = p[0];
				o[3] = p[3];
				p += nchannels;
				o += nchannels;
			}
		}
	}

	return (void*)pPixelData;
}
