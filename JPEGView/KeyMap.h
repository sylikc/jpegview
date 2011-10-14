#pragma once

#include <hash_map>

// KeyMap
class CKeyMap
{
public:
	// returns true if the system is a multi monitor system
	CKeyMap(LPCTSTR sKeyMapFile);

	// Gets a command Id for a keycode and modifier keys. Returns -1 if no command available for this key.
	int GetCommandIdForKey(int nVirtualKeyCode, bool bAlt, bool bCtrl, bool bShift);

	// Gets the string describing the keyboard shortcut for a given command, empty string if none
	CString GetKeyStringForCommand(int nCommandId);

private:

	// key is the key code, value the command ID
	stdext::hash_map<int, int> m_keyMap;
};
