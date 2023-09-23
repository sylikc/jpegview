@echo off

setlocal

REM The copying of DLLs was formerly the primary function of the Post-Build Event
REM but localization, generating, and copying things around from different places is necessary to avoid duplication

REM NOTE: this batch file does not fail on error, it will continue processing (just like the way the built-in post-build event works)


IF /I "%~4" EQU "" (
	echo ERROR: This batch file is meant to be run from src\JPEGView vcxproj's Post-Build Event
	echo;
	echo    It requires VS Macro parameters to function properly.
	exit /b 1
)

REM ----------- VARIABLES FROM Visual Studio -----------
REM these match the order in the call from post-build event

REM 32 or 64
SET PLATFORM_ARCHITECTURE=%~1

REM base name of the project, to detect if we're building the VS2017 version
SET PROJECT_NAME=%~2

REM absolute path to src\JPEGView, with trailing "\"
SET PROJECT_DIR=%~3

REM absolute path to the output dir (target to copy to) with trailing backslash
SET OUT_DIR_FULL_PATH=%~4


REM print the output
echo ----- Post-Build batch file: %0 -----
echo %% PLATFORM_ARCHITECTURE=%PLATFORM_ARCHITECTURE%
echo %% PROJECT_NAME=%PROJECT_NAME%
echo %% PROJECT_DIR=%PROJECT_DIR%
echo %% OUT_DIR_FULL_PATH=%OUT_DIR_FULL_PATH%



REM ----------- COPY COMMON CONFIG FILES -----------
REM these are common across all builds, regardless of architecture or version
echo + XCopy Config\* ...
xcopy "%PROJECT_DIR%Config\*" "%OUT_DIR_FULL_PATH%" /Y /C /D
echo ~ ErrorLevel: %ErrorLevel%

echo + XCopy LICENSE.txt ...
xcopy "%PROJECT_DIR%..\..\LICENSE.txt" "%OUT_DIR_FULL_PATH%" /Y /D
echo ~ ErrorLevel: %ErrorLevel%


REM ----------- COPY DLL FILES -----------
REM qbnu standardized JPEGView's DLL structure which puts DLLs in either the bin (32-bit) or bin64 (64-bit) folders

IF /I "%PROJECT_NAME%" EQU "JPEGView_VS2017" (
	REM specifically for VS2017:  Currently, VS2017 project file is used to build the XP-version ONLY
	REM the XP version does NOT support any of the new DLLs at this time
	exit /b 0
)

REM all other projects files get DLLs...
call :COPY_DLLS libjxl
call :COPY_DLLS libheif
call :COPY_DLLS libavif
call :COPY_DLLS lcms2
call :COPY_DLLS libraw


exit /b 0




REM ================================================================================

:COPY_DLLS

setlocal

SET DEP_NAME=%~1

REM set default as 32-bit version
SET SRC_DIR=%PROJECT_DIR%%DEP_NAME%\bin
IF /I "%PLATFORM_ARCHITECTURE%" EQU "64" (
	REM append "64" to the 32-bit version path
	SET SRC_DIR=%SRC_DIR%%PLATFORM_ARCHITECTURE%
)

echo + XCopy %PLATFORM_ARCHITECTURE%-bit DLLs for "%DEP_NAME%" if needed ...
xcopy "%SRC_DIR%\*.dll" "%OUT_DIR_FULL_PATH%" /Y /D
echo ~ ErrorLevel: %ErrorLevel%

exit /b %ErrorLevel%

REM ================================================================================
