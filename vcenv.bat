@ECHO OFF

WHERE cl
IF %ERRORLEVEL% NEQ 0 (
	set vsdir=
	set vcvarsall=""
	FOR /R "C:\Program Files (x86)\Microsoft Visual Studio\" %%f in (vcvarsall.bat) DO @IF EXIST "%%f" (
		CALL "%%f" x86_amd64
		GOTO END
	)
	IF NOT EXIST "!vcvarsall!" (
		ECHO Could not automatically find vcvarsall.bat to setup the build environment.
		ECHO If the Microsoft Build Tools are not installed, get them at:
		ECHO     https://visualstudio.microsoft.com/visual-cpp-build-tools/
		ECHO If the build tools are installed, run vcvarsall.bat before compilation
		GOTO ERROR
	)
)
:END
