#include "StdAfx.h"
#include "ParameterDB.h"
#include "SettingsProvider.h"
#include "HistogramCorr.h"
#include "Helpers.h"
#include "NLS.h"
#include <math.h>
#include <shlobj.h>

const TCHAR PARAM_DB_NAME[] = _T("ParamDB.db");
const uint32 MAGIC_HEADER_1 = 0xe8d2862a;
const uint32 MAGIC_HEADER_2 = 0xb651e752;
const uint32 DB_FILE_VERSION = 2;
const uint32 MAX_DB_FILE_SIZE = sizeof(CParameterDBEntry)*1024*100; // 3.2 MB, enough room for 100000 entries

/////////////////////////////////////////////////////////////////////////////////////////////
// Error handling helpers
/////////////////////////////////////////////////////////////////////////////////////////////

enum EFileError {
	errorOpenFailed,
	errorHeaderInvalid,
	errorWriteFailed,
	errorReadFailed,
	errorTooLarge,
	errorVersion,
	errorRenameFailed
};

static void HandleErrorAndCloseHandle(EFileError eError, LPCTSTR sParamDBName, HANDLE hFile) {
	CString sError;
	bool bLastError = false;
	switch (eError) {
		case errorOpenFailed:
			sError.Format(CNLS::GetString(_T("Parameter DB file '%s' could not be opened!")), sParamDBName);
			bLastError = true;
			break;
		case errorHeaderInvalid:
			sError.Format(CNLS::GetString(_T("Parameter DB file '%s' is not valid (invalid header)!")), sParamDBName);
			bLastError = false;
			break;
		case errorWriteFailed:
			sError.Format(CNLS::GetString(_T("Cannot write to parameter DB file '%s'!")), sParamDBName);
			bLastError = true;
			break;
		case errorReadFailed:
			sError.Format(CNLS::GetString(_T("Cannot read from parameter DB file '%s'!")), sParamDBName);
			bLastError = true;
			break;
		case errorTooLarge:
			sError.Format(CNLS::GetString(_T("Cannot read parameter DB file '%s' because it is too large!")), sParamDBName);
			bLastError = false;
			break;
		case errorVersion:
			sError.Format(CNLS::GetString(_T("The parameter DB file '%s' has been created by a newer version of JPEGView!")), sParamDBName);
			bLastError = false;
			break;
		case errorRenameFailed:
			sError.Format(CNLS::GetString(_T("The old parameter DB file '%s' could not be renamed during conversion to new format!")), sParamDBName);
			bLastError = true;
			break;
	}
	if (bLastError) {
		DWORD lastError = ::GetLastError();
		LPTSTR lpMsgBuf = NULL;
		::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, lastError,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL);
		sError += _T("\n");
		sError += CNLS::GetString(_T("Reason: "));
		sError += lpMsgBuf;
		LocalFree(lpMsgBuf);
	}

	::MessageBox(NULL, sError, CNLS::GetString(_T("Parameter DB error")), MB_OK | MB_ICONSTOP);

	if (hFile != 0) {
		::CloseHandle(hFile);
	}
}

