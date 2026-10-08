@echo off
setlocal
windres launcher.rc -O coff -o launcher_res.o
g++ -std=c++17 -Os -s launcher.cpp launcher_res.o -o launcher.exe -static -mwindows
.\upx.exe --best launcher.exe
endlocal
