#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
    Simple time ruler / timeline above the waveform.
*/
class TimelineComponent : public juce::Component
{
public:
    TimelineComponent (VocalEditorAudioProcessor& processor);
    ~TimelineComponent() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void setVisibleRange (double startTime, double endTime);
    void setTotalDuration (double duration);
    double getVisibleDuration() const { return visibleEndTime - visibleStartTime; }

private:
    double timeToX (double time) const;
    juce::String formatTime (double seconds) const;

    VocalEditorAudioProcessor& processor;
    
    double visibleStartTime = 0.0;
    double visibleEndTime = 10.0;
    double totalDuration = 0.0;
    
    juce::Colour backgroundColour { 0xff111827 };
    juce::Colour textColour       { 0xffd1d5db };
    juce::Colour tickColour       { 0xff6b7280 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimelineComponent)
};
