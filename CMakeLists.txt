cmake_minimum_required(VERSION 3.20)
project(FreakTubeMastering VERSION 0.1.0)

# Disable the VST2 replacement check and silence the parameter ID mismatch error
add_compile_definitions(
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Point to your JUCE installation
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
    add_subdirectory(JUCE)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE JUCE)
endif()

if (WIN32)
    set(FREAKTUBE_FORMATS VST3 Standalone)
else()
    set(FREAKTUBE_FORMATS LV2 Standalone)
endif()

juce_add_plugin(FreakTubeMastering
        COMPANY_NAME "HiFiFreak"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        FORMATS ${FREAKTUBE_FORMATS}
        LV2URI "urn:hififreak:freaktube"
        PRODUCT_NAME "FreakTube Mastering"
)

juce_generate_juce_header(FreakTubeMastering)

target_sources(FreakTubeMastering PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

target_link_libraries(FreakTubeMastering PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_dsp
)