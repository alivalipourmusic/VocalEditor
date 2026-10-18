cmake_minimum_required(VERSION 3.22)

project(VocalEditor VERSION 0.1.0)

include(CTest)

# Add JUCE as a subdirectory.
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/CMakeLists.txt")
    add_subdirectory(../JUCE JUCE)
else()
    message(FATAL_ERROR "JUCE not found. Clone it next to the project:\n  git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git ../JUCE")
endif()

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

juce_generate_juce_header(VocalEditor)

target_sources(VocalEditor
    PRIVATE
        Source/VocalEvent.h
        Source/AudioResampling.h
        Source/AudioProcessingMath.h
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
        Source/SelectedEventPanel.cpp
        Source/SelectedEventPanel.h
)

target_compile_definitions(VocalEditor
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
)

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

target_compile_features(VocalEditor PRIVATE cxx_std_17)

if (BUILD_TESTING)
    juce_add_console_app(VocalEditorTests
        PRODUCT_NAME "Vocal Editor Tests"
    )

    juce_generate_juce_header(VocalEditorTests)

    target_sources(VocalEditorTests
        PRIVATE
            Tests/VocalEventTests.cpp
            Source/VocalEvent.h
            Source/AudioResampling.h
            Source/AudioProcessingMath.h
    )

    target_include_directories(VocalEditorTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/Source
    )

    target_compile_definitions(VocalEditorTests
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_DISPLAY_SPLASH_SCREEN=0
            JUCE_REPORT_APP_USAGE=0
    )

    target_link_libraries(VocalEditorTests
        PRIVATE
            juce::juce_core
            juce::juce_audio_basics
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    target_compile_features(VocalEditorTests PRIVATE cxx_std_17)

    add_test(NAME VocalEventTests COMMAND VocalEditorTests)
endif()
