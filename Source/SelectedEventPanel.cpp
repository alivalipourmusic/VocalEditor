#include "SelectedEventPanel.h"

//==============================================================================
SelectedEventPanel::SelectedEventPanel (VocalEditorAudioProcessor& p)
    : processor (p)
{
    setOpaque (true);

    auto setupLabel = [this] (juce::Label& lab, const juce::String& text, float size = 13.0f, bool bold = false)
    {
        lab.setText (text, juce::dontSendNotification);
        lab.setFont (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
        lab.setColour (juce::Label::textColourId, textColour);
        addAndMakeVisible (lab);
    };

    setupLabel (titleLabel, "SELECTED EVENT", 14.0f, true);
    titleLabel.setColour (juce::Label::textColourId, accentColour);

    setupLabel (typeLabel,   "Type: —");
    setupLabel (timeLabel,   "Time: —");
    setupLabel (lengthLabel, "Length: —");

    setupLabel (gainLabel, "Gain (dB)");
    gainSlider.setRange (-24.0, 12.0, 0.1);
    gainSlider.setValue (0.0);
    gainSlider.setTextValueSuffix (" dB");
    gainSlider.setNumDecimalPlacesToDisplay (1);
    gainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 22);
    gainSlider.onValueChange = [this] { gainChanged(); };
    gainSlider.setColour (juce::Slider::thumbColourId, accentColour);
    addAndMakeVisible (gainSlider);

    resetGainButton.onClick = [this] { resetGainClicked(); };
    resetGainButton.setColour (juce::TextButton::buttonColourId, bgColour.brighter());
    resetGainButton.setColour (juce::TextButton::textColourOffId, textColour);
    addAndMakeVisible (resetGainButton);

    setupLabel (dynamicGainLabel, "Word Dynamics (dB)");
    dynamicGainSlider.setRange (-24.0, 24.0, 0.1);
    dynamicGainSlider.setValue (0.0);
    dynamicGainSlider.setTextValueSuffix (" dB");
    dynamicGainSlider.setNumDecimalPlacesToDisplay (1);
    dynamicGainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    dynamicGainSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 22);
    dynamicGainSlider.onValueChange = [this] { dynamicGainChanged(); };
    dynamicGainSlider.setColour (juce::Slider::thumbColourId, accentColour);
    addAndMakeVisible (dynamicGainSlider);

    setupLabel (transitionInLabel, "Soft Gain In (ms)");
    transitionInSlider.setRange (0.0, 200.0, 1.0);
    transitionInSlider.setValue (0.0);
    transitionInSlider.setTextValueSuffix (" ms");
    transitionInSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    transitionInSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 22);
    transitionInSlider.onValueChange = [this] { transitionInChanged(); };
    transitionInSlider.setColour (juce::Slider::thumbColourId, accentColour);
    addAndMakeVisible (transitionInSlider);

    setupLabel (transitionOutLabel, "Soft Gain Out (ms)");
    transitionOutSlider.setRange (0.0, 200.0, 1.0);
    transitionOutSlider.setValue (0.0);
    transitionOutSlider.setTextValueSuffix (" ms");
    transitionOutSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    transitionOutSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 22);
    transitionOutSlider.onValueChange = [this] { transitionOutChanged(); };
    transitionOutSlider.setColour (juce::Slider::thumbColourId, accentColour);
    addAndMakeVisible (transitionOutSlider);

    setupLabel (fadeInLabel, "Fade In (ms)");
    fadeInSlider.setRange (0.0, 500.0, 1.0);
    fadeInSlider.setValue (0.0);
    fadeInSlider.setTextValueSuffix (" ms");
    fadeInSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    fadeInSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 22);
    fadeInSlider.onValueChange = [this] { fadeInChanged(); };
    fadeInSlider.setColour (juce::Slider::thumbColourId, accentColour);
    addAndMakeVisible (fadeInSlider);

    setupLabel (fadeOutLabel, "Fade Out (ms)");
    fadeOutSlider.setRange (0.0, 500.0, 1.0);
    fadeOutSlider.setValue (0.0);
    fadeOutSlider.setTextValueSuffix (" ms");
    fadeOutSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    fadeOutSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 22);
    fadeOutSlider.onValueChange = [this] { fadeOutChanged(); };
    fadeOutSlider.setColour (juce::Slider::thumbColourId, accentColour);
    addAndMakeVisible (fadeOutSlider);

    splitButton.onClick = [this] { splitClicked(); };
    deleteButton.onClick = [this] { deleteClicked(); };

    for (auto* btn : { &splitButton, &deleteButton })
    {
        btn->setColour (juce::TextButton::buttonColourId, bgColour.brighter());
        btn->setColour (juce::TextButton::textColourOffId, textColour);
        addAndMakeVisible (btn);
    }
    deleteButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff7f1d1d));

    emptyLabel.setText ("No event selected.\n\nClick an event on the waveform\nor create one from a selection.",
                        juce::dontSendNotification);
    emptyLabel.setFont (juce::FontOptions (13.0f));
    emptyLabel.setColour (juce::Label::textColourId, mutedColour);
    emptyLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (emptyLabel);

    startTimerHz (15);
}

