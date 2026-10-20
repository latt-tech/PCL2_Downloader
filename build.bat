@echo off
chcp 65001
setlocal EnableExtensions
cd /d "%~dp0"

set "CC=clang++"
set "RES=windres"

echo [1/2] Compile resources (icon + bgm)...
"%RES%" resource.rc -O coff -o resource.o
if errorlevel 1 exit /b 1

echo [2/2] Compile installer (static linkage)...
"%CC%" -std=c++17 -O2 -s -mwindows -static ^
    -finput-charset=UTF-8 -fexec-charset=UTF-8 ^
    installer.cpp resource.o ^
    -o PCL2Downloader.exe ^
    -luser32 -lgdi32 -lshell32 -lole32 -lcomctl32 -lwininet -lwinmm -ladvapi32 -luuid
if errorlevel 1 exit /b 1

echo.
echo [OK] Build finished: PCL2Downloader.exe
endlocal