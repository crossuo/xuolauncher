set exePath="%cd%\release\xuolauncher.exe"
set targetPath="%cd%\release"
C:\Danny\Qt\5.11.1\msvc2017_64\bin\windeployqt.exe -dir %targetPath% %exePath%
pause