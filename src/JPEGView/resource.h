//{{NO_DEPENDENCIES}}
// Microsoft Visual C++ generated include file.
// Used by JPEGView.RC
//

// scripts in the extras directory will search this for strings as to which keys to include in KeyMap
// if a define is to be included in the publicly-exposed definitions, start the comment with ":KeyMap:"
// don't worry about the formatting, the script that auto-generates the definition will take care of it

#define JPEGVIEW_VERSION "1, 3, 46, 0\0"
// title for main window and msgbox so it can be change via actions
#define JPEGVIEW_TITLE "JPEGView"


#define IDD_ABOUTBOX				100
#define IDR_MAINFRAME				128
//#define IDR_JPEGVIEWTYPE	129
#define IDD_MAINDLG  				129
#define IDD_SLIDESHOWDLG            130

#define IDD_BATCHCOPY 1000
#define IDC_LIST_FILES 1001
#define IDC_FILES_IN_FOLDER 1002
#define IDC_RENAME 1003
#define IDC_PREVIEW 1004
#define IDC_CANCEL 1005
#define IDC_SAVE_PATTERN 1006
#define IDC_LBL_COPY 1007
#define IDC_PATTERN 1008
#define IDC_LBL_PLACEHOLDERS 1009
#define IDC_REMARK 1010
#define IDC_PLACEHOLDERS1 1011
#define IDC_PLACEHOLDERS2 1012
#define IDC_SELECTALL 1013
#define IDC_SELECTNONE 1014
#define IDC_RESULT 1015

#define IDD_SETFILEEXTENSIONS 1500
#define IDC_FILEEXTENSIONS 1001
#define IDC_HINT_FILE_EXT 1002
#define IDC_FE_CANCEL 1003
#define IDC_FE_OK 1004

#define IDD_HELP 7000

#define IDD_ABOUT 2000
#define IDC_CLOSE 1001
#define IDC_JPEGVIEW 1002
#define IDC_COPYRIGHT 1003
#define IDC_ICONJPEGVIEW 1004
#define IDC_LICENSE 1005
#define IDC_SIMDMODE 1006
#define IDC_NUMCORES 1007

#define IDD_SET_CROP_SIZE 3000
#define IDC_EDT_X 1001
#define IDC_EDT_Y 1002
#define IDC_RB_SCREEN 1003
#define IDC_RB_IMAGE 1004
#define IDC_BTN_CLOSE 1005
#define IDC_LBL_X 1006
#define IDC_LBL_Y 1007
#define IDC_LBL_PIXEL 1008
#define IDC_LBL_PIXEL2 1009

#define IDD_OPENWITH 4000
#define IDC_OPENWITHENTRIES 1001
#define IDC_OPENWITHMENULABEL 1002
#define IDC_OW_CLOSE 1003
#define IDC_OW_NEW 1004
#define IDC_OW_APP 1005
#define IDC_OW_BROWSE 1006
#define IDC_OW_TITLE 1007
#define IDC_OW_TITLE_LABEL 1008
#define IDC_OW_APP_LABEL 1009
#define IDC_OW_DELETE 1010
#define IDC_OW_SHORTCUT_LABEL 1011
#define IDC_OW_SHORTCUT 1012
#define IDC_OW_SHORTCUT_HELP 1013
#define IDC_OW_EDIT 1014

#define IDD_RESIZE                      5000
#define IDC_RS_CB_FILTER                2000
#define IDC_RS_LABEL_NEW_PERCENTS       2001
#define IDC_RS_LABEL_FILTER             2002
#define IDC_RS_LABEL_NEW_HEIGHT         2003
#define IDC_RS_LABEL_NEW__WIDTH         2004
#define IDC_RS_ED_PERCENTS              2005
#define IDC_RS_ED_HEIGHT                2006
#define IDC_RS_ED_WIDTH                 2007
#define IDC_RS_LABEL_PERCENTS           2008
#define IDC_RS_PIXELS                   2009
#define IDC_RS_PIXELS2                  2010

