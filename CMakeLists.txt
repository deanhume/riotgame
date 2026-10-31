cmake_minimum_required(VERSION 3.22)
project(RiotControl LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(RIOT_MODEL_ONLY "Build gameplay tests without the graphics engine" OFF)

add_library(RiotModel Source/GameModel.cpp Source/Progress.cpp Source/DefendModel.cpp)
target_include_directories(RiotModel PUBLIC Source)
if(MSVC)
    target_compile_options(RiotModel PRIVATE /W4 /permissive- /utf-8)
endif()

enable_testing()
add_executable(RiotModelTests Tests/GameModelTests.cpp)
target_link_libraries(RiotModelTests PRIVATE RiotModel)
add_test(NAME gameplay COMMAND RiotModelTests)

if(RIOT_MODEL_ONLY)
    return()
endif()
if(NOT WIN32)
    message(FATAL_ERROR "This prototype targets native Windows. Use RIOT_MODEL_ONLY for portable tests.")
endif()
enable_language(RC)

set(AX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.tools/axmol-2.11.5" CACHE PATH "Axmol 2.11.5 engine directory")
if(NOT EXISTS "${AX_ROOT}/core/axmol.h")
    message(FATAL_ERROR "Axmol is missing. Run build.ps1 to download the pinned engine.")
endif()
set(_AX_ROOT "${AX_ROOT}")
set(ENV{AX_ROOT} "${AX_ROOT}")
list(APPEND CMAKE_MODULE_PATH "${AX_ROOT}/cmake/Modules")
set(AX_EXT_HINT OFF CACHE BOOL "" FORCE)
set(AX_ENABLE_3D OFF CACHE BOOL "" FORCE)
set(AX_ENABLE_PHYSICS OFF CACHE BOOL "" FORCE)
set(AX_ENABLE_MSEDGE_WEBVIEW2 OFF CACHE BOOL "" FORCE)
set(AX_ENABLE_MFMEDIA OFF CACHE BOOL "" FORCE)
set(AX_ENABLE_CONSOLE OFF CACHE BOOL "" FORCE)
set(AX_ENABLE_HTTP OFF CACHE BOOL "" FORCE)
set(AX_ENABLE_WEBSOCKET OFF CACHE BOOL "" FORCE)
set(AX_ENABLE_OPUS OFF CACHE BOOL "" FORCE)
set(AX_WITH_CURL ON CACHE BOOL "" FORCE)
set(AX_WITH_DOCTEST OFF CACHE BOOL "" FORCE)
include(AXBuildSet)
add_subdirectory("${AX_ROOT}/core" "${ENGINE_BINARY_PATH}/axmol/core")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(RIOT_AUDIO_DIR "${CMAKE_BINARY_DIR}/generated/audio")
add_custom_command(OUTPUT "${RIOT_AUDIO_DIR}/generated.stamp"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_audio.py" "${RIOT_AUDIO_DIR}"
    DEPENDS tools/generate_audio.py
    VERBATIM)
add_custom_target(RiotAudio DEPENDS "${RIOT_AUDIO_DIR}/generated.stamp")

set(RIOT_BRANDING_DIR "${CMAKE_BINARY_DIR}/generated/branding")
set(RIOT_BRANDING_FILES
    "${RIOT_BRANDING_DIR}/RiotControl.ico"
    "${RIOT_BRANDING_DIR}/icon.png"
    "${RIOT_BRANDING_DIR}/icon.svg"
    "${RIOT_BRANDING_DIR}/logo.png"
    "${RIOT_BRANDING_DIR}/logo.svg"
    "${RIOT_BRANDING_DIR}/installer-sidebar.bmp")
add_custom_command(OUTPUT ${RIOT_BRANDING_FILES}
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_branding.py" "${RIOT_BRANDING_DIR}"
    DEPENDS tools/generate_branding.py
    VERBATIM)
add_custom_target(RiotBranding DEPENDS ${RIOT_BRANDING_FILES})

add_executable(RiotControl WIN32 Source/main.cpp Source/GameScene.cpp Source/GameSceneDefend.cpp Source/GameSceneCinematic.cpp Source/PixelArt.cpp Source/TargetArt.cpp
    Source/RiotControl.rc)
add_dependencies(RiotControl RiotAudio RiotBranding)
set_source_files_properties(Source/RiotControl.rc PROPERTIES OBJECT_DEPENDS "${RIOT_BRANDING_DIR}/RiotControl.ico")
target_link_libraries(RiotControl PRIVATE RiotModel ${_AX_CORE_LIB})
target_include_directories(RiotControl PRIVATE "${RIOT_BRANDING_DIR}")
target_include_directories(RiotControl SYSTEM PRIVATE
    "$<TARGET_PROPERTY:${_AX_CORE_LIB},INTERFACE_INCLUDE_DIRECTORIES>")
target_compile_options(RiotControl PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/W4;/utf-8>")
set_target_properties(RiotControl PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/RiotControl"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:RiotControl>")
ax_sync_target_dlls(RiotControl)
add_custom_target(RiotResources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${AXSLCC_OUT_DIR}" "$<TARGET_FILE_DIR:RiotControl>/axslc"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${RIOT_AUDIO_DIR}" "$<TARGET_FILE_DIR:RiotControl>/audio"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${RIOT_BRANDING_DIR}" "$<TARGET_FILE_DIR:RiotControl>/branding"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/big-beaver-games-logo.png"
        "$<TARGET_FILE_DIR:RiotControl>/branding/big-beaver-games-logo.png"
    DEPENDS RiotControl RiotAudio)
