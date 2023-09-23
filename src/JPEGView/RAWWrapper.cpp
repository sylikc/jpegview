#include "stdafx.h"

#include "RAWWrapper.h"
#include "libraw/libraw.h"
#include "Helpers.h"
#include "ICCProfileTransform.h"
#include "TJPEGWrapper.h"
#include "RawMetadata.h"
#include "MaxImageDef.h"

CJPEGImage* RawReader::ReadImage(LPCTSTR strFileName, bool& bOutOfMemory, bool bGetThumb)
{
	unsigned char* pPixelData = NULL;

	LibRaw RawProcessor;
	if (RawProcessor.open_file(strFileName) != LIBRAW_SUCCESS) {
		return NULL;
	}
	int width, height, colors, bps;
	
	CJPEGImage* Image = NULL;
	if (!bGetThumb) {
		RawProcessor.get_mem_image_format(&width, &height, &colors, &bps);
		RawProcessor.imgdata.params.output_bps = 8;

		if (width > MAX_IMAGE_DIMENSION || height > MAX_IMAGE_DIMENSION) {
			return NULL;
		}

		if ((double)width * height > MAX_IMAGE_PIXELS) {
			bOutOfMemory = true;
			return NULL;
		}

		if (RawProcessor.unpack() != LIBRAW_SUCCESS || RawProcessor.dcraw_process() != LIBRAW_SUCCESS) {
			return NULL;
		}

		int stride = Helpers::DoPadding(width * colors, 4);
		
		pPixelData = new(std::nothrow) unsigned char[stride * height];
		if (pPixelData == NULL) {
			bOutOfMemory = true;
			return NULL;
		}
		if (RawProcessor.copy_mem_image(pPixelData, stride, 1) != LIBRAW_SUCCESS) {
			delete[] pPixelData;
			return NULL;
		}

		void* transform = ICCProfileTransform::CreateTransform(RawProcessor.imgdata.color.profile, RawProcessor.imgdata.color.profile_length, ICCProfileTransform::FORMAT_BGR);
		ICCProfileTransform::DoTransform(transform, pPixelData, pPixelData, width, height, stride);
		ICCProfileTransform::DeleteTransform(transform);

		CRawMetadata* metadata = new CRawMetadata(RawProcessor.imgdata.idata.make, RawProcessor.imgdata.idata.model, RawProcessor.imgdata.other.timestamp,
			RawProcessor.imgdata.color.flash_used != 0.0f, RawProcessor.imgdata.other.iso_speed, RawProcessor.imgdata.other.shutter,
			RawProcessor.imgdata.other.focal_len, RawProcessor.imgdata.other.aperture, RawProcessor.imgdata.sizes.flip, width, height,
			RawProcessor.imgdata.other.parsed_gps.latitude, RawProcessor.imgdata.other.parsed_gps.latref, RawProcessor.imgdata.other.parsed_gps.longitude,
			RawProcessor.imgdata.other.parsed_gps.longref, RawProcessor.imgdata.other.parsed_gps.altitude, RawProcessor.imgdata.other.parsed_gps.altref);

		if (pPixelData)
			Image = new CJPEGImage(width, height, pPixelData, NULL, colors, 0, IF_CameraRAW, false, 0, 1, 0, NULL, false, metadata);
	} else if (RawProcessor.is_jpeg_thumb()) {
		TJSAMP eChromoSubSampling;
		if (RawProcessor.unpack_thumb() != LIBRAW_SUCCESS) {
			return NULL;
		}
		libraw_processed_image_t* thumb = RawProcessor.dcraw_make_mem_thumb();
		if (thumb == NULL) {
			return NULL;
		}
		pPixelData = (unsigned char*)TurboJpeg::ReadImage(width, height, colors, eChromoSubSampling, bOutOfMemory, thumb->data, thumb->data_size);
		if (pPixelData != NULL && (colors == 3 || colors == 1))
		{
			CRawMetadata* metadata = new CRawMetadata(RawProcessor.imgdata.idata.make, RawProcessor.imgdata.idata.model, RawProcessor.imgdata.other.timestamp,
				RawProcessor.imgdata.color.flash_used != 0.0f, RawProcessor.imgdata.other.iso_speed, RawProcessor.imgdata.other.shutter,
				RawProcessor.imgdata.other.focal_len, RawProcessor.imgdata.other.aperture, RawProcessor.imgdata.sizes.flip, width, height,
				RawProcessor.imgdata.other.parsed_gps.latitude, RawProcessor.imgdata.other.parsed_gps.latref, RawProcessor.imgdata.other.parsed_gps.longitude,
				RawProcessor.imgdata.other.parsed_gps.longref, RawProcessor.imgdata.other.parsed_gps.altitude, RawProcessor.imgdata.other.parsed_gps.altref);

			Image = new CJPEGImage(width, height, pPixelData, NULL /* Helpers::FindEXIFBlock(thumb->data, thumb->data_size) */, colors,
				Helpers::CalculateJPEGFileHash(thumb->data, thumb->data_size), IF_JPEG_Embedded, false, 0, 1, 0, NULL, false, metadata);

			Image->SetJPEGComment(Helpers::GetJPEGComment(thumb->data, thumb->data_size));
			Image->SetJPEGChromoSampling(eChromoSubSampling);
		}
		RawProcessor.dcraw_clear_mem(thumb);
	}
	// RawProcessor.recycle();

	return Image;
}
