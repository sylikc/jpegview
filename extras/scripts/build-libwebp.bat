@echo off

setlocal
REM this builds libwebp and replaces the libs in the JPEGView src folder

SET XSRC_DIR=%~dp0..\..\src
SET XLIB_DIR=%~dp0..\libwebp
SET XOUT_DIR=%~dp0libwebp

IF EXIST "%XOUT_DIR%" (
	echo libwebp output exists, please delete folder before trying to build
	exit /b 1
)

call :BUILD_WEBP x86
IF ERRORLEVEL 1 exit /b 1
call :BUILD_WEBP x64
IF ERRORLEVEL 1 exit /b 1


REM copy the libs over
REM error checking if a copy fails... throws error to caller
copy /y "%XOUT_DIR%\release-static\x86\lib\libwebp.lib" "%XSRC_DIR%\WEBP\lib\"
IF ERRORLEVEL 1 exit /b 1
copy /y "%XOUT_DIR%\release-static\x64\lib\libwebp.lib" "%XSRC_DIR%\WEBP\lib\libwebp64.lib"
IF ERRORLEVEL 1 exit /b 1


exit /b 0





:BUILD_WEBP

REM so the environments don't pollute each other
setlocal

mkdir "%XOUT_DIR%" 2>nul

call "%~dp0vs-init.bat" %1

pushd "%XLIB_DIR%"
nmake /f Makefile.vc CFG=release-static RTLIBCFG=static OBJDIR="%XOUT_DIR%"
exit /b %ERRORLEVEL%
