#include "StdAfx.h"
#include "EXIFHelpers.h"
#include "JPEGImage.h"
#include "EXIFReader.h"
#include <gdiplus.h>
#include <gdiplusimaging.h>

namespace EXIFHelpers {

static void FindFiles(LPCTSTR sDirectory, LPCTSTR sEnding, std::list<CString> &fileList) {
	CFindFile fileFind;
	CString sPattern = CString(sDirectory) + _T('\\') + sEnding;
	if (fileFind.FindFile(sPattern)) {
		fileList.push_back(fileFind.GetFilePath());
		while (fileFind.FindNextFile()) {
			fileList.push_back(fileFind.GetFilePath());
		}
	}
}

bool SetModificationDate(LPCTSTR sFileName, const SYSTEMTIME& time) {
	HANDLE hFile = ::CreateFile(sFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		return false;
	}
	FILETIME ft;
	::SystemTimeToFileTime(&time, &ft);  // converts to file time format
	BOOL bOk = ::SetFileTime(hFile, NULL, NULL, &ft);
	::CloseHandle(hFile);
	return bOk;
}

bool SetModificationDateToEXIF(LPCTSTR sFileName, CJPEGImage* pImage) {
	if (pImage->GetEXIFReader() == NULL || !pImage->GetEXIFReader()->GetAcquisitionTimePresent()) {
		return false;
	}
	SYSTEMTIME st = pImage->GetEXIFReader()->GetAcquisitionTime();
	// EXIF times are always local times, convert to UTC
	TIME_ZONE_INFORMATION tzi;
	::GetTimeZoneInformation(&tzi);
	::TzSpecificLocalTimeToSystemTime(&tzi, &st, &st);
	return SetModificationDate(sFileName, st);
}

bool SetModificationDateToEXIF(LPCTSTR sFileName) {
	bool bSuccess = false;
	Gdiplus::Bitmap* pBitmap = new Gdiplus::Bitmap(sFileName);
	if (pBitmap->GetLastStatus() == Gdiplus::Ok) {
		int nSize = pBitmap->GetPropertyItemSize(PropertyTagExifDTOrig);
		if (nSize > 0) {
			Gdiplus::PropertyItem* pItem = (Gdiplus::PropertyItem*)malloc(nSize);
			if (pBitmap->GetPropertyItem(PropertyTagExifDTOrig, nSize, pItem) == Gdiplus::Ok) {
				SYSTEMTIME time;
				if (CEXIFReader::ParseDateString(time, CString((char*)pItem->value))) {
					delete pBitmap; pBitmap = NULL; // else the file is locked
					TIME_ZONE_INFORMATION tzi;
					::GetTimeZoneInformation(&tzi);
					::TzSpecificLocalTimeToSystemTime(&tzi, &time, &time);
					bSuccess = SetModificationDate(sFileName, time);
				}
			}
			free(pItem);
		}
	}
	delete pBitmap;
	return bSuccess;
}

EXIFResult SetModificationDateToEXIFAllFiles(LPCTSTR sDirectory) {
	std::list<CString> listFileNames;
	FindFiles(sDirectory, _T("*.jpg"), listFileNames);
	FindFiles(sDirectory, _T("*.jpeg"), listFileNames);

	int nNumFailed = 0;
	std::list<CString>::iterator iter;
	for (iter = listFileNames.begin( ); iter != listFileNames.end( ); iter++ ) {
		if (!SetModificationDateToEXIF(*iter)) {
			nNumFailed++;
		}
	}

	EXIFResult result;
	result.NumberOfSucceededFiles = listFileNames.size() - nNumFailed;
	result.NumberOfFailedFiles = nNumFailed;
	return result;
}

}