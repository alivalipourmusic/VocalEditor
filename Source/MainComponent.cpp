#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent (VocalEditorAudioProcessor& p)
    : processor (p),
      timeline (p),
      waveform (p),
      selectedEventPanel (p)
{
    setOpaque (true);

    titleLabel.setText ("VOCAL EDITOR", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, accentColour);
    addAndMakeVisible (titleLabel);

    openButton.onClick       = [this] { openButtonClicked(); };
    playButton.onClick       = [this] { playButtonClicked(); };
    stopButton.onClick       = [this] { stopButtonClicked(); };
    zoomInButton.onClick     = [this] { zoomInClicked(); };
    zoomOutButton.onClick    = [this] { zoomOutClicked(); };
    zoomFitButton.onClick    = [this] { zoomFitClicked(); };
    createEventButton.onClick = [this] { createEventClicked(); };
    undoButton.onClick       = [this] { undoClicked(); };
    redoButton.onClick       = [this] { redoClicked(); };

    for (auto* btn : { &openButton, &playButton, &stopButton, &zoomInButton,
                       &zoomOutButton, &zoomFitButton, &createEventButton,
                       &undoButton, &redoButton })
    {
        btn->setColour (juce::TextButton::buttonColourId, panelColour);
        btn->setColour (juce::TextButton::textColourOffId, textColour);
        btn->setColour (juce::TextButton::buttonOnColourId, accentColour.darker());
        addAndMakeVisible (btn);
    }
    createEventButton.setColour (juce::TextButton::buttonColourId, accentColour.darker (0.55f));

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

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (bgColour);
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    auto topBar = bounds.removeFromTop (48);
    topBar.reduce (8, 6);

    titleLabel.setBounds (topBar.removeFromLeft (128));
    topBar.removeFromLeft (8);

    openButton.setBounds (topBar.removeFromLeft (54));
    topBar.removeFromLeft (4);
    playButton.setBounds (topBar.removeFromLeft (48));
    topBar.removeFromLeft (4);
    stopButton.setBounds (topBar.removeFromLeft (48));
    topBar.removeFromLeft (8);
    createEventButton.setBounds (topBar.removeFromLeft (96));
    topBar.removeFromLeft (4);
    undoButton.setBounds (topBar.removeFromLeft (48));
    topBar.removeFromLeft (3);
    redoButton.setBounds (topBar.removeFromLeft (48));
    topBar.removeFromLeft (8);
    zoomOutButton.setBounds (topBar.removeFromLeft (30));
    topBar.removeFromLeft (3);
    zoomInButton.setBounds (topBar.removeFromLeft (30));
    topBar.removeFromLeft (3);
    zoomFitButton.setBounds (topBar.removeFromLeft (38));
    timeLabel.setBounds (topBar.removeFromRight (82));

    auto statusBar = bounds.removeFromBottom (24);
    statusLabel.setBounds (statusBar.reduced (10, 3));

    const int panelWidth = juce::jlimit (220, 285, bounds.getWidth() / 4);
    auto editorArea = bounds;
    auto panelArea = editorArea.removeFromRight (panelWidth);
    selectedEventPanel.setBounds (panelArea);
    timeline.setBounds (editorArea.removeFromTop (26).reduced (4, 0));
    waveform.setBounds (editorArea.reduced (4, 2));
}

bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aiff")
            || f.endsWithIgnoreCase (".aif") || f.endsWithIgnoreCase (".flac")
            || f.endsWithIgnoreCase (".mp3") || f.endsWithIgnoreCase (".ogg"))
            return true;
    return false;
}

void MainComponent::filesDropped (const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused (x, y);
    if (! files.isEmpty())
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
        const double duration = processor.getDuration();
        timeline.setTotalDuration (duration);
        timeline.setVisibleRange (0.0, duration);
        statusLabel.setText ("Loaded: " + file.getFileName()
                             + "  •  " + juce::String (processor.getSampleRate() / 1000.0, 1)
                             + " kHz  •  " + juce::String (processor.getNumChannels()) + " ch",
                             juce::dontSendNotification);
    }
    else
    {
        statusLabel.setText ("Failed to load file", juce::dontSendNotification);
    }
}

void MainComponent::openButtonClicked()
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Select an audio file...", juce::File{}, "*.wav;*.aiff;*.aif;*.flac;*.mp3;*.ogg");
    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (flags, [this, chooser] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file.existsAsFile())
            loadFile (file);
    });
}

void MainComponent::playButtonClicked()
{
    if (! processor.hasAudio())
        return;

    if (processor.isPlaying())
        processor.stopPlayback();
    else
        processor.startPlayback();
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

void MainComponent::createEventClicked()
{
    const auto selection = processor.getSelection();
    if (! processor.hasAudio() || ! selection.isActive || selection.getLength() < 0.005)
    {
        statusLabel.setText ("Select a time range on the waveform first, then click Create Event.",
                             juce::dontSendNotification);
        return;
    }

    if (auto* event = processor.createEventFromSelection (VocalEvent::Type::Custom))
    {
        processor.setSelectedEvent (event);
        statusLabel.setText ("Event created  •  Adjust gain and fades in the Selected Event panel.",
                             juce::dontSendNotification);
        waveform.repaint();
    }
}

void MainComponent::undoClicked()
{
    if (processor.canUndo())
        processor.undo();
    waveform.repaint();
}

void MainComponent::redoClicked()
{
    if (processor.canRedo())
        processor.redo();
    waveform.repaint();
}

void MainComponent::timerCallback()
{
    const double pos = processor.getPlaybackPosition();
    const int mins = static_cast<int> (pos) / 60;
    const double secs = std::fmod (pos, 60.0);
    timeLabel.setText (juce::String::formatted ("%02d:%05.2f", mins, secs),
                       juce::dontSendNotification);

    timeline.setVisibleRange (waveform.getVisibleStartTime(), waveform.getVisibleEndTime());
    playButton.setButtonText (processor.isPlaying() ? "Pause" : "Play");
    undoButton.setEnabled (processor.canUndo());
    redoButton.setEnabled (processor.canRedo());
    createEventButton.setEnabled (processor.hasAudio());
    waveform.repaint();
}
