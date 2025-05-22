# Include the GoogleTest module for test discovery
include(GoogleTest)

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

# Option to build individual test executables (default) or a combined test suite
option(BUILD_INDIVIDUAL_TESTS "Build individual test executables for each test file" ON)

# Find all test files in the src directory
file(GLOB_RECURSE TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*_test.cpp")

# Common function to set up test executable
function(setup_test_executable target_name source_file)
    add_executable(${target_name} ${source_file})
    target_link_libraries(${target_name} 
        PRIVATE 
        equipment_tracker  # Link to your main library
        gtest
        gtest_main
        gmock
    )
    
    # Add pthread on Unix systems
    if(UNIX AND NOT APPLE)
        target_link_libraries(${target_name} PRIVATE pthread)
        
        # For Alpine Linux: Link with libstdc++ statically if needed
        if(DEFINED ENV{ALPINE})
            target_link_libraries(${target_name} PRIVATE -static-libstdc++)
        endif()
    endif()
    
    # Add test so CTest can run it
    add_test(NAME ${target_name} COMMAND ${target_name})
    
    # Enable test discovery
    gtest_discover_tests(${target_name})
endfunction()

if(BUILD_INDIVIDUAL_TESTS)
    # Build separate executable for each test file
    foreach(TEST_SOURCE ${TEST_SOURCES})
        # Extract the filename without extension
        get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
        setup_test_executable(${TEST_NAME} ${TEST_SOURCE})
    endforeach()
else()
    # Build a single executable with all tests
    setup_test_executable(all_tests "${TEST_SOURCES}")
endif()