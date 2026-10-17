#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent (VocalEditorAudioProcessor& p)
    : processor (p),
      timeline (p),
      waveform (p),
      eventPanel (p)
{
    setOpaque (true);
    setWantsKeyboardFocus (true);
    addKeyListener (this);

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
    createEventButton.onClick = [this] { createEventClicked(); };
    undoButton.onClick = [this] { undoClicked(); };
    redoButton.onClick = [this] { redoClicked(); };
    autoSegmentButton.onClick = [this] { autoSegmentClicked(); };
    levelProcessButton.onClick = [this] { levelProcessClicked(); };

    for (auto* btn : { &openButton, &playButton, &stopButton, 
                       &zoomInButton, &zoomOutButton, &zoomFitButton })
    {
        btn->setColour (juce::TextButton::buttonColourId, panelColour);
        btn->setColour (juce::TextButton::textColourOffId, textColour);
        btn->setColour (juce::TextButton::buttonOnColourId, accentColour.darker());
        btn->addKeyListener (this);
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

    for (auto* btn : { &createEventButton, &undoButton, &redoButton })
    {
        btn->setColour (juce::TextButton::buttonColourId, panelColour);
        btn->setColour (juce::TextButton::textColourOffId, textColour);
        btn->addKeyListener (this);
        addAndMakeVisible (btn);
    }


    // Analysis controls. These parameters remain editable so automatic detection
    // can be tuned to the noise floor and performance of each vocal recording.
    auto setupAnalysisSlider = [this] (juce::Slider& slider, double min, double max, double step, const juce::String& suffix)
    {
        slider.setRange (min, max, step);
        slider.setValue (min);
        slider.setTextValueSuffix (suffix);
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 54, 20);
        slider.setColour (juce::Slider::thumbColourId, accentColour);
        slider.setColour (juce::Slider::textBoxTextColourId, textColour);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, panelColour);
        addAndMakeVisible (slider);
        slider.addKeyListener (this);
    };
    setupAnalysisSlider (thresholdSlider, -60.0, -18.0, 1.0, " dB");
    thresholdSlider.setValue (-40.0, juce::dontSendNotification);
    setupAnalysisSlider (silenceSlider, 50.0, 500.0, 10.0, " ms");
    silenceSlider.setValue (120.0, juce::dontSendNotification);
    setupAnalysisSlider (paddingSlider, 0.0, 100.0, 5.0, " ms");
    paddingSlider.setValue (20.0, juce::dontSendNotification);
    setupAnalysisSlider (targetLevelSlider, -30.0, -12.0, 1.0, " dB");
    targetLevelSlider.setValue (-18.0, juce::dontSendNotification);

    analysisLabel.setText ("Silence (dB) / Min gap / Padding / Target RMS", juce::dontSendNotification);
    analysisLabel.setFont (juce::FontOptions (11.0f));
    analysisLabel.setColour (juce::Label::textColourId, juce::Colour (0xff94a3b8));
    addAndMakeVisible (analysisLabel);

    for (auto* btn : { &autoSegmentButton, &levelProcessButton })
    {
        btn->setColour (juce::TextButton::buttonColourId, panelColour);
        btn->setColour (juce::TextButton::textColourOffId, textColour);
        btn->addKeyListener (this);
        addAndMakeVisible (btn);
    }
    levelProcessButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff155e75));
    selectedOnlyToggle.setToggleState (false, juce::dontSendNotification);
    selectedOnlyToggle.setColour (juce::ToggleButton::textColourId, textColour);
    selectedOnlyToggle.addKeyListener (this);
    addAndMakeVisible (selectedOnlyToggle);
    segmentTypeCombo.addItem ("Phrase", 1);
    segmentTypeCombo.addItem ("Word preset", 2);
    segmentTypeCombo.setSelectedId (1, juce::dontSendNotification);
    segmentTypeCombo.setTooltip ("Word preset marks detected regions as Word; it does not perform speech recognition.");
    segmentTypeCombo.addKeyListener (this);
    addAndMakeVisible (segmentTypeCombo);

    addAndMakeVisible (timeline);
    addAndMakeVisible (waveform);
    addAndMakeVisible (eventPanel);
    timeline.addKeyListener (this);
    waveform.addKeyListener (this);
    eventPanel.addKeyListener (this);

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
    topBar.removeFromLeft (10);
    createEventButton.setBounds (topBar.removeFromLeft (100));
    topBar.removeFromLeft (4);
    undoButton.setBounds (topBar.removeFromLeft (48));
    topBar.removeFromLeft (3);
    redoButton.setBounds (topBar.removeFromLeft (48));

    timeLabel.setBounds (topBar.removeFromRight (100));

    // Analysis toolbar (kept separate from the transport toolbar).
    auto analysisBar = bounds.removeFromTop (58);
    analysisBar.reduce (8, 2);
    analysisLabel.setBounds (analysisBar.removeFromLeft (190));
    autoSegmentButton.setBounds (analysisBar.removeFromLeft (112).reduced (2, 5));
    levelProcessButton.setBounds (analysisBar.removeFromLeft (112).reduced (2, 5));
    analysisBar.removeFromLeft (4);
    thresholdSlider.setBounds (analysisBar.removeFromLeft (112).reduced (1, 5));
    silenceSlider.setBounds (analysisBar.removeFromLeft (112).reduced (1, 5));
    paddingSlider.setBounds (analysisBar.removeFromLeft (102).reduced (1, 5));
    targetLevelSlider.setBounds (analysisBar.removeFromLeft (112).reduced (1, 5));
    selectedOnlyToggle.setBounds (analysisBar.removeFromLeft (112).reduced (1, 5));
    segmentTypeCombo.setBounds (analysisBar.removeFromLeft (100).reduced (1, 5));

    // Status bar
    auto statusBar = bounds.removeFromBottom (28);
    statusLabel.setBounds (statusBar.reduced (10, 4));

    // Keep the event controls visible beside the waveform.
    eventPanel.setBounds (bounds.removeFromRight (260).reduced (4, 2));

    // Timeline and waveform share the remaining editing area.
    auto timelineArea = bounds.removeFromTop (28);
    timeline.setBounds (timelineArea.reduced (4, 0));
    waveform.setBounds (bounds.reduced (4, 2));
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

