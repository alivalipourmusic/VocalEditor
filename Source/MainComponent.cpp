#include "MainComponent.h"
#include "SelectedEventPanel.h"

//==============================================================================
MainComponent::MainComponent (VocalEditorAudioProcessor& p)
    : processor (p),
      timeline (p),
      waveform (p),
      selectedEventPanel (p)
{
    setOpaque (true);

    // Title
    titleLabel.setText ("VOCAL EDITOR", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, accentColour);
    addAndMakeVisible (titleLabel);

    // Buttons
    openButton.onClick = [this] { openButtonClicked(); };
    playButton.onClick = [this] { playButtonClicked(); };
    stopButton.onClick = [this] { stopButtonClicked(); };
    zoomInButton.onClick = [this] { zoomInClicked(); };
    zoomOutButton.onClick = [this] { zoomOutClicked(); };
    zoomFitButton.onClick = [this] { zoomFitClicked(); };

    for (auto* btn : { &openButton, &playButton, &stopButton, 
                       &zoomInButton, &zoomOutButton, &zoomFitButton })
    {
        btn->setColour (juce::TextButton::buttonColourId, panelColour);
        btn->setColour (juce::TextButton::textColourOffId, textColour);
        btn->setColour (juce::TextButton::buttonOnColourId, accentColour.darker());
        addAndMakeVisible (btn);
    }

    // Status
    statusLabel.setText ("Ready  •  A Product by Avlirecordlabel", juce::dontSendNotification);
    statusLabel.setFont (juce::FontOptions (12.0f));
    statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xff94a3b8));
    addAndMakeVisible (statusLabel);

    timeLabel.setText ("00:00.00", juce::dontSendNotification);
    timeLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    timeLabel.setColour (juce::Label::textColourId, textColour);
    timeLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (timeLabel);

    addAndMakeVisible (timeline);
    addAndMakeVisible (waveform);
    addAndMakeVisible (selectedEventPanel);

    startTimerHz (20);
}

MainComponent::~MainComponent()
{
    stopTimer();
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (bgColour);
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    // Top bar
    auto topBar = bounds.removeFromTop (44);
    topBar.reduce (8, 6);

    titleLabel.setBounds (topBar.removeFromLeft (160));
    topBar.removeFromLeft (12);

    openButton.setBounds (topBar.removeFromLeft (70));
    topBar.removeFromLeft (6);
    playButton.setBounds (topBar.removeFromLeft (60));
    topBar.removeFromLeft (4);
    stopButton.setBounds (topBar.removeFromLeft (60));
    topBar.removeFromLeft (16);

    zoomOutButton.setBounds (topBar.removeFromLeft (36));
    topBar.removeFromLeft (3);
    zoomInButton.setBounds (topBar.removeFromLeft (36));
    topBar.removeFromLeft (3);
    zoomFitButton.setBounds (topBar.removeFromLeft (48));

    timeLabel.setBounds (topBar.removeFromRight (100));

    // Status bar
    auto statusBar = bounds.removeFromBottom (28);
    statusLabel.setBounds (statusBar.reduced (10, 4));

    // Timeline
    auto timelineArea = bounds.removeFromTop (28);
    timeline.setBounds (timelineArea.reduced (4, 0));

    // Main editing area: waveform gets most of the width; inspector stays docked.
    auto content = bounds.reduced (4, 2);
    const int inspectorWidth = juce::jlimit (250, 340, content.getWidth() / 4);
    auto inspectorArea = content.removeFromRight (inspectorWidth);
    content.removeFromRight (6);
    waveform.setBounds (content);
    selectedEventPanel.setBounds (inspectorArea);
}

//==============================================================================
bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        if (f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aiff") ||
            f.endsWithIgnoreCase (".aif") || f.endsWithIgnoreCase (".flac") ||
            f.endsWithIgnoreCase (".mp3") || f.endsWithIgnoreCase (".ogg"))
            return true;
    }
    return false;
}

void MainComponent::filesDropped (const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused (x, y);
    if (files.isEmpty())
        return;

    loadFile (juce::File (files[0]));
}

void MainComponent::loadFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    processor.loadAudioFile (file);
    waveform.audioFileLoaded();

    if (processor.hasAudio())
    {
        const double dur = static_cast<double> (processor.getNumSamples()) / processor.getSampleRate();
        timeline.setTotalDuration (dur);
        timeline.setVisibleRange (0.0, dur);

        statusLabel.setText ("Loaded: " + file.getFileName() + 
                             "  •  " + juce::String (processor.getSampleRate() / 1000.0, 1) + " kHz  •  " +
                             juce::String (processor.getNumChannels()) + " ch",
                             juce::dontSendNotification);
    }
    else
    {
        statusLabel.setText ("Failed to load file", juce::dontSendNotification);
    }
}

//==============================================================================
void MainComponent::openButtonClicked()
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Select an audio file...",
        juce::File{},
        "*.wav;*.aiff;*.aif;*.flac;*.mp3;*.ogg");

    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (flags, [this, chooser] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file.existsAsFile())
            loadFile (file);
    });
}

void MainComponent::playButtonClicked()
{
    if (processor.hasAudio())
    {
        if (processor.isPlaying())
            processor.stopPlayback();
        else
            processor.startPlayback();
    }
}

void MainComponent::stopButtonClicked()
{
    processor.stopPlayback();
    processor.setPlaybackPosition (0.0);
}

void MainComponent::zoomInClicked()
{
    waveform.zoomIn();
    timeline.setVisibleRange (waveform.getVisibleStartTime(), waveform.getVisibleEndTime());
}

void MainComponent::zoomOutClicked()
{
    waveform.zoomOut();
    timeline.setVisibleRange (waveform.getVisibleStartTime(), waveform.getVisibleEndTime());
}

void MainComponent::zoomFitClicked()
{
    waveform.zoomToFit();
    timeline.setVisibleRange (waveform.getVisibleStartTime(), waveform.getVisibleEndTime());
}

//==============================================================================
void MainComponent::timerCallback()
{
    // Update time display
    double pos = processor.getPlaybackPosition();
    int mins = static_cast<int> (pos) / 60;
    double secs = std::fmod (pos, 60.0);
    timeLabel.setText (juce::String::formatted ("%02d:%05.2f", mins, secs),
                       juce::dontSendNotification);

    // Sync timeline with waveform view
    timeline.setVisibleRange (waveform.getVisibleStartTime(), waveform.getVisibleEndTime());

    // Update play button text
    playButton.setButtonText (processor.isPlaying() ? "Pause" : "Play");
}
