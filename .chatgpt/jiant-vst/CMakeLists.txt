cmake_minimum_required(VERSION 3.22)
project(JIANTFM VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(JUCE)

juce_add_plugin(JIANTFM
    COMPANY_NAME "JIANT Community"
    BUNDLE_ID "org.jiant.fm.vst3"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE JNTF
    PLUGIN_CODE JNF1
    FORMATS VST3
    PRODUCT_NAME "JIANT FM"
)

juce_generate_juce_header(JIANTFM)

target_sources(JIANTFM PRIVATE
    Plugin.cpp
    JIANT/web/emu/felucca_web.c
)

set_source_files_properties(JIANT/web/emu/felucca_web.c PROPERTIES LANGUAGE C)

target_include_directories(JIANTFM PRIVATE
    JIANT/build/gen
    JIANT/firmware/src
    JIANT/web/emu
)

if(MSVC)
    set_source_files_properties(JIANT/web/emu/felucca_web.c PROPERTIES
        COMPILE_OPTIONS "/w;/clang:-ffp-contract=off"
    )
else()
    set_source_files_properties(JIANT/web/emu/felucca_web.c PROPERTIES
        COMPILE_OPTIONS "-w;-ffp-contract=off"
    )
endif()

target_compile_definitions(JIANTFM PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_WEBP=0
    JUCE_INCLUDE_WEBPLIB_CODE=0
)

target_link_libraries(JIANTFM PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
