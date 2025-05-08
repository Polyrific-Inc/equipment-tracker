@echo off
REM Create build directory
if not exist build mkdir build
cd build

REM Configure with CMake and Clang
cmake .. -G "Ninja" ^
  -DCMAKE_C_COMPILER=clang ^
  -DCMAKE_CXX_COMPILER=clang++ ^
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ^
  -DCMAKE_BUILD_TYPE=Debug

REM Build the project
cmake --build .

REM Check if build succeeded
if %ERRORLEVEL% EQU 0 (
  echo Build successful!
  echo Running the test application:
  .\equipment_tracker_app.exe
) else (
  echo Build failed with error code %ERRORLEVEL%
)

cd ..