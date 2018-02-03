#include "StdAfx.h"
#include "HashCompareLPCTSTR.h"

// the hash function (string hash)
size_t CHashCompareLPCTSTR::operator( )(const LPCTSTR& Key) const {
	size_t nHash = 0;
	int nCnt = 0;
	while (Key[nCnt] != 0 && nCnt++ < 16) {
		nHash += Key[nCnt];
		nHash = (nHash << 8) + nHash;
	}
	return nHash;
}

// compare function
bool CHashCompareLPCTSTR::operator( )(const LPCTSTR& _Key1, const LPCTSTR& _Key2) const {
	return _tcscmp(_Key1, _Key2) < 0;
}