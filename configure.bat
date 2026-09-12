@echo off
call "D:\VSBuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set PATH=D:\Qt\6.11.2\msvc2022_64\bin;D:\Qt\Tools\Ninja;%PATH%
set CMAKE_PREFIX_PATH=D:\Qt\6.11.2\msvc2022_64
cd /d D:\C++\Breeze
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=D:\Qt\6.11.2\msvc2022_64
