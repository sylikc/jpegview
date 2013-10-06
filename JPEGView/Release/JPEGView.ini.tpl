[JPEGView]

; This user INI file has been created from template JPEGView.ini.tpl
; Settings in this file override the setting in the global INI file located in the EXE path


; Language used in the user interface. Set to 'auto' to use the language of the operating system.
; Other languages must use the ISO 639-1 language code (two letters)
; Currently supported:
; 'en'    English (default)
; 'es'    Spanish (Spain)
; 'es-ar' Spanish (latinoamerica)
; 'pt-br' Portuguese (Brasil)
; 'pt'    Portuguese
; 'de'    German
; 'it'    Italian
; 'fr'    French
; 'kr'    Korean
; 'ro'    Romanian
; 'ru'    Russian
; 'sv'    Swedish
; 'cs'    Czech
; 'el'    Greek
; 'eu'    Basque
Language=auto


; File endings of files to be decoded by WIC (Windows Image Converter)
; If the Microsoft Camera Codec pack is installed, full size camera RAW files can be read with WIC
; Add the file endings of the raw files to view here, e.g.
; FilesProcessedByWIC=*.wdp;*.hdp;*.jxr;*.nef
FilesProcessedByWIC=*.wdp;*.hdp;*.jxr

; File endings of camera RAW files to be searched for embedded JPEG thumb images to display
; Reading just these embedded JPEGs is much faster than decoding the RAW using WIC
FileEndingsRAW=*.pef;*.dng;*.crw;*.nef;*.cr2;*.mrw;*.rw2;*.orf;*.x3f;*.arw;*.kdc;*.nrw;*.dcr;*.sr2;*.raf

; Background color, R G B, each component must be in [0..255], e.g. "128 128 128" for a middle grey
BackgroundColor=0 0 0

; GUI colors, R G B format as used by BackgroundColor
GUIColor=243 242 231
HighlightColor=255 205 0
SelectionColor=255 205 0
SliderColor=255 0 80
FileNameColor=255 255 255

; Initial fixed contrast correction to apply to all images. Must be in -0.5 .. 0.5
; Values > 0 increase contrast, values < 0 decrease contrast
Contrast=0.0

; Initial fixed gamma correction to apply to all images. Must be between 0.5 and 2
; Use gamma<1 to increase brightness and gamma>1 to decrease brightness
Gamma=1.0

; Color saturation. Must be in 0.0 .. 2.0
; 0.0 means grey scale image, 1.0 no additional color saturation, 2.0 maximal saturation
Saturation=1.0

; Sharpening to apply for downsampled images. Must be in 0 .. 0.5
; Note that for 100 % zoom, the BestQuality filter will not apply any sharpening, only the other filters do
Sharpen=0.3

; Default parameters for unsharp masking: Radius Amount Threshold
; Note that no unsharp masking can be applied automatically to every image - this setting only provides the default parameters
; when entering the unsharp mask mode
UnsharpMaskParameters=1.0 1.0 4.0

; Default parameters for controlling rotation and perspective correction
RTShowGridLines=true
RTAutoCrop=true
RTPreserveAspectRatio=true

; Default color balance. Negative values for C,M,Y correction, positive for R,G,B.
; Values must be in -1.0 .. +1.0
CyanRed=0.0
MagentaGreen=0.0
YellowBlue=0.0

; Set to true to use high quality sampling as default.
HighQualityResampling=true

; Start in full screen or windowed mode
; 'true' or 'false' to always start in fullscreen, respectively windowed mode
; 'auto' to choose best mode depending on image size of first image - windowed when it is small, full screen when it is large
ShowFullScreen=auto

; Sets the default position and size of the window in window mode. Possible values:
; 'auto' for 2/3 of screen size
; 'max' to start with maximized window
; 'image' to adjust the window size automatically to the image size
; 'sticky' to automatically restore the last used window size (when ShowFullScreen=auto only the upper,left position is restored)
; 'left top right bottom', space separated e.g: 100 100 900 700
DefaultWindowRect=image

; Contains the stored window rectangel in case of DefaultWindowRect=sticky
StickyWindowRect=

; The initial crop window size when using 'Fixed Size' crop mode
DefaultFixedCropSize=320 200

; Set to true to initially display the filename of each image in the upper, left corner of the screen
ShowFileName=false

