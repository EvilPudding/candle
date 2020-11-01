@ECHO OFF
SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

CD /D %~dp0

CALL vcenv.bat

SET SAUCES=""
:PARSEPARAMS
	IF "%~1"=="" GOTO START
	IF "%~1"=="/SAUCES" (
		SET SAUCES=%~2
		SHIFT
	)
	SHIFT
GOTO :PARSEPARAMS
:START

set DIR=build
set GLFW=third_party\glfw\src
set TINYCTHREAD=third_party\tinycthread\source
set THIRD_PARTY_SRC= %GLFW%\context.c^
                     %GLFW%\init.c^
                     %GLFW%\input.c^
                     %GLFW%\monitor.c^
                     %GLFW%\vulkan.c^
                     %GLFW%\window.c^
                     %GLFW%\win32_init.c^
                     %GLFW%\win32_joystick.c^
                     %GLFW%\win32_monitor.c^
                     %GLFW%\win32_time.c^
                     %GLFW%\win32_thread.c^
                     %GLFW%\win32_window.c^
                     %GLFW%\wgl_context.c^
                     %GLFW%\egl_context.c^
                     %GLFW%\osmesa_context.c^
                     %TINYCTHREAD%\tinycthread.c^
                     third_party\miniz.c

:: set CFLAGS=/Z7 /I. /W1 /D_GLFW_WIN32 /Ithird_party\glfw\include /I%TINYCTHREAD% /DTHREADED^
	:: /D_CRT_SECURE_NO_WARNINGS
set CFLAGS=/O2 /I. /W1 /D_GLFW_WIN32 /Ithird_party\glfw\include /I%TINYCTHREAD% /DTHREADED^
	/D_CRT_SECURE_NO_WARNINGS

set sources=%THIRD_PARTY_SRC% candle.c

set subdirs=components systems formats utils vil ecs

FOR %%a IN (%subdirs%) DO @IF EXIST "%%a" (
	FOR %%f IN (%%a\*.c) DO @IF EXIST "%%f" set sources=!sources! %%f
)

mkdir %DIR%\components
mkdir %DIR%\systems
mkdir %DIR%\utils
mkdir %DIR%\formats
mkdir %DIR%\vil
mkdir %DIR%\ecs
mkdir %DIR%\%GLFW%
mkdir %DIR%\%TINYCTHREAD%

IF NOT EXIST %DIR%\datescomp.exe (
	cl buildtools\datescomp.c /Fe%DIR%\datescomp.exe /Fo%DIR%\datescomp.obj /O2
)

set objects=
FOR %%f IN (!sources!) DO @IF EXIST "%%f" (
	set src=%DIR%\%%f
	CALL set object=%%src:.c=.obj%%
	%DIR%\datescomp.exe %%f !object! || (
		cl /c "%%f" /Fo"!object!" %CFLAGS% || (
			echo Error compiling %%f
			GOTO END
		)
	)
	CALL set objects=!objects! !object!
)

%DIR%\datescomp.exe buildtools\packager.c %DIR%\packager.exe || (
	cl buildtools\packager.c %DIR%\third_party\miniz.obj /Fo%DIR%\packager.obj /Fe%DIR%\packager.exe /O2
)

set LIBS=gdi32.lib opengl32.lib kernel32.lib user32.lib shell32.lib candle\%DIR%\candle.lib candle\%DIR%\res.res

set RES=
FOR /d %%f IN (..\*.candle) DO (
	call %%f\build.bat
	set /p PLUGIN_LIBS=<%%f\build\libs
	set LIBS=!LIBS! !PLUGIN_LIBS!
	set /p PLUGIN_RES=<%%f\build\res
	for %%S IN (!PLUGIN_RES!) DO (
		set RES=!RES! %%f\%%~S
	)
)

CALL %DIR%\packager.exe ..\!SAUCES! !RES!

mkdir %DIR%
echo !LIBS! > %DIR%\libs

ECHO 1 RCDATA "%DIR%\data.zip" > %DIR%\res.rc
rc %DIR%\res.rc

lib !objects! /out:"%DIR%\candle.lib"
:END
