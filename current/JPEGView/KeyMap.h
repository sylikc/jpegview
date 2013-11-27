#pragma once

#include <hash_map>

#define VK_LBUTTONDBLCLK 7

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

	// Returns the virtual key code for a key name of the form 'Alt+A'.
	// Returns -1 if key name not valid
	static int GetVirtualKeyCode(LPCTSTR keyName);

	// gets a combinded key code
	static int GetCombinedKeyCode(int keyCode, bool alt, bool control, bool shift);

	// gets the shortcut key name, e.g. 'Ctrl+P'
	static CString GetShortcutKey(int combinedKeyCode);
private:

	// key is the key code, value the command ID
	stdext::hash_map<int, int> m_keyMap;

	void AddDefaultEscapeHandling();
};
