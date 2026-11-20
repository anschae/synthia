cmake_minimum_required(VERSION 3.24)

# ==============================================================================
# Plugin identity -- everything that names the plugin lives in this block.
# NOTE: the manufacturer/plugin codes and the bundle id identify the plugin to
# DAWs. Changing them after release makes existing projects lose the plugin.
# The manufacturer code is shared with IRene and Prog Delay (anschae/irene,
# anschae/prog-delay).
# ==============================================================================
set(SY_PLUGIN_NAME        "Synthia")
set(SY_TARGET             "Synthia")
set(SY_VERSION            "0.1.0")
set(SY_COMPANY_NAME       "anschae")
set(SY_MANUFACTURER_CODE  "Ansc")   # same as IRene / Prog Delay
set(SY_PLUGIN_CODE        "Snth")
set(SY_BUNDLE_ID          "com.anschae.synthia")
set(SY_JUCE_TAG           "8.0.15")  # pinned JUCE release
set(SY_CATCH2_TAG         "v3.16.1") # pinned Catch2 release

# ==============================================================================
# Platform settings (must be set before project())
# ==============================================================================
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures (universal)")
# Static MSVC runtime: no VC++ redistributable needed on the user's machine.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

project(Synthia VERSION ${SY_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_XCODE_GENERATE_SCHEME OFF)

option(SY_BUILD_TESTS  "Build the Catch2 unit tests" ON)
option(SY_BUILD_PLUGIN "Build the plugin targets (VST3/AU/Standalone)" ON)
option(SY_BUILD_RENDER "Build the synthia-render offline CLI" ON)
option(SY_BUILD_TOOLS  "Build developer tools (UI snapshot renderer)" OFF)

# ==============================================================================
# Dependencies
# ==============================================================================
include(FetchContent)
FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${SY_JUCE_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   FALSE)
FetchContent_MakeAvailable(juce)

if(SY_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        ${SY_CATCH2_TAG}
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   FALSE)
    FetchContent_MakeAvailable(Catch2)
endif()

# ==============================================================================
# Shared code: compiled into the plugin, the unit tests and the tools.
# (An INTERFACE library, because JUCE modules must be compiled once per final
# target; a static library would duplicate JUCE symbols.)
# ==============================================================================
add_library(SYShared INTERFACE)

file(GLOB_RECURSE SY_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
list(FILTER SY_SOURCES EXCLUDE REGEX ".*/src/PluginEntry\\.cpp$")
target_sources(SYShared INTERFACE ${SY_SOURCES})

target_include_directories(SYShared INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_definitions(SYShared INTERFACE
    SY_PLUGIN_NAME="${SY_PLUGIN_NAME}"
    SY_COMPANY_NAME="${SY_COMPANY_NAME}"
    SY_VERSION_STRING="${SY_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(SYShared INTERFACE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags)

if(MSVC)
    target_compile_options(SYShared INTERFACE /W4 /bigobj)
else()
    target_compile_options(SYShared INTERFACE -Wall -Wextra)
endif()

# ==============================================================================
# Plugin
# ==============================================================================
if(SY_BUILD_PLUGIN)
    set(SY_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND SY_FORMATS AU)
    endif()

    juce_add_plugin(${SY_TARGET}
        PRODUCT_NAME                "${SY_PLUGIN_NAME}"
        COMPANY_NAME                "${SY_COMPANY_NAME}"
        BUNDLE_ID                   "${SY_BUNDLE_ID}"
        PLUGIN_MANUFACTURER_CODE    ${SY_MANUFACTURER_CODE}
        PLUGIN_CODE                 ${SY_PLUGIN_CODE}
        FORMATS                     ${SY_FORMATS}
        IS_SYNTH                    FALSE
        NEEDS_MIDI_INPUT            FALSE
        NEEDS_MIDI_OUTPUT           FALSE
        IS_MIDI_EFFECT              FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        AU_MAIN_TYPE                kAudioUnitType_Effect
        VST3_CATEGORIES             Fx
        COPY_PLUGIN_AFTER_BUILD     FALSE)

    target_sources(${SY_TARGET} PRIVATE src/PluginEntry.cpp)
    target_link_libraries(${SY_TARGET} PRIVATE SYShared)
    # Plugins must never run modal loops: they would block the host.
    target_compile_definitions(${SY_TARGET} PUBLIC JUCE_MODAL_LOOPS_PERMITTED=0)
endif()

# ==============================================================================
# Offline renderer: synthia-render input.wav preset.xml output.wav [--bpm 120]
# ==============================================================================
if(SY_BUILD_RENDER)
    juce_add_console_app(SynthiaRender PRODUCT_NAME "synthia-render")
    target_sources(SynthiaRender PRIVATE tools/Render.cpp)
    target_link_libraries(SynthiaRender PRIVATE SYShared)
    target_compile_definitions(SynthiaRender PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
endif()

# ==============================================================================
# Unit tests (Catch2)
# ==============================================================================
if(SY_BUILD_TESTS)
    enable_testing()

    juce_add_console_app(SynthiaTests PRODUCT_NAME "SynthiaTests")
    file(GLOB SY_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
                                                ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h)
    target_sources(SynthiaTests PRIVATE ${SY_TEST_SOURCES})
    target_link_libraries(SynthiaTests PRIVATE SYShared Catch2::Catch2)
    target_compile_definitions(SynthiaTests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)

    add_test(NAME SynthiaTests COMMAND SynthiaTests)
endif()

# ==============================================================================
# Developer tools (not built by default)
# ==============================================================================
if(SY_BUILD_TOOLS)
    # Renders the editor to a PNG without a DAW: handy for checking layouts.
    juce_add_console_app(SynthiaSnapshot PRODUCT_NAME "SynthiaSnapshot")
    target_sources(SynthiaSnapshot PRIVATE tools/Snapshot.cpp)
    target_link_libraries(SynthiaSnapshot PRIVATE SYShared)
    target_compile_definitions(SynthiaSnapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
endif()
