cmake_minimum_required(VERSION 2.8)


option(BUILD_SAMPLE_V2_WITH_OPENCV "Enable opencv library dependencies " ON)


if (MSVC)
    if (BUILD_SAMPLE_V2_WITH_OPENCV)
        option(OpenCV_STATIC "Using static opencv" OFF)
    endif()
endif()

if (NOT TARGET tycam) 
    #only build samples )
    option (OpenCV_STATIC OFF)
    set(INCLUDE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    include_directories(${INCLUDE_PATH})
    set(ABSOLUTE_TYCAM_LIB tycam)
    add_library(${ABSOLUTE_TYCAM_LIB} SHARED IMPORTED)
    if (MSVC)#for windows
        if(CMAKE_CL_64) #x64
            set_property(TARGET ${ABSOLUTE_TYCAM_LIB} PROPERTY IMPORTED_LOCATION ${LIB_ROOT_PATH}/x64/tycam.dll)
            set_property(TARGET ${ABSOLUTE_TYCAM_LIB} PROPERTY IMPORTED_IMPLIB  ${LIB_ROOT_PATH}/x64/tycam.lib)
        else()
            set_property(TARGET ${ABSOLUTE_TYCAM_LIB} PROPERTY IMPORTED_LOCATION ${LIB_ROOT_PATH}/x86/tycam.dll)
            set_property(TARGET ${ABSOLUTE_TYCAM_LIB} PROPERTY IMPORTED_IMPLIB ${LIB_ROOT_PATH}/x86/tycam.lib)
        endif()
    else()
      if(ARCH)
          set_property(TARGET ${ABSOLUTE_TYCAM_LIB} PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../lib/linux/lib_${ARCH}/libtycam.so)
      else()
          set(ABSOLUTE_TYCAM_LIB -ltycam)
      endif()
    endif()
endif()


if (MSVC)
    if (BUILD_SAMPLE_V2_WITH_OPENCV)
        if (OpenCV_STATIC)
            message("using static opencv")
            set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
        endif()
        add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    endif()
endif()


# ========================================
# === common, build static lib to speed up
# ========================================
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/hpp/)
include_directories(${COMMON_INC}/)

set(CPLUSPLUS_SAMPLE_API_SOURCE 
    cpp/Device.cpp
    cpp/Frame.cpp
    )

if (BUILD_SAMPLE_V2_WITH_OPENCV)
    find_package(OpenCV)
    if (NOT OpenCV_FOUND)
        message(WARNING "OpenCV library not found")
        set(BUILD_SAMPLE_V2_WITH_OPENCV OFF)
    else()
        include_directories(${OpenCV_INCLUDE_DIRS})
        include_directories(${OpenCV2_INCLUDE_DIRS})
        # if(CMAKE_BUILD_TYPE STREQUAL Debug)
        # link_directories(${OpenCV_LIB_DIR_DBG})
        # else()
        # link_directories(${OpenCV_LIB_DIR_OPT})
        # endif()
        link_directories(${OpenCV_LIB_DIR})
    endif()
endif()

if (BUILD_SAMPLE_V2_WITH_OPENCV)
    add_definitions(-DOPENCV_DEPENDENCIES)
endif()

add_library(cpp_api_lib STATIC ${COMMON_SOURCES} ${CPLUSPLUS_SAMPLE_API_SOURCE})
if (DEFINED TARGET_LIB_API)
  add_dependencies(cpp_api_lib ${TARGET_LIB_API})
endif()


if(UNIX)
    target_link_libraries(cpp_api_lib pthread)
endif()


add_subdirectory( sample )