; The elements to show when showing the file name.
; Possible elements:
; %filename% : file name
; %filepath% : File name and path
; %index%    : Index of image in folder, e.g. [1/12]
; %zoom%     : Current zoom factor
; %size%     : Size of image in pixels (w x h)
; %filesize% : Size of image on disk
FileNameFormat=%index%  %filepath%

; Set to true to initially display the file info box (EXIF info if available)
ShowFileInfo=false

; Set to true to show the histogram on the file info panel by default
ShowHistogram=false

; Set to true to show JPEG comments (EXIF user comment, EXIF image description or JPEG comment) in the file info box
ShowJPEGComments=true

; Set to true to show the bottom panel when moving the mouse to the bottom of the screen/window.
; The bottom panel provides basic image processing functionality
ShowBottomPanel=true

; Set to false if the navigation panel shall not be blended to the image
ShowNavPanel=true

; Set to false to disable the thumbnail image blended in during zoom and pan
ShowZoomNavigator=true

; Blending factor of the navigation panel when the mouse is not on that panel. Set to 0.0 to only
; show the panel when the mouse is over the panel
BlendFactorNavPanel=0.5

; Scaling factor for the navigation panel. Increase if the buttons on the panel are too small, e.g. on a touchscreen.
ScaleFactorNavPanel=1.0

; Set to true to keep the zoom, pan, contrast, gamma, sharpen and rotation setting between the images 
KeepParameters=false

; CPUType can be AutoDetect, Generic, MMX or SSE
; Generic should work on all CPUs, MMX needs at least MMX II (starting from PIII)
; Use AutoDetect to detect the best possible algorithm to use
CPUType=AutoDetect

; Number of CPU cores used. Set to 0 for auto detection.
; Must be 1 to 4, or 0 for auto detect.
CPUCoresUsed=0

; DownSamplingFilter can be BestQuality, NoAliasing or Narrow
; The BestQuality filter produces a very small amount of aliasing.
; The NoAliasing filter is a Lanczos filter that has almost no aliasing when sharpen is set to zero
; The Narrow filter produces quite a lot of aliasing but will sharpen much and also sharpens 100% images
DownSamplingFilter=BestQuality

; Sorting order of the files when displaying the image files in a folder
; Can be LastModDate, CreationDate, FileName or Random
FileDisplayOrder=LastModDate

; Navigation within or between folders
; LoopFolder : Loop within the source folder and never leave this folder
; LoopSameFolderLevel: Loop to next folder on the same hierarchy level (sibling folders)
; LoopSubFolders: Loop into subfolders of the source folder
FolderNavigation=LoopFolder

; If true, the mouse wheel can be used to navigate forward and backward and zoom must be done with Ctrl-MouseWheel
; If false, zoom is done with the mousewheel (no Ctrl required)
NavigateWithMouseWheel=false

; If true, the extended mouse buttons (Forward and backward) are exchanged compared to Internet explorer
; This is useful to put the 'go to next image' functionality to the button that is better reachable
ExchangeXButtons=true

; If true the files in a folder are shown with wrap around, thus going from last to first image and vice versa
; If false navigation stops on the last and first image
WrapAroundFolder=true

; If true, JPEG images are auto rotated according to EXIF image orientation tag if present.
AutoRotateEXIF=true

; Auto zoom mode
; FitNoZoom : Fit images to screen, never enlarge image
; FillNoZoom : Fill screen with no black borders, crop if necessary but not too much, never enlarge image
; Fit : Fit images to screen
; Fill : Fill screen with no black borders, crop if necessary
AutoZoomMode=FitNoZoom

; Maximum size of slide show text files in KB
MaxSlideShowFileListSizeKB=200

; Transition effect for slide shows in full screen mode - ignored when used in window mode
; Possible transition effects: None, Blend, SlideRL, SlideLR, SlideTB, SlideBT, RollRL, RollLR, RollTB, RollBT, ScrollRL, ScrollLR, ScrollTB, ScrollBT
SlideShowTransitionEffect=Blend

; Time of the slide show transition effect in milliseconds, only used in full screen mode
SlideShowEffectTime=250

; If set to true, only one single instance of JPEGView runs at any time, if false multiple instances are allowed
; Set to true to open all images in the same JPEGView window.
SingleInstance=false

