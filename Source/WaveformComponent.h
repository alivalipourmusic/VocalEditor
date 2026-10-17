#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
    Waveform display with zoom, scroll, selection and playhead.
    Phase 1 core component + Phase 2 event visualization.
*/
class WaveformComponent : public juce::Component,
                          public juce::ChangeListener,
                          private juce::Timer
{
public:
    WaveformComponent (VocalEditorAudioProcessor& processor);
    ~WaveformComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    // Zoom & view
    void setZoomFactor (double factor);
    double getZoomFactor() const { return zoomFactor; }
    void zoomIn();
    void zoomOut();
    void zoomToFit();
    void setViewRange (double startTime, double endTime);
    
    double getVisibleStartTime() const { return visibleStartTime; }
    double getVisibleEndTime() const { return visibleEndTime; }
    double getVisibleDuration() const { return visibleEndTime - visibleStartTime; }

    // Selection helpers
    void clearSelection();
    
    // Load new audio
    void audioFileLoaded();

    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

private:
    void timerCallback() override;
    void updateVisibleRange();
    double timeToX (double time) const;
    double xToTime (float x) const;
    void drawWaveform (juce::Graphics& g);
    void drawEvents (juce::Graphics& g);
    void drawSelection (juce::Graphics& g);
    void drawPlayhead (juce::Graphics& g);
    void drawGrid (juce::Graphics& g);

    VocalEditorAudioProcessor& processor;
    
    std::unique_ptr<juce::AudioThumbnail> thumbnail;
    
    // View state
    double zoomFactor = 1.0;
    double visibleStartTime = 0.0;
    double visibleEndTime = 10.0;
    double totalDuration = 0.0;
    
    // Interaction
    bool isSelecting = false;
    bool isDraggingPlayhead = false;
    double selectionStartTime = 0.0;
    double dragStartX = 0.0;
    
    // Colors (from product design: graphite, turquoise, blue)
    juce::Colour backgroundColour { 0xff1e1e1e };
    juce::Colour waveformColour   { 0xff2dd4bf };  // turquoise
    juce::Colour selectionColour  { 0x5522d3ee };  // cyan transparent
    juce::Colour playheadColour   { 0xfff43f5e };  // rose
    juce::Colour gridColour       { 0xff374151 };
    juce::Colour textColour       { 0xff9ca3af };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformComponent)
};
