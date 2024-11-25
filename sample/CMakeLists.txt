cmake_minimum_required(VERSION 2.8)

option(BUILD_SAMPLE_V1 "Enable samle v1 build " ON)

option(BUILD_SAMPLE_V2 "Enable samle v2 build " ON)

if (DEFINED BUILD_SAMPLES AND NOT BUILD_SAMPLES)
    set(BUILD_SAMPLE_V1 OFF)
endif()

if (DEFINED BUILD_CPLUSPLUS_SAMPLE AND NOT BUILD_CPLUSPLUS_SAMPLE)
    set(BUILD_SAMPLE_V2 OFF)
endif()


set (COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/common)
set (COMMON_INC ${COMMON_DIR})
set (COMMON_SOURCES
    ${COMMON_DIR}/MatViewer.cpp
    ${COMMON_DIR}/TYThread.cpp
    ${COMMON_DIR}/crc32.cpp
    ${COMMON_DIR}/json11.cpp
    ${COMMON_DIR}/ParametersParse.cpp
    ${COMMON_DIR}/huffman.cpp
    ${COMMON_DIR}/ImageSpeckleFilter.cpp
    ${COMMON_DIR}/DepthInpainter.cpp)

if (MSVC)#for windows
    set (LIB_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../lib/win/hostapp/)
endif()
set (CLOUD_VIEW_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/cloud_viewer/")
file (GLOB CLOUD_VIEW_SRC ${CLOUD_VIEW_DIR}/*.c ${CLOUD_VIEW_DIR}/*.cpp )
set (CLOUD_VIEW_INC ${CLOUD_VIEW_DIR}/freeglut/include)

if (BUILD_SAMPLE_V1)
    message(STATUS "sample v1 ON ")
    add_subdirectory(sample_v1)
endif()

if (BUILD_SAMPLE_V2)
    message(STATUS "sample v2 ON ")
    add_subdirectory(sample_v2)
endif()
