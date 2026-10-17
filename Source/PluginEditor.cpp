#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
VocalEditorAudioProcessorEditor::VocalEditorAudioProcessorEditor (VocalEditorAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      mainComponent (p)
{
    addAndMakeVisible (mainComponent);

    // Default size - professional starting point
    setSize (1100, 620);
    setResizable (true, true);
    setResizeLimits (800, 400, 4096, 2160);
}

VocalEditorAudioProcessorEditor::~VocalEditorAudioProcessorEditor()
{
}

//==============================================================================
void VocalEditorAudioProcessorEditor::paint (juce::Graphics& g)
{
    // MainComponent handles its own background
    g.fillAll (juce::Colour (0xff0f172a));
}

void VocalEditorAudioProcessorEditor::resized()
{
    mainComponent.setBounds (getLocalBounds());
}
