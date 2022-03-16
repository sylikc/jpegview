#pragma once

#include "HashCompareLPCTSTR.h"
#include <hash_map>

using namespace stdext;

// Supports translation of texts from English to a target language using text files
// with (key, value) pairs and English text as key and translated text as value.
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

	static stdext::hash_map<LPCTSTR, LPCTSTR, CHashCompareLPCTSTR> sm_texts;
	static bool sm_bTableRead;
};
