#include "StdAfx.h"
#include "FileList.h"
#include "SettingsProvider.h"

///////////////////////////////////////////////////////////////////////////////////
// Helpers
///////////////////////////////////////////////////////////////////////////////////

// static intializers
Helpers::ESorting CFileDesc::sm_eSorting = Helpers::FS_LastModTime;
Helpers::ENavigationMode CFileList::sm_eMode = Helpers::NM_LoopDirectory;

// Helper to add the current file of filefind object to the list
static void AddToFileList(std::list<CFileDesc> & fileList, CFindFile & fileFind) {
	FILETIME lastWriteTime, creationTime;
	fileFind.GetLastWriteTime(&lastWriteTime);
	fileFind.GetCreationTime(&creationTime);
	CFileDesc thisFile(fileFind.GetFilePath(), &lastWriteTime, &creationTime);
	fileList.push_back(thisFile);
}

// Replace all numbers in string with asterix and return numbers separated with \0
static void ReplaceNumbers(LPCTSTR sFileName, TCHAR sNewFileName[MAX_PATH], TCHAR sNumberString[MAX_PATH]) {
	LPCTSTR source = sFileName;
	LPTSTR target = sNewFileName;
	LPTSTR numberString = sNumberString;
	numberString[1] = 0;
	int i = 0, j = 0;
	while (*source != 0 && i < MAX_PATH-1 && j < MAX_PATH-1) {
		while (*source != 0 && !(*source >= _T('0') && *source <= _T('9'))) {
			*target = *source;
			source++;
			if (i < MAX_PATH-1) {
				target++;
				i++;
			}
		}
		if (*source >= _T('0') && *source <= _T('9') && i < MAX_PATH-1) {
			*target++ = _T('*');
			i++;
			bool bTrailingZero = true;
			while (*source >= _T('0') && *source <= _T('9') && j < MAX_PATH-2) {
				if (*source != _T('0') || !bTrailingZero) {
					bTrailingZero = false;
					*numberString = *source;
					if (j < MAX_PATH-2) {
						numberString++;
						j++;
					}
				}
				source++;
			}
			if (j < MAX_PATH-1) {
				*numberString++ = 0;
				j++;
			}
		}

	}
	*target = 0;
	*numberString = 0;
}

///////////////////////////////////////////////////////////////////////////////////
// CFileDesc
///////////////////////////////////////////////////////////////////////////////////

CFileDesc::CFileDesc(const CString & sName, const FILETIME* lastModTime, const FILETIME* creationTime) {
	m_sName = sName;
	m_sTitle = (LPCTSTR)m_sName + sName.ReverseFind(_T('\\')) + 1;
	memcpy(&m_lastModTime, lastModTime, sizeof(FILETIME));
	memcpy(&m_creationTime, creationTime, sizeof(FILETIME));
	m_nRandomOrderNumber = rand();
}

