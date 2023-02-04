@echo off

setlocal
REM this builds libheif and replaces the libs in the JPEGView src folder

SET XSRC_DIR=%~dp0..\..\src\JPEGView\libheif
SET XLIB_DIR=%~dp0..\libheif
SET XOUT_DIR=%~dp0libheif

SET XVENV_DIR=%~dp0venv-meson

IF EXIST "%XOUT_DIR%" (
	echo libheif output exists, please delete folder before trying to build
	exit /b 1
)

echo + Looking up NASM
where nasm.exe
IF ERRORLEVEL 1 (
	echo NASM isn't found on path!
	exit /b 1
)

echo + Looking up Python
where python.exe
python -V
IF ERRORLEVEL 1 (
	echo Python isn't found on path!
	exit /b 1
)

echo + Create VENV
python.exe -m venv "%XVENV_DIR%"

echo + Activate VENV
call "%XVENV_DIR%\Scripts\activate.bat"

echo + Install Meson and Ninja inside VENV
python.exe -m pip install -U pip
python.exe -m pip install -U meson ninja



call :BUILD_COPY_DAV1D x86 bin
IF ERRORLEVEL 1 exit /b 1
call :BUILD_COPY_LIBDE x86 bin
IF ERRORLEVEL 1 exit /b 1
call :BUILD_COPY_LIBHEIF x86 ""
IF ERRORLEVEL 1 exit /b 1


call :BUILD_COPY_DAV1D x64 bin64
IF ERRORLEVEL 1 exit /b 1
call :BUILD_COPY_LIBDE x64 bin64
IF ERRORLEVEL 1 exit /b 1
call :BUILD_COPY_LIBHEIF x64 "64"
IF ERRORLEVEL 1 exit /b 1



exit /b 0




echo === HEADER FILES NOT MAINTAINED BY SCRIPT ===
echo NOTE: as for the header files, copy/replace files AS NEEDED
echo TO: src\JPEGView\libheif\include\libheif
echo .h FROM: extras\libheif\libheif\libheif
echo FROM: %OUT_DIR%\libheif-[arch]\libheif\heif_version.h

exit /b 0





:BUILD_COPY_DAV1D

REM so the environments don't pollute each other
setlocal

SET XBUILD_DIR=%XOUT_DIR%\dav1d-%1\build
SET XDIST_DIR=%XOUT_DIR%\dav1d-%1\dist

mkdir "%XBUILD_DIR%" 2>nul
mkdir "%XDIST_DIR%" 2>nul

call "%~dp0vs-init.bat" %1

pushd "%XLIB_DIR%\dav1d"

REM or use --reconfigure flag
REM rd /s /q "%XLIB_DIR%\dav1d\build" 2>nul

REM part of the commands came from dav1d.cmd in libheif third-party


REM meson.exe from pip install venv\scripts
REM if you want to build dynamic DLL do this:  meson setup --buildtype release --prefix "%XDIST_DIR%" "%XBUILD_DIR%"
meson setup --default-library=static --buildtype release --prefix "%XDIST_DIR%" "%XBUILD_DIR%"
IF ERRORLEVEL 1 exit /b 1

REM ninja.exe from pip install venv\scripts
ninja -C "%XBUILD_DIR%"
IF ERRORLEVEL 1 exit /b 1
REM libheif building requires things to be "installed" to work
ninja -C "%XBUILD_DIR%" install
IF ERRORLEVEL 1 exit /b 1

REM when dynamic, will copy this out
REM copy /y "%XBUILD_DIR%\src\dav1d.dll" "%XSRC_DIR%\%2\"
REM IF ERRORLEVEL 1 exit /b 1

popd

exit /b 0



:BUILD_COPY_LIBDE

REM so the environments don't pollute each other
setlocal

SET XLIB_DIR=%XLIB_DIR%\libde265

REM it builds into its own directory, clear it before proceeding
REM if upstream changes the build locations, then this will have to match what's in build.bat upstream
SET XBUILD_DIR=%XLIB_DIR%\bin_%1

rd /s /q "%XBUILD_DIR%"

REM build.bat has this, but it's fixed on some version which might not be what vs-init detects
call "%~dp0vs-init.bat" %1

pushd "%XLIB_DIR%"
call build.bat %1
IF ERRORLEVEL 1 exit /b 1


copy /y "%XBUILD_DIR%\libde265.dll" "%XSRC_DIR%\%2\"
IF ERRORLEVEL 1 exit /b 1


popd

exit /b 0








:BUILD_COPY_LIBHEIF

REM so the environments don't pollute each other
setlocal

SET XARCH=%1

SET XBUILD_DIR=%XOUT_DIR%\libheif-%XARCH%

SET XDAV1D_DIST=%XOUT_DIR%\dav1d-%XARCH%\dist
SET XDE265_DIR=%XLIB_DIR%\libde265


mkdir "%XBUILD_DIR%" 2>nul

call "%~dp0vs-init.bat" %XARCH%

pushd "%XBUILD_DIR%"


REM de265.h has an include
REM #include <libde265/de265-version.h>
REM which requires doing this since on windows there's no "install" which takes care of this
REM if you try to just include the extra directory, bad things happen since there are other .h files in there ... -DLIBDE265_INCLUDE_DIR="%XDE265_DIR%\extra"
copy /y "%XDE265_DIR%\extra\libde265\de265-version.h" "%XDE265_DIR%\libde265"

cmake.exe -G"NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DDAV1D_LIBRARY="%XDAV1D_DIST%\lib\libdav1d.a" -DDAV1D_INCLUDE_DIR="%XDAV1D_DIST%\include" -DLIBDE265_LIBRARY="%XDE265_DIR%\bin_%XARCH%\lib\libde265.lib" -DLIBDE265_INCLUDE_DIR="%XDE265_DIR%" "%XLIB_DIR%\libheif"
IF ERRORLEVEL 1 exit /b 1
nmake.exe
IF ERRORLEVEL 1 exit /b 1

popd

copy /y "%XBUILD_DIR%\libheif\heif.dll" "%XSRC_DIR%\bin%~2\"
IF ERRORLEVEL 1 exit /b 1
copy /y "%XBUILD_DIR%\libheif\heif.lib" "%XSRC_DIR%\lib%~2\"
IF ERRORLEVEL 1 exit /b 1


exit /b 0