#define IDD_PRINT                       6000
#define IDC_PD_LABEL_PRINTER            1000
#define IDC_PD_PRINTER                  1001
#define IDC_PD_BTN_CHANGE_PRINTER       1002
#define IDC_PD_CB_PAPER                 2000
#define IDC_PD_PAPER_TITLE              2001
#define IDC_PD_CB_ORIENTATION           2002
#define IDC_PD_TITLE_ORIENTATION        2003
#define IDC_PD_SIZE_TITLE               2004
#define IDC_PD_RB_FIT                   2005
#define IDC_PD_RB_FILL					2006
#define IDC_PD_RB_FIXED                 2007
#define IDC_PD_ED_WIDTH                 2008
#define IDC_PD_ED_HEIGHT                2009
#define IDC_PD_X                        2010
#define IDC_PD_CM                       2011
#define IDC_PD_SCALING_TITLE            2012
#define IDC_PD_RB_SCALING_DRIVER        2013
#define IDC_PD_RB_SCALING_JPEGVIEW      2014
#define IDC_PD_ALIGNMENT_TITLE          2015
#define IDC_PD_RB_00                    2016
#define IDC_PD_RB_10                    2017
#define IDC_PD_RB_20                    2018
#define IDC_PD_RB_01                    2019
#define IDC_PD_RB_11                    2020
#define IDC_PD_RB_21                    2021
#define IDC_PD_RB_02                    2022
#define IDC_PD_RB_12                    2023
#define IDC_PD_RB_22                    2024
#define IDC_PD_PAPER_SOURCE_TITLE       2025
#define IDC_PD_CB_PAPER_SOURCE          2026
#define IDC_PD_MARGINS_TITLE            2027
#define IDC_PD_LEFT_TITLE               2028
#define IDC_PD_ED_LEFT                  2029
#define IDC_PD_RIGHT_TITLE              2030
#define IDC_PD_ED_RIGHT                 2031
#define IDC_PD_CM2                      2032
#define IDC_PD_TOP_TITLE                2033
#define IDC_PD_ED_TOP                   2034
#define IDC_PD_BOTTOM_TITLE             2035
#define IDC_PD_ED_BOTTOM                2036
#define IDC_PD_CM3                      2037

// this is not used anywhere in code, but defined in KeyMap as an "invalid command", so make sure the defined number doesn't match anything!
#define IDM_DONOTHING		1			// :KeyMap: do nothing (can be used to disable a standard command for a mouse button)

