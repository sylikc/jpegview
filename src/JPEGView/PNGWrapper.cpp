#include "stdafx.h"

#include "PNGWrapper.h"
#include "png.h"
#include "MaxImageDef.h"
#include <stdexcept>
#include <stdlib.h>

// Uncomment to build without APNG support
//#undef PNG_APNG_SUPPORTED

/*
 * Modified from "load4apng.c"
 * Original: https://sourceforge.net/projects/apng/files/libpng/examples/
 *
 * loads APNG from memory one frame at a time (32bpp), restarts when done
 * including frames composition.
 *
 * needs apng-patched libpng.
 *
 * Copyright (c) 2012-2014 Max Stepin
 * maxst at users.sourceforge.net
 *
 * zlib license
 * ------------
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 */

struct PngReader::png_cache {
	png_structp png_ptr;
	png_infop info_ptr;
	png_uint_32 w0;
	png_uint_32 h0;
	png_uint_32 x0;
	png_uint_32 y0;
	unsigned short delay_num;
	unsigned short delay_den;
	unsigned char dop;
	unsigned char bop;
	unsigned int first;
	png_bytepp rows_image;
	png_bytepp rows_frame;
	unsigned char* p_image;
	unsigned char* p_frame;
	unsigned char* p_temp;
	unsigned int size;
	unsigned int width;
	unsigned int height;
	unsigned int channels;
	unsigned int frame_index;
	png_uint_32 frame_count;
	void* buffer;
	size_t buffer_size;
	size_t buffer_offset;
};

PngReader::png_cache PngReader::cache = { 0 };

#ifdef PNG_APNG_SUPPORTED
void BlendOver(unsigned char** rows_dst, unsigned char** rows_src, unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
	unsigned int  i, j;
	int u, v, al;

	for (j = 0; j < h; j++)
	{
		unsigned char* sp = rows_src[j];
		unsigned char* dp = rows_dst[j + y] + x * 4;

		for (i = 0; i < w; i++, sp += 4, dp += 4)
		{
			if (sp[3] == 255)
				memcpy(dp, sp, 4);
			else if (sp[3] != 0)
			{
				if (dp[3] != 0)
				{
					u = sp[3] * 255;
					v = (255 - sp[3]) * dp[3];
					al = u + v;
					dp[0] = (sp[0] * u + dp[0] * v) / al;
					dp[1] = (sp[1] * u + dp[1] * v) / al;
					dp[2] = (sp[2] * u + dp[2] * v) / al;
					dp[3] = al / 255;
				}
				else
					memcpy(dp, sp, 4);
			}
		}
	}
}
#endif

void* PngReader::ReadNextFrame(void** exif_chunk, png_uint_32* exif_size)
{
	unsigned int j;
	if (exif_chunk != NULL && exif_size != NULL) {
		png_get_eXIf_1(cache.png_ptr, cache.info_ptr, exif_size, (png_bytep*)exif_chunk);
	}
#ifdef PNG_APNG_SUPPORTED
	if (png_get_valid(cache.png_ptr, cache.info_ptr, PNG_INFO_acTL))
	{
		png_read_frame_head(cache.png_ptr, cache.info_ptr);
		png_get_next_frame_fcTL(cache.png_ptr, cache.info_ptr, &cache.w0, &cache.h0, &cache.x0, &cache.y0, &cache.delay_num, &cache.delay_den, &cache.dop, &cache.bop);
	}
	if (cache.frame_index == cache.first)
	{
		cache.bop = PNG_BLEND_OP_SOURCE;
		if (cache.dop == PNG_DISPOSE_OP_PREVIOUS)
			cache.dop = PNG_DISPOSE_OP_BACKGROUND;
	}
#endif
	png_read_image(cache.png_ptr, cache.rows_frame);

#ifdef PNG_APNG_SUPPORTED
	if (cache.dop == PNG_DISPOSE_OP_PREVIOUS)
		memcpy(cache.p_temp, cache.p_image, cache.size);

	if (cache.bop == PNG_BLEND_OP_OVER)
		BlendOver(cache.rows_image, cache.rows_frame, cache.x0, cache.y0, cache.w0, cache.h0);
	else
#endif
		for (j = 0; j < cache.h0; j++)
			memcpy(cache.rows_image[j + cache.y0] + cache.x0 * 4, cache.rows_frame[j], cache.w0 * 4);

	void* pixels = malloc(cache.width * cache.height * cache.channels);
	if (pixels == NULL)
		return NULL;
	for (j = 0; j < cache.height; j++)
		memcpy((char*)pixels + j * cache.width * cache.channels, cache.rows_image[j], cache.width * cache.channels);

#ifdef PNG_APNG_SUPPORTED
	if (cache.dop == PNG_DISPOSE_OP_PREVIOUS)
		memcpy(cache.p_image, cache.p_temp, cache.size);
	else
		if (cache.dop == PNG_DISPOSE_OP_BACKGROUND)
			for (j = 0; j < cache.h0; j++)
				memset(cache.rows_image[j + cache.y0] + cache.x0 * 4, 0, cache.w0 * 4);
#endif
	cache.frame_index++;
	cache.frame_index %= cache.frame_count;
	return pixels;
}

