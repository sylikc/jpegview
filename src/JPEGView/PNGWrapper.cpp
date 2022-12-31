#include "stdafx.h"

#include "PNGWrapper.h"
#include "MaxImageDef.h"
#include <stdexcept>

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
#include <stdlib.h>
#include "png.h"

struct FrameData {
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
	int buffer_offset;
};

struct FrameData env = { 0 };
size_t cached_buffer_size = 0;
void* cached_buffer = NULL;

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

void* ReadNextFrame()
{
	unsigned int j;
#ifdef PNG_APNG_SUPPORTED
	if (png_get_valid(env.png_ptr, env.info_ptr, PNG_INFO_acTL))
	{
		png_read_frame_head(env.png_ptr, env.info_ptr);
		png_get_next_frame_fcTL(env.png_ptr, env.info_ptr, &env.w0, &env.h0, &env.x0, &env.y0, &env.delay_num, &env.delay_den, &env.dop, &env.bop);
	}
	if (env.frame_index == env.first)
	{
		env.bop = PNG_BLEND_OP_SOURCE;
		if (env.dop == PNG_DISPOSE_OP_PREVIOUS)
			env.dop = PNG_DISPOSE_OP_BACKGROUND;
	}
#endif
	png_read_image(env.png_ptr, env.rows_frame);

#ifdef PNG_APNG_SUPPORTED
	if (env.dop == PNG_DISPOSE_OP_PREVIOUS)
		memcpy(env.p_temp, env.p_image, env.size);

	if (env.bop == PNG_BLEND_OP_OVER)
		BlendOver(env.rows_image, env.rows_frame, env.x0, env.y0, env.w0, env.h0);
	else
#endif
		for (j = 0; j < env.h0; j++)
			memcpy(env.rows_image[j + env.y0] + env.x0 * 4, env.rows_frame[j], env.w0 * 4);

	void* pixels = malloc(env.width * env.height * env.channels);
	if (pixels == NULL)
		return NULL;
	for (unsigned int j = 0; j < env.height; j++)
		memcpy((char*)pixels + j * env.width * env.channels, env.rows_image[j], env.width * env.channels);

#ifdef PNG_APNG_SUPPORTED
	if (env.dop == PNG_DISPOSE_OP_PREVIOUS)
		memcpy(env.p_image, env.p_temp, env.size);
	else
		if (env.dop == PNG_DISPOSE_OP_BACKGROUND)
			for (j = 0; j < env.h0; j++)
				memset(env.rows_image[j + env.y0] + env.x0 * 4, 0, env.w0 * 4);
#endif
	env.frame_index++;
	env.frame_index %= env.frame_count;
	return pixels;
}

void read_data_fn(png_structp png_ptr, png_bytep outbuffer, png_size_t sizebytes)
{
	png_voidp io_ptr = png_get_io_ptr(png_ptr);
	if (io_ptr == NULL)
		png_error(png_ptr, "png_get_io_ptr returned NULL");
	else if (env.buffer_offset + sizebytes > cached_buffer_size)
		png_error(png_ptr, "Attempted to read out of bounds");
	else
	{
		memcpy(outbuffer, (char*)io_ptr + env.buffer_offset, sizebytes);
		env.buffer_offset += sizebytes;
	}
}

bool BeginReading(void* buffer, size_t sizebytes, bool& outOfMemory)
{
	unsigned int    width, height, channels, rowbytes, size, i, j;
	png_bytepp      rows_image;
	png_bytepp      rows_frame;
	unsigned char*  p_image;
	unsigned char*  p_frame;
	unsigned char*  p_temp;


	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop   info_ptr = png_create_info_struct(png_ptr);
	if (png_ptr && info_ptr)
	{
		env.info_ptr = info_ptr;
		env.png_ptr = png_ptr;
		if (setjmp(png_jmpbuf(png_ptr)))
		{
			PngReader::DeleteCache();
			throw std::runtime_error::runtime_error("Image contains errors.");
		}
		// skip png signature since we already checked it
		png_set_sig_bytes(png_ptr, 8);
		// custom read function so we can read from memory
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
		rowbytes = png_get_rowbytes(png_ptr, info_ptr);
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
			env.bop = bop;
			// env.plays = plays
			env.delay_den = delay_den;
			env.delay_num = delay_num;
			env.dop = dop;
			env.first = first;
#endif
			env.channels = channels;
			env.h0 = h0;
			env.height = height;
			// env.info_ptr = info_ptr;
			// env.png_ptr = png_ptr;
			env.p_image = p_image;
			env.p_frame = p_frame;
			env.p_temp = p_temp;
			env.rows_frame = rows_frame;
			env.rows_image = rows_image;
			env.size = size;
			env.w0 = w0;
			env.width = width;
			env.x0 = x0;
			env.y0 = y0;
			env.frame_count = frames;
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

void DeleteCacheInternal(bool freeBuffer)
{
	// png_read_end(env.png_ptr, env.info_ptr);
	free(env.rows_frame);
	free(env.rows_image);
	free(env.p_temp);
	free(env.p_frame);
	free(env.p_image);
	png_destroy_read_struct(&env.png_ptr, &env.info_ptr, NULL);
	env = { 0 };
	if (freeBuffer) {
		free(cached_buffer);
		cached_buffer = NULL;
	}
}

void* PngReader::ReadImage(int& width,
	int& height,
	int& nchannels,
	bool& has_animation,
	int& frame_count,
	int& frame_time,
	bool& outOfMemory,
	void* buffer,
	size_t sizebytes)
{
	if (!cached_buffer) {
		if (sizebytes < 8)
			return NULL;
		cached_buffer = malloc(sizebytes-8);
		if (!cached_buffer)
			return NULL;
		// copy everything except the PNG signature (first 8 bytes)
		memcpy(cached_buffer, (char*)buffer+8, sizebytes-8);
		cached_buffer_size = sizebytes-8;
	}
	buffer = cached_buffer;
	sizebytes = cached_buffer_size;
	if (!env.png_ptr || env.frame_index == 0) {
		DeleteCacheInternal(false);
		if (!buffer || !BeginReading(buffer, sizebytes, outOfMemory)) {
			return NULL;
		}

	}

	bool read_two = env.frame_index < env.first;
	void* pixels = ReadNextFrame();
	if (pixels && read_two)
		pixels = ReadNextFrame();
	
	width = env.width;
	height = env.height;
	nchannels = env.channels;
	has_animation = (env.frame_count > 1);
	frame_count = env.frame_count;

	// https://wiki.mozilla.org/APNG_Specification
	// "If the denominator is 0, it is to be treated as if it were 100"
	if (!env.delay_den)
		env.delay_den = 100;
	frame_time = 1000.0 * env.delay_num / env.delay_den;

	return pixels;
}

void PngReader::DeleteCache() {
	DeleteCacheInternal(true);
}