#include "StdAfx.h"
#include "KeyMap.h"
#include "NLS.h"
#include "resource.h"

#define M_SHIFT 0x10000
#define M_ALT   0x20000
#define M_CTRL  0x40000

struct SKey {
	LPCTSTR Name;
	int KeyCode;
};

#define NUM_KEYS 30

static SKey KeyTable[NUM_KEYS] = {
	{ _T("Alt"), M_ALT },
	{ _T("Ctrl"), M_CTRL },
	{ _T("Shift"), M_SHIFT },
	{ _T("Esc"), VK_ESCAPE },
	{ _T("Return"), VK_RETURN },
	{ _T("Space"), VK_SPACE },
	{ _T("End"), VK_END },
	{ _T("Home"), VK_HOME },
	{ _T("Back"), VK_BACK },
	{ _T("Tab"), VK_TAB },
	{ _T("PgDn"), VK_PAGE_DOWN },
	{ _T("PgUp"), VK_PAGE_UP },
	{ _T("Left"), VK_LEFT },
	{ _T("Right"), VK_RIGHT },
	{ _T("Up"), VK_UP },
	{ _T("Down"), VK_DOWN },
	{ _T("Insert"), VK_INSERT },
	{ _T("Del"), VK_DELETE },
	{ _T("Plus"), VK_PLUS },
	{ _T("Minus"), VK_MINUS },
	{ _T("Mul"), VK_MULTIPLY },
	{ _T("Div"), VK_DIVIDE },
	{ _T("Comma"), VK_OEM_COMMA },
	{ _T("Period"), VK_OEM_PERIOD },
    { _T("MouseL"), VK_LBUTTON },
    { _T("MouseR"), VK_RBUTTON },
    { _T("MouseM"), VK_MBUTTON },
    { _T("MouseX1"), VK_XBUTTON1 },
    { _T("MouseX2"), VK_XBUTTON2 },
    { _T("MouseDblClk"), VK_LBUTTONDBLCLK }
};

static LPTSTR _SkipWhiteSpace(LPTSTR sText) {
	while (_istspace(*sText) && *sText != 0) sText++;
	return sText;
}

static bool _IsComment(LPTSTR sText) {
	return sText[0] != 0 && (sText[0] == _T('/') && sText[1] == _T('/'));
}

static LPTSTR _Parse(LPTSTR sText) {
	while (!_istspace(*sText) && *sText != 0) sText++;
	if (*sText == 0) {
		return NULL;
	}
	*sText++ = 0;
	sText = _SkipWhiteSpace(sText);
	if (*sText == 0) {
		return NULL;
	}
	LPTSTR sRight = sText;
	while (!_istspace(*sText) && *sText != 0) sText++;
	*sText = 0;
	return sRight;
}

static int _GetKeyCode(LPTSTR sKey) {
	for (int i = 0; i < NUM_KEYS; i++) {
		if (_tcsicmp(sKey, KeyTable[i].Name) == 0) {
			return KeyTable[i].KeyCode;
		}
	}

	int nFNumber;
	if (_stscanf(sKey, _T("F%d"), &nFNumber) == 1 && nFNumber >= 1 && nFNumber <= 24) {
		return VK_F1 + nFNumber - 1;
	}

	if (_tcslen(sKey) == 1) {
		int nChar = _totupper(sKey[0]);
		if ((nChar >= _T('0') && nChar <= _T('9')) || (nChar >= _T('A') && nChar <= _T('Z'))) {
			return nChar;
		}
	}

	return 0;
}

static int _ParseKeys(LPTSTR sKeys) {
	int nKeyCode = 0;
	bool bContinue = true;
	while (bContinue) {
		LPTSTR sStart = sKeys;
		while (*sKeys != _T('+') && *sKeys != 0) sKeys++;
		if (*sKeys == 0) bContinue = false;
		*sKeys = 0;
		nKeyCode += _GetKeyCode(sStart);
		sKeys++;
	}
	return nKeyCode;
}

static int _FindCommandId(stdext::hash_map<LPCTSTR, int, CNLS::CStringHashCompare> & symbolMap, LPCTSTR sSymbolName) {
	stdext::hash_map<LPCTSTR, int, CNLS::CStringHashCompare>::const_iterator iter;
	iter = symbolMap.find(sSymbolName);
	if (iter == symbolMap.end()) {
		return -1;
	} else {
		return iter->second;
	}
}

static CString _GetKeyShortcutName(int nShortcut) {
	CString sKeyDesc;
	if ((nShortcut & M_ALT) != 0) sKeyDesc += _T("Alt+");
	if ((nShortcut & M_CTRL) != 0) sKeyDesc += _T("Ctrl+");
	if ((nShortcut & M_SHIFT) != 0) sKeyDesc += _T("Shift+");
	int nRealShortcut = nShortcut & 0xFF;
	for (int i = 0; i < NUM_KEYS; i++) {
		if (KeyTable[i].KeyCode == nRealShortcut) {
			if (KeyTable[i].Name == _T("Plus")) {
				sKeyDesc += _T("+");
			} else if (KeyTable[i].Name == _T("Minus")) {
				sKeyDesc += _T("-");
			} else {
				sKeyDesc += KeyTable[i].Name;
			}
		}
	}
	if (nRealShortcut >= VK_F1 && nRealShortcut <= VK_F24) {
		TCHAR buff[8];
		_sntprintf(buff, 8, _T("F%d"), nRealShortcut - VK_F1 + 1);
		sKeyDesc += buff;
	}
	if ((nRealShortcut >= _T('0') && nRealShortcut <= _T('9')) || (nRealShortcut >= _T('A') && nRealShortcut <= _T('Z'))) {
		sKeyDesc += CString((TCHAR)nRealShortcut);
	}
	return sKeyDesc;
}

