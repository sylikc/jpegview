@echo off

setlocal
REM this builds libjxl and replaces the libs in the JPEGView src folder

REM for me it takes like 15min to build
echo NOTE: this takes a LONG time to build, don't be alarmed...
echo       if it looks like the build process hung... it didn't, it's just WAY SLOW!

SET XSRC_DIR=%~dp0..\..\src
SET XLIB_DIR=%~dp0..\libjxl
SET XOUT_DIR=%~dp0libjxl

IF EXIST "%XOUT_DIR%" (
	echo libjxl output exists, please delete folder before trying to build
	exit /b 1
)

call :BUILD_JXL x86 Win32
IF ERRORLEVEL 1 exit /b 1
call :BUILD_JXL x64 x64
IF ERRORLEVEL 1 exit /b 1


REM copy the libs over
copy /y "%XOUT_DIR%\x86\Release\jxl*.dll" "%XSRC_DIR%\JPEGView\libjxl\bin\"
IF ERRORLEVEL 1 exit /b 1
copy /y "%XOUT_DIR%\x86\third_party\brotli\Release\brotli*.dll" "%XSRC_DIR%\JPEGView\libjxl\bin\"
IF ERRORLEVEL 1 exit /b 1
copy /y "%XOUT_DIR%\x86\lib\Release\jxl*.lib" "%XSRC_DIR%\JPEGView\libjxl\lib\"
IF ERRORLEVEL 1 exit /b 1

copy /y "%XOUT_DIR%\x64\Release\jxl*.dll" "%XSRC_DIR%\JPEGView\libjxl\bin64\"
IF ERRORLEVEL 1 exit /b 1
copy /y "%XOUT_DIR%\x64\third_party\brotli\Release\brotli*.dll" "%XSRC_DIR%\JPEGView\libjxl\bin64\"
IF ERRORLEVEL 1 exit /b 1
copy /y "%XOUT_DIR%\x64\lib\Release\jxl*.lib" "%XSRC_DIR%\JPEGView\libjxl\lib64\"
IF ERRORLEVEL 1 exit /b 1


echo === HEADER FILES NOT MAINTAINED BY SCRIPT ===
echo NOTE: as for the header files, copy/replace files AS NEEDED
echo TO: src\JPEGView\libjxl\include\jxl
echo FROM: extras\libjxl\lib\include\jxl
echo FROM: %OUT_DIR%\[arch]\lib\include\jxl

exit /b 0





:BUILD_JXL

REM so the environments don't pollute each other
setlocal

SET XBUILD_DIR=%XOUT_DIR%\%1

mkdir "%XBUILD_DIR%" 2>nul

call "%~dp0vs-init.bat" %1

pushd "%XBUILD_DIR%"
cmake.exe -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -A %2 "%XLIB_DIR%"
msbuild.exe /p:Platform=%2 /p:configuration="Release" LIBJXL.sln /t:jxl_dec /t:jxl_threads
SET XERROR=%ERRORLEVEL%

popd


exit /b %XERROR%
