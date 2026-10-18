#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "WaveformComponent.h"
#include "TimelineComponent.h"
#include "SelectedEventPanel.h"

//==============================================================================
/**
    Main content area: toolbar + timeline + waveform + transport + Phase 2 controls
*/
class MainComponent : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      public juce::KeyListener,
                      private juce::Timer
{
public:
    MainComponent (VocalEditorAudioProcessor& processor);
    ~MainComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;

    void loadFile (const juce::File& file);

private:
    void timerCallback() override;
    void openButtonClicked();
    void openProjectButtonClicked();
    void saveProjectButtonClicked();
    void saveProjectToFile (const juce::File& file);
    void exportAudioButtonClicked();
    void playButtonClicked();
    void stopButtonClicked();
    void zoomInClicked();
    void zoomOutClicked();
    void zoomFitClicked();
    void createEventClicked();
    void undoClicked();
    void redoClicked();
    void gainSliderChanged();
    void autoSegmentClicked();
    void levelProcessClicked();

    VocalEditorAudioProcessor& processor;

    // Transport / file
    juce::TextButton openButton  { "Open Audio" };
    juce::TextButton openProjectButton { "Open Project" };
    juce::TextButton saveProjectButton { "Save Project" };
    juce::TextButton exportAudioButton { "Export Audio" };
    juce::TextButton playButton  { "Play" };
    juce::TextButton stopButton  { "Stop" };
    juce::TextButton zoomInButton  { "+" };
    juce::TextButton zoomOutButton { "-" };
    juce::TextButton zoomFitButton { "Fit" };

    // Phase 2
    juce::TextButton createEventButton { "Create Event" };
    juce::TextButton undoButton { "Undo" };
    juce::TextButton redoButton { "Redo" };
    juce::TextButton autoSegmentButton { "Auto Segment" };
    juce::TextButton levelProcessButton { "Level Process" };
    juce::Slider thresholdSlider;
    juce::Slider silenceSlider;
    juce::Slider paddingSlider;
    juce::Slider targetLevelSlider;
    juce::ToggleButton selectedOnlyToggle { "Selected only" };
    juce::ComboBox segmentTypeCombo;
    juce::Label analysisLabel;
    juce::Slider gainSlider;
    juce::Label  gainLabel;

    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label timeLabel;

    TimelineComponent timeline;
    WaveformComponent waveform;
    SelectedEventPanel eventPanel;

    juce::Colour bgColour     { 0xff0f172a };
    juce::Colour panelColour  { 0xff1e293b };
    juce::Colour accentColour { 0xff2dd4bf };
    juce::Colour textColour   { 0xffe2e8f0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
