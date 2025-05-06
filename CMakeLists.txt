cmake_minimum_required(VERSION 3.14)
project(EquipmentTracker VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Main library source files
set(SOURCES
    src/position.cpp
    src/equipment.cpp
    src/utils/time_utils.cpp
)

# Create a static library
add_library(equipment_tracker STATIC ${SOURCES})

# Main application
add_executable(equipment_tracker_app apps/tracker/main.cpp)
target_link_libraries(equipment_tracker_app PRIVATE equipment_tracker)