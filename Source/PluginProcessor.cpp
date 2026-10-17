#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
VocalEditorAudioProcessor::VocalEditorAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    formatManager.registerBasicFormats();
}

VocalEditorAudioProcessor::~VocalEditorAudioProcessor()
{
}

//==============================================================================
const juce::String VocalEditorAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool VocalEditorAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool VocalEditorAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool VocalEditorAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double VocalEditorAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int VocalEditorAudioProcessor::getNumPrograms()
{
    return 1;
}

int VocalEditorAudioProcessor::getCurrentProgram()
{
    return 0;
}

void VocalEditorAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String VocalEditorAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void VocalEditorAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void VocalEditorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    currentSampleRate = sampleRate;
}

void VocalEditorAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool VocalEditorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void VocalEditorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;
    
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Standalone/file audition reads an immutable processed snapshot. The
    // three-buffer handoff avoids blocking the real-time thread while edits render.
    if (currentlyPlaying && hasAudio())
    {
        int sourceIndex = 0;
        for (;;)
        {
            sourceIndex = activeProcessedBuffer.load (std::memory_order_acquire);
            readingProcessedBuffer.store (sourceIndex, std::memory_order_release);
            if (sourceIndex == activeProcessedBuffer.load (std::memory_order_acquire))
                break;
        }

        const auto& source = processedBuffers[static_cast<size_t> (sourceIndex)];
        const int numSamplesToPlay = buffer.getNumSamples();
        const int sourceLength = source.getNumSamples();
        const int position = playbackSamplePosition;
        const int remainingSamples = sourceLength - position;

        if (remainingSamples <= 0)
        {
            currentlyPlaying = false;
            playbackPosition = 0.0;
            playbackSamplePosition = 0;
            buffer.clear();
            readingProcessedBuffer.store (-1, std::memory_order_release);
            return;
        }

        const int samplesToCopy = juce::jmin (numSamplesToPlay, remainingSamples);
        const int numChannels = juce::jmin (buffer.getNumChannels(), source.getNumChannels());
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.copyFrom (ch, 0, source, ch, position, samplesToCopy);
        for (int ch = numChannels; ch < buffer.getNumChannels(); ++ch)
            buffer.clear (ch, 0, numSamplesToPlay);

        if (samplesToCopy < numSamplesToPlay)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.clear (ch, samplesToCopy, numSamplesToPlay - samplesToCopy);
            currentlyPlaying = false;
        }

        playbackSamplePosition = position + samplesToCopy;
        playbackPosition = static_cast<double> (playbackSamplePosition) / currentSampleRate;
        readingProcessedBuffer.store (-1, std::memory_order_release);
    }
    // When the file audition transport is stopped, VST3 remains a pass-through
    // processor for host input. Live host-input editing is a separate integration step.
}

//==============================================================================
bool VocalEditorAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* VocalEditorAudioProcessor::createEditor()
{
    return new VocalEditorAudioProcessorEditor (*this);
}

//==============================================================================
void VocalEditorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Phase 1: Basic state (will expand later)
    juce::ignoreUnused (destData);
}

void VocalEditorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ignoreUnused (data, sizeInBytes);
}

//==============================================================================
void VocalEditorAudioProcessor::applyEventToBuffer (const VocalEvent& event, juce::AudioBuffer<float>& target)
{
    if (target.getNumSamples() == 0 || target.getNumChannels() == 0 || currentSampleRate <= 0.0)
        return;

    const int totalSamples = target.getNumSamples();
    const int startSample = juce::jlimit (0, totalSamples, static_cast<int> (std::floor (event.startTime * currentSampleRate)));
    const int endSample = juce::jlimit (startSample, totalSamples, static_cast<int> (std::ceil (event.endTime * currentSampleRate)));
    const int eventLength = endSample - startSample;
    if (eventLength <= 0)
        return;

    int fadeInSamples = juce::jlimit (0, eventLength, static_cast<int> (std::round (event.fadeInMs * 0.001 * currentSampleRate)));
    int fadeOutSamples = juce::jlimit (0, eventLength, static_cast<int> (std::round (event.fadeOutMs * 0.001 * currentSampleRate)));

    // On very short regions, scale both fades so they do not overlap.
    const int fadeTotal = fadeInSamples + fadeOutSamples;
    if (fadeTotal > eventLength && fadeTotal > 0)
    {
        const double scale = static_cast<double> (eventLength) / static_cast<double> (fadeTotal);
        fadeInSamples = static_cast<int> (std::floor (fadeInSamples * scale));
        fadeOutSamples = eventLength - fadeInSamples;
    }

    auto curveValue = [] (double x, VocalEvent::FadeCurve curve)
    {
        x = juce::jlimit (0.0, 1.0, x);
        return curve == VocalEvent::FadeCurve::Smooth ? x * x * (3.0 - 2.0 * x) : x;
    };

    for (int sample = startSample; sample < endSample; ++sample)
    {
        const int localSample = sample - startSample;
        const double normalized = eventLength > 1 ? static_cast<double> (localSample) / (eventLength - 1) : 0.0;
        const float envelopeDb = event.envelope.isEmpty()
            ? event.gainDb
            : event.getEnvelopeGainDb (static_cast<float> (normalized));
        const float gain = juce::Decibels::decibelsToGain (envelopeDb);

        double fadeGain = 1.0;
        if (fadeInSamples > 0 && localSample < fadeInSamples)
        {
            const double progress = static_cast<double> (localSample) / juce::jmax (1, fadeInSamples);
            fadeGain *= curveValue (progress, event.fadeInCurve);
        }
        if (fadeOutSamples > 0 && localSample >= eventLength - fadeOutSamples)
        {
            const double progress = static_cast<double> (eventLength - 1 - localSample) / juce::jmax (1, fadeOutSamples);
            fadeGain *= curveValue (progress, event.fadeOutCurve);
        }

        const float totalGain = gain * static_cast<float> (fadeGain);
        for (int channel = 0; channel < target.getNumChannels(); ++channel)
            target.setSample (channel, sample, target.getSample (channel, sample) * totalGain);
    }
}