; Force using GDI+ for reading JPEGS. Only use when you have problems reading your JPEGs with the default Intel library.
; Note that using GDI+ is slower than the Intel JPEG library!
ForceGDIPlus=false

; Quality when saving JPEG files (in 0..100 where 100 is the highest quality)
JPEGSaveQuality=85

; Quality when saving WEBP files with lossy compression (in 0..100 where 100 is the highest quality)
WEBPSaveQuality=85

; Default format for saving files. Supported formats: jpg, bmp, png, tif, webp
DefaultSaveFormat=jpg

; Set to true to create a parameter DB entry when saving an image with JPEGView to avoid processing it again
CreateParamDBEntryOnSave=true

; If set to true, Ctrl-S overrides the original file on disk, applying the current processings without
; showing a dialog or prompting the user to confirm.
; CAUTION: Use at your own risk! Be aware that the original image file is overriden and cannot be restored anymore!
OverrideOriginalFileWithoutSaveDialog=false

; If set to true, lossless JPEG transformations will trim the image as needed without prompting the user.
; This will remove 15 pixel rows/colums at the image borders in worst case.
; CAUTION: Use at your own risk! Be aware that the original image file is overriden and the trimmed borders cannot be restored anymore!
TrimWithoutPromptLosslessJPEG=false

; Only for multi-monitor systems!
; Monitor to start the application on
; -1: Use monitor with largest resolution, primary monitor if several monitors have the same resolution
;  0: Use primary monitor
;  1...n: Use the non-primary monitor with index n
DisplayMonitor=-1

; Automatic contrast correction by histogram equalization
; F5 enables/disables the correction on the current image.
AutoContrastCorrection=false

; Using the following two keys, it is possible to explicitely exclude/include folders from the contrast correction.
; More specific patterns have presedence over less specific patterns and inclusion has presedence over exclusion if
; a folder matches both. Example: '*\pics\orig\* has precedence over *\pics\* because it is more specific
; Use the ; character to separate two expressions.
; Example: ACCExclude=%mypictures%\Digicam\edited\*;*.bmp
; This will exclude all files in the ..\My Pictures\Digicam\edited\ folder and all its subfolders and all bmp files
; from automatic contrast correction.
; The following two placeholders are recognized:
;   %mypictures%  : "My documents\My Pictures" folder
;   %mydocuments% : "My documents" folder
ACCExclude=
ACCInclude=

; Amount of automatic contrast correction
; 0 means no contrast correction, 1 means full (to black and white point) contrast correction. Must be in (0 .. 1)
AutoContrastCorrectionAmount=0.5

; Amount of color correction in the color channels reg, green, blue, cyan, magenta and yellow
; The numbers must be between 0.0 (no correction) and 1.0 (total correction towards the grey world model)
ColorCorrection = "R: 0.2 G: 0.1 B: 0.35 C: 0.1 M: 0.3 Y: 0.15"

; Amount of automatic brightness correction
; 0 means no brightness correction, 1 means full correction to middle grey. Must be in (0 .. 1)
AutoBrightnessCorrectionAmount=0.2

; Automatic correction of local density (local brightness of images)
; Can be enabled/disabled on the image with F6
LocalDensityCorrection=false

; See remark about exclusion/inclusion at the ACCExclude setting.
; The same applies to these settings.
LDCExclude=
LDCInclude=

; Amount of local density correction of shadows
; Can be in [0, 1]
LDCBrightenShadows=0.5

; Deep shadow steepness of enhancement
; Can be in [0, 1], values bigger than 0.9 are not recommended
LDCBrightenShadowsSteepness=0.5

; Amount of local density correction of highlights
; Can be in [0, 1]
LDCDarkenHighlights=0.25

; Parameter set used in landscape enhancement mode
; Space separated, use -1 to leave parameter untouched
; Contrast Gamma Sharpen ColorCorrection ContrastCorrection LightenShadows DarkenHighlights DeepShadows CyanRed MagentaGreen YellowBlue Saturation
LandscapeModeParams=-1 -1 -1 -1 0.5 1.0 0.75 0.4 -1 -1 -1 -1

