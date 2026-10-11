@echo off
setlocal
cd src
windres main.rc -O coff -o main_res.o
g++ -std=c++17 -Os -s -ffunction-sections -fdata-sections -Wl,--gc-sections -Wl,--build-id=none main.cpp terminate.cpp main_res.o -o launcher.exe -static -mwindows
.\upx.exe --best --ultra-brute launcher.exe
endlocal
