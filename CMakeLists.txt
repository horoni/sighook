cmake_minimum_required(VERSION 3.15)

project(sighook LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

file(GLOB_RECURSE LIB_SRCS "src/*.c" "src/*.s")

add_library(sighook STATIC ${LIB_SRCS})

target_include_directories(sighook PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_options(sighook PRIVATE -Wall -Wextra -Wpedantic)
set_property(TARGET sighook PROPERTY POSITION_INDEPENDENT_CODE ON)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    enable_testing()

    file(GLOB TEST_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.c")

    foreach(test_source ${TEST_SRCS})
        get_filename_component(test_name ${test_source} NAME_WE)
        add_executable(${test_name} ${test_source})
        target_link_libraries(${test_name} PRIVATE sighook)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()