; User commands
; User command must be named UserCmd# where # stands for a number. The numbers 0 to 2 are already used by the global INI file.
; User command must have the following form:
; UserCmd#="KeyCode: %Key% Cmd: '%Cmd%' [Confirm: '%confirm%'] [HelpText: '%help%'] [Flags: '%flags%']"
; %Key% :   Keycode (virtual key code) of the key that invokes the command. Do not define keys already used by JPEGView.
;           For the character keys A to Z, the literal character can be used instead of the key code, e.g. A for the A-key.
; %Cmd% :   Application to start, including command arguments. Enclose the application name with double quotes ("") if the path contains
;           spaces. To execute command line commands or batch files, use 'cmd /c command' respectively 'cmd /c MyBatchFile.bat'.
;           The following placeholders can be used in the %cmd% argument:
;           %filename%   : Filename of current image, including path
;           %filetitle%  : Filename of current image, excluding path
;           %directory%  : Directory or current image, without trailing backslash
;           %mydocuments%: The 'My Documents' folder without trailing backslash
;           %mypictures% : The 'My Pictures' folder without trailing backslash
;           %exepath%    : Path to EXE folder where JPEGView is running
;           %exedrive%   : Drive letter of the EXE path, e.g. "c:"
;           The resulting names are enclosed with double quotes automatically by JPEGView when no backslash if before or after the placeholder.
; %confirm% : Message text that is shown and must be confirmed before the command is executed. 
;           This is an optional argument, if not used, no confirmation is needed for the command.
; %help% :  Help string that is displayed in JPEG view when F1 is pressed.
;           This is an optional argument, if not used, no help text will be available.
; %flags% : The following flags are supported:
;           NoWindow :         For console applications - if set do not display a console window.
;                              If the started application is the command interpreter (cmd.exe) this flag is implicitly set.
;           ShortFilename :    If set, the short (8.3) file name (and path) is passed to the executing application.
;                              Set this flag if the executing application cannot handle long file names or files and path
;                              names containing spaces.
;           WaitForTerminate : If set, JPEGView is blocked until the command has finished execution. If not set, the command is
;                              started and JPEGView continues.
;           MoveToNext :       If set, JPEGView moves to the next image in the folder after the command has been executed.
;                              Cannot be combined with the ReloadCurrent flag.
;           ReloadFileList :   If set, the file-list of the current folder is reloaded after execution of the command. Set this
;                              flag when the command modifies the content of the folder (e.g. moves, renames or deletes files).
;           ReloadCurrent :    If set, the currently shown file is re-loaded from disk after execution of the command.
;                              Set this flag if the command changes the pixel data of the current image.
;           ReloadAll:         If set, the file-list of the current folder is reloaded and the current image and all cached images
;                              are reloaded. Set this flag only if the command changes pixel data of images other than the current.
;           KeepModDate:       Keeps the modification date/time of the current image. Can be used to preserve this time stamp
;                              when modifications on the image are done to keep sort ordering.
;                              Caution: When using this flag, always combine with the WaitForTerminate flag!
;           ShellExecute:      Uses the ShellExecute() system call to start the external process instead of CreateProcess().
;                              Some applications do not start properly with CreateProcess(). If this flag is set, the WaitForTerminate
;                              must not be used. All flags requiring WaitForTerminate to be set must not be set either.
;                              Typically ShellExecute is set to start large external applications, e.g. an image editor.


; IMPORTANT: Start with UserCmd1 except you want to override the Delete file command

; Here are some examples for user commands
;UserCmd1="KeyCode: P  Cmd: 'C:\WINDOWS\system32\mspaint.exe /p %filename%' Confirm: 'Do you really want to print the file %filename%?' HelpText: 'P\tPrint current image'"
;UserCmd2="KeyCode: Q  Cmd: 'cmd /c move %filename% "%mypictures%\trash\"' Confirm: 'Really move file to %mypictures%\trash\%filetitle%' HelpText: 'Q\tMove file to trash directory' Flags: 'WaitForTerminate ReloadFileList MoveToNext'"
;UserCmd3="KeyCode: W  Cmd: 'cmd /c copy %filename% "%mypictures%\trash\%filetitle%"' Confirm: 'Really copy file to %mypictures%\trash\%filetitle%' HelpText: 'W\tCopy file to trash directory' Flags: 'WaitForTerminate'"
;UserCmd4="KeyCode: A  Cmd: 'cmd /u /c echo %filename% >> "%mydocuments%\test.txt"' HelpText: 'A\tAppend to file list'"
