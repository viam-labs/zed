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
find_package(ZED REQUIRED)
find_package(CUDAToolkit REQUIRED)

# Jetson (L4T) hosts libnvbufsurface.so under /usr/lib/aarch64-linux-gnu/tegra
# which isn't in the default linker search path.
set(ZED_EXTRA_LIBS "")
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    find_library(NVBUFSURFACE_LIB nvbufsurface
        PATHS /usr/lib/aarch64-linux-gnu/tegra
        REQUIRED
    )
    list(APPEND ZED_EXTRA_LIBS ${NVBUFSURFACE_LIB})
endif()

add_executable(viam-camera-zed
    src/module/main.cpp
    src/module/zed_camera.cpp
)
target_compile_features(viam-camera-zed PRIVATE cxx_std_17)
target_include_directories(viam-camera-zed PRIVATE ${ZED_INCLUDE_DIRS})
target_link_libraries(viam-camera-zed
    PRIVATE
        viam-cpp-sdk::viamsdk
        Threads::Threads
        ${ZED_LIBRARIES}
        CUDA::cudart
        CUDA::cuda_driver
        ${ZED_EXTRA_LIBS}
        ${CMAKE_DL_LIBS}
)

install(TARGETS viam-camera-zed DESTINATION .)
install(FILES meta.json DESTINATION .)
install(PROGRAMS first_run.sh DESTINATION .)
