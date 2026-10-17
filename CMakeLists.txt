cmake_minimum_required(VERSION 3.22)

project(VocalEditor VERSION 0.1.0)

# Add JUCE as a subdirectory.
# Recommended: put JUCE next to this project (../JUCE) or as a git submodule in JUCE/
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/CMakeLists.txt")
    add_subdirectory(../JUCE JUCE)
else()
    message(FATAL_ERROR "JUCE not found. Clone it next to the project:\n  git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git ../JUCE")
endif()

# Plugin / Standalone target
juce_add_plugin(VocalEditor
    COMPANY_NAME "Avlirecordlabel"
    BUNDLE_ID "com.avlirecordlabel.VocalEditor"
    PLUGIN_MANUFACTURER_CODE Avli
    PLUGIN_CODE Vced
    FORMATS VST3 Standalone
    PRODUCT_NAME "Vocal Editor"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Fx" "Analyzer"
)

# Generate JuceHeader.h
juce_generate_juce_header(VocalEditor)

# Source files
target_sources(VocalEditor
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/WaveformComponent.cpp
        Source/WaveformComponent.h
        Source/TimelineComponent.cpp
        Source/TimelineComponent.h
        Source/MainComponent.cpp
        Source/MainComponent.h
)

# Compile definitions
target_compile_definitions(VocalEditor
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
)

# Link modules
target_link_libraries(VocalEditor
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# C++ standard
target_compile_features(VocalEditor PRIVATE cxx_std_17)
