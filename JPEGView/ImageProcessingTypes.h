
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
	IF_CameraRAW,
	IF_Unknown
};

// Horizontal trapezoid
class CTrapezoid {
public:
	int x1s;
	int x1e;
	int y1;
	int x2s;
	int x2e;
	int y2;

	CTrapezoid() {
		x1s = x1e = x2s = x2e = y1 = y2 = 0;
	}

	CTrapezoid(int x1s, int x1e, int y1, int x2s, int x2e, int y2) {
		this->x1s = min(x1s, x1e);
		this->x1e = max(x1s, x1e);
		this->y1 = min(y1, y2);
		this->x2s = min(x2s, x2e);
		this->x2e = max(x2s, x2e);
		this->y2 = max(y1, y2);
	}

	bool operator ==(const CTrapezoid& other) const {
		return x1s == other.x1s && x1e == other.x1e && x2s == other.x2s && x2e == other.x2e && y1 == other.y1 && y2 == other.y2;
	}

	bool operator !=(const CTrapezoid& other) const {
		return !(*this == other);
	}

	int Width() const { return max(x1e - x1s, x2e - x2s); }
	int Height() const { return y2 - y1; }
};