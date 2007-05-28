#pragma once

#include "Helpers.h"

// Entry in the file list, allowing to sort by name, creation date and modification date
class CFileDesc 
{
public:
	CFileDesc(const CString & sName, const FILETIME* lastModTime, const FILETIME* creationTime);

	// STL sort only needs this operator to order CFileDesc objects
	bool operator < (const CFileDesc& other) const;

	// Get and set the sorting method - the sorting method is global
	static Helpers::ESorting GetSorting() { return sm_eSorting; }
	static void SetSorting(Helpers::ESorting eSorting) { sm_eSorting = eSorting; }

	// Full name of file
	const CString& GetName() const { return m_sName; }
	// Only use after a rename of a file
	void SetName(LPCTSTR sNewName);

	// File title (without path)
	LPCTSTR GetTitle() const { return m_sTitle; }

	// Gets last modification time
	const FILETIME& GetLastModTime() const { return m_lastModTime; }

	// Gets creation time
	const FILETIME& GetCreationTime() const { return m_creationTime; }

private:
	static Helpers::ESorting sm_eSorting;

	CString m_sName;
	LPCTSTR m_sTitle;
	FILETIME m_lastModTime;
	FILETIME m_creationTime;
};


// Manages list of files to display and navigation between image files
class CFileList
{
public:
	// sInitialFile is the initial file to show with path. It can either be an image file, a directory
	// (must end with backslash in this case) or a text file containing file names to display.
	// Supported text files are ANSI, Unicode or UTF-8.
	// nLevel is increased when recursively create lists for sub-folders
	CFileList(const CString & sInitialFile, Helpers::ESorting eInitialSorting, int nLevel = 0);
	~CFileList();

	// Gets a list of all supported file endings, separated by semicolon
	static CString GetSupportedFileEndings();

	// Reload file list for this folder
	void Reload();

	// Tells the file list that a file has been renamed externally
	void FileHasRenamed(LPCTSTR sOldFileName, LPCTSTR sNewFileName);

	// Move to next file in list, wrap according to navigation mode. The filelist object
	// of the next item is returned (in most situation the same object)
	CFileList* Next();
	// Move to previous file in list. The filelist object of the prev item is returned.
	CFileList* Prev();
	// Move to first file in current folder (according to sort order)
	void First();
	// Move to last file in current folder (according to sort order)
	void Last();
	// Filename (with path) of the current item in the list, NULL if none
	LPCTSTR Current() const;
	// File title of the current item in the list, NULL if none
	LPCTSTR CurrentFileTitle() const;
	// Current directory without file name, NULL if none
	LPCTSTR CurrentDirectory() const;
	// Get the n-next file, does not change the internal state
	LPCTSTR PeekNextPrev(int nIndex, bool bForward);
	// Number of files in file list (for current directory)
	int Size() const { return m_fileList.size(); }
	// Index of current file in file list (zero based)
	int CurrentIndex() const;

	// Sets the sorting of the file list and resorts the list
	void SetSorting(Helpers::ESorting eSorting);
	// Gets the sorting mode
	Helpers::ESorting GetSorting() const;

	// Set the directory navigation mode
	void SetNavigationMode(Helpers::ENavigationMode eMode);
	// Gets current navigation mode
	Helpers::ENavigationMode GetNavigationMode() const { return sm_eMode; }

	// Returns if the current file list is based on a slide show text file
	bool IsSlideShowList() const { return m_bIsSlideShowList; }

	// Returns the raw file list of the current folder
	std::list<CFileDesc> & GetFileList() { return m_fileList; }

private:
	static Helpers::ENavigationMode sm_eMode;

	bool m_bDeleteHistory;
	bool m_bIsSlideShowList;
	int m_nLevel;
	CString m_sInitialFile;
	CString m_sDirectory;
	// filelists for several folders are chained
	CFileList* m_next;
	CFileList* m_prev;
	std::list<CFileDesc> m_fileList;
	std::list<CFileDesc>::iterator m_iter; // current position in m_fileList
	std::list<CFileDesc>::iterator m_iterStart; // start of iteration in m_fileList

	void DeleteHistory();
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
