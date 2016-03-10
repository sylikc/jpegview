#pragma once

class CJPEGImage;

namespace SetDesktopWallpaper {
	// Sets the specified image file as desktop wallpaper. Must be an image format supported by the OS as wallpaper.
	// Windows XP only supports BMP, later also JPG, TIFF, PNG and TIFF are supported.
	void SetFileAsWallpaper(CJPEGImage& image, LPCTSTR fileName);

	// Sets the specified image as desktop wallpaper, using its currently shown size and resolution and
	// having its current processings applied.
	void SetProcessedImageAsWallpaper(CJPEGImage& image);
}