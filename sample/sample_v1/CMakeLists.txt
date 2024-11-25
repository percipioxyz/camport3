cmake_minimum_required(VERSION 2.8)

set(ALL_SAMPLES
    DumpAllFeatures
    DumpCalibInfo
    ListDevices
    LoopDetect
    ForceDeviceIP
    NetStatistic
    DeviceStorage
    SimpleView_FetchFrame
    SimpleView_Callback
    SimpleView_Registration
    SimpleView_PixelsRegistration
    SimpleView_FetchHisto
    SimpleView_MultiDevice
    SimpleView_TriggerMode0
    SimpleView_TriggerMode1
    SimpleView_TriggerMode20
    SimpleView_TriggerMode21
    SimpleView_TriggerMode28
    SimpleView_TriggerMode29
    SimpleView_TriggerMode30
    SimpleView_TriggerMode31
    SimpleView_TriggerMode_M2S1
    SimpleView_TriggerMode_M3S1
    SimpleView_KeepAlive
    SimpleView_TriggerDelay
    SimpleView_StreamAsync
    SimpleView_StreamAsync1
    SimpleView_ImuData
    SimpleView_DI_DO
    SimpleView_OpenWithIP
    SimpleView_HDR
    SimpleView_SGBM
    SimpleView_XYZ48
    SimpleView_SaveLoadConfig
    )

set(SAMPLES_USED_GLUT
    SimpleView_Point3D
    SimpleView_Point3D_XYZ48
    )

if (MSVC)
    option(OpenCV_STATIC "Using static opencv" OFF)
endif()

option(OPENGL_VIEW "Enable GLUT OpenGL point cloud view " OFF)
if (NOT TARGET tycam) 
    #only build samples 
    option (OpenCV_STATIC OFF)
    set(INCLUDE_PATH ../../include)
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

if (NOT TARGET tyimgproc)
    option (OpenCV_STATIC OFF)
    set(ABSOLUTE_TYIMGPROC_LIB tyimgproc)
    add_library(${ABSOLUTE_TYIMGPROC_LIB} SHARED IMPORTED)
    if (MSVC)#for windows
        if(CMAKE_CL_64) #x64
            set_property(TARGET ${ABSOLUTE_TYIMGPROC_LIB} PROPERTY IMPORTED_LOCATION ${LIB_ROOT_PATH}/x64/tyimgproc.dll)
            set_property(TARGET ${ABSOLUTE_TYIMGPROC_LIB} PROPERTY IMPORTED_IMPLIB  ${LIB_ROOT_PATH}/x64/tyimgproc.lib)
        else()
            set_property(TARGET ${ABSOLUTE_TYIMGPROC_LIB} PROPERTY IMPORTED_LOCATION ${LIB_ROOT_PATH}/x86/tyimgproc.dll)
            set_property(TARGET ${ABSOLUTE_TYIMGPROC_LIB} PROPERTY IMPORTED_IMPLIB ${LIB_ROOT_PATH}/x86/tyimgproc.lib)
        endif()
    else()
        if(ARCH)
            set_property(TARGET ${ABSOLUTE_TYIMGPROC_LIB} PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../lib/linux/lib_${ARCH}/tyimgproc.so)
        else()
            set(ABSOLUTE_TYIMGPROC_LIB -ltyimgproc)
        endif()
    endif()
endif()

if (MSVC)
    if (OpenCV_STATIC)
        message("using static opencv")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    endif()
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

# ========================================
# === common, build static lib to speed up
# ========================================
include_directories(${COMMON_INC}/)
add_library(sample_common STATIC ${COMMON_SOURCES})
if(UNIX)
  target_link_libraries(sample_common pthread)
endif()

# ========================================
# === OpenCV
# ========================================
find_package(OpenCV REQUIRED)
if (NOT OpenCV_FOUND)
    message(FATAL_ERROR "OpenCV library not found")
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

