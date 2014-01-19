#pragma once

#include "Helpers.h"

class CDirectoryWatcher;

// Entry in the file list, allowing to sort by name, creation date and modification date
class CFileDesc 
{
public:
	CFileDesc(const CString & sName, const FILETIME* lastModTime, const FILETIME* creationTime, __int64 fileSize);

	// STL sort only needs this operator to order CFileDesc objects
	bool operator < (const CFileDesc& other) const;
	bool SortUpcounting(const CFileDesc& other) const;

	// Get and set the sorting method - the sorting method is global
	static Helpers::ESorting GetSorting() { return sm_eSorting; }
	static bool IsSortedUpcounting() { return sm_bSortUpcounting; }
	static void SetSorting(Helpers::ESorting eSorting, bool bUpcounting) { sm_eSorting = eSorting; sm_bSortUpcounting = bUpcounting; }

	// Full name of file
	const CString& GetName() const { return m_sName; }
	// Only use after a rename of a file, respectively changing its modification date
	void SetName(LPCTSTR sNewName);
	void SetModificationDate(const FILETIME& lastModDate);

	// File title (without path)
	LPCTSTR GetTitle() const { return m_sTitle; }

	// Gets last modification time
	const FILETIME& GetLastModTime() const { return m_lastModTime; }

	// Gets creation time
	const FILETIME& GetCreationTime() const { return m_creationTime; }

	// Gets file size in bytes
	__int64 GetFileSize() const { return m_fileSize; } 

private:
	static Helpers::ESorting sm_eSorting;
	static bool sm_bSortUpcounting;

	CString m_sName;
	LPCTSTR m_sTitle;
	FILETIME m_lastModTime;
	FILETIME m_creationTime;
	int m_nRandomOrderNumber;
	__int64 m_fileSize;
};


// Manages list of files to display and navigation between image files
class CFileList
{
public:
	// sInitialFile is the initial file to show with path. It can either be an image file, a directory
	// (must end with backslash in this case) or a text file containing file names to display.
	// Supported text files are ANSI, Unicode or UTF-8.
	// nLevel is increased when recursively create lists for sub-folders
	CFileList(const CString & sInitialFile, CDirectoryWatcher & directoryWatcher, 
		Helpers::ESorting eInitialSorting, bool isSortedUpcounting, bool bWrapAroundFolder, int nLevel = 0);
	~CFileList();

	// Gets a list of all supported file endings, separated by semicolon
	static CString GetSupportedFileEndings();

	// Reload file list for given file, if NULL for current file
	void Reload(LPCTSTR sFileName = NULL, bool clearForwardHistory = true);

	// Tells the file list that a file has been renamed externally
	void FileHasRenamed(LPCTSTR sOldFileName, LPCTSTR sNewFileName);

	// Tells that the modification date of the current file has changed
	void ModificationTimeChanged();

	// Move to next file in list, wrap according to navigation mode. The filelist object
	// of the next item is returned (in most situation the same object)
	CFileList* Next();
	// Move to previous file in list. The filelist object of the prev item is returned.
	CFileList* Prev();
	// Move to first file in current folder (according to sort order)
	void First();
	// Move to last file in current folder (according to sort order)
	void Last();
    // Move away from current image, either forward or backward (if on last image)
    CFileList* AwayFromCurrent();
	// Filename (with path) of the current item in the list, NULL if none
	LPCTSTR Current() const;
	// File title of the current item in the list, NULL if none
	LPCTSTR CurrentFileTitle() const;
	// Current directory without file name, NULL if none
	LPCTSTR CurrentDirectory() const;
	// Modification time of current file
	const FILETIME* CurrentModificationTime() const;
	// Get the n-next file, does not change the internal state
	LPCTSTR PeekNextPrev(int nIndex, bool bForward, bool bToggle);
	// Number of files in file list (for current directory)
	int Size() const { return m_fileList.size(); }
	// Index of current file in file list (zero based)
	int CurrentIndex() const;

	// Sets the sorting of the file list and resorts the list
	void SetSorting(Helpers::ESorting eSorting, bool sortUpcounting);
	// Gets the sorting mode
	Helpers::ESorting GetSorting() const;
	bool IsSortedUpcounting() const;

	// Set the directory navigation mode
	void SetNavigationMode(Helpers::ENavigationMode eMode);
	// Gets current navigation mode
	Helpers::ENavigationMode GetNavigationMode() const { return sm_eMode; }

	// Marks the current file for toggling between this file and the current file
	void MarkCurrentFile();
	// Returns if there is a file marked for toggling
	bool FileMarkedForToggle();
	// Toggle between marked and current file
	void ToggleBetweenMarkedAndCurrentFile();

	// Sets a checkpoint on the current image
	void SetCheckpoint() { m_iterCheckPoint = m_iter; }
	// Check if we are now on another image since the last checkpoint was set
	bool ChangedSinceCheckpoint() { return m_iterCheckPoint != m_iter; }

	// Returns if the current file list is based on a slide show text file
	bool IsSlideShowList() const { return m_bIsSlideShowList; }

	// Returns if the current file exists
	bool CurrentFileExists() const;

    // Returns if the current file exists and can be opened for reading
    bool CanOpenCurrentFileForReading() const;

	// Returns the raw file list of the current folder
	std::list<CFileDesc> & GetFileList() { return m_fileList; }

	// delete the chain of CFileLists forward and backward and only leave the current node alive
	void DeleteHistory(bool onlyForward = false);

private:
	static Helpers::ENavigationMode sm_eMode;

	bool m_bDeleteHistory;
	bool m_bIsSlideShowList;
	bool m_bWrapAroundFolder;
	int m_nLevel;
	CString m_sInitialFile;
	CString m_sDirectory;
	// filelists for several folders are chained
	CFileList* m_next;
	CFileList* m_prev;
	std::list<CFileDesc> m_fileList;
	std::list<CFileDesc>::iterator m_iter; // current position in m_fileList
	std::list<CFileDesc>::iterator m_iterStart; // start of iteration in m_fileList
	std::list<CFileDesc>::iterator m_iterCheckPoint;

	CString m_sMarkedFile;
	CString m_sMarkedFileCurrent;
	int m_nMarkedIndexShow;

    CDirectoryWatcher & m_directoryWatcher;

	void MoveIterToLast();
	void NextInFolder();
	std::list<CFileDesc>::iterator FindFile(const CString& sName);
	CFileList* FindFileRecursively (const CString& sDirectory, const CString& sFindAfter, 
		bool bSearchThisFolder, int nLevel, int nRecursion);
	CFileList* TryCreateFileList(const CString& directory, int nNewLevel);
	CFileList* WrapToNextImage();
	CFileList* WrapToPrevImage();
	void FindFiles();
	void VerifyFiles();
	bool IsImageFile(const CString & sEnding);
	bool TryReadingSlideShowList(const CString & sSlideShowFile);
};