// check the header of the parameter DB file
static bool CheckHeader(HANDLE hFile, int & nVersion) {
	DWORD numRead;
	ParameterDBHeader header;
	::ReadFile(hFile, &header, sizeof(ParameterDBHeader), &numRead, NULL);
	bool bHeaderOk = numRead == sizeof(ParameterDBHeader);
	if (bHeaderOk) {
		bHeaderOk = (header.nZero1 == 0 && header.nZero2 == 0 && 
			header.nMagic1 == MAGIC_HEADER_1 && header.nMagic2 == MAGIC_HEADER_2);
		nVersion = header.nVersion;
	}
	return bHeaderOk;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// CParameterDBEntry
/////////////////////////////////////////////////////////////////////////////////////////////

CParameterDBEntry::CParameterDBEntry() {
	memset(this, 0, sizeof(CParameterDBEntry));
}

void CParameterDBEntry::InitFromProcessParams(const CImageProcessingParams & processParams, 
											  EProcessingFlags eFlags, int nRotation) {

	contrast = Convert((float)processParams.Contrast, -0.5f, 0.5f, false);
	brightness = Convert((float)processParams.Gamma, 0.5f, 2.0f, false);
	sharpen = Convert((float)processParams.Sharpen, 0.0f, 0.5f, false);
	lightenShadows = Convert((float)processParams.LightenShadows, 0.0f, 1.0f, false);
	darkenHighlights = Convert((float)processParams.DarkenHighlights, 0.0f, 1.0f, false);
	lightenShadowSteepness = Convert((float)processParams.LightenShadowSteepness, 0.0f, 1.0f, false);
	colorCorrection = Convert((float)processParams.ColorCorrectionFactor, -0.5f, 0.5f, false);
	contrastCorrection = Convert((float)processParams.ContrastCorrectionFactor, 0.0f, 1.0f, false);
	cyanRed = Convert((float)processParams.CyanRed, -1.0f, 1.0f, false);
	magentaGreen = Convert((float)processParams.MagentaGreen, -1.0f, 1.0f, false);
	yellowBlue = Convert((float)processParams.YellowBlue, -1.0f, 1.0f, false);

	flags = 0;
	if (GetProcessingFlag(eFlags, PFLAG_AutoContrast)) flags |= 1;
	if (GetProcessingFlag(eFlags, PFLAG_LDC)) flags |= 2;
	if (GetProcessingFlag(eFlags, PFLAG_HighQualityResampling)) flags |= 4;
	if (nRotation == 90) {
		flags |= 8;
	} else if (nRotation == 180) {
		flags |= 16;
	} else if (nRotation == 270) {
		flags |= 24;
	}

	float * pCorrections = CSettingsProvider::This().ColorCorrectionAmounts();
	for (int i = 0; i < 6; i++) {
		colorCorrectionAmount[i] = Convert(pCorrections[i], 0.0f, 1.0f, false);
	}
}

void CParameterDBEntry::WriteToProcessParams(CImageProcessingParams & processParams, 
											 EProcessingFlags & eFlags, int & nRotation) const {
 	processParams.Contrast = Convert(contrast, -0.5f, 0.5f, false);
	processParams.Gamma = Convert(brightness, 0.5f, 2.0f, false);
	processParams.Sharpen = Convert(sharpen, 0.0f, 0.5f, false);
	processParams.LightenShadows = Convert(lightenShadows, 0.0f, 1.0f, false);
	processParams.DarkenHighlights = Convert(darkenHighlights, 0.0f, 1.0f, false);
	processParams.LightenShadowSteepness = Convert(lightenShadowSteepness, 0.0f, 1.0f, false);
	processParams.ColorCorrectionFactor = Convert(colorCorrection, -0.5f, 0.5f, false);
	processParams.ContrastCorrectionFactor = Convert(contrastCorrection, 0.0f, 1.0f, false);
	processParams.CyanRed = Convert(cyanRed, -1.0f, 1.0f, false);
	processParams.MagentaGreen = Convert(magentaGreen, -1.0f, 1.0f, false);
	processParams.YellowBlue = Convert(yellowBlue, -1.0f, 1.0f, false);

	eFlags = SetProcessingFlag(eFlags, PFLAG_AutoContrast, (flags & 1) != 0);
	eFlags = SetProcessingFlag(eFlags, PFLAG_LDC, (flags & 2) != 0);
	eFlags = SetProcessingFlag(eFlags, PFLAG_HighQualityResampling, (flags & 4) != 0);

	if ((flags & 24) == 0) {
		nRotation = 0;
	} else if ((flags & 24) == 8) {
		nRotation = 90;
	} else if ((flags & 24) == 16) {
		nRotation = 180;
	} else if ((flags & 24) == 24) {
		nRotation = 270;
	}
}

void CParameterDBEntry::InitGeometricParams(CSize sourceSize, double fZoom, CPoint offsets, CSize targetSize) {
	CSize zoomSourceSize = CSize((int)(sourceSize.cx*fZoom + 0.5f), (int)(sourceSize.cy*fZoom + 0.5f));
	CRect rectSource(CPoint(-zoomSourceSize.cx/2, -zoomSourceSize.cy/2), CSize(zoomSourceSize.cx, zoomSourceSize.cy));
	rectSource.OffsetRect(offsets);
	CRect rectTarget(CPoint(-targetSize.cx/2, -targetSize.cy/2), CSize(targetSize.cx, targetSize.cy));

	if (rectSource.Width() == 0 || rectTarget.Width() == 0 || rectSource.Height() == 0 || rectTarget.Height() == 0) {
		assert(false);
		return;
	}

	// Target values are in (-32767, 0) if scaled image is smaller than screen and in (0, 32767) if scaled image
	// is larger than screen.
	// In case A, the values denote the fraction of the screen that the scaled image uses, in case B the values
	// denote the fraction of the image that is visible.
	
	int nStartX, nEndX;
	if (rectTarget.Width() > rectSource.Width()) {
		nStartX = -32767*(rectSource.left - rectTarget.left)/rectTarget.Width();
		nEndX = -32767*(rectSource.right - rectTarget.left)/rectTarget.Width();
		nStartX = max(-32767, min(0, nStartX));
		nEndX = max(-32767, min(0, nEndX));
	} else {
		nStartX = 32767*(rectTarget.left - rectSource.left)/rectSource.Width();
		nEndX = 32767*(rectTarget.right - rectSource.left)/rectSource.Width();
		nStartX = max(0, min(32767, nStartX));
		nEndX = max(0, min(32767, nEndX));
	}

	int nStartY, nEndY;
	if (rectTarget.Height() > rectSource.Height()) {
		nStartY = -32767*(rectSource.top - rectTarget.top)/rectTarget.Height();
		nEndY = -32767*(rectSource.bottom - rectTarget.top)/rectTarget.Height();
		nStartY = max(-32767, min(0, nStartY));
		nEndY = max(-32767, min(0, nEndY));
	} else {
		nStartY = 32767*(rectTarget.top - rectSource.top)/rectSource.Height();
		nEndY = 32767*(rectTarget.bottom - rectSource.top)/rectSource.Height();
		nStartY = max(0, min(32767, nStartY));
		nEndY = max(0, min(32767, nEndY));
	}

	zoomRectLeft = (int16) nStartX;
	zoomRectTop = (int16) nStartY;
	zoomRectRigth = (int16) nEndX;
	zoomRectBottom = (int16) nEndY;
}

void CParameterDBEntry::WriteToGeometricParams(double & fZoom, CPoint & offsets, CSize imageSize, CSize windowSize) {
	if (!HasZoomOffsetStored()) {
		return; // no zoom is stored
	}

	double fOldZoom = fZoom;
	if (zoomRectRigth < 0 && zoomRectBottom < 0) {
		offsets = CPoint(0, 0);
		if ((float)imageSize.cx/imageSize.cy < (float)windowSize.cx/windowSize.cy) {
			// Height is dominating
			fZoom = windowSize.cy*((double)(-zoomRectBottom + zoomRectTop)/32767)/imageSize.cy;
		} else {
			// Width is dominating
			fZoom = windowSize.cx*((double)(-zoomRectRigth + zoomRectLeft)/32767)/imageSize.cx;
		}
		if (fZoom < 0.01 || fZoom > 40) {
			fZoom = fOldZoom;
		}
		return;
	}

	float fSectionW = imageSize.cx*(zoomRectRigth - zoomRectLeft)/32767.0f;
	float fSectionH = imageSize.cy*(zoomRectBottom - zoomRectTop)/32767.0f;
	if (zoomRectRigth < 0) {
		fSectionW = (float)imageSize.cx;
	} else if (zoomRectBottom < 0) {
		fSectionH = (float)imageSize.cy;
	}
	bool bTakeW = ((float)windowSize.cx/windowSize.cy < fSectionW/fSectionH);
	if (bTakeW) {
		// Width is dominating
		fZoom = windowSize.cx/fSectionW;
	} else {
		// Height is dominating
		fZoom = windowSize.cy/fSectionH;
	}
	if (fZoom < 0.01 || fZoom > 40) {
		fZoom = fOldZoom;
		return;
	}

	int nOffsetX = 0;
	if (zoomRectRigth >= 0) {
		double fZoomedImageSX = fZoom*imageSize.cx;
		int nZoomRectMiddleX = (zoomRectRigth + zoomRectLeft)/2;
		nOffsetX = Helpers::RoundToInt(fZoomedImageSX*0.5 - fZoomedImageSX*(double)nZoomRectMiddleX/32767);
	}
	int nOffsetY = 0;
	if (zoomRectBottom >= 0) {
		double fZoomedImageSY = fZoom*imageSize.cy;
		int nZoomRectMiddleY = (zoomRectBottom + zoomRectTop)/2;
		nOffsetY = Helpers::RoundToInt(fZoomedImageSY*0.5 - fZoomedImageSY*(double)nZoomRectMiddleY/32767);
	}
	offsets = CPoint(nOffsetX, nOffsetY);
}

void CParameterDBEntry::GetColorCorrectionAmounts(float corrections[6]) const {
	for (int i = 0; i < 6; i++) {
		corrections[i] = Convert(colorCorrectionAmount[i], 0.0f, 1.0f, false);
	}
}

uint8 CParameterDBEntry::Convert(float value, float lowerLimit, float upperLimit, bool isLog10) const {
	int nValue = 0;
	if (isLog10) {
		value = log10f(value);
	}
	value = max(lowerLimit, min(upperLimit, value));
	return (uint8)(0.5f + 255*(value - lowerLimit)/(upperLimit - lowerLimit));
}

float CParameterDBEntry::Convert(uint8 value, float lowerLimit, float upperLimit, bool isLog10) const {
	float fValue = value*(upperLimit - lowerLimit)/255.0f + lowerLimit;
	if (isLog10) {
		fValue = pow(fValue, 10);
	}
	return fValue;
}

bool CParameterDBEntry::HasZoomOffsetStored() const {
	if (zoomRectRigth == zoomRectLeft || zoomRectBottom == zoomRectTop) {
		return false; // no zoom is stored
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// CParameterDB
/////////////////////////////////////////////////////////////////////////////////////////////

CParameterDB* CParameterDB::sm_instance = NULL;

CParameterDB& CParameterDB::This() {
	if (sm_instance == NULL) {
		sm_instance = new CParameterDB();
	}
	return *sm_instance;
}
	
CParameterDBEntry* CParameterDB::FindEntry(__int64 nHash) {
	if (nHash == 0) {
		return NULL;
	}

	// Acquire lock before modifying the class
	Helpers::CAutoCriticalSection lock(m_csDBLock);

	// maintain the least recently used entry for fast access
	if (nHash == m_LRUHash) {
		return m_pLRUEntry;
	}
	int notUsed;
	CParameterDBEntry* pEntry = FindEntryInternal(nHash, notUsed);
	m_LRUHash = nHash;
	m_pLRUEntry = pEntry;
	return pEntry;
}

bool CParameterDB::DeleteEntry(__int64 nHash) {
	// Acquire lock before modifying the class
	Helpers::CAutoCriticalSection lock(m_csDBLock);

	if (nHash == m_LRUHash) {
		m_LRUHash = 0;
		m_pLRUEntry = NULL;
	}
	int nIndex = -1;
	CParameterDBEntry* pEntry = FindEntryInternal(nHash, nIndex);
	if (pEntry != NULL) {
		pEntry->SetHash(0);
		if (!SaveToFile(nIndex, *pEntry)) {
			// restore...
			pEntry->SetHash(nHash);
			return false;
		}
		return true;
	}
	return false;
}

bool CParameterDB::AddEntry(const CParameterDBEntry& newEntry) {
	// Acquire lock before modifying the class
	Helpers::CAutoCriticalSection lock(m_csDBLock);

	int nIndex = -1;
	CParameterDBEntry* pNewEntry = FindEntryInternal(newEntry.GetHash(), nIndex);
	if (pNewEntry == NULL) {
		pNewEntry = AllocateNewEntry(nIndex);
	}
	*pNewEntry = newEntry;
	if (!SaveToFile(nIndex, *pNewEntry)) {
		// restore...
		pNewEntry->SetHash(0);
		return false;
	}
	m_LRUHash = pNewEntry->GetHash();
	m_pLRUEntry = pNewEntry;
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Private
/////////////////////////////////////////////////////////////////////////////////////////////

CParameterDB::CParameterDB(void) {

	memset(&m_csDBLock, 0, sizeof(CRITICAL_SECTION));
	::InitializeCriticalSection(&m_csDBLock);

	m_LRUHash = 0;
	m_pLRUEntry = NULL;
	LoadFromFile();
}

CParameterDB::~CParameterDB(void) {
	// not implemented, never deleted (singleton)
}

CParameterDBEntry* CParameterDB::FindEntryInternal(__int64 nHash, int& nIndex) {
	nIndex = -1;
	std::list<DBBlock*>::iterator iter;
	for (iter = m_blockList.begin( ); iter != m_blockList.end( ); iter++ ) {
		DBBlock* pCurrentBlock = *iter;
		for (int i = 0; i < pCurrentBlock->UsedEntries; i++) {
			nIndex++;
			__int64 nThisHash = pCurrentBlock->Block[i].GetHash();
			if (nThisHash != 0 && nThisHash == nHash) {
				return &(pCurrentBlock->Block[i]);
			}
		}
	}
	return NULL;
}

CParameterDBEntry* CParameterDB::AllocateNewEntry(int& nIndex) {
	const int BLOCK_SIZE = 64;
	// First try to find a free entry, marked by a hash of 0
	nIndex = -1;
	std::list<DBBlock*>::iterator iter;
	for (iter = m_blockList.begin( ); iter != m_blockList.end( ); iter++ ) {
		DBBlock* pCurrentBlock = *iter;
		for (int i = 0; i < pCurrentBlock->BlockLen; i++) {
			nIndex++;
			if (i >= pCurrentBlock->UsedEntries || pCurrentBlock->Block[i].GetHash() == 0) {
				pCurrentBlock->UsedEntries = max(i+1, pCurrentBlock->UsedEntries);
				return &(pCurrentBlock->Block[i]);
			}
		}
	}
	// No free entry found, create new block
	DBBlock* pBlock = new DBBlock();
	pBlock->Block = new CParameterDBEntry[BLOCK_SIZE];
	pBlock->BlockLen = BLOCK_SIZE;
	pBlock->UsedEntries = 1;
	memset(pBlock->Block, 0, sizeof(CParameterDBEntry)*BLOCK_SIZE);
	m_blockList.push_back(pBlock);
	nIndex++;
	return &(pBlock->Block[0]);
}

bool CParameterDB::LoadFromFile() {
	CString sParamDBName = CString(Helpers::JPEGViewAppDataPath()) + PARAM_DB_NAME;

	// file does not exist - no error
	if (::GetFileAttributes(sParamDBName) == INVALID_FILE_ATTRIBUTES) {
		return true;
	}

	// file exists, open it
	HANDLE hFile = ::CreateFile(sParamDBName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		HandleErrorAndCloseHandle(errorOpenFailed, sParamDBName, 0);
		return true;
	}

	int nVersion;
	if (!CheckHeader(hFile, nVersion)) {
		HandleErrorAndCloseHandle(errorHeaderInvalid, sParamDBName, hFile);
		return false;
	}

	// check version and convert if possible
	if (nVersion == 1) {
		// convert parameter DB file
		if (ConvertVersion1To2(hFile, sParamDBName)) {
			return LoadFromFile();
		} else {
			return false;
		}
	} else if (nVersion > DB_FILE_VERSION) {
		HandleErrorAndCloseHandle(errorHeaderInvalid, sParamDBName, hFile);
		return false;
	}

	// Check if file is too large
	__int64 nFileSize;
	::GetFileSizeEx(hFile, (PLARGE_INTEGER)&nFileSize);
	if (nFileSize > MAX_DB_FILE_SIZE) {
		HandleErrorAndCloseHandle(errorTooLarge, sParamDBName, hFile);
		return false;
	}
	int nBlocks = (uint32)(nFileSize - sizeof(ParameterDBHeader))/sizeof(CParameterDBEntry);

	// if not, read whole file into one block
	DBBlock* pBlock = new DBBlock();
	pBlock->Block = new CParameterDBEntry[nBlocks];
	pBlock->BlockLen = nBlocks;
	DWORD numRead;
	::ReadFile(hFile, pBlock->Block, sizeof(CParameterDBEntry)*nBlocks, &numRead, NULL);
	if (numRead == 0) {
		HandleErrorAndCloseHandle(errorReadFailed, sParamDBName, hFile);
		delete[] pBlock->Block;
		delete pBlock;
		return false;
	}

	pBlock->UsedEntries = numRead/sizeof(CParameterDBEntry);
	m_blockList.push_back(pBlock);

	// Close file and exit
	::CloseHandle(hFile);
	return true;
}

bool CParameterDB::SaveToFile(int nIndex, const CParameterDBEntry & dbEntry) {
	CString sParamDBName = CString(Helpers::JPEGViewAppDataPath()) + PARAM_DB_NAME;

	// Create the directory in the application data path if it does not exist
	if (::GetFileAttributes(Helpers::JPEGViewAppDataPath()) == INVALID_FILE_ATTRIBUTES) {
		::CreateDirectory(Helpers::JPEGViewAppDataPath(), NULL);
	}

	HANDLE hFile = ::CreateFile(sParamDBName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		HandleErrorAndCloseHandle(errorOpenFailed, sParamDBName, 0);
		return false;
	}

	// Check header, do not override unknown files or versions
	__int64 nFileSize;
	::GetFileSizeEx(hFile, (PLARGE_INTEGER)&nFileSize);
	if (nFileSize >= sizeof(ParameterDBHeader)) {
		// check the header
		int nVersion = 0;
		if (!CheckHeader(hFile, nVersion) || nVersion != DB_FILE_VERSION) {
			HandleErrorAndCloseHandle(errorHeaderInvalid, sParamDBName, hFile);
			return false;
		}
	} else if (nFileSize == 0) {
		// new file created, write the header
		ParameterDBHeader header;
		memset(&header, 0, sizeof(ParameterDBHeader));
		header.nMagic1 = MAGIC_HEADER_1;
		header.nMagic2 = MAGIC_HEADER_2;
		header.nVersion = DB_FILE_VERSION;
		DWORD numWritten;
		::WriteFile(hFile, &header, sizeof(ParameterDBHeader), &numWritten, NULL);
		if (numWritten != sizeof(ParameterDBHeader)) {
			HandleErrorAndCloseHandle(errorWriteFailed, sParamDBName, hFile);
			return false;
		}
	} else {
		HandleErrorAndCloseHandle(errorHeaderInvalid, sParamDBName, hFile);
		return false;
	}

	// Seek to position of new entry to write
	if (::SetFilePointer(hFile, (nIndex + 1)*sizeof(CParameterDBEntry), NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
		HandleErrorAndCloseHandle(errorWriteFailed, sParamDBName, hFile);
		return false;
	}

	// Write entry
	DWORD numWritten;
	::WriteFile(hFile, &dbEntry, sizeof(ParameterDBHeader), &numWritten, NULL);
	if (numWritten != sizeof(ParameterDBHeader)) {
		HandleErrorAndCloseHandle(errorWriteFailed, sParamDBName, hFile);
		return false;
	}

	// Close file and exit
	::CloseHandle(hFile);
	return true;
}

bool CParameterDB::ConvertVersion1To2(HANDLE hFile, const CString& sFileName) {
	__int64 nFileSize;
	::GetFileSizeEx(hFile, (PLARGE_INTEGER)&nFileSize);
	if (nFileSize > MAX_DB_FILE_SIZE) {
		HandleErrorAndCloseHandle(errorTooLarge, sFileName, hFile);
		return false;
	}
	DWORD numRead;
	uint8* pSource = new uint8[(int)nFileSize];
	::SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
	::ReadFile(hFile, pSource, (DWORD)nFileSize, &numRead, NULL);
	if (numRead == 0) {
		HandleErrorAndCloseHandle(errorReadFailed, sFileName, hFile);
		delete[] pSource;
		return false;
	}
	int nOldEntries = (int)nFileSize/32;
	uint8* pTarget = new uint8[nOldEntries*sizeof(CParameterDBEntry)];
	memset(pTarget, 0, nOldEntries*sizeof(CParameterDBEntry));
	for (int i = 0; i < nOldEntries; i++) {
		CParameterDBEntry* pTgt = (CParameterDBEntry*)(pTarget + i*sizeof(CParameterDBEntry));
		memcpy(pTgt, pSource + i*32, 32);
		if (i > 0) {
			pTgt->cyanRed = 128;
			pTgt->magentaGreen = 128;
			pTgt->yellowBlue = 128;
		}
	}
	((ParameterDBHeader*)pTarget)->nVersion = DB_FILE_VERSION;

	delete[] pSource;

	// rename old file and save new
	::CloseHandle(hFile);
	if (!::MoveFile(sFileName, sFileName + _T(".v1"))) {
		delete[] pTarget;
		HandleErrorAndCloseHandle(errorRenameFailed, sFileName, 0);
		return false;
	}

	HANDLE hFileWrite = ::CreateFile(sFileName, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		delete[] pTarget;
		HandleErrorAndCloseHandle(errorOpenFailed, sFileName, 0);
		return false;
	}
	DWORD numWritten;
	::WriteFile(hFileWrite, pTarget, nOldEntries*sizeof(CParameterDBEntry), &numWritten, NULL);
	delete[] pTarget;
	if (numWritten != nOldEntries*sizeof(CParameterDBEntry)) {
		HandleErrorAndCloseHandle(errorWriteFailed, sFileName, hFileWrite);
		return false;
	}
	::CloseHandle(hFileWrite);
	return true;
}