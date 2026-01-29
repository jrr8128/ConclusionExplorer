@echo off
setlocal
call "%ProgramFiles(x86)%\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
build\app.exe
