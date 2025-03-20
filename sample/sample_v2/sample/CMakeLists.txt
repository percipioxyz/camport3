
set(ALL_CPP_API_SAMPLES
    ListDevices
    ForceDeviceIP
    DepthStream
    TofDepthStream
    SoftTrigger
    ExposureTimeSetting
    ResolutionSetting
    OfflineReconnection
    MultiDeviceOfflineReconnection
    OpenWithIP
    OpenWithInterface
    NetStatistic
    SaveLoadConfig
    GetCalibData
    PointCloud
    StreamAsync
    )

set(SAMPLES_DEPENDS_OPENCV
    Registration
    IREnhance
    )


if (BUILD_SAMPLE_V2_WITH_OPENCV)
    set(ALL_CPP_API_SAMPLES ${ALL_CPP_API_SAMPLES} ${SAMPLES_DEPENDS_OPENCV})
endif()

if (MSVC)
    option(OpenCV_STATIC "Using static opencv" OFF)
endif()

if (NOT TARGET tycam) 
    #only build samples 
    option (OpenCV_STATIC OFF)
    set(INCLUDE_PATH ../include)
    include_directories(${INCLUDE_PATH})
    set(ABSOLUTE_TYCAM_LIB tycam)
    add_library(${ABSOLUTE_TYCAM_LIB} SHARED IMPORTED)
    if (MSVC)#for windows
        set (LIB_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../lib/win/hostapp/)
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
        set (LIB_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../lib/win/hostapp/)
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

foreach(sample ${ALL_CPP_API_SAMPLES})
    set(sample_exec ${sample}_v2)
    get_filename_component(spath "${sample}" ABSOLUTE )
    if (EXISTS "${spath}/")
        file(GLOB sources ${sample}/*.cpp)
        
        add_executable(${sample_exec} ${sources})
        add_dependencies(${sample_exec} cpp_api_lib ${TARGET_LIB})
        target_link_libraries(${sample_exec} cpp_api_lib ${ABSOLUTE_TYCAM_LIB})
        #Some compiler versions require linking libusb
        if(UNIX)
            target_link_libraries(${sample_exec} usb-1.0)
        endif()
        if(BUILD_IMGPROC)
            target_link_libraries(${sample_exec} ${ABSOLUTE_TYIMGPROC_LIB})
        endif()
        if (BUILD_SAMPLE_V2_WITH_OPENCV)
            add_definitions(-DOPENCV_DEPENDENCIES)
            target_link_libraries(${sample_exec} ${OpenCV_LIBS})
        endif()
        set_target_properties(${sample_exec} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
        set_target_properties(${sample_exec} PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON )
        install(TARGETS ${sample_exec} RUNTIME DESTINATION samples/)
    endif()
endforeach()