void VocalEditorAudioProcessor::updateProcessedBuffer()
{
    // The message thread is the only writer. Never overwrite the published
    // buffer or the buffer currently being read by processBlock().
    const int active = activeProcessedBuffer.load (std::memory_order_acquire);
    const int reading = readingProcessedBuffer.load (std::memory_order_acquire);
    int targetIndex = -1;
    for (int i = 0; i < static_cast<int> (processedBuffers.size()); ++i)
        if (i != active && i != reading)
        {
            targetIndex = i;
            break;
        }

    if (targetIndex < 0)
        return;

    auto& target = processedBuffers[static_cast<size_t> (targetIndex)];
    if (audioBuffer.getNumSamples() <= 0 || audioBuffer.getNumChannels() <= 0)
    {
        target.setSize (0, 0);
        activeProcessedBuffer.store (targetIndex, std::memory_order_release);
        return;
    }

    target.setSize (audioBuffer.getNumChannels(), audioBuffer.getNumSamples(), false, false, true);
    target.makeCopyOf (audioBuffer, true);
    for (const auto* event : eventList.getAll())
        if (event != nullptr)
            applyEventToBuffer (*event, target);

    activeProcessedBuffer.store (targetIndex, std::memory_order_release);
}

VocalEvent* VocalEditorAudioProcessor::createEventFromSelection (VocalEvent::Type type)
{
    if (! hasAudio() || ! selection.isActive || selection.getLength() < 0.005)
        return nullptr;

    const double duration = getDuration();
    const double start = juce::jlimit (0.0, duration, selection.startTime);
    const double end = juce::jlimit (start, duration, selection.endTime);
    if (end - start < 0.005)
        return nullptr;

    auto* event = eventList.add (start, end, type);
    setSelectedEvent (event);
    updateProcessedBuffer();
    return event;
}

void VocalEditorAudioProcessor::setEventGain (const juce::Uuid& id, float gainDb)
{
    if (auto* event = eventList.getById (id))
    {
        event->gainDb = juce::jlimit (-96.0f, 24.0f, gainDb);
        if (! event->envelope.isEmpty())
            for (auto& point : event->envelope)
                point.gainDb = event->gainDb;
        updateProcessedBuffer();
    }
}

void VocalEditorAudioProcessor::setEventFades (const juce::Uuid& id, double fadeInMs, double fadeOutMs)
{
    if (auto* event = eventList.getById (id))
    {
        const double maxFadeMs = event->getLength() * 500.0;
        event->fadeInMs = juce::jlimit (0.0, maxFadeMs, fadeInMs);
        event->fadeOutMs = juce::jlimit (0.0, maxFadeMs, fadeOutMs);
        updateProcessedBuffer();
    }
}

void VocalEditorAudioProcessor::setSelectedEvent (VocalEvent* event)
{
    selectedEvent = event;
    selectedEventIds.clear();
    if (event != nullptr)
        selectedEventIds.add (event->id);
}

void VocalEditorAudioProcessor::deleteEvent (const juce::Uuid& id)
{
    if (selectedEvent != nullptr && selectedEvent->id == id)
        selectedEvent = nullptr;
    selectedEventIds.removeAllInstancesOf (id);
    eventList.removeById (id);
    updateProcessedBuffer();
}

void VocalEditorAudioProcessor::loadAudioFile (const juce::File& file)
{
    clearAudio();
    
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    
    if (reader != nullptr)
    {
        const int numSamples = static_cast<int> (reader->lengthInSamples);
        const int numChannels = static_cast<int> (reader->numChannels);
        
        audioBuffer.setSize (numChannels, numSamples);
        reader->read (&audioBuffer, 0, numSamples, 0, true, true);

        currentAudioFile = file;
        currentSampleRate = reader->sampleRate;
        playbackPosition = 0.0;
        playbackSamplePosition = 0;
        currentlyPlaying = false;

        clearSelection();
        updateProcessedBuffer();
    }
}

void VocalEditorAudioProcessor::clearAudio()
{
    audioBuffer.setSize (0, 0);
    playbackPosition = 0.0;
    playbackSamplePosition = 0;
    currentlyPlaying = false;
    clearSelection();
    eventList.clear();
    selectedEvent = nullptr;
    selectedEventIds.clear();
    currentAudioFile = {};
    updateProcessedBuffer();
}

void VocalEditorAudioProcessor::setPlaybackPosition (double positionInSeconds)
{
    playbackPosition = juce::jlimit (0.0, 
                                     hasAudio() ? static_cast<double> (audioBuffer.getNumSamples()) / currentSampleRate : 0.0,
                                     positionInSeconds);
    playbackSamplePosition = static_cast<int> (playbackPosition * currentSampleRate);
}

void VocalEditorAudioProcessor::startPlayback()
{
    if (hasAudio())
        currentlyPlaying = true;
}

void VocalEditorAudioProcessor::stopPlayback()
{
    currentlyPlaying = false;
}

void VocalEditorAudioProcessor::setSelection (double start, double end)
{
    if (end < start)
        std::swap (start, end);
    
    selection.startTime = juce::jmax (0.0, start);
    selection.endTime = end;
    selection.isActive = true;
}

void VocalEditorAudioProcessor::clearSelection()
{
    selection.isActive = false;
    selection.startTime = 0.0;
    selection.endTime = 0.0;
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalEditorAudioProcessor();
}
