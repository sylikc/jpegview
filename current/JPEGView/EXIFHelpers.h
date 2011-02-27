#pragma once

class CJPEGImage;

namespace EXIFHelpers {

	struct EXIFResult {
		int NumberOfSucceededFiles;
		int NumberOfFailedFiles;
	};

	// Sets the modification date of the given file to the given time in UTC (Caution!)
	bool SetModificationDate(LPCTSTR sFileName, const SYSTEMTIME& time);

	// Sets the modification date of the given file to the EXIF date of the given image
	bool SetModificationDateToEXIF(LPCTSTR sFileName, CJPEGImage* pImage);

	// Sets the modification date of the given file to the EXIF date, using GDI+
	bool SetModificationDateToEXIF(LPCTSTR sFileName);

	// Sets the modification date of all JPEG files in the given directory to the EXIF date.
	EXIFResult SetModificationDateToEXIFAllFiles(LPCTSTR sDirectory);

}