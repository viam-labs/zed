cmake_minimum_required(VERSION 3.25 FATAL_ERROR)
set(CMAKE_PROJECT_VERSION 0.0.1)

project(viam-camera-zed
    DESCRIPTION "Viam Module for the Stereolabs ZED Camera"
    HOMEPAGE_URL https://github.com/viam-labs/zed
    LANGUAGES CXX
)

if (APPLE)
    list(PREPEND CMAKE_INSTALL_RPATH "@loader_path/../lib")
else()
    list(PREPEND CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(viam-cpp-sdk REQUIRED)

list(APPEND CMAKE_PREFIX_PATH "/usr/local/zed")
# ZED's zed-config.cmake uses the legacy FindCUDA module which needs this set
# explicitly; modern CUDAToolkit doesn't populate it.
if(NOT DEFINED CUDA_TOOLKIT_ROOT_DIR)
    set(CUDA_TOOLKIT_ROOT_DIR "/usr/local/cuda")
endif()
find_package(CUDAToolkit REQUIRED)
find_package(ZED REQUIRED)
find_library(TURBOJPEG_LIB turbojpeg REQUIRED)

set(ZED_EXTRA_LINK_OPTIONS "")
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    # Jetson-specific NvBufSurface libs live in /usr/lib/aarch64-linux-gnu/tegra
    # and are hardware-locked to real Tegra chips (mounted in via nvidia-runtime
    # on actual Jetsons). When cross-building in a container on generic aarch64
    # hardware, those libs aren't present, but libsl_zed.so references them.
    # Defer resolution to runtime on the target Jetson.
    list(APPEND ZED_EXTRA_LINK_OPTIONS "-Wl,--allow-shlib-undefined")
endif()

add_executable(viam-camera-zed
    src/module/main.cpp
    src/module/zed_camera.cpp
)
target_compile_features(viam-camera-zed PRIVATE cxx_std_17)
target_include_directories(viam-camera-zed PRIVATE ${ZED_INCLUDE_DIRS})
target_link_options(viam-camera-zed PRIVATE ${ZED_EXTRA_LINK_OPTIONS})
target_link_libraries(viam-camera-zed
    PRIVATE
        viam-cpp-sdk::viamsdk
        Threads::Threads
        ${ZED_LIBRARIES}
        CUDA::cudart
        CUDA::cuda_driver
        ${TURBOJPEG_LIB}
        ${CMAKE_DL_LIBS}
)

install(TARGETS viam-camera-zed DESTINATION .)
install(FILES meta.json DESTINATION .)
install(PROGRAMS first_run.sh DESTINATION .)
