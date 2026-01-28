cmake -S . -B build -G Ninja `
  -DCMAKE_MAKE_PROGRAM="C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" `
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build