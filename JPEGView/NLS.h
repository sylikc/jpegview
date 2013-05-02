#pragma once

#include <hash_map>

using namespace stdext;

class CNLS
{
public:
    // Gets a localized name of a file located in the EXE folder with given prefix and extension,
    // e.g. GetLocalizedFileName("", "readme", "html", "ru") returns "readme_ru.html"
    static CString GetLocalizedFileName(LPCTSTR sPath, LPCTSTR sPrefixName, LPCTSTR sExtension, LPCTSTR sLanguageCode);

	// Gets the file name of the string table to use for the given ISO 639 language code 
	static CString GetStringTableFileName(LPCTSTR sLanguageCode);

	// Reads the string table mapping strings from English to target language
	static void ReadStringTable(LPCTSTR sFileName);

	// Translates a string, key is the English string, output the translated string
	static LPCTSTR GetString(LPCTSTR sString);

private:
	CNLS(void);
	~CNLS(void);

public:
	class CStringHashCompare {
		public:
			static const size_t bucket_size = 4;
			static const size_t min_buckets = 8;
			CStringHashCompare() {}
			size_t operator( )( const LPCTSTR& Key ) const;
			bool operator( )( const LPCTSTR& _Key1, const LPCTSTR& _Key2 ) const;
	};

private:

	static stdext::hash_map<LPCTSTR, LPCTSTR, CStringHashCompare> sm_texts;
	static bool sm_bTableRead;
};
