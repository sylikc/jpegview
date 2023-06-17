#include "stdafx.h"

#include "libraw/libraw.h"
#include "RAWWrapper.h"
#include "Helpers.h"
#include "ICCProfileTransform.h"

CJPEGImage* RawReader::ReadImage(LPCTSTR strFileName, bool& bOutOfMemory)
{
	LibRaw RawProcessor;
	int ret = RawProcessor.open_file(strFileName);
	int width, height, colors, bps;
	RawProcessor.get_mem_image_format(&width, &height, &colors, &bps);
	if (bps != 8) {
		int j = 2;
		j = j + 7;
		j = 5;
	}
	
	ret = RawProcessor.unpack();
	
	RawProcessor.imgdata.params.output_bps = 8;
	ret = RawProcessor.dcraw_process();
	
	int stride = Helpers::DoPadding(width * colors, 4);
	unsigned char* pPixelData = NULL;
	pPixelData = new(std::nothrow) unsigned char[stride * height];
	if (pPixelData == NULL) {
		bOutOfMemory = true;
		RawProcessor.free_image();
		return NULL;
	}
	ret = RawProcessor.copy_mem_image(pPixelData, stride, 1);
	//void* transform = ICCProfileTransform::CreateTransform(RawProcessor.imgdata.color.profile, RawProcessor.imgdata.color.profile_length, ICCProfileTransform::FORMAT_BGRA);
	//ICCProfileTransform::DoTransform(transform, pPixelData, pPixelData, width, height, stride);
	RawProcessor.free_image();
	return new CJPEGImage(width, height, pPixelData, NULL, colors, 0, IF_CameraRAW, false, 0, 1, 0);
}