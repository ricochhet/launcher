@echo off
setlocal
clang-format -i ./src/*.cpp
clang-format -i ./src/*.hpp
endlocal
