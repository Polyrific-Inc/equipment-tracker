cmake_minimum_required(VERSION 3.14)
project(EquipmentTracker VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Disable compiler-specific extensions

# Build type and optimization
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(WIN32)
  add_compile_options(/W4 /D_CRT_SECURE_NO_WARNINGS)
  # Add threading support for Windows
  if(MSVC)
      add_compile_options(/MP)  # Multi-processor compilation
  endif()
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
  # Add debug info in debug builds
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      add_compile_options(-g -O0)
  else()
      add_compile_options(-O3 -DNDEBUG)
  endif()
endif()

if(DEFINED ENV{ALPINE})
  add_definitions(-DALPINE_LINUX)
  # Static linking might be preferred for Alpine
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Find required packages
find_package(Threads REQUIRED)

# Main library source files
set(LIBRARY_SOURCES
    src/position.cpp
    src/equipment.cpp
    src/utils/time_utils.cpp
)

# Check if source files exist and warn if missing
foreach(SOURCE_FILE ${LIBRARY_SOURCES})
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE})
        message(WARNING "Source file ${SOURCE_FILE} not found - you may need to create it")
    endif()
endforeach()

# Create the main library with modern CMake practices
add_library(equipment_tracker STATIC ${LIBRARY_SOURCES})

# Set target properties
set_target_properties(equipment_tracker PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Target-specific include directories (modern CMake approach)
target_include_directories(equipment_tracker
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link required libraries
target_link_libraries(equipment_tracker
    PUBLIC
        Threads::Threads  # For std::mutex and threading support
)

# Compiler-specific definitions
target_compile_definitions(equipment_tracker
    PRIVATE
        $<$<CONFIG:Debug>:DEBUG>
        $<$<CONFIG:Release>:NDEBUG>
)

# Main application executable
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/apps/tracker/main.cpp)
    add_executable(equipment_tracker_app apps/tracker/main.cpp)
    
    # Link the library
    target_link_libraries(equipment_tracker_app 
        PRIVATE 
            equipment_tracker
    )
    
    # Set executable properties
    set_target_properties(equipment_tracker_app PROPERTIES
        OUTPUT_NAME "equipment_tracker"
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
else()
    # Fallback to main.cpp in root if apps/tracker/main.cpp doesn't exist
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
        message(STATUS "Using main.cpp from root directory")
        add_executable(equipment_tracker_app main.cpp)
        target_link_libraries(equipment_tracker_app 
            PRIVATE 
                equipment_tracker
        )
        set_target_properties(equipment_tracker_app PROPERTIES
            OUTPUT_NAME "equipment_tracker"
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
        )
    else()
        message(WARNING "No main.cpp found in apps/tracker/ or root directory")
    endif()
endif()

# Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    enable_testing()
    add_subdirectory(tests)
endif()