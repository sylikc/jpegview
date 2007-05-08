#pragma once

#include <hash_map>

using namespace stdext;

class CNLS
{
public:
	// Reads the string table mapping strings from English to target language
	static void ReadStringTable(LPCTSTR sFileName);

	// Translates a string, key is the English string, output the translated string
	static LPCTSTR GetString(LPCTSTR sString);

private:
	CNLS(void);
	~CNLS(void);

private:
	class CStringHashCompare {
		public:
			static const size_t bucket_size = 4;
			static const size_t min_buckets = 8;
			CStringHashCompare() {}
			size_t operator( )( const LPCTSTR& Key ) const;
			bool operator( )( const LPCTSTR& _Key1, const LPCTSTR& _Key2 ) const;
	};

	static stdext::hash_map<LPCTSTR, LPCTSTR, CStringHashCompare> sm_texts;
	static bool sm_bTableRead;
};