bool CFileDesc::operator < (const CFileDesc& other) const {
	if (sm_eSorting == Helpers::FS_CreationTime || sm_eSorting == Helpers::FS_LastModTime) {
		const FILETIME* pTime = (sm_eSorting == Helpers::FS_LastModTime) ? &m_lastModTime : &m_creationTime;
		const FILETIME* pTimeOther = (sm_eSorting == Helpers::FS_LastModTime) ? &(other.m_lastModTime) : &(other.m_creationTime);
		if (pTime->dwHighDateTime < pTimeOther->dwHighDateTime) {
			return true;
		} else if (pTime->dwHighDateTime > pTimeOther->dwHighDateTime) {
			return false;
		} else {
			return pTime->dwLowDateTime < pTimeOther->dwLowDateTime;
		}
	} else if (sm_eSorting == Helpers::FS_Random) {
		return m_nRandomOrderNumber < other.m_nRandomOrderNumber;
	} else {
		// If the filename contains numbers, we want to sort the files
		// according to the numbers to place 'File9' before 'File10'
		// That's why sorting is this complicated
		TCHAR sNewFileNameThis[MAX_PATH];
		TCHAR sNewFileNameOther[MAX_PATH];
		TCHAR sNumberStringThis[MAX_PATH];
		TCHAR sNumberStringOther[MAX_PATH];
		ReplaceNumbers(m_sTitle, sNewFileNameThis, sNumberStringThis);
		ReplaceNumbers(other.m_sTitle, sNewFileNameOther, sNumberStringOther);
		LPCTSTR sNewNameThis = sNewFileNameThis;
		LPCTSTR sNewNameOther = sNewFileNameOther;
		LPCTSTR sNumberThis = sNumberStringThis;
		LPCTSTR sNumberOther = sNumberStringOther;
		int nCharCount = 0;

		while (*sNewNameThis != 0 && *sNewNameOther != 0) {
			if (*sNewNameThis == _T('*') || *sNewNameOther == _T('*')) {
				// One of the involved characters is an asterix and thus represents a number
				
				// compare substrings having no numbers involved
				if (nCharCount > 0) {
					int nCmp = _tcsnicoll(sNewNameThis - nCharCount, sNewNameOther - nCharCount, nCharCount);
					if (nCmp != 0) {
						return nCmp < 0;
					}
					nCharCount = 0;
				}

				if (*sNewNameThis == _T('*') && *sNewNameOther == _T('*')) {
					// both have a number at the same position, compare numbers
					if (*sNumberThis != 0 && *sNumberOther != 0) {
						// a longer number string is always bigger than a shorter (as trailing zeros are removed)
						int nLenThis = _tcslen(sNumberThis);
						int nLenOther = _tcslen(sNumberOther);
						int nLenCmp =  nLenThis - nLenOther;
						if (nLenCmp != 0) {
							return nLenCmp < 0;
						}
						// both strings have same length and contain only numbers, do ordinal string compare
						int nStrCmp = _tcscmp(sNumberThis, sNumberOther);
						if (nStrCmp != 0) {
							return nStrCmp < 0;
						}
						// both number strings are equal, goto next pair
						sNumberThis += nLenThis + 1;
						sNumberOther += nLenOther + 1;
					}
				} else if (*sNewNameThis == _T('*')) {
					return ((int)_T('0') - *sNewNameOther) < 0;
				} else if (*sNewNameOther == _T('*')) {
					return ((int)*sNewNameThis - _T('0')) < 0;
				}
			} else {
				nCharCount++;
			}
			sNewNameThis++;
			sNewNameOther++;
		}
		return _tcsicoll(m_sTitle, other.m_sTitle) < 0; // compare original ones if new strings are equal
	}
}


void CFileDesc::SetName(LPCTSTR sNewName) {
	m_sName = sNewName;
	m_sTitle = (LPCTSTR)m_sName + m_sName.ReverseFind(_T('\\')) + 1;
}

void CFileDesc::SetModificationDate(const FILETIME& lastModDate) {
	memcpy(&m_lastModTime, &lastModDate, sizeof(FILETIME));
}

///////////////////////////////////////////////////////////////////////////////////
// Public interface
///////////////////////////////////////////////////////////////////////////////////

// image file types supported internally (there are additional endings for RAW and WIC - these come from INI file)
static const int cnNumEndingsInternal = 8;
static const TCHAR* csFileEndingsInternal[cnNumEndingsInternal] = {_T("jpg"), _T("jpeg"), _T("bmp"), _T("png"), 
	_T("tif"), _T("tiff"), _T("gif"), _T("webp")};

static const int MAX_ENDINGS = 48;
static int nNumEndings;
static LPCTSTR* sFileEndings;

__declspec(dllimport) bool __stdcall WICPresent(void);

// Check if Windows Image Codecs library is present
static bool WICPresentGuarded(void) {
    try {
        return WICPresent();
    } catch (...) {
        return false;
    }
}

// Parses a semicolon separated list of file endings of the form "*.nef;*.cr2;*.dng"
static void ParseAndAddFileEndings(LPCTSTR sEndings) {
    if (_tcslen(sEndings) > 2) {
        LPTSTR buffer = new TCHAR[_tcslen(sEndings) + 1]; // this buffer will not be freed deliberately!
        _tcscpy(buffer, sEndings);
        LPTSTR sStart = buffer, sCurrent = buffer;
        while (*sCurrent != 0 && nNumEndings < MAX_ENDINGS) {
            while (*sCurrent != 0 && *sCurrent != _T(';')) {
                sCurrent++;
            }
            if (*sCurrent == _T(';')) {
                *sCurrent = 0;
                sCurrent++;
            }
            if (_tcslen(sStart) > 2) {
                sFileEndings[nNumEndings++] = sStart + 2;
            }
            sStart = sCurrent;
        }
	}
}

