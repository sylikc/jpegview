#include "StdAfx.h"
#include "JPEGLosslessTransform.h"
#include "libjpeg-turbo\include\turbojpeg.h"

CJPEGLosslessTransform::EResult _DoTransformation(LPCTSTR sInputFile, LPCTSTR sOutputFile, tjtransform &transform);
unsigned char* _ReadFile(LPCTSTR sFileName, unsigned int & nLengthBytes);
bool _WriteFile(LPCTSTR sFileName, unsigned char* pBuffer, unsigned int nLengthBytes);
int _TransformationEnumToOpCode(CJPEGLosslessTransform::ETransformation transformation);

// Performs a lossless JPEG transformation, transforming the input file and writing the result to the output file.
// Input and output file can be identical, then the input file is overriden by the resulting output file.
CJPEGLosslessTransform::EResult CJPEGLosslessTransform::PerformTransformation(LPCTSTR sInputFile, LPCTSTR sOutputFile, 
	CJPEGLosslessTransform::ETransformation transformation, bool bAllowTrim) {
	tjtransform transform;
	memset(&transform, 0, sizeof(tjtransform));
	transform.op = _TransformationEnumToOpCode(transformation);
	transform.options = bAllowTrim ? TJXOPT_TRIM : TJXOPT_PERFECT;

	return _DoTransformation(sInputFile, sOutputFile, transform);
}

// Performs a lossless JPEG crop, using the input file and writing the result to the output file.
// Input and output file can be identical, then the input file is overriden by the resulting output file.
CJPEGLosslessTransform::EResult CJPEGLosslessTransform::PerformCrop(LPCTSTR sInputFile, LPCTSTR sOutputFile, const CRect& cropRect) {
	tjtransform transform;
	memset(&transform, 0, sizeof(tjtransform));
	transform.op = TJXOP_NONE;
	transform.options = TJXOPT_PERFECT | TJXOPT_CROP;
	transform.r.x = cropRect.left;
	transform.r.y = cropRect.top;
	transform.r.w = cropRect.Width();
	transform.r.h = cropRect.Height();

	return _DoTransformation(sInputFile, sOutputFile, transform);
}


static CJPEGLosslessTransform::EResult _DoTransformation(LPCTSTR sInputFile, LPCTSTR sOutputFile, tjtransform &transform) {
	CJPEGLosslessTransform::EResult eResult = CJPEGLosslessTransform::Success;

	tjhandle hTransform = tjInitTransform();

	unsigned int nNumBytesInput;
	unsigned char* pInputJPEGBytes = _ReadFile(sInputFile, nNumBytesInput);
	if (pInputJPEGBytes != NULL) {
		unsigned char* pOutputJPEGBytes = NULL;
		unsigned long nNumBytesOutput = 0;
		if (0 == tjTransform(hTransform, pInputJPEGBytes, nNumBytesInput, 1, &pOutputJPEGBytes, &nNumBytesOutput, &transform, 0) && pOutputJPEGBytes != NULL) {
			if (!_WriteFile(sOutputFile, pOutputJPEGBytes, nNumBytesOutput)) {
				eResult = CJPEGLosslessTransform::WriteFileFailed;
			}
		} else {
			eResult = CJPEGLosslessTransform::TransformationFailed;
		}
		if (pOutputJPEGBytes != NULL) {
			tjFree(pOutputJPEGBytes);
		}
	} else {
		eResult = CJPEGLosslessTransform::ReadFileFailed;
	}

	delete[] pInputJPEGBytes;

	tjDestroy(hTransform);

	return eResult;
}

static unsigned char* _ReadFile(LPCTSTR sFileName, unsigned int & nLengthBytes) {
	const unsigned int MAX_JPEG_FILE_SIZE = 1024*1024*50; // 50 MB

	nLengthBytes = 0;
	HANDLE hFile = ::CreateFile(sFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		return NULL;
	}

	unsigned int nFileSize = ::GetFileSize(hFile, NULL);
	if (nFileSize > MAX_JPEG_FILE_SIZE) {
		::CloseHandle(hFile);
		return NULL;
	}

	unsigned char *pBuffer = new(std::nothrow) unsigned char[nFileSize];
	if (pBuffer == NULL) {
		::CloseHandle(hFile);
		return NULL;
	}

	unsigned int nNumBytesRead;
	if (::ReadFile(hFile, pBuffer, nFileSize, (LPDWORD) &nNumBytesRead, NULL) && nNumBytesRead == nFileSize) {
		::CloseHandle(hFile);
		nLengthBytes = nFileSize;
		return pBuffer;
	}
	delete[] pBuffer;
	::CloseHandle(hFile);
	return NULL;
}

static bool _WriteFile(LPCTSTR sFileName, unsigned char* pBuffer, unsigned int nLengthBytes) {
	LPCTSTR sTempEnding = _T("");
	FILETIME lastWriteTime;
	bool bRestoreWriteTime = false;
	HANDLE hFile = ::CreateFile(sFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile != INVALID_HANDLE_VALUE) {
		bRestoreWriteTime = ::GetFileTime(hFile, NULL, NULL, &lastWriteTime);
		::CloseHandle(hFile);
		sTempEnding = _T(".tmp");
	}

	// For security reasons, we never write to an existing file. So generate a tmp file if the file already exists.
	// If all writing succeeds, the existing file is finally replaced by the temporary one.
	CString sNewFileName = CString(sFileName) + sTempEnding;
	hFile = ::CreateFile(sNewFileName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		return false;
	}

	unsigned int nNumBytesWritten;
	bool bOk = ::WriteFile(hFile, pBuffer, nLengthBytes, (LPDWORD) &nNumBytesWritten, NULL) && nNumBytesWritten == nLengthBytes;
	
	if (bOk && bRestoreWriteTime) {
		::SetFileTime(hFile, NULL, NULL, &lastWriteTime);
	}

	::CloseHandle(hFile);

	// If the file was written to a temporary file and it succeeded, replace now the existing file with the temporary
	if (bOk && sTempEnding[0] != 0) {
		bOk = ::DeleteFile(sFileName);
		if (bOk) {
			bOk = ::MoveFile(sNewFileName, sFileName);
		}
	} else if (!bOk) {
		// new file is only partly written or not at all, make sure it is deleted
		::DeleteFile(sNewFileName);
	}

	return bOk;
}

static int _TransformationEnumToOpCode(CJPEGLosslessTransform::ETransformation transformation) {
	switch (transformation) {
		case CJPEGLosslessTransform::Rotate90:
			return TJXOP_ROT90;
		case CJPEGLosslessTransform::Rotate270:
			return TJXOP_ROT270;
		case CJPEGLosslessTransform::Rotate180:
			return TJXOP_ROT180;
		case CJPEGLosslessTransform::MirrorH:
			return TJXOP_HFLIP;
		case CJPEGLosslessTransform::MirrorV:
			return TJXOP_VFLIP;
		default:
			return TJXOP_NONE;
	}
}