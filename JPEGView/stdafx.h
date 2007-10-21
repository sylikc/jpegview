// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#pragma once

// disable these useless warnings
#pragma warning(disable:4018)
#pragma warning(disable:4800)

// Change these values to use different versions
#define WINVER		0x0500
#define _WIN32_WINNT	0x0501
#define _WIN32_IE	0x0500
#define _RICHEDIT_VER	0x0100

#define _CRT_SECURE_NO_DEPRECATE

#include <tchar.h>
#include <atlbase.h>
#include <atlapp.h>
#include <assert.h>

extern CAppModule _Module;

#include <atlwin.h>

#include <atlframe.h>
#include <atlctrls.h>
#include <atldlgs.h>
#include <atlmisc.h>

// STL stuff
#include  <list>

// own stuff
#include "ImageProcessingTypes.h"

#define VK_PAGE_UP 0x021
#define VK_PAGE_DOWN 0x22
#define VK_PLUS 0x6b
#define VK_MINUS 0x6d

// a type that has enough bits to hold a pointer and allowing arithmetic operations
#define PTR_INTEGRAL_TYPE unsigned int

#if defined _M_IX86
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