CKeyMap::CKeyMap(LPCTSTR sKeyMapFile) {
	FILE *fptr = _tfopen(sKeyMapFile, _T("r"));
	if (fptr == NULL) {
		AddDefaultEscapeHandling();
		return;
	}

	const int BUFF_LEN = 256;
	TCHAR lineBuff[BUFF_LEN];

	const int SYMBOL_TABLE_LEN = 4096;
	TCHAR symbolTable[SYMBOL_TABLE_LEN];
	TCHAR* pSymbolTableEnd = &(symbolTable[SYMBOL_TABLE_LEN - 1]);
	TCHAR* pSymbolTable = &(symbolTable[0]);

	stdext::hash_map<LPCTSTR, int, CNLS::CStringHashCompare> m_symbolMap;

	while (_fgetts(lineBuff, BUFF_LEN, fptr) != NULL) {
		LPTSTR sLine = _SkipWhiteSpace(lineBuff);
		if (*sLine == 0) continue;
		if (_IsComment(sLine)) continue;
		if (_tcsncmp(sLine, _T("#define"), 7) == 0) {
			LPTSTR sSymbol = sLine + 7;
			sSymbol = _SkipWhiteSpace(sSymbol);
			LPTSTR sValue = _Parse(sSymbol);
			if (sValue == NULL) continue;
			int nValue = _ttoi(sValue);
			if (nValue == 0) continue;
			int nSymbolLen = _tcslen(sSymbol);
			if (pSymbolTable + nSymbolLen + 1 <= pSymbolTableEnd) {
				_tcscpy(pSymbolTable, sSymbol);
				m_symbolMap[pSymbolTable] = nValue;
				pSymbolTable += nSymbolLen + 1;
			} // else the symbol table is full
		} else {
			LPTSTR sCommandId = _Parse(sLine);
			if (*sCommandId == 0) continue;
			int nCommandId = _FindCommandId(m_symbolMap, sCommandId);
			if (nCommandId < 0) continue;
			int nKeyCode = _ParseKeys(sLine);
			if (nKeyCode == 0) continue;
			m_keyMap[nKeyCode] = nCommandId;
		}
		
	}

	AddDefaultEscapeHandling();

	fclose(fptr);
}

int CKeyMap::GetVirtualKeyCode(LPCTSTR keyName) {
	CString sBuffer(keyName);
	LPTSTR buffer = sBuffer.GetBuffer(sBuffer.GetLength());
	int key = _ParseKeys(buffer);
	return (key == 0) ? -1 : key;
}

int CKeyMap::GetCombinedKeyCode(int keyCode, bool alt, bool control, bool shift) {
	int nKeyCode = keyCode;
	if (alt) nKeyCode += M_ALT;
	if (control) nKeyCode += M_CTRL;
	if (shift) nKeyCode += M_SHIFT;
	return nKeyCode;
}

CString CKeyMap::GetShortcutKey(int combinedKeyCode) {
	return _GetKeyShortcutName(combinedKeyCode);
}

void CKeyMap::AddDefaultEscapeHandling() {
	if (m_keyMap.find(VK_ESCAPE) == m_keyMap.end()) {
		// ESC key not in keymap - map to default command for ESC
		m_keyMap[VK_ESCAPE] = IDM_DEFAULT_ESC;
	}
}

int CKeyMap::GetCommandIdForKey(int nVirtualKeyCode, bool bAlt, bool bCtrl, bool bShift) {
	int nKeyCode = GetCombinedKeyCode(nVirtualKeyCode, bAlt, bCtrl, bShift);
	stdext::hash_map<int, int>::const_iterator iter = m_keyMap.find(nKeyCode);
	if (iter == m_keyMap.end()) {
		return -1;
	} else {
		return iter->second;
	}
}

CString CKeyMap::GetKeyStringForCommand(int nCommandId) {
	stdext::hash_map<int, int>::const_iterator iter = m_keyMap.begin();
	std::list<CString> keys;
	while (iter != m_keyMap.end()) {
		if (nCommandId == iter->second) {
			CString sShortCutName = _GetKeyShortcutName(iter->first);
			if (!sShortCutName.IsEmpty()) {
				if (sShortCutName[0] == _T('P')) {
					keys.push_front(sShortCutName);
				} else {
					keys.push_back(sShortCutName);
				}
			}
		}
		iter++;
	}

	CString sKeyDesc;
	for (std::list<CString>::iterator iter = keys.begin(); iter != keys.end(); iter++) {
		if (!sKeyDesc.IsEmpty()) {
			sKeyDesc += _T("/");
		}
		sKeyDesc += *iter;
	}

	return sKeyDesc;
}