@echo off

setlocal
REM this builds libjpeg-turbo and replaces the libs in the JPEGView src folder

SET XSRC_DIR=%~dp0..\..\src
SET XLIB_DIR=%~dp0..\third_party\libjpeg-turbo
SET XOUT_DIR=%~dp0libjpeg-turbo

IF EXIST "%XOUT_DIR%" (
	echo libjpeg-turbo output exists, please delete folder before trying to build
	exit /b 1
)

echo + Looking up NASM
where nasm.exe
IF ERRORLEVEL 1 (
	echo !!! NASM isn't required for building but highly recommended for speed improvements. !!!
	echo NASM isn't found on path!
	IF /I "%~1" NEQ "nonasm" (
		echo pass in parameter "nonasm" to skip this check
		exit /b 1
	)
)

call :BUILD_COPY_JPEGT x86 lib
IF ERRORLEVEL 1 exit /b 1
call :BUILD_COPY_JPEGT x64 lib64
IF ERRORLEVEL 1 exit /b 1




echo === HEADER FILES NOT MAINTAINED BY SCRIPT ===
echo NOTE: as for the header files, copy/replace files AS NEEDED
echo TO: src\JPEGView\libjpeg-turbo\include
echo FROM: extras\third_party\libjpeg-turbo
echo FROM: jconfig.h is in output directory


exit /b 0





:BUILD_COPY_JPEGT

REM so the environments don't pollute each other
setlocal

SET XBUILD_DIR=%XOUT_DIR%\%1

mkdir "%XBUILD_DIR%" 2>nul

call "%~dp0vs-init.bat" %1

pushd "%XBUILD_DIR%"
cmake.exe -G"NMake Makefiles" -DCMAKE_BUILD_TYPE=Release "%XLIB_DIR%"
IF ERRORLEVEL 1 exit /b 1
nmake.exe
IF ERRORLEVEL 1 exit /b 1

popd

REM copy the libs over
REM error checking if a copy fails... throws error to caller
copy /y "%XBUILD_DIR%\turbojpeg-static.lib" "%XSRC_DIR%\JPEGView\libjpeg-turbo\%~2\"
IF ERRORLEVEL 1 exit /b 1



exit /b 0