SelectedEventPanel::~SelectedEventPanel()
{
    stopTimer();
}

//==============================================================================
void SelectedEventPanel::paint (juce::Graphics& g)
{
    g.fillAll (bgColour);
    g.setColour (juce::Colour (0xff334155));
    g.drawVerticalLine (0, 0.0f, static_cast<float> (getHeight()));
}

void SelectedEventPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 10);

    titleLabel.setBounds (r.removeFromTop (22));
    r.removeFromTop (8);

    auto* e = processor.getSelectedEvent();
    const bool hasEvent = (e != nullptr);

    emptyLabel.setVisible (! hasEvent);
    emptyLabel.setBounds (r);

    juce::Component* controls[] = {
        &typeLabel, &timeLabel, &lengthLabel,
        &gainLabel, &gainSlider, &resetGainButton,
        &dynamicGainLabel, &dynamicGainSlider,
        &transitionInLabel, &transitionInSlider,
        &transitionOutLabel, &transitionOutSlider,
        &fadeInLabel, &fadeInSlider, &fadeOutLabel, &fadeOutSlider,
        &splitButton, &deleteButton
    };
    for (auto* c : controls)
        c->setVisible (hasEvent);

    if (! hasEvent)
        return;

    typeLabel.setBounds (r.removeFromTop (20));
    timeLabel.setBounds (r.removeFromTop (20));
    lengthLabel.setBounds (r.removeFromTop (20));
    r.removeFromTop (12);

    gainLabel.setBounds (r.removeFromTop (18));
    auto gainRow = r.removeFromTop (28);
    resetGainButton.setBounds (gainRow.removeFromRight (60));
    gainRow.removeFromRight (6);
    gainSlider.setBounds (gainRow);
    r.removeFromTop (8);

    dynamicGainLabel.setBounds (r.removeFromTop (18));
    dynamicGainSlider.setBounds (r.removeFromTop (28));
    r.removeFromTop (6);
    transitionInLabel.setBounds (r.removeFromTop (18));
    transitionInSlider.setBounds (r.removeFromTop (28));
    r.removeFromTop (6);
    transitionOutLabel.setBounds (r.removeFromTop (18));
    transitionOutSlider.setBounds (r.removeFromTop (28));
    r.removeFromTop (8);

    fadeInLabel.setBounds (r.removeFromTop (18));
    fadeInSlider.setBounds (r.removeFromTop (28));
    r.removeFromTop (8);

    fadeOutLabel.setBounds (r.removeFromTop (18));
    fadeOutSlider.setBounds (r.removeFromTop (28));
    r.removeFromTop (16);

    splitButton.setBounds (r.removeFromTop (32));
    r.removeFromTop (8);
    deleteButton.setBounds (r.removeFromTop (32));
}

//==============================================================================
void SelectedEventPanel::timerCallback()
{
    updateFromEvent();
}