// Gets all supported file endings, including the ones from WIC.
// The length of the returned list is nNumEndings
static LPCTSTR* GetSupportedFileEndingList() {
    if (sFileEndings == NULL) {
        sFileEndings = new LPCTSTR[MAX_ENDINGS];
        for (nNumEndings = 0; nNumEndings < cnNumEndingsInternal; nNumEndings++) {
            sFileEndings[nNumEndings] = csFileEndingsInternal[nNumEndings];
        }

		LPCTSTR sFileEndingsWIC = CSettingsProvider::This().FilesProcessedByWIC();
		if (_tcslen(sFileEndingsWIC) > 2 && WICPresentGuarded()) {
			ParseAndAddFileEndings(sFileEndingsWIC);
		}
		ParseAndAddFileEndings(CSettingsProvider::This().FileEndingsRAW());
    }
    return sFileEndings;
}

CFileList::CFileList(const CString & sInitialFile, Helpers::ESorting eInitialSorting, bool bWrapAroundFolder, int nLevel) {
	CFileDesc::SetSorting(eInitialSorting);

	m_bDeleteHistory = true;
	m_bWrapAroundFolder = bWrapAroundFolder;
	m_sInitialFile = sInitialFile;
	m_nLevel = nLevel;
	m_next = m_prev = NULL;
	int nPos = sInitialFile.ReverseFind(_T('\\'));
	m_sDirectory = (nPos > 0) ? sInitialFile.Left(nPos) : _T(""); // the backslash is stripped away!
	nPos = sInitialFile.ReverseFind(_T('.'));
	bool bIsDirectory = (::GetFileAttributes(sInitialFile) & FILE_ATTRIBUTE_DIRECTORY) != 0;
	bool bImageFile = IsImageFile((nPos > 0) ? sInitialFile.Right(sInitialFile.GetLength()-nPos-1) : _T(""));
	m_bIsSlideShowList = !bImageFile && TryReadingSlideShowList(sInitialFile);
	m_nMarkedIndexShow = -1;

	if (!m_bIsSlideShowList) {
		if (bImageFile || bIsDirectory) {
			FindFiles();
			m_iter = FindFile(sInitialFile);
			m_iterStart = bWrapAroundFolder ? m_iter : m_fileList.begin();
		} else {
			// neither image file nor directory nor list of file names - try to read anyway but normally will fail
			CFindFile fileFind;
			fileFind.FindFile(sInitialFile);
			AddToFileList(m_fileList, fileFind);
			m_iter = m_iterStart = m_fileList.begin();
		}
	} else {
		sm_eMode = Helpers::NM_LoopDirectory;
		m_iter = m_iterStart = m_fileList.begin();
	}
}

CFileList::~CFileList() {
	if (m_bDeleteHistory) {
		DeleteHistory();
	}
	m_fileList.clear();
}

CString CFileList::GetSupportedFileEndings() {
    LPCTSTR* allFileEndings = GetSupportedFileEndingList();
	CString sList;
	for (int i = 0; i < nNumEndings; i++) {
		sList += _T("*.");
		sList += allFileEndings[i];
		if (i+1 < nNumEndings) sList += _T(";");
	}
	return sList;
}

void CFileList::Reload(LPCTSTR sFileName) {
	LPCTSTR sCurrent = sFileName;
	if (sCurrent == NULL) {
		sCurrent = Current();
		if (sCurrent == NULL) {
			return;
		}
	}

	// delete the chain of CFileLists forward
	CFileList* pFileList = this->m_next;
	while (pFileList != NULL && pFileList->m_prev != NULL) {
		CFileList* thisList = pFileList;
		pFileList = pFileList->m_next;
		delete thisList;
	}
	m_next = NULL;

	CString sCurrentFile = sCurrent;
	if (!m_bIsSlideShowList) {
		FindFiles();
		m_iterStart = m_bWrapAroundFolder ? FindFile(m_sInitialFile) : m_fileList.begin();
	} else {
		VerifyFiles(); // maybe some of the files got deleted or moved
		m_iterStart = m_fileList.begin();
	}
	m_iter = FindFile(sCurrentFile); // go again to current file
}

