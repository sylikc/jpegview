@echo off

setlocal
REM this builds libpng's zlib and replaces the libs in the JPEGView src folder

REM if zlib gets dirty after building
REM  $ git submodule deinit -f -- extras/libpng-apng.src-patch/zlib
REM  $ git submodule update --init -- extras/libpng-apng.src-patch/zlib


SET XSRC_DIR=%~dp0..\..\src
SET XLIB_DIR=%~dp0..\libpng-apng.src-patch\zlib
SET XOUT_DIR=%~dp0zlib

IF EXIST "%XOUT_DIR%" (
	echo zlib output exists, please delete folder before trying to build
	exit /b 1
)

call :BUILD_CNMAKE x86
IF ERRORLEVEL 1 exit /b 1
call :BUILD_CNMAKE x64
IF ERRORLEVEL 1 exit /b 1


REM copy the libs over
REM error checking if a copy fails... throws error to caller
copy /y "%XOUT_DIR%\x86\zlib.lib" "%XSRC_DIR%\JPEGView\libpng-apng\lib\"
IF ERRORLEVEL 1 exit /b 1
copy /y "%XOUT_DIR%\x64\zlib.lib" "%XSRC_DIR%\JPEGView\libpng-apng\lib64\"
IF ERRORLEVEL 1 exit /b 1

echo NOTE: No zlib header files used by JPEGView


exit /b 0





:BUILD_CNMAKE

REM so the environments don't pollute each other
setlocal

SET XBUILD_DIR=%XOUT_DIR%\%1

mkdir "%XBUILD_DIR%" 2>nul

call "%~dp0vs-init.bat" %1

pushd "%XBUILD_DIR%"
cmake.exe -G"NMake Makefiles" -DCMAKE_BUILD_TYPE=Release "%XLIB_DIR%"
nmake.exe
SET XERROR=%ERRORLEVEL%

popd


exit /b %XERROR%
