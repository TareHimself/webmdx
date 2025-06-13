cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
include(FetchContent)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)
# include(ExternalProject)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

project(webmd LANGUAGES C CXX VERSION 1.0.0 DESCRIPTION "C++ library for decoding of webm files")

option(WEBMD_BUILD_TEST_MAIN "Build a test main file for debugging" OFF)

file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_LIST_DIR}/lib/*.cpp")

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

find_package(webm REQUIRED)
find_package(vpx REQUIRED)
target_include_directories(${PROJECT_NAME} PRIVATE ${webm_INCLUDE_DIRS} ${vpx_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${webm_LIBRARIES} ${vpx_LIBRARIES})

if(WEBMD_BUILD_TEST_MAIN)
    add_executable(test ${CMAKE_CURRENT_LIST_DIR}/test/main.cpp)
    target_link_libraries(test ${PROJECT_NAME})
    target_include_directories(test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
endif()

install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION include/
)

install(
        TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}-targets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
)

install(
        EXPORT ${PROJECT_NAME}-targets
        FILE ${PROJECT_NAME}Config.cmake
        NAMESPACE ${PROJECT_NAME}::
        DESTINATION lib/cmake/${PROJECT_NAME}
)