void CFileList::FileHasRenamed(LPCTSTR sOldFileName, LPCTSTR sNewFileName) {
	if (_tcsicmp(sOldFileName, m_sInitialFile) == 0) {
		m_sInitialFile = sNewFileName;
	}
	std::list<CFileDesc>::iterator iter;
	for (iter = m_fileList.begin( ); iter != m_fileList.end( ); iter++ ) {
		if (_tcsicmp(sOldFileName, iter->GetName()) == 0) {
			iter->SetName(sNewFileName);
		}
	}
}

void CFileList::ModificationTimeChanged() {
	if (m_iter != m_fileList.end()) {
		LPCTSTR sName = m_iter->GetName();
		HANDLE hFile = ::CreateFile(sName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		if (hFile != NULL) {
			FILETIME lastModTime;
			if (::GetFileTime(hFile, NULL, NULL, &lastModTime)) {
				m_iter->SetModificationDate(lastModTime);
			}
			::CloseHandle(hFile);
		}
	}
}

CFileList* CFileList::Next() {
	m_nMarkedIndexShow = -1;
	if (m_fileList.size() > 0) {
		std::list<CFileDesc>::iterator iterTemp = m_iter;
		iterTemp++;
		if (iterTemp == m_fileList.end()) {
			iterTemp = m_fileList.begin();
		}
		if (iterTemp == m_iterStart) {
			// we are finished with this folder
			if (!m_bWrapAroundFolder && sm_eMode == Helpers::NM_LoopDirectory) {
				return this;
			}
			CFileList* pNextList = WrapToNextImage();
			if (pNextList == NULL) {
				// no next found, go to source of the prev->prev chain
				pNextList = this;
				while (pNextList->m_prev != NULL) pNextList = pNextList->m_prev;
				if (pNextList != this) {
					return this; // stop here, do not wrap around
				}
			}
			if (pNextList != this) {
				// leave current iterator on m_iterStart and return the new list
				return pNextList;
			}
		}
		NextInFolder();
	} else {
		// current filelist is empty, try to go to another folder
		CFileList* pNextList = WrapToNextImage();
		if (pNextList != NULL) {
			return pNextList;
		}
	}
	return this;
}

CFileList* CFileList::Prev() {
	m_nMarkedIndexShow = -1;
	if (m_iter == m_iterStart) {
		if (sm_eMode == Helpers::NM_LoopDirectory) {
			if (!m_bWrapAroundFolder) {
				return this;
			}
			if (m_iter == m_fileList.begin()) {
				MoveIterToLast();
			} else {
				m_iter--;
			}
			return this;
		} else {
			return WrapToPrevImage();
		}
	}
	if (m_fileList.size() > 0) {
		if (m_iter == m_fileList.begin()) {
			m_iter = m_fileList.end();
			m_iter--;
		} else {
			m_iter--;
		}
	}
	return this;
}

void CFileList::First() {
	m_nMarkedIndexShow = -1;
	m_iter = m_iterStart = m_fileList.begin();
}

void CFileList::Last() {
	m_nMarkedIndexShow = -1;
	MoveIterToLast();
	m_iterStart = m_fileList.begin();
}

LPCTSTR CFileList::Current() const {
	if (m_iter != m_fileList.end()) {
		return m_iter->GetName();
	} else {
		return NULL;
	}
}

LPCTSTR CFileList::CurrentFileTitle() const {
	LPCTSTR sCurrent = Current();
	if (sCurrent != NULL) {
		LPCTSTR sTitle = _tcsrchr(sCurrent, _T('\\'));
		if (sTitle == NULL) sTitle = sCurrent; else sTitle++;
		return sTitle;
	} else {
		return NULL;
	}
}

LPCTSTR CFileList::CurrentDirectory() const {
	if (m_sDirectory.GetLength() > 0) {
		return m_sDirectory;
	} else {
		return NULL;
	}
}

int CFileList::CurrentIndex() const {
	int i = 0;
	std::list<CFileDesc>::const_iterator iter;
	for (iter = m_fileList.begin( ); iter != m_fileList.end( ); iter++ ) {
		if (iter == m_iter) {
			return i;
		}
		i++;
	}
	return -1;
}

const FILETIME* CFileList::CurrentModificationTime() const {
	if (m_iter != m_fileList.end()) {
		return &(m_iter->GetLastModTime());
	} else {
		return NULL;
	}
}

LPCTSTR CFileList::PeekNextPrev(int nIndex, bool bForward, bool bToggle) {
	if (bToggle) {
		return (m_nMarkedIndexShow == 0) ? m_sMarkedFile : m_sMarkedFileCurrent;
	} else {
		std::list<CFileDesc>::iterator thisIter = m_iter;
		LPCTSTR sFileName;
		if (nIndex != 0) {
			CFileList* pFL = bForward ? Next() : Prev();
			sFileName = pFL->PeekNextPrev(nIndex-1, bForward, bToggle);
		} else {
			sFileName = Current();
		}
		m_iter = thisIter;
		return sFileName;
	}
}

void CFileList::SetSorting(Helpers::ESorting eSorting) {
	if (eSorting != CFileDesc::GetSorting()) {
		CString sThisFile = (m_iter != m_fileList.end()) ? m_iter->GetName() : "";
		CFileDesc::SetSorting(eSorting);
		m_fileList.sort();
		m_iter = FindFile(sThisFile);
		m_iterStart = m_bWrapAroundFolder ? m_iter : m_fileList.begin();
	}
}

Helpers::ESorting CFileList::GetSorting() const {
	return CFileDesc::GetSorting();
}

void CFileList::SetNavigationMode(Helpers::ENavigationMode eMode) {
	if (m_bIsSlideShowList) {
		return;
	}

	sm_eMode = eMode;
	DeleteHistory();
	m_nLevel = 0;
	m_iterStart = m_bWrapAroundFolder ? m_iter : m_fileList.begin();
}

void CFileList::MarkCurrentFile() {
	LPCTSTR sCurrent = Current();
	if (::GetFileAttributes(sCurrent) != INVALID_FILE_ATTRIBUTES) {
		m_sMarkedFile = CString(sCurrent);
		m_sMarkedFileCurrent = _T("");
		m_nMarkedIndexShow = -1;
	}
}

bool CFileList::FileMarkedForToggle() {
	return !m_sMarkedFile.IsEmpty();
}

void CFileList::ToggleBetweenMarkedAndCurrentFile() {
	if (m_nMarkedIndexShow == -1) {
		m_nMarkedIndexShow = 0;
	}
	LPCTSTR sNewFile = (m_nMarkedIndexShow == 0) ? m_sMarkedFile : m_sMarkedFileCurrent;
	if (sNewFile[0] == 0) {
		return;
	}
	if (m_nMarkedIndexShow == 0) {
		m_sMarkedFileCurrent = Current();
	}

	m_sInitialFile = sNewFile;
	int nPos = m_sInitialFile.ReverseFind(_T('\\'));
	m_sDirectory = (nPos > 0) ? m_sInitialFile.Left(nPos) : _T(""); // the backslash is stripped away!

	DeleteHistory();
	Reload(m_sInitialFile);

	m_nMarkedIndexShow = (m_nMarkedIndexShow + 1) & 1;
}

///////////////////////////////////////////////////////////////////////////////////
// Private
///////////////////////////////////////////////////////////////////////////////////

void CFileList::DeleteHistory() {
	// delete the chain of CFileLists forward and backward and only leave the current node alive
	CFileList* pFileList = this->m_next;
	while (pFileList != NULL && pFileList->m_prev != NULL) {
		CFileList* thisList = pFileList;
		pFileList = pFileList->m_next;
		thisList->m_bDeleteHistory = false; // prevent multiple delete
		delete thisList;
	}
	pFileList = this->m_prev;
	while (pFileList != NULL) {
		CFileList* thisList = pFileList;
		pFileList = pFileList->m_prev;
		thisList->m_bDeleteHistory = false;  // prevent multiple delete
		delete thisList;
	}
	m_next = NULL;
	m_prev = NULL;
}

void CFileList::MoveIterToLast() {
	std::list<CFileDesc>::iterator lastIter = m_iter;
	while (m_iter != m_fileList.end()) {
		lastIter = m_iter;
		m_iter++;
	}
	m_iter = lastIter;
}

std::list<CFileDesc>::iterator CFileList::FindFile(const CString& sName) {
	int nStart = sName.ReverseFind(_T('\\')) + 1;
	if (nStart == sName.GetLength()) {
		return m_fileList.begin();
	}
	std::list<CFileDesc>::iterator iter;
	for (iter = m_fileList.begin( ); iter != m_fileList.end( ); iter++ ) {
		if (_tcsicmp((LPCTSTR)sName + nStart, iter->GetTitle()) == 0) {
			return iter;
		}
	}
	return m_fileList.begin(); // in case the file was not found
}

CFileList* CFileList::WrapToNextImage() {
	if (m_next != NULL) {
		assert(sm_eMode != Helpers::NM_LoopDirectory);
		return m_next;
	}

	if (sm_eMode == Helpers::NM_LoopDirectory) {
		return NULL;
	} else if (sm_eMode == Helpers::NM_LoopSameDirectoryLevel) {
		int nPos = m_sDirectory.ReverseFind(_T('\\'));
		if (nPos <= 0) {
			return NULL; // root dir - no siblings
		}
		CString sNextDirRoot = m_sDirectory.Left(nPos);
		CString sThisDirTitle = m_sDirectory.Right(m_sDirectory.GetLength() - nPos - 1);
		// collect all sibling folders
		CFindFile fileFind;
		std::list<CString> dirList;
		if (fileFind.FindFile(sNextDirRoot + "\\*")) {
			if (fileFind.IsDirectory() && !fileFind.IsDots()) {
				dirList.push_back(fileFind.GetFileName());
			}
			while (fileFind.FindNextFile()) {
				if (fileFind.IsDirectory() && !fileFind.IsDots()) {
					dirList.push_back(fileFind.GetFileName());
				}
			}
		}
		if (dirList.size() == 0) {
			return NULL; // no sibling folders
		}
		dirList.sort(); // sort is alphabetically

		// now find the current folder and take the next in list
		// to handle the wrap around, two iterations are needed (in case the current folder is the last in the list)
		std::list<CString>::iterator iter;
		bool bFound = false;
		for (int nStep = 0; nStep < 2; nStep++) {
			for (iter = dirList.begin( ); iter != dirList.end( ); iter++ ) {
				if (iter->CompareNoCase(sThisDirTitle) == 0) {
					bFound = true;
				} else if (bFound) {
					CFileList* pNewList = TryCreateFileList(sNextDirRoot + '\\' + *iter + '\\', 0);
					if (pNewList != NULL) {
						return pNewList;
					}
				}
			}
		}
	} else if (sm_eMode == Helpers::NM_LoopSubDirectories) {
		CFileList* pFileList = FindFileRecursively(m_sDirectory, _T(""), false, m_nLevel, 0);
		return pFileList;
	}
	return NULL;
}

CFileList* CFileList::FindFileRecursively (const CString& sDirectory, const CString& sFindAfter,
										   bool bSearchThisFolder, int nLevel, int nRecursion) {
	if (bSearchThisFolder) {
		CFileList* pFileList = TryCreateFileList(sDirectory + "\\", nLevel);
		if (pFileList != NULL) {
			return pFileList;
		}
	}
	// create a list of all child directories
	CFindFile fileFind;
	std::list<CString> dirList;
	if (fileFind.FindFile(sDirectory + "\\*")) {
		if (fileFind.IsDirectory() && !fileFind.IsDots()) {
			dirList.push_back(fileFind.GetFileName());
		}
		while (fileFind.FindNextFile()) {
			if (fileFind.IsDirectory() && !fileFind.IsDots()) {
				dirList.push_back(fileFind.GetFileName());
			}
		}
	}

	if (dirList.size() > 0) {
		dirList.sort();

		bool bStart = sFindAfter.GetLength() == 0;
		std::list<CString>::iterator iter;
		for (iter = dirList.begin( ); iter != dirList.end( ); iter++ ) {
			if (bStart) {
				CFileList* pFileList = FindFileRecursively(sDirectory + "\\" + *iter, _T(""), true, nLevel+1, nRecursion+1);
				if (pFileList != NULL) {
					return pFileList;
				}
			}
			if (iter->CompareNoCase(sFindAfter) == 0) {
				bStart = true;
			}
		}
	}

	// terminate recursion if on start level
	if (nLevel == 0) {
		return NULL;
	}

	if (nRecursion > 0) {
		return NULL; // this branch is not containing any images
	} else {
		// backtrack
		int nPos = sDirectory.ReverseFind(_T('\\'));
		CString sParentDir = (nPos > 0) ? sDirectory.Left(nPos) : _T("");
		CString sThisDir = sDirectory.Right(sDirectory.GetLength() - nPos - 1);
		return FindFileRecursively(sParentDir, sThisDir, false, nLevel - 1, max(0, nRecursion - 1));
	}
}

CFileList* CFileList::WrapToPrevImage() {
	// This is much easier than going to next image as we never go back to new unknown folders
	if (m_prev != NULL) {
		return m_prev;
	}
	return this;
}

void CFileList::NextInFolder() {
	if (m_fileList.size() > 0) {
		m_iter++;
		if (m_iter == m_fileList.end()) {
			m_iter = m_fileList.begin();
		}
	}
}

CFileList* CFileList::TryCreateFileList(const CString& directory, int nNewLevel) {
	// Check if we already had this directory in the loop, do not create a new loop
	CFileList* pList = this;
	while (pList != NULL) {
		if (pList->m_sDirectory.CompareNoCase(directory.Left(directory.GetLength() - 1)) == 0) {
			return NULL;
		}
		pList = pList->m_prev;
	}

	CFileList* pNewList = new CFileList(directory, CFileDesc::GetSorting(), m_bWrapAroundFolder, nNewLevel);
	if (pNewList->m_fileList.size() > 0) {
		pNewList->m_prev = this;
		m_next = pNewList;
		return pNewList;
	} else {
		delete pNewList;
		return NULL;
	}
}

void CFileList::FindFiles() {
	m_fileList.clear();
	if (!m_sDirectory.IsEmpty()) {
		CFindFile fileFind;
        LPCTSTR* allFileEndings = GetSupportedFileEndingList();
		for (int i = 0; i < nNumEndings; i++) {
			// Windows bug: *.tif also finds *.tiff, so only search for *.tif else we have duplicated files...
			if (_tcsicmp(allFileEndings[i], _T("tiff")) != 0 && fileFind.FindFile(m_sDirectory + _T("\\*.") + allFileEndings[i])) {
				AddToFileList(m_fileList, fileFind);
				while (fileFind.FindNextFile()) {
					AddToFileList(m_fileList, fileFind);
				}
			}
		}
	}

	m_fileList.sort();
}

void CFileList::VerifyFiles() {
	std::list<CFileDesc>::iterator iter;
	for (iter = m_fileList.begin( ); iter != m_fileList.end( ); iter++ ) {
		if (::GetFileAttributes(iter->GetName()) == INVALID_FILE_ATTRIBUTES) {
			iter = m_fileList.erase(iter);
			if (iter == m_fileList.end()) {
				break;
			}
		}
	}
}

bool CFileList::IsImageFile(const CString & sEnding) {
	CString sEndingLC = sEnding;
	sEndingLC.MakeLower();
    LPCTSTR* allFileEndings = GetSupportedFileEndingList();
	for (int i = 0; i < nNumEndings; i++) {
		if (allFileEndings[i] == sEndingLC) {
			return true;
		}
	}
	return false;
}

bool CFileList::TryReadingSlideShowList(const CString & sSlideShowFile) {
	FILE *fptr;
	if ((fptr = _tfopen(sSlideShowFile,_T("rb"))) == NULL) {
		return false;
	}
	// Try to detect if this is a UTF16 Unicode text file or a ANSI or UTF-8 coded file
	const int NUMPROBE = 64;
	uint8 buff[NUMPROBE];
	bool bUnicode;
	int nRealProbe = fread(buff, 1, NUMPROBE, fptr);
	if (nRealProbe < 16) {
		// file is too short for a good guess
		bUnicode = false;
	} else {
		if (buff[0] == 0xFF && buff[1] == 0xFE) {
			// Unicode byte order marker, only supporting little endian unicode files
			bUnicode = true;
		} else {
			// Use a heuristic if the BOM is missing
			int nGood = 0;
			for (int i = 0; i < nRealProbe; i++) {
				if (i & 1) {
					if (buff[i] == 0) nGood++;
				} else {
					if (buff[i] != 0) nGood++;
				}
			}
			bUnicode = nGood > nRealProbe*2/3;
		}
	}

	int nSeekPos = 0;
	if (strstr((char*)buff, "encoding=\"utf-8\"") != NULL) {
		// UTF-8 coded XML file, open the file in UTF-8 mode, it will be read as Unicode
		fclose(fptr);
		if ((fptr = _tfopen(sSlideShowFile,_T("r, ccs=UTF-8"))) == NULL) {
			return false;
		}
		nSeekPos = max(0, strchr((char*)buff, '<') - (char*)buff);
		bUnicode = true;
	}

	const int FILE_BUFFER_SIZE = 200*1024; // cannot read files > 200 KB for simplicity
	char* fileBuff = new char[FILE_BUFFER_SIZE];
	char* fileBuffOrig = fileBuff;
	wchar_t* wideFileBuff = (wchar_t*)fileBuff; // same buffer interpreted as Unicode
	fseek(fptr, nSeekPos, SEEK_SET);
	int nRealFileSizeChars = fread(fileBuff, 1, FILE_BUFFER_SIZE, fptr);
	if (bUnicode) nRealFileSizeChars = nRealFileSizeChars/2;

	const int LINE_BUFF_SIZE = 512;
	TCHAR lineBuff[LINE_BUFF_SIZE];
	bool bValid = true;
	int nTotalChars = 0;

	do {
		int nNumChars = 0;
		if (bUnicode) {
			wchar_t* pRunning = wideFileBuff;
			bool bIsNull = false;
			while (*pRunning != L'\n' && nNumChars < 512 && nTotalChars < nRealFileSizeChars) {
				if (*pRunning == 0) {
					bIsNull = true;
				}
				pRunning++; nNumChars++; nTotalChars++;
			}
			bValid = nNumChars < 512 && !bIsNull;
			if (bValid) {
#ifdef _UNICODE
				memcpy(lineBuff, wideFileBuff, (pRunning - wideFileBuff)*sizeof(TCHAR));
#else
				size_t nDummy;
				wcstombs_s(&nDummy, lineBuff, LINE_BUFF_SIZE, wideFileBuff, pRunning - wideFileBuff);
#endif
			}
			wideFileBuff = pRunning+1;
			nTotalChars++;
		} else {
			char* pRunning = fileBuff;
			bool bIsNull = false;
			while (*pRunning != '\n' && nNumChars < 512 && nTotalChars < nRealFileSizeChars) {
				if (*pRunning == 0) {
					bIsNull = true;
				}
				pRunning++; nNumChars++; nTotalChars++;
			}
			bValid = nNumChars < 512 && !bIsNull;
			if (bValid) {
#ifdef _UNICODE
				size_t nDummy;
				mbstowcs_s(&nDummy, lineBuff, LINE_BUFF_SIZE, fileBuff, pRunning - fileBuff);
#else
				memcpy(lineBuff, fileBuff, pRunning - fileBuff);
#endif
			}
			fileBuff = pRunning+1;
			nTotalChars++;
		}
		if (!bValid) {
			// the file contains very long lines or null characters, it is most likely not a text file
			fclose(fptr);
			delete[] fileBuffOrig;
			return false;
		}
		// one line in lineBuff, check if there is a valid file name
		bool bRelativePath = false;
		lineBuff[nNumChars] = _T('\n'); // force end
		TCHAR* pStart = _tcsstr(lineBuff, _T("\\\\"));
		if (pStart == NULL) {
			pStart = _tcsstr(lineBuff+1, _T(":\\"));
			if (pStart != NULL) {
				pStart--;
			} else {
				// try to find file name with relative path
                LPCTSTR* allFileEndings = GetSupportedFileEndingList();
				for (int i = 0; i < nNumEndings; i++) {
					pStart = _tcsstr(lineBuff, CString(_T(".")) + allFileEndings[i]);
					if (pStart != NULL) {
						while (pStart >= lineBuff && *pStart != _T('"') && *pStart != _T('>')) pStart--;
						pStart++;
						bRelativePath = true;
						break;
					}
				}
			}
			
		}
		if (pStart != NULL) {
			// extract file name and add to list of files to show
			TCHAR* pEnd = pStart;
			while (*pEnd != _T('\r') && *pEnd != _T('\n') && *pEnd != _T('"') && *pEnd != _T('<')) {
				pEnd++;
			}
			*pEnd = 0;
			CFindFile fileFind;
			if (fileFind.FindFile(bRelativePath ? (m_sDirectory + _T('\\') + pStart) : pStart)) {
				AddToFileList(m_fileList, fileFind);
			}
		}
	} while (nTotalChars < nRealFileSizeChars);

	fclose(fptr);
	delete[] fileBuffOrig;
	return true;
}