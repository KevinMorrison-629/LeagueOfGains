cmake_minimum_required(VERSION 3.21)

set(PROJECT_NAME "LeagueOfGains")
project(${PROJECT_NAME} VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# This option is controlled by the CMake presets.
# It determines whether your subdirectories are built as static or shared libraries.
option(BUILD_STATIC_DEPS "Build custom dependencies as static libraries" OFF)

if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0601)
endif()

if(BUILD_STATIC_DEPS)
    message(STATUS "Building custom external libraries as STATIC")
    set(BUILD_SHARED_LIBS OFF)
    # CRITICAL: D++ requires this definition when linking statically on Windows
    # to avoid looking for DLL exports that don't exist (fixing LNK2019/LNK2001).
    add_compile_definitions(DPP_STATIC)
else()
    message(STATUS "Building custom external libraries as SHARED (Dynamic)")
    set(BUILD_SHARED_LIBS ON)
endif()

# Add Subprojects
# The vcpkg submodule does not need to be added here.
# CMake automatically uses it via the CMAKE_TOOLCHAIN_FILE variable.
add_subdirectory(server)