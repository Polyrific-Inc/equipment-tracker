# Fetch and build Google Test if not found
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# For Alpine Linux: Build with position-independent code
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Make GoogleTest available
FetchContent_MakeAvailable(googletest)

# Include project headers
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Add the test executable
add_executable(position_test ${CMAKE_CURRENT_SOURCE_DIR}/src/position_test.cpp)

# Link the test executable with GoogleTest and the main library
target_link_libraries(position_test 
    PRIVATE 
    equipment_tracker  # Link to your main library
    gtest
    gtest_main  # Provides a default main() function
    gmock
)

# Add pthread on Unix systems
if(UNIX AND NOT APPLE)
    target_link_libraries(position_test PRIVATE pthread)
    
    # For Alpine Linux: Link with libstdc++ statically if needed
    if(DEFINED ENV{ALPINE})
        target_link_libraries(position_test PRIVATE -static-libstdc++)
    endif()
endif()

# Add test so CTest can run it
add_test(NAME position_test COMMAND position_test)

# Enable test discovery
include(GoogleTest)
gtest_discover_tests(position_test)