#include "StdAfx.h"
#include "NLS.h"
#include "SettingsProvider.h"

stdext::hash_map<LPCTSTR, LPCTSTR, CNLS::CStringHashCompare> CNLS::sm_texts;
bool CNLS::sm_bTableRead = false;

// the hash function (string hash)
size_t CNLS::CStringHashCompare::operator( )( const LPCTSTR& Key ) const {
	size_t nHash = 0;
	int nCnt = 0;
	while (Key[nCnt] != 0 && nCnt++ < 16) {
		nHash += Key[nCnt];
		nHash = (nHash << 8) + nHash ;
	}
	return nHash;
}

// compare function
bool CNLS::CStringHashCompare::operator( )( const LPCTSTR& _Key1, const LPCTSTR& _Key2 ) const {
	return _tcscmp(_Key1, _Key2) < 0;
}

CString CNLS::GetLocalizedFileName(LPCTSTR sPath, LPCTSTR sPrefixName, LPCTSTR sExtension, LPCTSTR sLanguageCode) {
	CString sNLSFile;
	TCHAR buff[16], buff2[16];
	if (_tcscmp(sLanguageCode, _T("auto")) == 0) {
		LCID threadLocale = ::GetThreadLocale();
		::GetLocaleInfo(threadLocale, LOCALE_SISO639LANGNAME, (LPTSTR) &buff, sizeof(buff));
		::GetLocaleInfo(threadLocale, LOCALE_SISO3166CTRYNAME, (LPTSTR) &buff2, sizeof(buff2));
		sLanguageCode = buff;
		sNLSFile = CString(sPath) + sPrefixName + _T("_") + sLanguageCode + "-" + buff2 + _T(".") + sExtension;
		if (::GetFileAttributes(sNLSFile) == INVALID_FILE_ATTRIBUTES) {
			sNLSFile.Empty();
		}
	}
	if (sNLSFile.IsEmpty()) {
		sNLSFile = CString(sPath) + sPrefixName + _T("_") + sLanguageCode + __T(".") + sExtension;
	}
	return sNLSFile;
}

CString CNLS::GetStringTableFileName(LPCTSTR sLanguageCode) {
	return GetLocalizedFileName(CSettingsProvider::This().GetEXEPath(), _T("strings"), _T("txt"), sLanguageCode);
}

void CNLS::ReadStringTable(LPCTSTR sFileName) {
	FILE *fptr = _tfopen(sFileName, _T("r, ccs=UTF-8"));
	if (fptr == NULL) {
		return;
	}

	const int FILE_BUFF_SIZE = 32000;
	TCHAR* pLineBuff = new TCHAR[FILE_BUFF_SIZE];
	memset(pLineBuff, 0, FILE_BUFF_SIZE * sizeof(TCHAR));
	size_t nReadBytes = fread(pLineBuff, 1, FILE_BUFF_SIZE*sizeof(TCHAR), fptr);

	TCHAR* pRunning = pLineBuff;
	while (*pRunning != 0) {
		if (pRunning[0] == _T('/') && pRunning[1] == _T('/')) {
			// comment line, ignore
			while (*pRunning != 0 && *pRunning != _T('\n') && *pRunning != _T('\r')) {
				pRunning++;
			}
		} else {
			TCHAR* pStartLine = pRunning;
			while (*pRunning != _T('\t') && *pRunning != _T('\n') && *pRunning != 0) {
				pRunning++;
			}
			if (*pRunning == _T('\t')) {
				*pRunning = 0;
				pRunning++;
				TCHAR* pStartTranslated = pRunning;
				while (*pRunning != _T('\r') && *pRunning != _T('\n') && *pRunning != 0) {
					pRunning++;
				}
				bool bIncrement = (*pRunning != 0);
				*pRunning = 0;
				if (*pStartTranslated != 0) {
					// copy strings to permanent buffer
					int nTargetBuffSize = (int)(pRunning - pStartLine + 1);
					TCHAR* pTargetBuff = new TCHAR[nTargetBuffSize];
					memcpy(pTargetBuff, pStartLine, nTargetBuffSize*sizeof(TCHAR));
					TCHAR* pTranslated = pTargetBuff + (pStartTranslated - pStartLine);
					sm_texts[pTargetBuff] = pTranslated;
				}
				if (bIncrement) pRunning++;
			}
		}
		while (*pRunning != 0 && (*pRunning == _T('\n') || *pRunning == _T('\r'))) {
			pRunning++;
		}
	}
	fclose(fptr);

	delete[] pLineBuff;

	sm_bTableRead = true;
}

LPCTSTR CNLS::GetString(LPCTSTR sString) {
	if (!sm_bTableRead) {
		return sString; // no translation available, use English
	}
	hash_map<LPCTSTR, LPCTSTR, CStringHashCompare>::const_iterator iter;
	iter = sm_texts.find(sString);
	if (iter == sm_texts.end()) {
		return sString; // not found
	} else {
		return iter->second;
	}
}