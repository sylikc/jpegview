JPEGView source code readme
***************************

To compile JPEGView you need:

- Visual Studio 2005 Express Edition with VC++ package (of course the standard or professional editions also work)
- Windows Platform SDK (platform SDK for Windows Server 2003 or later)
- Windows Template Library (WTL), Version 8.0 (http://sourceforge.net/projects/wtl/)
- To edit the resource file (rc) a free resource editor as www.radasm.com/resed/ is recommended

The include directories of the platform SDK and WTL must be added to the include directories for VC++:
Extras > Options > Projects and Solutions > VC++ directories > Include files


Debug version:
Before compiling the debug version for the first time, copy the five files included in this distribution
from JPEGView\Release
to JPEGView\Debug


Changelog
*********

[1.0.14]

Bugs removed:
- If a JPEG cannot be read with the IJL library, GDI+ is tried as a second chance
- Copy/rename dialog: Replacement text can now be longer than text input field (auto scroll enabled)
New features:
- File open... always uses tumbnail view (not only in my picture directory)
- Copy/rename dialog: New placeholder %n for number from original file name
- Panning is faster and uses high quality resampling mode during pan
- New command to reload current image
- Crop image section or zoom to image section with CTRL-Left mouse and dragging

[1.0.13]

Bugs removed:
- Sorting by name now works correctly for names of the form X1 X1_y X2 X3
- When saving default values to INI file using the context menu, these default values are now immediately in effect for the next images
New features:
- File lists (text files) now support relative paths (relative to file list location)
- Movie mode enhanced and improved, now accessible using the context menu and automatically using fastest processing modes
- Batch rename/copy, allowing to copy and rename a series of files, e.g. from the digicam to the harddisk. Several placeholders are
  supported, e.g. file dates and consecutive numbers.
- Possibility to set the auto zoom mode, enabling to zoom images to fit the screen with or without border
Other changes:
- Keeping parameters between images (F4) will now override any values from the parameter DB while active.
  Note that while keeping parameters is on, storing to parameter DB is disabled.
- ICO removed from supported format list - GDI+ claims to be able to read it but can't

[1.0.12]

First release to public