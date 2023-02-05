@echo off

if -%1-==-- echo Argument must be target release version e.g. 1.0.34 & exit /b

set source32=JPEGView\x86\Release
set source64=JPEGView\x64\Release
set sourceConfig=JPEGView\Config

set target=..\JPEGViewReleases\JPEGView_%~1
set target32=..\JPEGViewReleases\JPEGView_%~1\JPEGView32
set target64=..\JPEGViewReleases\JPEGView_%~1\JPEGView64

echo ------------------------------------------------------
echo Creating release %1 in folder %target%
echo ------------------------------------------------------

mkdir %target32%
mkdir %target64%

xcopy HowToInstall.txt %target%
xcopy HowToInstall_ru.txt %target%

xcopy %source32%\JPEGView.exe %target32%
xcopy %source32%\webp.dll %target32%
xcopy %source32%\WICLoader.dll %target32%
xcopy %sourceConfig%\* %target32%

xcopy %source64%\JPEGView.exe %target64%
xcopy %source64%\webp.dll %target64%
xcopy %source64%\WICLoader.dll %target64%
xcopy %sourceConfig%\* %target64%

echo Done