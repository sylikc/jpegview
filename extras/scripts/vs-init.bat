@echo off

REM this is a routine that would detect and initialize visual studio environment.
REM It's not going to be tested very well for new versions because I only have VS2017 and VS2019 Community installed

REM if a special variable comes in, it'll use that version XVS_INIT_VER=

IF /I "%~1" EQU "" (
	echo ERROR: Pass in an [arch] to be passed to a vcvarsall.bat
	exit /b 1
)

SET XVS_BASE_COMMON=%ProgramFiles(x86)%\Microsoft Visual Studio

IF /I "%XVS_INIT_VER%" EQU "" GOTO SKIP_VER
	REM need to do it this way so that () chars don't cause a syntax error
	SET XVS_BASE_COMMON=%XVS_BASE_COMMON%\%XVS_INIT_VER%
:SKIP_VER

IF NOT EXIST "%XVS_BASE_COMMON%" (
	echo Visual Studio not installed or installed in a non-standard location!
	exit /b 1
)

SET XVS_VCVARS_BAT=

REM because the directories in theory should come out in order
REM it'll automatically pick up the LAST vcvarsall.bat (in theory)
FOR /F "usebackq tokens=*" %%I IN (`dir /b /on /s "%XVS_BASE_COMMON%\vcvarsall.bat"`) DO (
	echo + Found VCVARSALL.BAT at: %%I
	SET XVS_VCVARS_BAT=%%I
)


echo == Using: %XVS_VCVARS_BAT% ==
call "%XVS_VCVARS_BAT%" %~1



SET XVS_BASE_COMMON=
SET XVS_VCVARS_BAT=


exit /b 0
