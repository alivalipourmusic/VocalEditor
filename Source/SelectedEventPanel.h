#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
    Right-side panel showing properties of the currently selected event.
    Phase 2: Gain, Fade In/Out, Split, Delete.
*/
class SelectedEventPanel : public juce::Component,
                           private juce::Timer
{
public:
    SelectedEventPanel (VocalEditorAudioProcessor& processor);
    ~SelectedEventPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateFromEvent();
    void gainChanged();
    void fadeInChanged();
    void fadeOutChanged();
    void splitClicked();
    void deleteClicked();
    void resetGainClicked();

    VocalEditorAudioProcessor& processor;
    juce::Uuid lastEventId;

    // Labels
    juce::Label titleLabel;
    juce::Label typeLabel;
    juce::Label timeLabel;
    juce::Label lengthLabel;

    // Gain
    juce::Label gainLabel;
    juce::Slider gainSlider;
    juce::TextButton resetGainButton { "Reset" };

    // Fades
    juce::Label fadeInLabel;
    juce::Slider fadeInSlider;
    juce::Label fadeOutLabel;
    juce::Slider fadeOutSlider;

    // Actions
    juce::TextButton splitButton  { "Split at Playhead" };
    juce::TextButton deleteButton { "Delete Event" };

    // Empty state
    juce::Label emptyLabel;

    juce::Colour bgColour     { 0xff1e293b };
    juce::Colour accentColour { 0xff2dd4bf };
    juce::Colour textColour   { 0xffe2e8f0 };
    juce::Colour mutedColour  { 0xff94a3b8 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SelectedEventPanel)
};
