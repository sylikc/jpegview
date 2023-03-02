@echo off

setlocal
REM this builds lcms2 and replaces the dlls/libs in the JPEGView src folder

SET XSRC_DIR=%~dp0..\..\src\JPEGView\lcms2
SET XLIB_DIR=%~dp0..\third_party\Little-CMS
SET XOUT_DIR=%~dp0lcms2

SET XVS_VER=2019
IF /I "%XVS_INIT_VER%" NEQ "" (
	REM override the build version for the solutions provided
	SET XVS_VER=%XVS_INIT_VER%
)

IF EXIST "%XOUT_DIR%" (
	echo lcms2 output exists, please delete folder before trying to build
	exit /b 1
)


call :BUILD_COPY_LCMS x86 Win32 ""
IF ERRORLEVEL 1 exit /b 1

call :BUILD_COPY_LCMS x64 x64 "64"
IF ERRORLEVEL 1 exit /b 1



echo === HEADER FILES NOT MAINTAINED BY SCRIPT ===
echo NOTE: as for the header files, copy/replace files AS NEEDED
echo;
echo TO:   src\JPEGView\lcms2\include
echo FROM: extras\third_party\Little-CMS\include

exit /b 0




REM ===============================================================================================

:BUILD_COPY_LCMS

REM so the environments don't pollute each other
setlocal

call "%~dp0vs-init.bat" %1

pushd "%XLIB_DIR%"

REM delete any previous build files, if exists
del "bin\lcms2.*" 2>nul

msbuild /t:lcms2_DLL /p:Platform=%2 /p:Configuration=Release .\Projects\VC%XVS_VER%\lcms2.sln
IF ERRORLEVEL 1 exit /b 1

copy /y "bin\lcms2.lib" "%XSRC_DIR%\lib%~3"
IF ERRORLEVEL 1 exit /b 1
copy /y "bin\lcms2.dll" "%XSRC_DIR%\bin%~3"
IF ERRORLEVEL 1 exit /b 1

exit /b 0
