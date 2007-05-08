JPEGView source code readme
***************************

To compile JPEGView you need:

- Visual Studio 2005 Express Edition with VC++ package (of course the standard or professional editions also work)
- Windows Platform SDK (platform SDK for Windows Server 2003 or later)
- Windows Template Library (WTL), Version 8.0 (http://sourceforge.net/projects/wtl/)

The include directories of the platform SDK and WTL must be added to the include directories for VC++:
Extras > Options > Projects and Solutions > VC++ directories > Include files


Debug version:
Before compiling the debug version for the first time, copy the five files included in this distribution
from JPEGView\Release
to JPEGView\Debug