bool PngReader::BeginReading(void* buffer, size_t sizebytes, bool& outOfMemory)
{
	unsigned int    width, height, channels, rowbytes, size, j;
	png_bytepp      rows_image;
	png_bytepp      rows_frame;
	unsigned char*  p_image;
	unsigned char*  p_frame;
	unsigned char*  p_temp;


	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop   info_ptr = png_create_info_struct(png_ptr);

	if (png_ptr && info_ptr)
	{
		// don't cache ptrs until image is valid or else causes a double-free later when destroying object in DeleteCacheInternal()

		if (setjmp(png_jmpbuf(png_ptr)))
		{
			PngReader::DeleteCache();
			throw std::runtime_error::runtime_error("Image contains errors.");
		}
		// skip png signature since we already checked it
		png_set_sig_bytes(png_ptr, 8);
		// custom read function so we can read from memory
		png_rw_ptr read_data_fn = [](png_structp png_ptr, png_bytep outbuffer, png_size_t sizebytes)
		{
			png_voidp io_ptr = png_get_io_ptr(png_ptr);
			if (io_ptr == NULL)
				png_error(png_ptr, "png_get_io_ptr returned NULL");
			else if (cache.buffer_offset + sizebytes > cache.buffer_size)
				png_error(png_ptr, "Attempted to read out of bounds");
			else
			{
				memcpy(outbuffer, (char*)io_ptr + cache.buffer_offset, sizebytes);
				cache.buffer_offset += sizebytes;
			}
		};
		png_set_read_fn(png_ptr, (char*)buffer, read_data_fn);
		png_read_info(png_ptr, info_ptr);
		png_set_expand(png_ptr);
		png_set_strip_16(png_ptr);
		png_set_gray_to_rgb(png_ptr);
		png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
		png_set_bgr(png_ptr);
		(void)png_set_interlace_handling(png_ptr);
		png_read_update_info(png_ptr, info_ptr);
		width = png_get_image_width(png_ptr, info_ptr);
		height = png_get_image_height(png_ptr, info_ptr);
		if (width > MAX_IMAGE_DIMENSION || height > MAX_IMAGE_DIMENSION) {
			png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
			return false;
		}
		if (abs((double)width * height) > MAX_IMAGE_PIXELS) {
			png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
			outOfMemory = true;
			return false;
		}
		channels = png_get_channels(png_ptr, info_ptr);
		if (channels != 4)
		{
			png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
			return false;
		}
		rowbytes = (unsigned int)png_get_rowbytes(png_ptr, info_ptr);
		size = height * rowbytes;
		p_image = (unsigned char*)malloc(size);
		p_frame = (unsigned char*)malloc(size);
		p_temp = (unsigned char*)malloc(size);
		rows_image = (png_bytepp)malloc(height * sizeof(png_bytep));
		rows_frame = (png_bytepp)malloc(height * sizeof(png_bytep));
		if (p_image && p_frame && p_temp && rows_image && rows_frame)
		{
			png_uint_32     frames = 1;
			png_uint_32     x0 = 0;
			png_uint_32     y0 = 0;
			png_uint_32     w0 = width;
			png_uint_32     h0 = height;
#ifdef PNG_APNG_SUPPORTED
			png_uint_32     plays = 0;
			unsigned short  delay_num = 1;
			unsigned short  delay_den = 10;
			unsigned char   dop = 0;
			unsigned char   bop = 0;
			unsigned int    first = (png_get_first_frame_is_hidden(png_ptr, info_ptr) != 0) ? 1 : 0;
			if (png_get_valid(png_ptr, info_ptr, PNG_INFO_acTL))
				png_get_acTL(png_ptr, info_ptr, &frames, &plays);
#endif
			for (j = 0; j < height; j++)
				rows_image[j] = p_image + j * rowbytes;

			for (j = 0; j < height; j++)
				rows_frame[j] = p_frame + j * rowbytes;

#ifdef PNG_APNG_SUPPORTED
			cache.bop = bop;
			// cache.plays = plays
			cache.delay_den = delay_den;
			cache.delay_num = delay_num;
			cache.dop = dop;
			cache.first = first;
#endif
			cache.channels = channels;
			cache.h0 = h0;
			cache.height = height;
			// cache ptrs here, only if valid
			cache.info_ptr = info_ptr;
			cache.png_ptr = png_ptr;
			cache.p_image = p_image;
			cache.p_frame = p_frame;
			cache.p_temp = p_temp;
			cache.rows_frame = rows_frame;
			cache.rows_image = rows_image;
			cache.size = size;
			cache.w0 = w0;
			cache.width = width;
			cache.x0 = x0;
			cache.y0 = y0;
			cache.frame_count = frames;
			return frames > 0;
		}
		free(p_image);
		free(rows_image);
		free(rows_frame);
		free(p_frame);
		free(p_temp);
	}
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	return false;
}