#define IDM_DEFAULT_ESC		255
#define IDM_STOP_MOVIE		1000
#define IDM_MINIMIZE		1010		// :KeyMap: minimize window
#define IDM_OPEN			1100		// :KeyMap: open file
#define IDM_EXPLORE			1101		// :KeyMap: open and select file in Explorer
#define IDM_SAVE			1500		// :KeyMap: save to file in original size, don't allow overwrite without prompt
#define IDM_SAVE_SCREEN		1501		// :KeyMap: save to file in screen size
#define IDM_SAVE_ALLOW_NO_PROMPT	1502		// :KeyMap: save to file in original size, allow overwrite without prompt when option 'OverwriteOriginalFileWithoutSaveDialog=true' is enabled in INI
#define IDM_RELOAD			1600		// :KeyMap: reload image
#define IDM_PRINT			1700		// :KeyMap: print image
#define IDM_COPY			2000		// :KeyMap: copy image to clipboard in screen size
#define IDM_COPY_FULL		2500		// :KeyMap: copy image to clipboard in original size
#define IDM_COPY_PATH		2525		// :KeyMap: copy image path to clipboard
#define IDM_PASTE			2550		// :KeyMap: paste image from clipboard
#define IDM_BATCH_COPY		2600		// :KeyMap: open batch copy dialog
#define IDM_RENAME			2610		// :KeyMap: rename current file, only works when image processing panel can be shown (i.e. panel not disabled in INI and window size is big enough)
#define IDM_MOVE_TO_RECYCLE_BIN 2620		// :KeyMap: delete the current file on disk - no confirmation (by moving it to the recycle bin)
#define IDM_MOVE_TO_RECYCLE_BIN_CONFIRM 2621		// :KeyMap: delete the current file on disk - with user confirmation (by moving it to the recycle bin)
#define IDM_MOVE_TO_RECYCLE_BIN_CONFIRM_PERMANENT_DELETE 2622		// :KeyMap: delete the current file on disk - confirmation when no recycle bin for this drive
#define IDM_TOUCH_IMAGE		2700		// :KeyMap: set modification time to current time
#define IDM_TOUCH_IMAGE_EXIF 2710		// :KeyMap: set modification time to EXIF time
#define IDM_TOUCH_IMAGE_EXIF_FOLDER 2720		// :KeyMap: set modification time to EXIF time for all images in folder
#define IDM_SET_WALLPAPER_ORIG 2770		// :KeyMap: Set original image file as desktop wallpaper
#define IDM_SET_WALLPAPER_DISPLAY 2774		// :KeyMap: Set image as displayed as desktop wallpaper
#define IDM_SHOW_FILEINFO   2800		// :KeyMap: toggle show file and EXIF info box in top, left corner
#define IDM_SHOW_FILENAME	3000		// :KeyMap: toggle show file name on top of screen
#define IDM_SHOW_NAVPANEL   3100		// :KeyMap: toggle show navigation panel
#define IDM_NEXT			4000		// :KeyMap: go to next image
#define IDM_PREV			5000		// :KeyMap: go to previous image
#define IDM_FIRST			5100		// :KeyMap: go to first image
#define IDM_LAST			5200		// :KeyMap: go to last image
#define IDM_LOOP_FOLDER		6000		// :KeyMap: set navigation mode loop through folder
#define IDM_LOOP_RECURSIVELY 6010		// :KeyMap: set navigation mode loop through folder and subfolders
#define IDM_LOOP_SIBLINGS	6020		// :KeyMap: set navigation mode loop through folders on same level
#define IDM_SORT_MOD_DATE	7000		// :KeyMap: sorting order by modification date
#define IDM_SORT_CREATION_DATE 7010		// :KeyMap: sorting order by creation date
#define IDM_SORT_NAME		7020		// :KeyMap: sorting order by name
#define IDM_SORT_RANDOM		7030		// :KeyMap: sorting order random
#define IDM_SORT_SIZE		7040		// :KeyMap: sorting order file size in bytes
#define IDM_SORT_ASCENDING	7100		// :KeyMap: sort ascending (increasing in value, e.g. A->Z, 0->9)
#define IDM_SORT_DESCENDING 7110		// :KeyMap: sort descending (decreasing in value, e.g. Z->A, 9->0)
#define IDM_SLIDESHOW_RESUME 7399		// :KeyMap: resume slide show (after stop)
#define IDM_SLIDESHOW_START 7400
#define IDM_SLIDESHOW_1		7401
#define IDM_SLIDESHOW_2		7402
#define IDM_SLIDESHOW_3		7403
#define IDM_SLIDESHOW_4		7404
#define IDM_SLIDESHOW_5		7405
#define IDM_SLIDESHOW_7		7407
#define IDM_SLIDESHOW_10	7410
#define IDM_SLIDESHOW_20	7420
#define IDM_EFFECT_NONE     7450
#define IDM_EFFECT_BLEND    7451
#define IDM_EFFECT_SLIDE_RL 7452
#define IDM_EFFECT_SLIDE_LR 7453
#define IDM_EFFECT_SLIDE_TB 7454
#define IDM_EFFECT_SLIDE_BT 7455
#define IDM_EFFECT_ROLL_RL  7456
#define IDM_EFFECT_ROLL_LR  7457
#define IDM_EFFECT_ROLL_TB  7458
#define IDM_EFFECT_ROLL_BT  7459
#define IDM_EFFECT_SCROLL_RL 7460
#define IDM_EFFECT_SCROLL_LR 7461
#define IDM_EFFECT_SCROLL_TB 7462
#define IDM_EFFECT_SCROLL_BT 7463
#define IDM_EFFECTTIME_VERY_FAST  7470
#define IDM_EFFECTTIME_FAST  7471
#define IDM_EFFECTTIME_NORMAL 7472
#define IDM_EFFECTTIME_SLOW  7473
#define IDM_EFFECTTIME_VERY_SLOW  7474
#define IDM_MOVIE_START_FPS 7500 // Pseudo entry (diff of the rest of the IDM_MOVIE_* values used to calculate actual FPS)
#define IDM_MOVIE_5_FPS		7505
#define IDM_MOVIE_10_FPS	7510
#define IDM_MOVIE_25_FPS	7525
#define IDM_MOVIE_30_FPS	7530
#define IDM_MOVIE_50_FPS	7550
#define IDM_MOVIE_100_FPS	7600
#define IDM_ROTATE_90		8000		// :KeyMap: rotate image 90 deg
#define IDM_ROTATE_270		9000		// :KeyMap: rotate image 270 deg
#define IDM_ROTATE          9100		// :KeyMap: show free rotation dialog
#define IDM_CHANGESIZE      9120		// :KeyMap: show dialog to change size of image
#define IDM_PERSPECTIVE     9150		// :KeyMap: show perpective correction dialog
#define IDM_MIRROR_H        9200		// :KeyMap: mirror image horizontally
#define IDM_MIRROR_V        9300		// :KeyMap: mirror image vertically
#define IDM_ROTATE_90_LOSSLESS 9400		// :KeyMap: Lossless JPEG transformation: rotate image 90 deg
#define IDM_ROTATE_90_LOSSLESS_CONFIRM 9401		// :KeyMap: Lossless JPEG transformation: rotate image 90 deg with user confirmation
#define IDM_ROTATE_270_LOSSLESS 9410		// :KeyMap: Lossless JPEG transformation: rotate image 270 deg
#define IDM_ROTATE_270_LOSSLESS_CONFIRM 9411		// :KeyMap: Lossless JPEG transformation: rotate image 270 deg with user confirmation
#define IDM_ROTATE_180_LOSSLESS 9420		// :KeyMap: Lossless JPEG transformation: rotate image 180 deg
#define IDM_MIRROR_H_LOSSLESS 9430		// :KeyMap: Lossless JPEG transformation: mirror image horizontally
#define IDM_MIRROR_V_LOSSLESS 9440		// :KeyMap: Lossless JPEG transformation: mirror image vertically
#define IDM_AUTO_CORRECTION	10000		// :KeyMap: toggle auto color and contrast correction
#define IDM_AUTO_CORRECTION_SECTION	10100		// :KeyMap: auto contrast correction on visible section only
#define IDM_LDC				10500		// :KeyMap: toggle local density correction (lighten shadows, darken highlights)
#define IDM_LANDSCAPE_MODE	10700		// :KeyMap: toogle landscape picture enhancement mode
#define IDM_KEEP_PARAMETERS	11000		// :KeyMap: toogle keep parameters between images
#define IDM_SAVE_PARAMETERS 11010		// :KeyMap: save to parameter DB
#define IDM_SAVE_PARAM_DB   11500		// :KeyMap: delete from parameter DB
#define IDM_CLEAR_PARAM_DB  11510
#define IDM_FIT_TO_SCREEN	12000		// :KeyMap: fit image to screen
#define IDM_FILL_WITH_CROP	12001		// :KeyMap: fit image to fill screen with crop
#define IDM_FIT_TO_SCREEN_NO_ENLARGE 12002
#define IDM_FIT_WINDOW_TO_IMAGE 12005		// :KeyMap: toggle fit window automatically to image, avoiding black borders
#define IDM_SPAN_SCREENS	12010		// :KeyMap: span over all screens
#define IDM_TOGGLE_MONITOR	14900		// :KeyMap: move to other monitor
#define IDM_FULL_SCREEN_MODE 12011		// :KeyMap: toggle full screen mode
#define IDM_HIDE_TITLE_BAR  12012		// :KeyMap: toggle hiding window title bar.  Note: in this mode, the window can't be manually resized
#define IDM_ALWAYS_ON_TOP   12013		// :KeyMap: toggle window mode to always on top
#define IDM_ZOOM_400        12020		// :KeyMap: zoom to 400 %
#define IDM_ZOOM_200		12030		// :KeyMap: zoom to 200 %
#define IDM_ZOOM_100		12040		// :KeyMap: zoom to 100 %
#define IDM_ZOOM_50			12050		// :KeyMap: zoom to 50 %
#define IDM_ZOOM_25			12060		// :KeyMap: zoom to 25 %
#define IDM_ZOOM_INC		12100		// :KeyMap: zoom in
#define IDM_ZOOM_DEC		12200		// :KeyMap: zoom out
#define IDM_ZOOM_MODE		12300
#define IDM_AUTO_ZOOM_FIT_NO_ZOOM	12500		// :KeyMap: set auto zoom mode fit to screen, never zoom
#define IDM_AUTO_ZOOM_FILL_NO_ZOOM	12510		// :KeyMap: set auto zoom mode fill screen, never zoom
#define IDM_AUTO_ZOOM_FIT	12520		// :KeyMap: set auto zoom mode fit to screen
#define IDM_AUTO_ZOOM_FILL	12530		// :KeyMap: set auto zoom mode fill screen
#define IDM_EDIT_GLOBAL_CONFIG 12600		// :KeyMap: edit global configuration
#define IDM_EDIT_USER_CONFIG   12610		// :KeyMap: edit user configuration
#define IDM_MANAGE_OPEN_WITH_MENU 12612
#define IDM_SET_AS_DEFAULT_VIEWER 12615
#define IDM_UPDATE_USER_CONFIG 12617
#define IDM_BACKUP_PARAMDB     12620		// :KeyMap: backup parameter DB
#define IDM_RESTORE_PARAMDB    12630		// :KeyMap: restore parameter DB
#define IDM_ABOUT			12900		// :KeyMap: show about box
#define IDM_EXIT			13000		// :KeyMap: exit JPEGView application
#define IDM_HELP			14000
#define IDM_TOGGLE			14100		// :KeyMap: toggle between marked image and current image
#define IDM_MARK_FOR_TOGGLE			14101		// :KeyMap: mark current image for toggling
#define IDM_COLOR_CORRECTION_INC	14200		// :KeyMap: increase color correction amount
#define IDM_COLOR_CORRECTION_DEC	14201		// :KeyMap: decrease color correction amount
#define IDM_CONTRAST_CORRECTION_INC	14300		// :KeyMap: increase contrast correction amount
#define IDM_CONTRAST_CORRECTION_DEC	14301		// :KeyMap: decrease contrast correction amount
#define IDM_CONTRAST_INC	14400		// :KeyMap: increase contrast
#define IDM_CONTRAST_DEC	14401		// :KeyMap: decrease contrast
#define IDM_GAMMA_INC		14500		// :KeyMap: increase gamma (brightness)
#define IDM_GAMMA_DEC		14501		// :KeyMap: decrease gamma (brightness)
#define IDM_LDC_SHADOWS_INC		14600		// :KeyMap: increase lighting shadows
#define IDM_LDC_SHADOWS_DEC		14601		// :KeyMap: decrease lighting shadows
#define IDM_LDC_HIGHLIGHTS_INC		14700		// :KeyMap: increase darken highlights
#define IDM_LDC_HIGHLIGHTS_DEC		14701		// :KeyMap: decrease darken highlights
#define IDM_SHARPEN_INC		15300		// :KeyMap: increase sharpness
#define IDM_SHARPEN_DEC		15301		// :KeyMap: decrease sharpness
#define IDM_TOGGLE_RESAMPLING_QUALITY	14800		// :KeyMap: toggle resampling quality (between point sampling and filtering)
#define IDM_TOGGLE_FIT_TO_SCREEN_100_PERCENTS	15000		// :KeyMap: toggle between fit to screen and 100 % zoom
#define IDM_TOGGLE_FILL_WITH_CROP_100_PERCENTS	15001		// :KeyMap: toggle between fill with crop and 100 % zoom
#define IDM_EXCHANGE_PROC_PARAMS	15100		// :KeyMap: toggle between two sets of image processing parameters
#define IDM_PAN_UP	15200		// :KeyMap: pan up
#define IDM_PAN_DOWN	15201		// :KeyMap: pan down
#define IDM_PAN_RIGHT	15202		// :KeyMap: pan right
#define IDM_PAN_LEFT	15203		// :KeyMap: pan left
#define IDM_CONTEXT_MENU	16000		// :KeyMap: display context menu