void SelectedEventPanel::updateFromEvent()
{
    auto* e = processor.getSelectedEvent();
    const bool hasEvent = (e != nullptr);

    if ((hasEvent && e->id != lastEventId) || (! hasEvent && lastEventId.isNull() == false))
    {
        lastEventId = hasEvent ? e->id : juce::Uuid::null();
        resized();
    }

    if (! hasEvent)
        return;

    typeLabel.setText ("Type: " + e->getTypeName(), juce::dontSendNotification);
    timeLabel.setText ("Time: " + juce::String (e->startTime, 2) + "s  →  " + juce::String (e->endTime, 2) + "s",
                       juce::dontSendNotification);
    lengthLabel.setText ("Length: " + juce::String (e->getLength() * 1000.0, 0) + " ms",
                         juce::dontSendNotification);

    if (! gainSlider.isMouseButtonDown())
        if (std::abs (gainSlider.getValue() - e->gainDb) > 0.05)
            gainSlider.setValue (e->gainDb, juce::dontSendNotification);

    if (! dynamicGainSlider.isMouseButtonDown())
        if (std::abs (dynamicGainSlider.getValue() - e->dynamicGainDb) > 0.05)
            dynamicGainSlider.setValue (e->dynamicGainDb, juce::dontSendNotification);

    if (! transitionInSlider.isMouseButtonDown())
        if (std::abs (transitionInSlider.getValue() - e->gainTransitionInMs) > 0.5)
            transitionInSlider.setValue (e->gainTransitionInMs, juce::dontSendNotification);

    if (! transitionOutSlider.isMouseButtonDown())
        if (std::abs (transitionOutSlider.getValue() - e->gainTransitionOutMs) > 0.5)
            transitionOutSlider.setValue (e->gainTransitionOutMs, juce::dontSendNotification);

    if (! fadeInSlider.isMouseButtonDown())
        if (std::abs (fadeInSlider.getValue() - e->fadeInMs) > 0.5)
            fadeInSlider.setValue (e->fadeInMs, juce::dontSendNotification);

    if (! fadeOutSlider.isMouseButtonDown())
        if (std::abs (fadeOutSlider.getValue() - e->fadeOutMs) > 0.5)
            fadeOutSlider.setValue (e->fadeOutMs, juce::dontSendNotification);
}

void SelectedEventPanel::gainChanged()
{
    if (auto* e = processor.getSelectedEvent())
        processor.setEventGain (e->id, static_cast<float> (gainSlider.getValue()));
}

void SelectedEventPanel::fadeInChanged()
{
    if (auto* e = processor.getSelectedEvent())
        processor.setEventFades (e->id, fadeInSlider.getValue(), e->fadeOutMs);
}

void SelectedEventPanel::fadeOutChanged()
{
    if (auto* e = processor.getSelectedEvent())
        processor.setEventFades (e->id, e->fadeInMs, fadeOutSlider.getValue());
}

void SelectedEventPanel::dynamicGainChanged()
{
    if (auto* e = processor.getSelectedEvent())
        processor.setEventDynamics (e->id, static_cast<float> (dynamicGainSlider.getValue()),
                                   e->gainTransitionInMs, e->gainTransitionOutMs);
}

void SelectedEventPanel::transitionInChanged()
{
    if (auto* e = processor.getSelectedEvent())
        processor.setEventDynamics (e->id, e->dynamicGainDb,
                                   transitionInSlider.getValue(), e->gainTransitionOutMs);
}

void SelectedEventPanel::transitionOutChanged()
{
    if (auto* e = processor.getSelectedEvent())
        processor.setEventDynamics (e->id, e->dynamicGainDb,
                                   e->gainTransitionInMs, transitionOutSlider.getValue());
}

void SelectedEventPanel::splitClicked()
{
    if (auto* e = processor.getSelectedEvent())
    {
        const double pos = processor.getPlaybackPosition();
        if (pos > e->startTime + 0.01 && pos < e->endTime - 0.01)
        {
            processor.splitEvent (e->id, pos);
            processor.setSelectedEvent (processor.getEventList().getById (e->id));
        }
    }
}

void SelectedEventPanel::deleteClicked()
{
    if (auto* e = processor.getSelectedEvent())
    {
        processor.deleteEvent (e->id);
        lastEventId = juce::Uuid::null();
        resized();
    }
}

void SelectedEventPanel::resetGainClicked()
{
    if (auto* e = processor.getSelectedEvent())
    {
        processor.setEventGain (e->id, 0.0f);
        gainSlider.setValue (0.0, juce::dontSendNotification);
    }
}