void* PngReader::ReadImage(int& width,
	int& height,
	int& nchannels,
	bool& has_animation,
	int& frame_count,
	int& frame_time,
	void*& exif_chunk,
	bool& outOfMemory,
	void* buffer,
	size_t sizebytes)
{
	exif_chunk = NULL;
	if (!cache.buffer) {
		if (sizebytes < 8)
			return NULL;
		cache.buffer = malloc(sizebytes-8);
		if (!cache.buffer)
			return NULL;
		// copy everything except the PNG signature (first 8 bytes)
		memcpy(cache.buffer, (char*)buffer+8, sizebytes-8);
		cache.buffer_size = sizebytes-8;
	}
	buffer = cache.buffer;
	sizebytes = cache.buffer_size;
	if (!cache.png_ptr || cache.frame_index == 0) {
		DeleteCacheInternal(false);
		if (!buffer || !BeginReading(buffer, sizebytes, outOfMemory)) {
			return NULL;
		}

	}

	void* exif = NULL;
	unsigned int exif_size = 0;
	bool read_two = cache.frame_index < cache.first;
	void* pixels = ReadNextFrame(&exif, &exif_size);
	if (pixels && read_two)
		pixels = ReadNextFrame(&exif, &exif_size);
	
	width = cache.width;
	height = cache.height;
	nchannels = cache.channels;
	has_animation = (cache.frame_count > 1);
	frame_count = cache.frame_count;

	// https://wiki.mozilla.org/APNG_Specification
	// "If the denominator is 0, it is to be treated as if it were 100"
	if (!cache.delay_den)
		cache.delay_den = 100;
	frame_time = (int)(1000.0 * cache.delay_num / cache.delay_den);

	if (exif_size > 8 && exif_size < 65528 && exif != NULL) {
		exif_chunk = malloc(exif_size + 10);
		if (exif_chunk != NULL) {
			memcpy(exif_chunk, "\xFF\xE1\0\0Exif\0\0", 10);
			*((unsigned short*)exif_chunk + 1) = _byteswap_ushort(exif_size + 8);
			memcpy((uint8_t*)exif_chunk + 10, exif, exif_size);
		}
		
	}
	if (!has_animation)
		DeleteCache();
	return pixels;
}

void PngReader::DeleteCacheInternal(bool free_buffer)
{
	// png_read_end(cache.png_ptr, cache.info_ptr);
	free(cache.rows_frame);
	free(cache.rows_image);
	free(cache.p_temp);
	free(cache.p_frame);
	free(cache.p_image);
	png_destroy_read_struct(&cache.png_ptr, &cache.info_ptr, NULL);
	void* temp_buffer = cache.buffer;
	size_t temp_buffer_size = cache.buffer_size;
	cache = { 0 };
	if (free_buffer) {
		free(temp_buffer);
	} else {
		cache.buffer = temp_buffer;
		cache.buffer_size = temp_buffer_size;
	}
}

void PngReader::DeleteCache() {
	DeleteCacheInternal(true);
}

bool PngReader::IsAnimated(void* buffer, size_t sizebytes) {
	// Valid APNGs must have an acTL chunk before the first IDAT chunk, this lets us quickly determine if a PNG is animated

	size_t offset = 8; // skip PNG signature
	while (offset + 7 < sizebytes) {
		if (memcmp((char*)buffer + offset + 4, "acTL", 4) == 0)
			return true;
		if (memcmp((char*)buffer + offset + 4, "IDAT", 4) == 0)
			return false;
		unsigned int chunksize = *(unsigned int*)((char*)buffer + offset);

		// PNG chunk sizes are big-endian and must be converted to little-endian
		chunksize = _byteswap_ulong(chunksize);

		// Prevent infinite loop
		if (chunksize > PNG_UINT_31_MAX) return false;

		// 12 comes from 4 bytes for chunk size, 4 for chunk name, and 4 for CRC32
		offset += chunksize + 12;
	}
	return false;
}
