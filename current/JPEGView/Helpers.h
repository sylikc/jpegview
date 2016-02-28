#pragma once

#include "ImageProcessingTypes.h"

class CJPEGImage;
class CFileList;

namespace Helpers {

	// Capabilities of CPU
	enum CPUType {
		CPU_Unknown,
		CPU_Generic,
		CPU_MMX,
		CPU_SSE
	};

	// Sorting order of image files
	enum ESorting {
		FS_Undefined = -1,
		FS_LastModTime,
		FS_CreationTime,
		FS_FileName,
		FS_Random,
		FS_FileSize
	};

	// Navigation mode over directories
	enum ENavigationMode {
		NM_Undefined = -1,
		NM_LoopDirectory,
		NM_LoopSubDirectories,
		NM_LoopSameDirectoryLevel
	};

	// Auto zoom modes
	enum EAutoZoomMode {
		ZM_FitToScreenNoZoom,
		ZM_FillScreenNoZoom,
		ZM_FitToScreen,
		ZM_FillScreen
	};

	// Transition effects for full screen slideshow
	enum ETransitionEffect {
		TE_None,
		TE_Blend,
		TE_SlideRL,
		TE_SlideLR,
		TE_SlideTB,
		TE_SlideBT,
		TE_RollRL,
		TE_RollLR,
		TE_RollTB,
		TE_RollBT,
		TE_ScrollRL,
		TE_ScrollLR,
		TE_ScrollTB,
		TE_ScrollBT,
	};

	// Confirmation required for file deletion
	enum EDeleteConfirmation {
		DC_Always,
		DC_OnlyWhenNoRecycleBin,
		DC_Never
	};

	// Units used for measurements (e.g. lengths)
	enum EMeasureUnits {
		MU_Metric,
		MU_English
	};

	// Maximum and minimum allowed zoom factors for images
	const double ZoomMax = 16.0;
	const double ZoomMin = 0.1;

	// Round to integer
	inline int RoundToInt(double d) {
		return (d < 0) ? (int)(d - 0.5) : (int)(d + 0.5);
	}

	// Exact tick count in milliseconds using the high resolution timer
	double GetExactTickCount();

	// Converts the system time to a string
	CString SystemTimeToString(const SYSTEMTIME &time);

	// Gets the image size to be used when fitting the image to screen, either using 'fit to screen'
	// or 'fill with crop' method. If 'fill with crop' is used, the bLimitAR can be set to avoid
	// filling when to less pixels remain visible
	// Outputs also the zoom factor to resize to this new size.
	// nWidth, nHeight are the original image width and height
	CSize GetImageRect(int nWidth, int nHeight, int nScreenWidth, int nScreenHeight, 
		bool bAllowZoomIn, bool bFillCrop, bool bLimitAR, double & dZoom);

	// Gets the image size to be used when fitting the image to screen according to the auto zoom mode given
	CSize GetImageRect(int nWidth, int nHeight, int nScreenWidth, int nScreenHeight, EAutoZoomMode eAutoZoomMode, double & dZoom);

	// If dZoom > 0: Gets the virtual image size when applying the given zoom factor
	// If dZoom < 0: Gets the image size to fit the image to the given screen size, using the given auto zoom mode.
	//               dZoom is filled with the used zoom factor to resize to this new size in this case.
	// The returned size is limited to 65535 in cx and cy
	CSize GetVirtualImageSize(CSize originalImageSize, CSize screenSize, EAutoZoomMode eAutoZoomMode, double & dZoom);

	// Limits the given center based offsets so that the rectangle created by the offset and the given rectSize is totally inside of outerRect
	// outerRect has top,left coordinates 0/0
	CPoint LimitOffsets(const CPoint& offsets, const CSize & rectSize, const CSize & outerRect);

	// Inflate rectangle by given factor
	CRect InflateRect(const CRect& rect, float fAmount);

	// Fits a rectangle with the specified size into a fit rectangle and returns the adjusted rectangle, maintaining the aspect ratio of the size
	CRect FitRectIntoRect(CSize size, CRect fitRect);

	// Fills a rectangle with the specified size around a fit rectangle and returns the adjusted rectangle, maintaining the aspect ratio of the size
	CRect FillRectAroundRect(CSize size, CRect fillRect);

	// Gets the parameters for zooming the given zoom rectangle in an image of given size to a given window size.
	void GetZoomParameters(float & fZoom, CPoint & offsets, CSize imageSize, CSize windowSize, CRect zoomRect);

	// Gets a window rectangle (in screen coordinates) that fits the given image
	CRect GetWindowRectMatchingImageSize(HWND hWnd, CSize minSize, CSize maxSize, double& dZoom, CJPEGImage* pImage, bool bForceCenterWindow, bool bKeepAspectRatio);

	// Gets if the given image can be displayed without sampling down the image.
	bool CanDisplayImageWithoutResize(HWND hWnd, CJPEGImage* pImage);

	// Calculates the maximum included rectangle in a trapezoid, keeping the given aspect ratio of the sides
	CRect CalculateMaxIncludedRectKeepAR(const CTrapezoid& trapezoid, double dAspectRatio);

	// Gets the maximum client size for a framed window that fits into the working area of the screen the given window is placed on
	CSize GetMaxClientSize(HWND hWnd);

	// Gets the total border size of a window. This includes the frame sizes and the size of the window caption area.
	// The window size equals the client area size plus this border size
	CSize GetTotalBorderSize();

