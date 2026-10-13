cmake_minimum_required(VERSION 3.22)
project(FM1FirmwareHost VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(JUCE)

juce_add_plugin(FM1FirmwareHost
    COMPANY_NAME "FM1 Community"
    BUNDLE_ID "org.fm1.firmwarehost"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE FMHC
    PLUGIN_CODE FMH1
    FORMATS VST3
    PRODUCT_NAME "FM1 Firmware Host"
)

juce_generate_juce_header(FM1FirmwareHost)

target_sources(FM1FirmwareHost PRIVATE Plugin.cpp)
target_include_directories(FM1FirmwareHost PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(FM1FirmwareHost PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_WEBP=0
    JUCE_INCLUDE_WEBPLIB_CODE=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)
target_link_libraries(FM1FirmwareHost PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

add_library(JIANTBackend SHARED
    JiantBackend.cpp
    JIANT/web/emu/felucca_web.c
)
set_source_files_properties(JIANT/web/emu/felucca_web.c PROPERTIES LANGUAGE C)
target_include_directories(JIANTBackend PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    JIANT/build/gen
    JIANT/firmware/src
    JIANT/web/emu
)
if(MSVC)
    set_source_files_properties(JIANT/web/emu/felucca_web.c PROPERTIES COMPILE_OPTIONS "/w;/clang:-ffp-contract=off")
else()
    set_source_files_properties(JIANT/web/emu/felucca_web.c PROPERTIES COMPILE_OPTIONS "-w;-ffp-contract=off")
endif()
set_target_properties(JIANTBackend PROPERTIES
    OUTPUT_NAME "fm1_backend"
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/firmware-bundles/JIANT"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/firmware-bundles/JIANT"
)

add_library(BSIIBackend SHARED BSIIBackend.cpp)
target_include_directories(BSIIBackend PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(MSVC)
    target_compile_options(BSIIBackend PRIVATE /FIcstdio)
endif()
set_target_properties(BSIIBackend PROPERTIES
    OUTPUT_NAME "fm1_backend"
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/firmware-bundles/BSII-Prototype"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/firmware-bundles/BSII-Prototype"
)
