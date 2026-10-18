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

    analysisLabel.setText ("Silence (dB) / Min gap / Padding / Ride target RMS", juce::dontSendNotification);
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
    levelProcessButton.setTooltip ("Create a smoothed gain envelope from short RMS windows. Near-silent windows are not boosted.");
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

    deEsserToggle.setColour (juce::ToggleButton::textColourId, textColour);
    deEsserToggle.setToggleState (processor.isDeEsserEnabled(), juce::dontSendNotification);
    deEsserToggle.onClick = [this] { processor.setDeEsserEnabled (deEsserToggle.getToggleState()); };
    deEsserToggle.setTooltip ("Enable split-band de-essing on the processed audition/export.");
    addAndMakeVisible (deEsserToggle);

    auto setupDeEsserSlider = [this] (juce::Slider& slider, double min, double max, double step, const juce::String& suffix, double initial)
    {
        slider.setRange (min, max, step);
        slider.setValue (initial, juce::dontSendNotification);
        slider.setTextValueSuffix (suffix);
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
        slider.setColour (juce::Slider::thumbColourId, accentColour);
        slider.setColour (juce::Slider::textBoxTextColourId, textColour);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, panelColour);
        slider.addKeyListener (this);
        addAndMakeVisible (slider);
    };
    setupDeEsserSlider (deEsserFrequencySlider, 2500.0, 12000.0, 100.0, " Hz", processor.getDeEsserFrequency());
    setupDeEsserSlider (deEsserThresholdSlider, -60.0, -6.0, 1.0, " dB", processor.getDeEsserThreshold());
    setupDeEsserSlider (deEsserReductionSlider, 0.0, 18.0, 1.0, " dB", processor.getDeEsserMaxReduction());
    deEsserFrequencySlider.setTooltip ("Sibilance detection frequency");
    deEsserThresholdSlider.setTooltip ("Detection threshold");
    deEsserReductionSlider.setTooltip ("Maximum high-band attenuation");
    for (auto* slider : { &deEsserFrequencySlider, &deEsserThresholdSlider, &deEsserReductionSlider })
        slider->onValueChange = [this] { deEsserChanged(); };

    dePlosiveToggle.setColour (juce::ToggleButton::textColourId, textColour);
    dePlosiveToggle.setToggleState (processor.isDePlosiveEnabled(), juce::dontSendNotification);
    dePlosiveToggle.onClick = [this] { processor.setDePlosiveEnabled (dePlosiveToggle.getToggleState()); };
    dePlosiveToggle.setTooltip ("Reduce excessive low-frequency plosive bursts.");
    addAndMakeVisible (dePlosiveToggle);
    setupDeEsserSlider (dePlosiveCutoffSlider, 60.0, 300.0, 5.0, " Hz", processor.getDePlosiveCutoff());
    setupDeEsserSlider (dePlosiveThresholdSlider, -60.0, -6.0, 1.0, " dB", processor.getDePlosiveThreshold());
    setupDeEsserSlider (dePlosiveReductionSlider, 0.0, 24.0, 1.0, " dB", processor.getDePlosiveMaxReduction());
    dePlosiveCutoffSlider.setTooltip ("Low-frequency detector cutoff");
    dePlosiveThresholdSlider.setTooltip ("Low-frequency burst threshold");
    dePlosiveReductionSlider.setTooltip ("Maximum low-band attenuation");
    for (auto* slider : { &dePlosiveCutoffSlider, &dePlosiveThresholdSlider, &dePlosiveReductionSlider })
        slider->onValueChange = [this] { dePlosiveChanged(); };

    noiseReductionToggle.setColour (juce::ToggleButton::textColourId, textColour);
    noiseReductionToggle.setToggleState (processor.isNoiseReductionEnabled(), juce::dontSendNotification);
    noiseReductionToggle.onClick = [this] { processor.setNoiseReductionEnabled (noiseReductionToggle.getToggleState()); };
    noiseReductionToggle.setTooltip ("Adaptive downward expansion for low-level passages; not spectral denoising under speech.");
    addAndMakeVisible (noiseReductionToggle);
    setupDeEsserSlider (noiseReductionThresholdSlider, -80.0, -18.0, 1.0, " dB", processor.getNoiseReductionThreshold());
    setupDeEsserSlider (noiseReductionAmountSlider, 0.0, 36.0, 1.0, " dB", processor.getNoiseReductionAmount());
    setupDeEsserSlider (noiseReductionReleaseSlider, 20.0, 1000.0, 10.0, " ms", processor.getNoiseReductionRelease());
    noiseReductionThresholdSlider.setTooltip ("Noise floor threshold");
    noiseReductionAmountSlider.setTooltip ("Maximum attenuation for low-level audio");
    noiseReductionReleaseSlider.setTooltip ("Release time to reduce pumping and abrupt tails");
    for (auto* slider : { &noiseReductionThresholdSlider, &noiseReductionAmountSlider, &noiseReductionReleaseSlider })
        slider->onValueChange = [this] { noiseReductionChanged(); };

    roomTailReductionToggle.setColour (juce::ToggleButton::textColourId, textColour);
    roomTailReductionToggle.setToggleState (processor.isRoomTailReductionEnabled(), juce::dontSendNotification);
    roomTailReductionToggle.onClick = [this] { processor.setRoomTailReductionEnabled (roomTailReductionToggle.getToggleState()); };
    roomTailReductionToggle.setTooltip ("Conservatively attenuate low-level tails after louder vocal material; not full de-reverberation.");
    addAndMakeVisible (roomTailReductionToggle);
    setupDeEsserSlider (roomTailThresholdSlider, -60.0, -12.0, 1.0, " dB", processor.getRoomTailThreshold());
    setupDeEsserSlider (roomTailReductionSlider, 0.0, 24.0, 1.0, " dB", processor.getRoomTailMaxReduction());
    setupDeEsserSlider (roomTailSensitivitySlider, 0.1, 1.0, 0.05, "", processor.getRoomTailSensitivity());
    roomTailThresholdSlider.setTooltip ("Tail detection threshold");
    roomTailReductionSlider.setTooltip ("Maximum tail attenuation");
    roomTailSensitivitySlider.setTooltip ("Tail sensitivity; higher values affect more of the decay");
    for (auto* slider : { &roomTailThresholdSlider, &roomTailReductionSlider, &roomTailSensitivitySlider })
        slider->onValueChange = [this] { roomTailReductionChanged(); };

    spectralBalanceToggle.setColour (juce::ToggleButton::textColourId, textColour);
    spectralBalanceToggle.setToggleState (processor.isSpectralBalanceEnabled(), juce::dontSendNotification);
    spectralBalanceToggle.onClick = [this] { processor.setSpectralBalanceEnabled (spectralBalanceToggle.getToggleState()); };
    spectralBalanceToggle.setTooltip ("Enable broad three-band tone shaping on the rendered audio.");
    addAndMakeVisible (spectralBalanceToggle);
    setupDeEsserSlider (spectralLowSlider, -12.0, 12.0, 0.5, " dB", processor.getSpectralLowDb());
    setupDeEsserSlider (spectralMidSlider, -12.0, 12.0, 0.5, " dB", processor.getSpectralMidDb());
    setupDeEsserSlider (spectralHighSlider, -12.0, 12.0, 0.5, " dB", processor.getSpectralHighDb());
    spectralLowSlider.setTooltip ("Low band below approximately 180 Hz");
    spectralMidSlider.setTooltip ("Mid band between approximately 180 Hz and 4.5 kHz");
    spectralHighSlider.setTooltip ("High band above approximately 4.5 kHz");
    for (auto* slider : { &spectralLowSlider, &spectralMidSlider, &spectralHighSlider })
        slider->onValueChange = [this] { spectralBalanceChanged(); };

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

    auto deEsserBar = bounds.removeFromTop (38);
    deEsserBar.reduce (8, 1);
    deEsserToggle.setBounds (deEsserBar.removeFromLeft (100).reduced (1, 3));
    deEsserFrequencySlider.setBounds (deEsserBar.removeFromLeft (220).reduced (2, 3));
    deEsserThresholdSlider.setBounds (deEsserBar.removeFromLeft (220).reduced (2, 3));
    deEsserReductionSlider.setBounds (deEsserBar.removeFromLeft (220).reduced (2, 3));

    auto dePlosiveBar = bounds.removeFromTop (38);
    dePlosiveBar.reduce (8, 1);
    dePlosiveToggle.setBounds (dePlosiveBar.removeFromLeft (100).reduced (1, 3));
    dePlosiveCutoffSlider.setBounds (dePlosiveBar.removeFromLeft (220).reduced (2, 3));
    dePlosiveThresholdSlider.setBounds (dePlosiveBar.removeFromLeft (220).reduced (2, 3));
    dePlosiveReductionSlider.setBounds (dePlosiveBar.removeFromLeft (220).reduced (2, 3));

    auto noiseReductionBar = bounds.removeFromTop (38);
    noiseReductionBar.reduce (8, 1);
    noiseReductionToggle.setBounds (noiseReductionBar.removeFromLeft (130).reduced (1, 3));
    noiseReductionThresholdSlider.setBounds (noiseReductionBar.removeFromLeft (220).reduced (2, 3));
    noiseReductionAmountSlider.setBounds (noiseReductionBar.removeFromLeft (220).reduced (2, 3));
    noiseReductionReleaseSlider.setBounds (noiseReductionBar.removeFromLeft (220).reduced (2, 3));

    auto roomTailBar = bounds.removeFromTop (38);
    roomTailBar.reduce (8, 1);
    roomTailReductionToggle.setBounds (roomTailBar.removeFromLeft (130).reduced (1, 3));
    roomTailThresholdSlider.setBounds (roomTailBar.removeFromLeft (220).reduced (2, 3));
    roomTailReductionSlider.setBounds (roomTailBar.removeFromLeft (220).reduced (2, 3));
    roomTailSensitivitySlider.setBounds (roomTailBar.removeFromLeft (220).reduced (2, 3));

    auto spectralBalanceBar = bounds.removeFromTop (38);
    spectralBalanceBar.reduce (8, 1);
    spectralBalanceToggle.setBounds (spectralBalanceBar.removeFromLeft (150).reduced (1, 3));
    spectralLowSlider.setBounds (spectralBalanceBar.removeFromLeft (220).reduced (2, 3));
    spectralMidSlider.setBounds (spectralBalanceBar.removeFromLeft (220).reduced (2, 3));
    spectralHighSlider.setBounds (spectralBalanceBar.removeFromLeft (220).reduced (2, 3));

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
    const auto segmentLabel = params.eventType == VocalEvent::Type::Word ? "word-labelled region(s)" : "phrase(s)";
    statusLabel.setText ("Auto Segment: created " + juce::String (count)
                         + " " + segmentLabel + ". This is silence-based detection; review boundaries.",
                         juce::dontSendNotification);
}

