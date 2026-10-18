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

    // JUCE TextButton has no setFont(); this LookAndFeel scales its font with button height.
    for (auto* button : { &openButton, &playButton, &stopButton, &zoomInButton, &zoomOutButton,
                          &zoomFitButton, &comparisonButton, &createEventButton, &automationButton,
                          &undoButton, &redoButton, &autoSegmentButton, &levelProcessButton })
        button->setLookAndFeel (&scaledButtonLookAndFeel);

    // Buttons
    openButton.onClick = [this] { openButtonClicked(); };
    playButton.onClick = [this] { playButtonClicked(); };
    stopButton.onClick = [this] { stopButtonClicked(); };
    zoomInButton.onClick = [this] { zoomInClicked(); };
    zoomOutButton.onClick = [this] { zoomOutClicked(); };
    zoomFitButton.onClick = [this] { zoomFitClicked(); };
    createEventButton.onClick = [this] { createEventClicked(); };
    automationButton.onClick = [this]
    {
        const bool enabled = ! waveform.isAutomationMode();
        waveform.setAutomationMode (enabled);
        automationButton.setButtonText (enabled ? "Volume Draw: ON" : "Volume Draw: OFF");
        automationButton.setColour (juce::TextButton::buttonColourId,
                                    enabled ? juce::Colour (0xffa16207) : panelColour);
        statusLabel.setText (enabled ? "Volume automation: click to add points, drag points to edit, right-click a point to remove."
                                     : "Volume automation drawing disabled.",
                             juce::dontSendNotification);
    };
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

    for (auto* btn : { &createEventButton, &automationButton, &undoButton, &redoButton })
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
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 20);
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
    waveform.addKeyListener (this);
    eventPanel.addKeyListener (this);
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

    g.setColour (juce::Colour (0xff94a3b8));
    g.setFont (juce::FontOptions (10.5f));

    auto caption = [&g, this] (const juce::Slider& slider, const juce::String& text)
    {
        const auto b = slider.getBounds();
        g.drawText (text, b.getX() + 3, b.getY() - 12, b.getWidth() - 6, 12,
                    juce::Justification::centredLeft, true);
    };

    caption (deEsserFrequencySlider, "Frequency");
    caption (deEsserThresholdSlider, "Threshold");
    caption (deEsserReductionSlider, "Max reduction");
    caption (dePlosiveCutoffSlider, "Cutoff");
    caption (dePlosiveThresholdSlider, "Threshold");
    caption (dePlosiveReductionSlider, "Reduction");
    caption (noiseReductionThresholdSlider, "Threshold");
    caption (noiseReductionAmountSlider, "Amount");
    caption (noiseReductionReleaseSlider, "Release");
    caption (roomTailThresholdSlider, "Threshold");
    caption (roomTailReductionSlider, "Max reduction");
    caption (roomTailSensitivitySlider, "Sensitivity");
    caption (spectralLowSlider, "Low");
    caption (spectralMidSlider, "Mid");
    caption (spectralHighSlider, "High");

    // Live output meter in dBFS, next to the event inspector.
    if (! meterBounds.isEmpty())
    {
        auto meter = meterBounds.reduced (4, 18);
        g.setColour (juce::Colour (0xff111827));
        g.fillRoundedRectangle (meter.toFloat(), 5.0f);
        const float peak = juce::jlimit (0.0f, 1.0f, processor.getOutputPeak());
        const float db = peak > 0.000001f ? juce::Decibels::gainToDecibels (peak) : -60.0f;
        const float normalized = juce::jmap (juce::jlimit (-60.0f, 0.0f, db), -60.0f, 0.0f, 0.0f, 1.0f);
        auto track = meter.reduced (7, 8);
        const int filled = juce::roundToInt (track.getHeight() * normalized);
        g.setColour (juce::Colour (0xff334155));
        g.fillRoundedRectangle (track.toFloat(), 3.0f);
        if (filled > 0)
        {
            auto bar = track.removeFromBottom (filled);
            g.setColour (db > -3.0f ? juce::Colour (0xfff87171)
                                    : db > -12.0f ? juce::Colour (0xfffbbf24)
                                                  : juce::Colour (0xff34d399));
            g.fillRoundedRectangle (bar.toFloat(), 3.0f);
        }
        g.setColour (juce::Colour (0xff94a3b8));
        g.setFont (juce::FontOptions (10.0f));
        for (const auto dbMark : { 0, -6, -12, -24, -36, -48, -60 })
        {
            const int y = juce::roundToInt (juce::jmap (static_cast<float> (dbMark), -60.0f, 0.0f,
                                                       static_cast<float> (meter.getBottom() - 8),
                                                       static_cast<float> (meter.getY() + 8)));
            g.drawHorizontalLine (y, static_cast<float> (meter.getX()), static_cast<float> (meter.getX() + 5));
            g.drawText (juce::String (dbMark), meter.getX() + 2, y - 6, 25, 12, juce::Justification::centredLeft);
        }
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("dB", meterBounds.getX(), meterBounds.getY(), meterBounds.getWidth(), 16, juce::Justification::centred);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (db <= -59.9f ? "-inf" : juce::String (db, 1), meterBounds.getX(), meterBounds.getBottom() - 14,
                    meterBounds.getWidth(), 14, juce::Justification::centred);
    }
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();
    const float scale = juce::jlimit (0.80f, 1.60f,
        juce::jmin (static_cast<float> (getWidth()) / 1440.0f,
                    static_cast<float> (getHeight()) / 900.0f));
    const auto S = [scale] (int value) { return juce::roundToInt (static_cast<float> (value) * scale); };

    // Scale control typography and value boxes with the editor.
    titleLabel.setFont (juce::FontOptions (static_cast<float> (S (18)), juce::Font::bold));
    statusLabel.setFont (juce::FontOptions (static_cast<float> (S (12))));
    timeLabel.setFont (juce::FontOptions (static_cast<float> (S (13)), juce::Font::bold));
    analysisLabel.setFont (juce::FontOptions (static_cast<float> (S (11))));
    for (auto* slider : { &thresholdSlider, &silenceSlider, &paddingSlider, &targetLevelSlider,
                          &deEsserFrequencySlider, &deEsserThresholdSlider, &deEsserReductionSlider,
                          &dePlosiveCutoffSlider, &dePlosiveThresholdSlider, &dePlosiveReductionSlider,
                          &noiseReductionThresholdSlider, &noiseReductionAmountSlider, &noiseReductionReleaseSlider,
                          &roomTailThresholdSlider, &roomTailReductionSlider, &roomTailSensitivitySlider,
                          &spectralLowSlider, &spectralMidSlider, &spectralHighSlider })
        slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, S (48), S (20));

    // Top bar
    auto topBar = bounds.removeFromTop (S (44));
    topBar.reduce (S (8), S (6));

    titleLabel.setBounds (topBar.removeFromLeft (S (160)));
    topBar.removeFromLeft (S (12));

    openButton.setBounds (topBar.removeFromLeft (S (70)));
    topBar.removeFromLeft (S (6));
    playButton.setBounds (topBar.removeFromLeft (S (60)));
    topBar.removeFromLeft (S (4));
    stopButton.setBounds (topBar.removeFromLeft (S (60)));
    topBar.removeFromLeft (S (16));

    zoomOutButton.setBounds (topBar.removeFromLeft (S (36)));
    topBar.removeFromLeft (S (3));
    zoomInButton.setBounds (topBar.removeFromLeft (S (36)));
    topBar.removeFromLeft (S (3));
    zoomFitButton.setBounds (topBar.removeFromLeft (S (48)));
    topBar.removeFromLeft (S (10));
    createEventButton.setBounds (topBar.removeFromLeft (S (100)));
    topBar.removeFromLeft (S (4));
    automationButton.setBounds (topBar.removeFromLeft (S (118)));
    topBar.removeFromLeft (S (4));
    undoButton.setBounds (topBar.removeFromLeft (S (48)));
    topBar.removeFromLeft (S (3));
    redoButton.setBounds (topBar.removeFromLeft (S (48)));

    timeLabel.setBounds (topBar.removeFromRight (S (100)));

    // Analysis toolbar (kept separate from the transport toolbar).
    auto analysisBar = bounds.removeFromTop (S (58));
    analysisBar.reduce (S (8), S (2));
    analysisLabel.setBounds (analysisBar.removeFromLeft (S (190)));
    autoSegmentButton.setBounds (analysisBar.removeFromLeft (S (112)).reduced (S (2), S (5)));
    levelProcessButton.setBounds (analysisBar.removeFromLeft (S (112)).reduced (S (2), S (5)));
    analysisBar.removeFromLeft (S (4));
    thresholdSlider.setBounds (analysisBar.removeFromLeft (S (112)).reduced (S (1), S (5)));
    silenceSlider.setBounds (analysisBar.removeFromLeft (S (112)).reduced (S (1), S (5)));
    paddingSlider.setBounds (analysisBar.removeFromLeft (S (102)).reduced (S (1), S (5)));
    targetLevelSlider.setBounds (analysisBar.removeFromLeft (S (112)).reduced (S (1), S (5)));
    selectedOnlyToggle.setBounds (analysisBar.removeFromLeft (S (112)).reduced (S (1), S (5)));
    segmentTypeCombo.setBounds (analysisBar.removeFromLeft (S (100)).reduced (S (1), S (5)));

    const int sidePadding = S (8);
    const int effectLabelWidth = juce::jlimit (S (112), S (170), bounds.getWidth() / 7);
    const int sliderGap = S (6);
    const int sliderWidth = juce::jmax (S (90), (bounds.getWidth() - sidePadding * 2 - effectLabelWidth - sliderGap * 3) / 3);

    auto deEsserBar = bounds.removeFromTop (S (44));
    deEsserBar.reduce (sidePadding, 1);
    deEsserToggle.setBounds (deEsserBar.removeFromLeft (effectLabelWidth).reduced (S (2), S (3)));
    deEsserFrequencySlider.setBounds (deEsserBar.removeFromLeft (sliderWidth).withTrimmedTop (S (12)).reduced (S (2), S (1)));
    deEsserBar.removeFromLeft (sliderGap);
    deEsserThresholdSlider.setBounds (deEsserBar.removeFromLeft (sliderWidth).withTrimmedTop (S (12)).reduced (S (2), S (1)));
    deEsserBar.removeFromLeft (sliderGap);
    deEsserReductionSlider.setBounds (deEsserBar.removeFromLeft (sliderWidth).withTrimmedTop (S (12)).reduced (S (2), S (1)));

    auto dePlosiveBar = bounds.removeFromTop (S (44));
    dePlosiveBar.reduce (sidePadding, 1);
    dePlosiveToggle.setBounds (dePlosiveBar.removeFromLeft (effectLabelWidth).reduced (S (2), S (3)));
    dePlosiveCutoffSlider.setBounds (dePlosiveBar.removeFromLeft (sliderWidth).withTrimmedTop (S (12)).reduced (S (2), S (1)));
    dePlosiveBar.removeFromLeft (sliderGap);
    dePlosiveThresholdSlider.setBounds (dePlosiveBar.removeFromLeft (sliderWidth).withTrimmedTop (S (12)).reduced (S (2), S (1)));
    dePlosiveBar.removeFromLeft (sliderGap);
    dePlosiveReductionSlider.setBounds (dePlosiveBar.removeFromLeft (sliderWidth).withTrimmedTop (S (12)).reduced (S (2), S (1)));

    auto noiseReductionBar = bounds.removeFromTop (S (44));
    noiseReductionBar.reduce (sidePadding, 1);
    noiseReductionToggle.setBounds (noiseReductionBar.removeFromLeft (effectLabelWidth).reduced (S (2), S (3)));
    noiseReductionThresholdSlider.setBounds (noiseReductionBar.removeFromLeft (sliderWidth).withTrimmedTop (S (12)).reduced (S (2), S (1)));
    noiseReductionBar.removeFromLeft (sliderGap);
    noiseReductionAmountSlider.setBounds (noiseReductionBar.removeFromLeft (sliderWidth).withTrimmedTop (S (12)).reduced (S (2), S (1)));
    noiseReductionBar.removeFromLeft (sliderGap);
    noiseReductionReleaseSlider.setBounds (noiseReductionBar.removeFromLeft (sliderWidth).withTrimmedTop (S (12)).reduced (S (2), S (1)));

    auto roomTailBar = bounds.removeFromTop (S (44));
    roomTailBar.reduce (sidePadding, 1);
    roomTailReductionToggle.setBounds (roomTailBar.removeFromLeft (effectLabelWidth).reduced (S (2), S (3)));
    roomTailThresholdSlider.setBounds (roomTailBar.removeFromLeft (sliderWidth).withTrimmedTop (S (12)).reduced (S (2), S (1)));
    roomTailBar.removeFromLeft (sliderGap);
    roomTailReductionSlider.setBounds (roomTailBar.removeFromLeft (sliderWidth).withTrimmedTop (S (12)).reduced (S (2), S (1)));
    roomTailBar.removeFromLeft (sliderGap);
    roomTailSensitivitySlider.setBounds (roomTailBar.removeFromLeft (sliderWidth).withTrimmedTop (S (12)).reduced (S (2), S (1)));

    auto spectralBalanceBar = bounds.removeFromTop (S (44));
    spectralBalanceBar.reduce (sidePadding, 1);
    spectralBalanceToggle.setBounds (spectralBalanceBar.removeFromLeft (effectLabelWidth).reduced (S (2), S (3)));
    spectralLowSlider.setBounds (spectralBalanceBar.removeFromLeft (sliderWidth).withTrimmedTop (S (12)).reduced (S (2), S (1)));
    spectralBalanceBar.removeFromLeft (sliderGap);
    spectralMidSlider.setBounds (spectralBalanceBar.removeFromLeft (sliderWidth).withTrimmedTop (S (12)).reduced (S (2), S (1)));
    spectralBalanceBar.removeFromLeft (sliderGap);
    spectralHighSlider.setBounds (spectralBalanceBar.removeFromLeft (sliderWidth).withTrimmedTop (S (12)).reduced (S (2), S (1)));

    // Status bar
    auto statusBar = bounds.removeFromBottom (S (28));
    statusLabel.setBounds (statusBar.reduced (S (10), S (4)));

    // Keep the event controls visible beside the waveform.
    meterBounds = bounds.removeFromRight (S (54)).reduced (S (2), S (4));
    eventPanel.setBounds (bounds.removeFromRight (S (260)).reduced (S (4), S (2)));

    // Timeline and waveform share the remaining editing area.
    auto timelineArea = bounds.removeFromTop (S (28));
    timeline.setBounds (timelineArea.reduced (S (4), S (0)));
    waveform.setBounds (bounds.reduced (S (4), S (2)));
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

    auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (chooserFlags, [this, chooser] (const juce::FileChooser& fc)
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
    {
        processor.setEventGain (event->id, static_cast<float> (gainSlider.getValue()));
        waveform.repaint();
        eventPanel.repaint();
    }
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
    repaint(); // Refresh the live output meter at the UI timer rate.
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
