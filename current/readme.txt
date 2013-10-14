JPEGView source code readme
***************************

To compile JPEGView you need:

- Visual Studio 2010 Express Edition with VC++ package (of course all better editions also work)
- Windows Template Library (WTL), Version 8.0 (http://sourceforge.net/projects/wtl/)
- When using the express edition: ATL headers. Get them from a standard or professional edition from the directory $(VCInstallDir)\atlmfc\include
  or google for it. The ATL header files cannot be included into the JPEGView source code distribution due to copyright issues.
- To edit the resource file (rc) a free resource editor as www.radasm.com/resed/ is recommended

The include directories of WTL must be added to the include directories for VC++:
In Visual Studio 2010: Properties of JPEGView project > VC++-Directories-Include Directories and Library Directories

Please note: Windows XP SP2 or later is needed to run the compiled binary.


Debug version:
Before compiling the debug version for the first time, copy the files included in this distribution
from JPEGView\Release
to JPEGView\Debug


Changelog
*********

[1.0.30]
Bugs fixed:
- Slide shows with alpha blending: maybe now also works correctly if graphics driver implements alpha blending somewhat inexactly
- NoStrCmpLogical registry setting also checked in "LOCAL MACHINE" registry hive
- Fixed size cropping bug fixed
New features:
- New command line parameter '/autoexit'. If passed, ESC key will terminate JPEGView also if a slideshow is running or an animated GIF is playing.
  Additionally it will auto-terminate JPEGView after the slideshow has finished when combined with the '/slideshow' command line parameter.
  Thanks to SourceForge user manuthie for implementing this.
- New placeholders that can be used in user commands:
  %exepath% : Path to EXE folder where JPEGView is running
  %exedrive% : Drive letter of the EXE path, e.g. "c:"
- Displayed image is reloaded automatically when it changes on disk, using a file system change notifier.
  The image list of the current directory is also reloaded when images are added or deleted in this directory.
  To disable the file system notifier, set ReloadWhenDisplayedImageChanged=false in the INI file

[1.0.29]
Bugs fixed:
- EXIF (F2) and file name info (Ctrl+F2) no longer flicker when active during slide show with transition effect
- No logical sorting of file names is done if HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Policies\Explorer\NoStrCmpLogical
  registry value is set to 1. This matches the behavior of the Windows explorer.
  Note that the default registry value for this is 0, therefore almost all users will notice no change.
New features:
- Support for animated GIFs.
  Limitations:
  - Saving GIFs will only save the currently displayed frame.
  - Some image processings (e.g. free rotation, perspective correction) will stop the animation and work on a single frame only
  - No caching of the decoded frames. This has pros and cons. Pro is that memory consumption is limited. Con is that the CPU
    load while playing can be rather high. However on a dual core CPU it's not a problem.
- Support for multi-page TIFF. Note that (singlepage or multipage) TIFFs compressed with JPEG compression are not supported by GDI+.
  This is a known issue of GDI+ that cannot be fixed in JPEGView (except if another TIFF library would be used)
- New INI file setting: FileNameFormat allowing to display configurable information with Ctrl-F2. See INI file for information.
Other changes:
- WebP library 0.3.0 from Google is now used. This version supports lossy and lossless compression.
  To use lossless compression with the WEBP format, select the "WEBP lossless" file format in the "File Save" dialog.
  Be warned that lossless WEBP compression is very, very slow (decompression is fast however).

[1.0.28]
Bugs fixed:
- Fixed bug in the window resizing code when 'DefaultWindowRect=max' and toggling between 100 % zoom and fit to window (space key)
- Fixed bug causing that zoom was not saved to parameter DB when auto fitting window to image was on
- Fixed bug that file pointer was invalid when deleting last file in folder and WrapAroundFolder=false in INI file
- New algorithm to guess encoding (UTF-8, ANSI) of JPEG comments when no encoding is explicitly specified
New features:
- Support for slide show transition effects (only in full screen mode)
  This can also be set in the INI file and on the command line, see readme.html for more info about command line parameters.
- File extensions can be registered to be opened by JPEGView over the 'Administration-Set as default viewer...' menu item
- New info button in top, left corner of screen in fullscreen mode to show EXIF info. EXIF into closable on the EXIF panel itself.
- When a supported camera RAW file was excluded in INI file but opened nevertheless in JPEGView (e.g. from explorer), the camera
  raw file type is added temporarily until program termination
- Russian translation of readme.html
- Improved Chinese translation

[1.0.27]
Replaced Intel JPEG library by Turbo JPEG (tjpeg, http://sourceforge.net/projects/libjpeg-turbo/)
This replacement has many reasons:
- The Intel library license was not an open source type license
- Newer versions of the Intel library are not free of charge. The old versions are not supported by Intel anymore.
- Some JPEG files could not be read by the Intel library. tjpeg can read these files.
- There is no free 64 bit version of the Intel library.
  (This is important for the future when there may be a 64 bit version of JPEGView)
The tjpeg library is open source and (at least on modern CPUs) performs comparable to the Intel library. It is used by many other
viewers and several Linux distributions.
Note that tjpeg including lossless JPEG transformations has been statically linked to JPEGView. Therefore the application has grown
by 300 KB. However the total size of the package is lower than before because IJL15.dll and jpegtran.exe are no longer needed.

Bugs fixed:
- When a folder is called 'pictures.jpg' for example, JPEGView no longer tries to process this folder as image
- Exposure time display: When nominator and denominator are both big numbers, a fractional value is shown instead of bigNum/bigDenum
- Reading slideshow list files (txt) encoded with UTF-8 and UTF-16 (little endian only) correctly
- Reading slideshow list files (txt) using uppercase file endings for image files correctly
New features:
- Lossless JPEG transformations integrated into JPEGView. The jpegtran.exe application is no longer part of the distribution of JPEGView.
- Basque translation, thanks to Xabier Aramendi
- Chinese translation, thanks to Yia Guo
- Keymap.txt now also supports to remap mouse buttons to commands.
- New INI file setting: 'TrimWithoutPromptLosslessJPEG'. Set to true to trim image before lossless JPEG transformations if needed without
                        prompting the user for each image.
- New INI file setting: DefaultWindowRect=sticky to save and restore the window size and position in window mode
- New INI file setting: 'SingleInstance'. Set to true to open all images in one single instance (window) of JPEGView.
- New INI file setting: 'ShowBottomPanel'. Set to false to disable showing the bottom panel when hovering with the mouse on bottom part
  of window/screen.
  Default is true (as before, was not configurable).
- New INI file setting: 'MaxSlideShowFileListSizeKB'. Sets maximum size of slide show text files in KB.
- New INI file setting: 'ForceGDIPlus'. If true, use GDI+ for reading JPEGS. Note that using GDI+ is much slower than the tjpeg library!
- New command line parameters:
  /slideshow xx    Starts JPEGView in slideshow mode, xx denotes the switching time in seconds. Only use when providing a file name,
                   folder name or file list on the command line as first parameter.
                   Example: jpegview.exe "filelist.txt" /slideshow 2
  /fullscreen      Starts JPEGView in full screen mode, ignoring the INI file setting that is currently active
  /order x         Forces file ordering mode 'x', ignoring the INI file setting that is currently active
                   'x' can be M, C, N or Z, for the file ordering modes 'M:modification date', 'C:creation date', 'N:file name', 'Z:random'
                   Example: jpegview.exe c:\pictures /slideshow 2 /order z
                   Shows a slideshow of the files in the folder "c:\pictures" using random ordering and an interval of 2 seconds.
Other changes:
- Because the tjpeg static library cannot be linked with VS2005, JPEGView is now compiled with VS2010. If you are using Windows XP 
  before SP2, install SP2 or keep using version 1.0.26.

[1.0.26]
Bugs fixed:
- Lossless rotate and crop now works in folders containing Unicode characters in the path.
  Note that the file title must not contain Unicode characters due to the fact that jpegtrans.exe does not support this.
New features:
- Portuguese (Portugal) translation, thanks to Sérgio Marques
- Support for viewing embedded JPEG images in camera RAW files using code from the dcraw project.
- Support for loading images using Windows Image Codecs (WIC). By default WIC is used to load JPEG XR files (also called
  High definition photo). When using Windows Vista or Windows 7, the Windows Camera Codec pack can be installed to get
  support for full size camera RAW images in JPEGView. Refer to the readme.html for instructions.
- New image processing: Perspective correction, use button on navigation panel to activate
- Rotation and perspective correction: Option to keep aspect ratio when auto cropping
- New INI file setting: 'AutoRotateEXIF' to disable auto rotation of images based on EXIF image orientation tag.
  Default is true (the EXIF auto rotation is supported since many versions, this setting is introduced to turn this off).
- New INI file setting: 'ScaleFactorNavPanel' to scale the navigation panel buttons, e.g. to make them larger on touch screens
- Clicking into the image (without dragging) toggles visibility of navigation panel
- Zoom by dragging mouse, activate with 'Shift-left mouse click' or with button on navigation panel
- Random sorting order for images within folder, can be used e.g. for random slideshow mode (activate with 'z' key)
- Improved crop handling: Crop rectangle can be resized using handles
- When in 'Adjust window to image' zoom mode and in window mode, the window adjusts automatically to zoomed image size while zooming
Other changes:
- Russian version of keymap and INI file (KeyMap_ru.txt, JPEGView_ru.ini) containig explanations in Russian. Be aware that
  these files must be renamed to KeyMap.txt and JPEGView.ini and copied over the original (English) versions when JPEGView shall use them.


[1.0.25.1]
- Rollback to VS2005 to compile the project because of problems with Windows 7 and Windows XP before SP2.
  The Windows 7 issues can be solved by using the latest versions of Windows SDK, WTL and ATL but the XP issue can't.
  JPEGView will remain on VS2005 at least for some versions.
  For VS2010 users, a solution and project file for VS2010 is included in the source code distribution.
  WebP library needs to be compiled with VS2010 as the VS2005 compiler produces crashing code from the Google source -
  so WebP will not work in XP SP1 or earlier.
- libwebp_a.dll made delay loaded - therefore JPEGView runs without this DLL when you do not need webp.

[1.0.25]
Bugs fixed:
- Previous image now works correctly when WrapAroundFolder is set to false
New features:
- Czech translation, thanks to Milos Koutny
- Greek translation, thanks to Paris Setos
- WEBP image format supported (load and save), using the source code provided by Google
- Keyboard shortcuts are now defined in the file KeyMap.txt, located in the EXE folder. Users can edit the key map if desired.
- New INI file setting: ExchangeXButtons to exchange the forward and backward mouse buttons (default is true, now can be switched to false)
- Putting file name first in window title (as it is convention in Windows)
- Double-click to fit image to screen (according auto zoom mode currently set), respectively 100 % when already fit
- New transformations: Mirror horizontally and vertically (and put the rotation commands also under the new transformations submenu)
Other changes:
- After having had massive problems with a compiler bug when compiling the WEBP source code, I decided to switch to Visual Studio 2010.
  Install VS 2010 to compile JPEGView - Express Edition (free) is sufficient.

[1.0.24.1]
Bugs fixed:
- Crash fixed when zooming down images of some sizes to 10 %

[1.0.24]
Bugs fixed:
- Navigation panel not truncated when window is small, instead the buttons are smaller.
  Tooltip texts are also no longer truncated in small windows.
- Modification date displayed with correct time zone adjustment
New features:
- Portuguese translation, thanks to Carpegieri Torezan
- New windowed mode that fits window size to image size, avoiding black borders. 
  Enable in INI file using:
  DefaultWindowRect=image
- New auto full screen mode that starts JPEGView in fullscreen mode when the image is larger than the screen
  or in windowed mode (window fit to image) when the image is small enough to fit to the screen.
  Enable in INI file using:
  ShowFullScreen=auto
  Note: This is the new default value in the INI file.
Changed behaviour:
- In the EXIF display (F2), the file name is broken into multiple lines when it is very long
- F11 key is now used for toggling full screen (used to be Ctrl-W). Navigation panel visibility now uses Ctrl-N (instead of F11)
- If the first image shown is smaller than the screen, JPEGView now starts in 'window fit to image' mode.
  To restore the old behavior (always use fullscreen), set ShowFullScreen=true in INI file

[1.0.23]
Bugs fixed:
- Improved memory handling for very large images. Read ahead turned off for very large images.
  Also improved memory footprint for processing large images by stripwise processing.
  Note that images > 100 MPixel still fail loading in most cases (tested with XP and 2GB memory)
- When out of memory, image loading fails with error message instead of crashing
- Set file modification time to EXIF time now correctly considers time zone
New features:
- New image processing: Rotation by arbitrary angle
  Using a button on the navigation panel, the image can be rotated with the mouse by a user-defined angle.
  The preview of rotation is real-time and uses simple point-sampling. When the rotation is applied to the
  image, bicubic interpolation is done, providing a good quality rotated image.
  Note that this processing cannot be stored in the parameter DB.
- Lossless crop
- Display of JPEG comments (EXIF user comment, EXIF image description, JPEG COM marker, in this order)
  Use INI file setting ShowJPEGComments to disable display of comments
- Set modification time to EXIF time for all files in folder
- New INI file setting: WrapAroundFolder.  Can be set to false to disable cyclic navigation
  within a folder (going from last to first image)

[1.0.22.1]
Bugs fixed:
- Fixed crash on startup when ShowFullScreen=true in INI file
New features:
- Swedish translation (thanks to Åke)

[1.0.22]
Bugs fixed:
- Copying EXIF data when saving image in screen size (Ctrl-Shift-S)
- Fixed crash when window is minimized while slideshow is playing
- Close button is active when moving mouse to top, right corner of screen
New features:
- Histogram can be displayed on the information panel (below EXIF information). Use ShowHistogram INI file entry
  to set if the histogram shall be shown on the panel or not. If not shown by default it can be blended in with
  a small button on the information panel.
- Updating EXIF thumbnail image when saving processed image
- New button on navigation panel: Keep processing parameters between images
- Changed behavior of fit to screen/actual size button, fit to screen is now default when not yet fit to screen
- Keeping fit to screen when rezising JPEGView window
- Image processing panel: Temporarily reset slider by clicking with the mouse on slider number
- Image processing panel: Reset slider to default value by double-clicking above slider
- Romanian translation (thanks to Silviu)
- Russian translation (thanks to Dmitry)
- Updated Spanish translation (thanks to Daniel)

[1.0.21]
Bugs fixed:
- After saving, navigation panel was not painted correctly sometimes
New features:
- New image processing: Unsharp mask
  Using a button on the image processing panel, an unsharp mask can be applied to the original pixels.
  Note that preview is always in 100 % zoom. Unsharp masking cannot be stored in the parameter DB and it is not possible to
  apply unsharp masking automatically after loading each image (unlike other processings)
- New image processing: Color saturation
  Support includes: Saturation on image processing panel, default saturation in INI file, parameter DB and landscape picture mode
- UI colors can be set in INI file. New default colors instead of the traditional green.
  To restore the old colors, change them in the INI file to:
  GUIColor=0 255 0
  HighlightColor=255 255 255
  SelectionColor=255 255 0
  SliderColor=255 0 0
- New INI file setting: OverrideOriginalFileWithoutSaveDialog
  If set to true, Ctrl-S overrides the original file on disk, applying the current processings without showing a dialog or prompting the user to confirm.
  Use with care, the original file will be replaced by the processed file!


[1.0.20]

Bugs fixed:
- Flag 'ReloadCurrent' used in user commands does no longer keep geometric parameters after reloading (zoom, pan, rotate)
New features:
- French translation
- Korean translation
- New INI file setting: DefaultSaveFormat to set the default format when saving images

[1.0.19]

Bugs fixed:
- Corrected Italian translation
- Hiding of mouse cursor when starting up in window mode and switching to fullscreen mode
- Application shown in taskbar also when the file open dialog is displayed after startup
- File names having a & in the name are now displayed correctly
New features:
- 'DefaultWindowRect=max' in INI file will start with maximized window (if fullscreen mode is false)
- Dropping files or folders to JPEGView from Windows Explorer now works
- Edit global and user INI file using context menu
- Backup and restore/merge parameter DB using context menu
- Small buttons for minimize/restore/close on top, right corner of screen in full screen mode (only shown
  when mouse moved to top of screen)
- New crop modes with fixed aspect ratio and fixed crop size
- Set modification date to current date or to EXIF date
- New command: Ctrl-M to mark file for toggling. Use Ctrl+Left/Right to toggle between marked files
- Showing file name in window title
- Smooth blending in and out of the navigation panel if mouse not moved for some time
- Thumbnail view behavior changed somewhat - pan by click supported


[1.0.18]

Bugs fixed:
- Resize now possible to sizes smaller than 800 x 600
- Renaming files with %x placeholder now works correctly
New features:
- Auto-rotate image using EXIF image orientation
- Save image in screen size (Ctrl-Shift-S)
- Keeping last selected image format when saving images
- Italian translation updated
- Toggle to window mode and back now first uses low quality sampling and improves quality on idle

[1.0.17]

Bugs removed:
- Image processing panel no longer truncated on 1024x768 screens, sliders are smaller in this case
New features:
- Toggle screen with Ctrl-F12 on multiscreen systems
- Window mode, toggle between window mode and full screen mode with Ctrl-W or with button on navigation panel
  The window mode can be made default in INI file and the starting window size can also be set in the INI file
- New INI file setting (LandscapeModeParams) that allows defining the parameters used in landscape enhancement mode
- New INI file setting (BackgroundColor) that allows setting the background color
- New INI file setting (NavigateWithMouseWheel) that allows using the mouse wheel for navigation (Ctrl-Wheel for zoom)


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