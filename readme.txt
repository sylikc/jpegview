JPEGView source code readme
***************************

To compile JPEGView you need:

- Visual Studio 2005 Express Edition with VC++ package (of course the standard or professional editions also work)
- Windows Platform SDK (platform SDK for Windows Server 2003 or later) - only Core SDK is needed
- Windows Template Library (WTL), Version 8.0 (http://sourceforge.net/projects/wtl/)
  (read http://www.codeproject.com/KB/wtl/WTLExpress.aspx how to patch the platform SDK headers so that WTL compiles with VC++ Express edition and PSDK)
- To edit the resource file (rc) a free resource editor as www.radasm.com/resed/ is recommended

The include directories of the platform SDK and WTL must be added to the include directories for VC++:
Extras > Options > Projects and Solutions > VC++ directories > Include files


Debug version:
Before compiling the debug version for the first time, copy the files included in this distribution
from JPEGView\Release
to JPEGView\Debug


Changelog
*********

[1.0.17]

Bugs removed:
- Image processing panel not truncated on 1024x768 screens, sliders are smaller in this case
New features:
- Toggle screen with Ctrl-F12 on multiscreen systems
- Window mode, toggle between window mode and full screen mode with Ctrl-W or with button on navigation panel
  The window mode can be made default in INI file and the starting window size can also be set in the INI
- New INI file setting (LandscapeModeParams) that allows to define the parameters used in landscape enhancement mode


[1.0.16.1] (repack - binary files equal to 1.0.16)

New features:
- Italian translation (thanks to Max)

[1.0.16]

Bugs removed:
- PNGs with transparency (alpha channel) are now rendered correctly. Other formats supporting alpha channel
  are rendered correctly when GDI+ can render them correctly - I have not tested this.
- The 'R' key for rotating lossless works again
- Navigation panel was not visible when OS regional settings did not use a point as decimal number separator
  (All other INI file settings represented by floating point numbers were also not read correctly in this case)
- Crash removed when scaling up images with heigth=1
- Cropping small areas (1 x n, n x 1) now possible
- Not using IJL for 1 channel JPEGs anymore - seems to crash sometimes. GDI+ is used for these images instead.
- Two bugs fixed causing crashes with very small images or images with extreme aspect ratio
New features:
- Support for multiple CPU cores. To set the number of cores to use to a specific value, set the CPUCoresUsed key
  to a specific value in the INI file. Default is to use all cores (max 4) of the CPU.
- Small thumbnail displayed when zooming, showing the visible section of the image. The section can be moved with
  the mouse in the thumbnail image. To disable this feature set ShowZoomNavigator=false in INI file.
- Spanish translation (thanks to Franco Bianconi)
- About dialog
- Tooltips for navigation panel buttons
- New button on navigation panel to switch to actual image size / fit to screen
- New INI file setting (Language=xx) to force user interface language (default is to use operating system language if supported by JPEGView)
- New INI file setting (StoreToEXEPath=true/false) forcing to write to config data and parameter DB to EXE path (used when installed on USB stick)

[1.0.15]

Bugs removed:
- If using ClearType fonts, the button text on the image processing area is now rendered correctly
- EXIF information is now also found if not placed directly after SOI in JPEG image (e.g. after an APP0 block)
- File name flickering fixed in image processing area (was happening when file name was long)
- Zoom to 100% now always correctly zooms mouse cursor centered when the cursor is visible
New features:
- Display of EXIF information from digicam JPEG files (F2) - can be made permanantly using the INI file
- Smoother screen update with less flickering by using back buffering of transparent areas
- Navigation panel blended into image. The navigation panel is turned on by default but can be disabled with the F11 key.
  To disable it permanently, press F11 then save the settings to the INI file with the context menu.
  The blending factor of the panel can be configured also in the INI file.
- New mode for optimal display of landscape pictures - lightens shadows and darkens highlights very progressively
  Use Ctrl-L to enable or the button on the navigation panel
Other changes:
- Using smaller font for help display (F1)
- Exclusion/inclusion folders for LDC and color correction: More specific folder overrides less specific when matching for inclusion
  and exclusion
- Using Crop without Ctrl pressed when image cannot be dragged
- New INI file entry: CreateParamDBEntryOnSave to disable/enable creation of a parameter DB entry for images saved in JPEGView

[1.0.14]

Bugs removed:
- If a JPEG cannot be read with the IJL library, GDI+ is tried as a second chance
- Copy/rename dialog: Replacement text can now be longer than text input field (auto scroll enabled)
- Reader no longer crashes when stripe is negative for images read with GDI+
- Images with ending TIFF are no longer duplicated in file list
New features:
- File open... always uses tumbnail view (not only in 'my picture' directory)
- Saving files is now possible in the following formats: JPG, BMP, TIFF, PNG
- Copy/rename dialog: New placeholder %n for number from original file name
- Panning is faster and uses high quality resampling mode during pan
- New command to reload current image (Ctrl-R)
- Crop image section or zoom to image section with CTRL-Left mouse and dragging the mouse
- Paste image from clipboard (Ctrl-V)
Other changes:
- Color and contrast correction switched off in default INI file.
  It is recommended to switch it on for dedicated folders only, e.g. folders with digicam photos.
  Of course any user INI file remains untouched by this change.
- Due to an internal code simplification, all parameter DB entries for BMP images created with older versions 
  are lost with 1.0.14. Note that other formats (e.g. JPG) are not affected.

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