void MainComponent::deEsserChanged()
{
    processor.setDeEsserParameters (
        static_cast<float> (deEsserFrequencySlider.getValue()),
        static_cast<float> (deEsserThresholdSlider.getValue()),
        static_cast<float> (deEsserReductionSlider.getValue()));
}

void MainComponent::dePlosiveChanged()
{
    processor.setDePlosiveParameters (
        static_cast<float> (dePlosiveCutoffSlider.getValue()),
        static_cast<float> (dePlosiveThresholdSlider.getValue()),
        static_cast<float> (dePlosiveReductionSlider.getValue()));
}

void MainComponent::noiseReductionChanged()
{
    processor.setNoiseReductionParameters (
        static_cast<float> (noiseReductionThresholdSlider.getValue()),
        static_cast<float> (noiseReductionAmountSlider.getValue()),
        static_cast<float> (noiseReductionReleaseSlider.getValue()));
}

void MainComponent::roomTailReductionChanged()
{
    processor.setRoomTailReductionParameters (
        static_cast<float> (roomTailThresholdSlider.getValue()),
        static_cast<float> (roomTailReductionSlider.getValue()),
        static_cast<float> (roomTailSensitivitySlider.getValue()));
}

void MainComponent::spectralBalanceChanged()
{
    processor.setSpectralBalanceParameters (
        static_cast<float> (spectralLowSlider.getValue()),
        static_cast<float> (spectralMidSlider.getValue()),
        static_cast<float> (spectralHighSlider.getValue()));
}

