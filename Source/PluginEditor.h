#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "MainComponent.h"

//==============================================================================
/**
    Editor window - hosts the MainComponent
*/
class VocalEditorAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    VocalEditorAudioProcessorEditor (VocalEditorAudioProcessor&);
    ~VocalEditorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    VocalEditorAudioProcessor& processorRef;
    MainComponent mainComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalEditorAudioProcessorEditor)
};
