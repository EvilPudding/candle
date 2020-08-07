@ECHO OFF
SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

CD /D %~dp0

set DIR=build
set GLFW=third_party\glfw\src
set TINYCTHREAD=third_party\tinycthread\source
set THIRD_PARTY_SRC= %GLFW%\win32_init.c^
                     %GLFW%\win32_joystick.c^
                     %GLFW%\win32_monitor.c^
                     %GLFW%\win32_time.c^
                     %GLFW%\win32_thread.c^
                     %GLFW%\win32_window.c^
                     %GLFW%\wgl_context.c^
                     %GLFW%\egl_context.c^
                     %GLFW%\osmesa_context.c^
                     %TINYCTHREAD%\tinycthread.c

set CFLAGS=/O2 /I. /W1 /D_GLFW_WIN32 /Ithird_party\glfw\include /I%TINYCTHREAD% /DTHREADED^
	/D_CRT_SECURE_NO_WARNINGS

WHERE cl
IF %ERRORLEVEL% NEQ 0 (
	set vsdir=
	set vcvarsall=""
	FOR /R "C:\Program Files (x86)\Microsoft Visual Studio\" %%f in (vcvarsall.bat) DO @IF EXIST "%%f" (
		set vcvarsall=%%f
	)
	IF NOT EXIST "!vcvarsall!" (
		ECHO Could not automatically find vcvarsall.bat to setup the build environment.
		ECHO If the Microsoft Build Tools are not installed, get them at:
		ECHO     https://visualstudio.microsoft.com/visual-cpp-build-tools/
		ECHO If the build tools are installed, run vcvarsall.bat before calling build.bat
		GOTO ERROR
	)
	CALL "!vcvarsall!" x86_amd64
)

set sources=%THIRD_PARTY_SRC% candle.c

set subdirs=components systems formats utils vil ecs

FOR %%a IN (%subdirs%) DO @IF EXIST "%%a" (
	FOR %%f in (%%a\*.c) DO @IF EXIST "%%f" set sources=!sources! %%f
)

mkdir %DIR%\components
mkdir %DIR%\systems
mkdir %DIR%\utils
mkdir %DIR%\formats
mkdir %DIR%\vil
mkdir %DIR%\ecs
mkdir %DIR%\%GLFW%
mkdir %DIR%\%TINYCTHREAD%

set objects=
FOR %%f IN (!sources!) DO @IF EXIST "%%f" (
	set src=%DIR%\%%f
	call set object=%%src:.c=.obj%%
	cl %CFLAGS% /c "%%f" /Fo"!object!" || (
		echo Error compiling %%f
		GOTO ERROR
	)
	call set objects=!objects! !object!
)

ECHO !objects!

lib !objects! /out:"%DIR%\candle.lib"
GOTO END

:ERROR
ECHO ERROR
:END