#define IDM_CROP_SEL		20000
#define IDM_LOSSLESS_CROP_SEL 20010
#define IDM_COPY_SEL		20100
#define IDM_ZOOM_SEL		20200

// all crop menu IDs must be between IDM_CROPMODE_FREE and IDM_CROPMODE_USER,
// because the actual numbers are used for arithmetic comparison in CCropCtl::ShowCropContextMenu()
#define IDM_CROPMODE_FREE          20300
#define IDM_CROPMODE_FIXED_SIZE    20400
#define IDM_CROPMODE_5_4           20450
#define IDM_CROPMODE_4_3           20500
#define IDM_CROPMODE_7_5           20550
#define IDM_CROPMODE_3_2           20600
#define IDM_CROPMODE_16_9          20700
#define IDM_CROPMODE_16_10         20800
#define IDM_CROPMODE_1_1           20801
#define IDM_CROPMODE_IMAGE         20899
#define IDM_CROPMODE_USER          20900

#define IDM_FIRST_USER_CMD  22000
#define IDM_LAST_USER_CMD   22099

#define IDM_FIRST_OPENWITH_CMD  22100
#define IDM_LAST_OPENWITH_CMD   22199

// in the main menu
// these position must be changed if menu items are inserted
#define SUBMENU_POS_OPENWITH 3
#define SUBMENU_POS_MODDATE 9
#define SUBMENU_POS_WALLPAPER 10
#define SUBMENU_POS_NAVIGATION 23
#define SUBMENU_POS_DISPLAY_ORDER 24
#define SUBMENU_POS_MOVIE 25
#define SUBMENU_POS_TRANSFORM 27
#define SUBMENU_POS_TRANSFORM_LOSSLESS 28
#define SUBMENU_POS_ZOOM 36
#define SUBMENU_POS_AUTOZOOMMODE 37
#define SUBMENU_POS_SETTINGS 39
#define SUBMENU_POS_USER_COMMANDS 41

// in the crop menu
#define SUBMENU_POS_CROPMODE 3

// Next default values for new objects
// 
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE	2000
#define _APS_NEXT_CONTROL_VALUE		2000
#define _APS_NEXT_SYMED_VALUE		101
#define _APS_NEXT_COMMAND_VALUE		32772
#endif
#endif
