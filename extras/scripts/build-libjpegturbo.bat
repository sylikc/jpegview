@echo off

setlocal
REM this builds libwebp and replaces the libs in the JPEGView src folder

SET XSRC_DIR=%~dp0..\..\src
SET XLIB_DIR=%~dp0..\libjpeg-turbo
SET XOUT_DIR=%~dp0libjpeg-turbo

IF EXIST "%XOUT_DIR%" (
	echo libjpeg-turbo output exists, please delete folder before trying to build
	exit /b 1
)

call :BUILD_JPEGT x86
IF ERRORLEVEL 1 exit /b 1
call :BUILD_JPEGT x64
IF ERRORLEVEL 1 exit /b 1



REM copy the libs over
REM error checking if a copy fails... throws error to caller
copy /y "%XOUT_DIR%\x86\*.lib" "%XSRC_DIR%\JPEGView\libjpeg-turbo\lib\"
IF ERRORLEVEL 1 exit /b 1
copy /y "%XOUT_DIR%\x64\*.lib" "%XSRC_DIR%\JPEGView\libjpeg-turbo\lib64\"
IF ERRORLEVEL 1 exit /b 1


echo === HEADER FILES NOT MAINTAINED BY SCRIPT ===
echo NOTE: as for the header files, copy/replace files AS NEEDED
echo TO: src\JPEGView\libjpeg-turbo\include
echo FROM: extras\libjpeg-turbo
echo FROM: jconfig.h is in output directory


exit /b 0





:BUILD_JPEGT

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