	// Converts the transition effect from string (as in INI or command line parameter) to enum
	ETransitionEffect ConvertTransitionEffectFromString(LPCTSTR str);

	// Tests if the CPU supports SSE or MMX(2)
	CPUType ProbeCPU(void);

	// Get number of cores per physical processor, not counting hyperthreading
	int NumCoresPerPhysicalProc(void);

	// Gets the path where JPEGView stores its application data, including a trailing backslash
	LPCTSTR JPEGViewAppDataPath();

	// Sets the path where JPEGView stores its application data, including a trailing backslash.
	// Default if not set is CSIDL_APPDATA/JPEGView/
	void SetJPEGViewAppDataPath(LPCTSTR sPath);

	// Checks if the given string matches the pattern definition given.
	// Pattern: * - will be matched by any number of characters
	//          ; - separates patterns
	//          other chars - must match (ignoring case)
	// In sMatchingPattern, the matching pattern is returned (ended with a ; or a null character)
	bool PatternMatch(LPCTSTR & sMatchingPattern, LPCTSTR sString, LPCTSTR sPattern);

	// Given two matching patterns as returned by PatternMatch() method, finds the more specific
	// pattern, e.g. C:\images\jan\* is more specific than C:\images\*
	// Returns 0 if equal patterns, 1 if pattern 1 is more specific and -1 if pattern 2 is more specific
	int FindMoreSpecificPattern(LPCTSTR sPattern1, LPCTSTR sPattern2);

	// Pad the given value to the next multiple of padvalue
	inline int DoPadding(int value, int padvalue) {
		return (value + padvalue - 1) & ~(padvalue - 1);
	}

	// Difference of next padded value and value
	inline int PaddedBytes(int value, int padvalue) {
		return (padvalue - ((value & (padvalue - 1)))) & (padvalue - 1);
	}

	// calculate CRC table
	void CalcCRCTable(unsigned int crc_table[256]);

	// Finds a JPEG marker (beginning with 0xFF) in a JPEG stream. 
	// To find the first non-marker block, set nMarker to zero.
	void* FindJPEGMarker(void* pJPEGStream, int nStreamLength, unsigned char nMarker);

	// Finds the EXIF block in the JPEG bytes stream, NULL if no EXIF block
	void* FindEXIFBlock(void* pJPEGStream, int nStreamLength);

	// Calculates a hash value over the given JPEG stream having the given length (in bytes).
	// All blocks and tables in the JPEG stream are not included into the hash to allow
	// e.g. commenting the JPEG or changing some EXIF information without changing the hash.
	__int64 CalculateJPEGFileHash(void* pJPEGStream, int nStreamLength);

	// try to convert a string from UTF-8. Returns empty string if no valid UTF-8 encoded string.
	CString TryConvertFromUTF8(uint8* pComment, int nLengthInBytes);

	// Gets the content of the JPEG comment tag, empty string if no comment
	CString GetJPEGComment(void* pJPEGStream, int nStreamLength);

	// Clears the JPEG comment (by filling with NULL characters)
	void ClearJPEGComment(void* pJPEGStream, int nStreamLength);

	// Gets the image format given a file name (uses the file extension)
	EImageFormat GetImageFormat(LPCTSTR sFileName);

	// Returns the short form of the path (including the short form of the file name)
	CString GetShortFilePath(LPCTSTR sPath);

	// Returns the short form of the path but the file name remains untouched
	CString ReplacePathByShortForm(LPCTSTR sPath);

	// search string in string (as strstr()) ignoring case
	LPTSTR stristr(LPCTSTR szStringToBeSearched, LPCTSTR szSubstringToSearchFor);
	
	// file size for file with path
	__int64 GetFileSize(LPCTSTR sPath);

	// Gets the frame index of the next frame, depending on the index of the last image (relevant if the image is a multiframe image)
	int GetFrameIndex(CJPEGImage* pImage, bool bNext, bool bPlayAnimation, bool & switchImage);

	// Gets an index string of the form [a/b] for multiframe images, empty string for single frame images
	CString GetMultiframeIndex(CJPEGImage* pImage);

	// replaces the file info format string by the actual values from the image and file list
	CString GetFileInfoString(LPCTSTR sFormat, CJPEGImage* pImage, CFileList* pFilelist, double dZoom);

	// Returns the windows version in the format Major * 100 + Minor, e.g. 602 for Windows 8
	int GetWindowsVersion();

	// Conversion class that replaces the | by null character in a string.
	// Caution: Uses a static buffer and can therefore only one string can be replaced concurrently
	const int MAX_SIZE_REPLACE_PIPE = 512;

	class CReplacePipe {
	public:
		CReplacePipe(LPCTSTR sText);;

		operator LPCTSTR() const {
			return sm_buffer;
		}

	private:
		static TCHAR sm_buffer[MAX_SIZE_REPLACE_PIPE];
	};
	//------------------------------------------------------------------------------------------------


	// Critical section that is acquired on construction and freed on destruction
	class CAutoCriticalSection {
	public:
		// The critical section mush have been initialized prior to using it with this class
		CAutoCriticalSection(CRITICAL_SECTION & cs) : m_cs(cs) {
			::EnterCriticalSection(&m_cs);
		}

		~CAutoCriticalSection() {
			::LeaveCriticalSection(&m_cs);
		}
	private:
		CRITICAL_SECTION & m_cs;
	};
}