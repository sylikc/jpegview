
#pragma once

/// signed 8 bit integer value
typedef signed char int8;
/// unsigned 8 bit integer value
typedef unsigned char uint8;
/// signed 16 bit integer value
typedef signed short int16;
/// unsigned 16 bit integer value
typedef unsigned short uint16;
/// signed 32 bit integer value
typedef signed int int32;
/// unsigned 32 bit integer value
typedef unsigned int uint32;

enum EFilterType {
	Filter_Downsampling_Best_Quality,  // prefer this filter for sampling down
	Filter_Downsampling_No_Aliasing, // this is a Lanczos type filter
	Filter_Downsampling_Narrow,
	Filter_Upsampling_Bicubic
};

// Image formats (can be other than JPEG...)
enum EImageFormat {
	IF_JPEG,
	IF_WindowsBMP,
	IF_PNG,
	IF_GIF,
	IF_TIFF,
	IF_WEBP,
    IF_WIC,
	IF_CLIPBOARD,
	IF_Unknown
};