void MainComponent::createEventClicked()
{
    if (auto* event = processor.createEventFromSelection())
    {
        processor.setSelectedEvent (event);
        statusLabel.setText ("Event created • adjust Gain and Fade in the Selected Event panel",
                             juce::dontSendNotification);
        waveform.repaint();
        eventPanel.repaint();
    }
    else
    {
        statusLabel.setText ("Select at least 5 ms of audio before creating an event",
                             juce::dontSendNotification);
    }
}

void MainComponent::undoClicked()
{
    processor.undo();
}

void MainComponent::redoClicked()
{
    processor.redo();
}

void MainComponent::gainSliderChanged()
{
    if (auto* event = processor.getSelectedEvent())
        processor.setEventGain (event->id, static_cast<float> (gainSlider.getValue()));
}


void MainComponent::autoSegmentClicked()
{
    VocalEditorAudioProcessor::SegmentParams params;
    params.silenceThresholdDb = static_cast<float> (thresholdSlider.getValue());
    params.minSilenceMs = silenceSlider.getValue();
    params.minEventMs = 80.0;
    params.padMs = paddingSlider.getValue();
    params.clearExisting = true;
    params.eventType = segmentTypeCombo.getSelectedId() == 2
        ? VocalEvent::Type::Word : VocalEvent::Type::Phrase;

    const int count = processor.autoSegmentFromSilence (params);
    waveform.repaint();
    eventPanel.repaint();
    statusLabel.setText ("Auto Segment: created " + juce::String (count)
                         + " phrase(s). Review boundaries before processing.",
                         juce::dontSendNotification);
}

void MainComponent::levelProcessClicked()
{
    const int count = processor.levelEventsToTarget (
        static_cast<float> (targetLevelSlider.getValue()),
        selectedOnlyToggle.getToggleState());
    waveform.repaint();
    eventPanel.repaint();
    statusLabel.setText (count > 0
        ? "Level Process: adjusted " + juce::String (count) + " event(s) toward target RMS."
        : "Level Process: no eligible non-silent events. Select events or disable Selected only.",
        juce::dontSendNotification);
}

bool MainComponent::keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent)
{
    juce::ignoreUnused (originatingComponent);
    const int code = key.getKeyCode();
    const bool command = key.getModifiers().isCommandDown();
    const bool shift = key.getModifiers().isShiftDown();

    if (code == juce::KeyPress::spaceKey)
    {
        playButtonClicked();
        return true;
    }
    if (command && code == 'Z')
    {
        undoClicked();
        return true;
    }
    if (command && (code == 'Y' || (shift && code == 'Z')))
    {
        redoClicked();
        return true;
    }
    if (command && code == 'O')
    {
        openButtonClicked();
        return true;
    }
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
    {
        if (processor.getNumSelectedEvents() > 0)
            processor.deleteSelectedEvents();
        else if (auto* event = processor.getSelectedEvent())
            processor.deleteEvent (event->id);
        waveform.repaint();
        eventPanel.repaint();
        return true;
    }
    if (! command && ! key.getModifiers().isAltDown() && code == 'S')
    {
        if (auto* event = processor.getSelectedEvent())
        {
            const double pos = processor.getPlaybackPosition();
            processor.splitEvent (event->id, pos);
            waveform.repaint();
            eventPanel.repaint();
        }
        return true;
    }
    return false;
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