add_definitions(-DOPENCV_DEPENDENCIES)
# ========================================
# === OpenGL
# ========================================
if (OPENGL_VIEW)
    include_directories(${CLOUD_VIEW_INC})
    add_library(cloud_viewer STATIC ${CLOUD_VIEW_SRC})
    target_compile_definitions(cloud_viewer PUBLIC FREEGLUT_STATIC)

    if (WIN32)
        #build freeglut by ourselves
        include(ExternalProject)
        ExternalProject_Add( glut_proj
            PREFIX "${CMAKE_CURRENT_SOURCE_DIR}"
            SOURCE_DIR "${CLOUD_VIEW_DIR}/freeglut"
            BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/freeglut_build"
            CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/freeglut_install
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
            -DCMAKE_STATIC_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}
            -DCMAKE_STATIC_LINKER_FLAGS_DEBUG=${CMAKE_EXE_LINKER_FLAGS_DEBUG}
            -DCMAKE_STATIC_LINKER_FLAGS_RELEASE=${CMAKE_EXE_LINKER_FLAGS_RELEASE}
            -DCMAKE_CXX_FLAGS_RELEASE=${CMAKE_CXX_FLAGS_RELEASE}
            -DCMAKE_CXX_FLAGS_DEBUG=${CMAKE_CXX_FLAGS_DEBUG}
            -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
            -DCMAKE_C_FLAGS_DEBUG=${CMAKE_C_FLAGS_DEBUG}
            -DCMAKE_C_FLAGS_RELEASE=${CMAKE_C_FLAGS_RELEASE}
            )

        add_library(myglut STATIC IMPORTED)
        add_dependencies(myglut glut_proj)
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/freeglut_install/include)
        set_target_properties(myglut PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_BINARY_DIR}/freeglut_install/include)
        set( glut_lib_file ${CMAKE_CURRENT_BINARY_DIR}/freeglut_install/lib/freeglut_staticd.lib)
        set_property(TARGET myglut PROPERTY IMPORTED_LOCATION_DEBUG ${glut_lib_file} )
        set( glut_lib_file ${CMAKE_CURRENT_BINARY_DIR}/freeglut_install/lib/freeglut_static.lib)
        set_property(TARGET myglut PROPERTY IMPORTED_LOCATION ${glut_lib_file} )

        add_dependencies(cloud_viewer glut_proj)
        target_link_libraries(cloud_viewer PUBLIC myglut)
    else()
        find_package(GLUT REQUIRED)
        find_package(OpenGL REQUIRED)
        include_directories(${GLUT_INCLUDE_DIR} ${OPENGL_INCLUDE_DIRS})
        target_link_libraries(cloud_viewer PUBLIC ${GLUT_LIBRARIES} ${OPENGL_LIBRARIES})
    endif()
    include_directories(cloud_viewer)
    set(ALL_SAMPLES ${ALL_SAMPLES} ${SAMPLES_USED_GLUT})
    set(CLOUD_VIEWER  ${GLUT_LIBRARIES} cloud_viewer)

endif()

#install for sample header file

foreach(sample ${ALL_SAMPLES})
    get_filename_component(spath "${sample}" ABSOLUTE )
    if (EXISTS "${spath}/")
        file(GLOB sources ${sample}/*.cpp)
        add_executable(${sample} ${sources})
        add_dependencies(${sample} sample_common ${TARGET_LIB})
        target_link_libraries(${sample} sample_common ${ABSOLUTE_TYCAM_LIB}  ${OpenCV_LIBS} ${CLOUD_VIEWER})
        if(BUILD_IMGPROC)
            target_link_libraries(${sample} ${ABSOLUTE_TYIMGPROC_LIB})
        endif()
        set_target_properties(${sample} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)
        set_target_properties(${sample} PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON )
        install(TARGETS ${sample} RUNTIME DESTINATION samples/)
    endif()
endforeach()

