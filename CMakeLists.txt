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

add_executable(viam-camera-zed
    src/module/main.cpp
    src/module/zed_camera.cpp
)
target_compile_features(viam-camera-zed PRIVATE cxx_std_17)
target_link_libraries(viam-camera-zed
    PRIVATE
        viam-cpp-sdk::viamsdk
        Threads::Threads
        ${CMAKE_DL_LIBS}
)

install(TARGETS viam-camera-zed DESTINATION .)
install(FILES meta.json DESTINATION .)
install(PROGRAMS first_run.sh DESTINATION .)
