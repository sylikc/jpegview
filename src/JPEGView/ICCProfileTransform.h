#pragma once

class ICCProfileTransform
{
public:
	enum PixelFormat {
		FORMAT_BGRA,
		FORMAT_RGBA,
		FORMAT_BGR,
		FORMAT_RGB,
		FORMAT_Lab,
		FORMAT_LabA,
	};

	// Create a transform from given ICC Profile to standard sRGB color space.
	static void* CreateTransform(
		const void* profile, // pointer to ICC profile
		unsigned int size, // size of ICC profile in bytes
		PixelFormat format // format of input pixels
	);

	// Apply color transform to image. Returns true on success, false otherwise.
	static bool DoTransform(
		void* transform, // ICCP transform
		const void* inputBuffer, // 4-byte BGRA or RGBA input depending on transform pixel format
		void* outputBuffer, // 4-byte BGRA output
		unsigned int width, // width of image in pixels
		unsigned int height, // height of image in pixels
		unsigned int stride=0 // number of bytes per row of pixels in the input, only needed if not equal to width * 4
	);

	// Free memory associated with the given transform
	static void DeleteTransform(void* transform);

	static void* CreateLabTransform(PixelFormat format);

private:
	static void* sRGBProfile;
};