void MainComponent::levelProcessClicked()
{
    const int count = processor.levelEventsToTarget (
        static_cast<float> (targetLevelSlider.getValue()),
        selectedOnlyToggle.getToggleState());
    waveform.repaint();
    eventPanel.repaint();
    statusLabel.setText (count > 0
        ? "Level Ride: created smoothed gain automation for " + juce::String (count) + " event(s)."
        : "Level Ride: no eligible events. Select events or disable Selected only.",
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
    if (code == juce::KeyPress::escapeKey)
    {
        stopButtonClicked();
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
        {
            const auto selectedIds = processor.getSelectedEventIds();
            for (const auto& id : selectedIds)
                processor.deleteEvent (id);
        }
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

    // Reflect host-restored de-esser state without triggering another render.
    if (! deEsserToggle.isMouseButtonDown())
        deEsserToggle.setToggleState (processor.isDeEsserEnabled(), juce::dontSendNotification);
    if (! deEsserFrequencySlider.isMouseButtonDown()
        && std::abs (deEsserFrequencySlider.getValue() - processor.getDeEsserFrequency()) > 1.0)
        deEsserFrequencySlider.setValue (processor.getDeEsserFrequency(), juce::dontSendNotification);
    if (! deEsserThresholdSlider.isMouseButtonDown()
        && std::abs (deEsserThresholdSlider.getValue() - processor.getDeEsserThreshold()) > 0.1)
        deEsserThresholdSlider.setValue (processor.getDeEsserThreshold(), juce::dontSendNotification);
    if (! deEsserReductionSlider.isMouseButtonDown()
        && std::abs (deEsserReductionSlider.getValue() - processor.getDeEsserMaxReduction()) > 0.1)
        deEsserReductionSlider.setValue (processor.getDeEsserMaxReduction(), juce::dontSendNotification);
    if (! dePlosiveToggle.isMouseButtonDown())
        dePlosiveToggle.setToggleState (processor.isDePlosiveEnabled(), juce::dontSendNotification);
    if (! dePlosiveCutoffSlider.isMouseButtonDown()
        && std::abs (dePlosiveCutoffSlider.getValue() - processor.getDePlosiveCutoff()) > 1.0)
        dePlosiveCutoffSlider.setValue (processor.getDePlosiveCutoff(), juce::dontSendNotification);
    if (! dePlosiveThresholdSlider.isMouseButtonDown()
        && std::abs (dePlosiveThresholdSlider.getValue() - processor.getDePlosiveThreshold()) > 0.1)
        dePlosiveThresholdSlider.setValue (processor.getDePlosiveThreshold(), juce::dontSendNotification);
    if (! dePlosiveReductionSlider.isMouseButtonDown()
        && std::abs (dePlosiveReductionSlider.getValue() - processor.getDePlosiveMaxReduction()) > 0.1)
        dePlosiveReductionSlider.setValue (processor.getDePlosiveMaxReduction(), juce::dontSendNotification);
    if (! noiseReductionToggle.isMouseButtonDown())
        noiseReductionToggle.setToggleState (processor.isNoiseReductionEnabled(), juce::dontSendNotification);
    if (! noiseReductionThresholdSlider.isMouseButtonDown()
        && std::abs (noiseReductionThresholdSlider.getValue() - processor.getNoiseReductionThreshold()) > 0.1)
        noiseReductionThresholdSlider.setValue (processor.getNoiseReductionThreshold(), juce::dontSendNotification);
    if (! noiseReductionAmountSlider.isMouseButtonDown()
        && std::abs (noiseReductionAmountSlider.getValue() - processor.getNoiseReductionAmount()) > 0.1)
        noiseReductionAmountSlider.setValue (processor.getNoiseReductionAmount(), juce::dontSendNotification);
    if (! noiseReductionReleaseSlider.isMouseButtonDown()
        && std::abs (noiseReductionReleaseSlider.getValue() - processor.getNoiseReductionRelease()) > 0.1)
        noiseReductionReleaseSlider.setValue (processor.getNoiseReductionRelease(), juce::dontSendNotification);
    if (! roomTailReductionToggle.isMouseButtonDown())
        roomTailReductionToggle.setToggleState (processor.isRoomTailReductionEnabled(), juce::dontSendNotification);
    if (! roomTailThresholdSlider.isMouseButtonDown()
        && std::abs (roomTailThresholdSlider.getValue() - processor.getRoomTailThreshold()) > 0.1)
        roomTailThresholdSlider.setValue (processor.getRoomTailThreshold(), juce::dontSendNotification);
    if (! roomTailReductionSlider.isMouseButtonDown()
        && std::abs (roomTailReductionSlider.getValue() - processor.getRoomTailMaxReduction()) > 0.1)
        roomTailReductionSlider.setValue (processor.getRoomTailMaxReduction(), juce::dontSendNotification);
    if (! roomTailSensitivitySlider.isMouseButtonDown()
        && std::abs (roomTailSensitivitySlider.getValue() - processor.getRoomTailSensitivity()) > 0.01)
        roomTailSensitivitySlider.setValue (processor.getRoomTailSensitivity(), juce::dontSendNotification);
    if (! spectralBalanceToggle.isMouseButtonDown())
        spectralBalanceToggle.setToggleState (processor.isSpectralBalanceEnabled(), juce::dontSendNotification);
    if (! spectralLowSlider.isMouseButtonDown()
        && std::abs (spectralLowSlider.getValue() - processor.getSpectralLowDb()) > 0.1)
        spectralLowSlider.setValue (processor.getSpectralLowDb(), juce::dontSendNotification);
    if (! spectralMidSlider.isMouseButtonDown()
        && std::abs (spectralMidSlider.getValue() - processor.getSpectralMidDb()) > 0.1)
        spectralMidSlider.setValue (processor.getSpectralMidDb(), juce::dontSendNotification);
    if (! spectralHighSlider.isMouseButtonDown()
        && std::abs (spectralHighSlider.getValue() - processor.getSpectralHighDb()) > 0.1)
        spectralHighSlider.setValue (processor.getSpectralHighDb(), juce::dontSendNotification);

    // Update play button text
    playButton.setButtonText (processor.isPlaying() ? "Pause" : "Play");
}
