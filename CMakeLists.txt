cmake_minimum_required(VERSION 3.14)
project(EquipmentTracker VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
  add_compile_options(/W4 /D_CRT_SECURE_NO_WARNINGS)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(DEFINED ENV{ALPINE})
  add_definitions(-DALPINE_LINUX)
  # Static linking might be preferred for Alpine
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Main library source files
set(SOURCES
    src/position.cpp
    src/equipment.cpp
    src/data_storage.cpp
    src/utils/time_utils.cpp
)

# Create a static library
add_library(equipment_tracker STATIC ${SOURCES})

# Main application
add_executable(equipment_tracker_app apps/tracker/main.cpp)
target_link_libraries(equipment_tracker_app PRIVATE equipment_tracker)

# Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    enable_testing()
    add_subdirectory(tests)
endif()