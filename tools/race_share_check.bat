@echo off
setlocal
REM Compiles tools\race_share_check.cpp (include\RaceShare.h: the share of carry weight and the starting attributes this
REM mod owns - additive over the race, or our base values - 1.1.10) with MSVC and runs it.
call "%~dp0..\find-msvc.bat"
if errorlevel 1 exit /b 1
set OUT=%TEMP%\cpc-race-share-check-build
if not exist "%OUT%" mkdir "%OUT%"
cl /nologo /std:c++20 /EHsc /O2 /W4 /Fo"%OUT%\\" /Fe"%OUT%\race_share_check.exe" "%~dp0race_share_check.cpp" >"%OUT%\cl.log" || (type "%OUT%\cl.log" & exit /b 1)
"%OUT%\race_share_check.exe" %*
