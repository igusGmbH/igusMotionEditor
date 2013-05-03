@echo off

set IMAGE=IgusMotionEditor

rd /Q /S %IMAGE%
mkdir %IMAGE%

copy CP210x_VCP_Win_XP_S2K3_Vista_7.exe %IMAGE%\
copy NanoJMotorControl.java %IMAGE%\
copy release\IgusMotionEditor.exe %IMAGE%\
xcopy /Q /I calibs %IMAGE%\calibs
xcopy /Q /I images %IMAGE%\images
xcopy /Q /I motions %IMAGE%\motions
copy styles.css %IMAGE%\

xcopy /Q /I microcontroller\avrdude %IMAGE%\microcontroller
copy microcontroller\microcontroller.hex %IMAGE%\microcontroller\
copy microcontroller\bootloader.hex %IMAGE%\microcontroller\

copy flashtool\release\flashtool.exe %IMAGE%\

REM Qt/System libraries

set QT=C:\QtSDK\Desktop\Qt\4.8.0\mingw\bin
set MINGW=C:\QtSDK\mingw\bin
set QGLVIEWER=C:\workspace\libQGLViewer\QGLViewer\release

for %%i in (QtGui4.dll, QtCore4.dll, QtOpenGL4.dll, QtXml4.dll) do xcopy /Q %QT%\%%i %IMAGE%\

copy %MINGW%\mingwm10.dll %IMAGE%\
copy %MINGW%\libgcc_s_dw2-1.dll %IMAGE%\
copy %QGLVIEWER%\QGLViewer2.dll %IMAGE%\

mkdir %IMAGE%\doc
echo "Please save the documentation as doc/MotionEditorHandbook.pdf"
"C:\Programme\OpenOffice.org 3\program\swriter.exe" doc/MotionEditorHandbook.doc

pause

set WCREV=unknown

C:\Programme\TortoiseSVN\bin\SubWcRev.exe . svn_tpl.bat %TEMP%\svn.bat
call %TEMP%\svn.bat

echo %WCREV%

del /Q IgusMotionEditor_%WCREV%.zip
contrib\7z a -r IgusMotionEditor_%WCREV%.zip IgusMotionEditor

pause
