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

    // Phase 1: Simple playback of loaded audio (for Standalone mainly)
    if (currentlyPlaying && hasAudio())
    {
        const int numSamplesToPlay = buffer.getNumSamples();
        const int remainingSamples = audioBuffer.getNumSamples() - playbackSamplePosition;
        
        if (remainingSamples <= 0)
        {
            currentlyPlaying = false;
            playbackPosition = 0.0;
            playbackSamplePosition = 0;
            buffer.clear();
            return;
        }
        
        const int samplesToCopy = juce::jmin (numSamplesToPlay, remainingSamples);
        const int numChannels = juce::jmin (buffer.getNumChannels(), audioBuffer.getNumChannels());
        
        for (int ch = 0; ch < numChannels; ++ch)
        {
            buffer.copyFrom (ch, 0, audioBuffer, ch, playbackSamplePosition, samplesToCopy);
        }
        
        // Clear remaining if any
        if (samplesToCopy < numSamplesToPlay)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.clear (ch, samplesToCopy, numSamplesToPlay - samplesToCopy);
            
            currentlyPlaying = false;
        }
        
        playbackSamplePosition += samplesToCopy;
        playbackPosition = static_cast<double> (playbackSamplePosition) / currentSampleRate;
    }
    else
    {
        // Pass-through or silence when not playing
        // For VST3 we typically pass through input
        // For now keep buffer as is (input)
    }
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
        
        currentSampleRate = reader->sampleRate;
        playbackPosition = 0.0;
        playbackSamplePosition = 0;
        currentlyPlaying = false;
        
        clearSelection();
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

VocalEvent* VocalEditorAudioProcessor::createEventFromSelection (VocalEvent::Type type)
{
    if (! selection.isActive || selection.getLength() < 0.005 || ! hasAudio())
        return nullptr;
    auto* event = eventList.add (selection.startTime, selection.endTime, type);
    setSelectedEvent (event);
    return event;
}

void VocalEditorAudioProcessor::setSelectedEvent (VocalEvent* event)
{
    selectedEvent = event;
    selectedEventIds.clear();
    if (event != nullptr)
        selectedEventIds.addIfNotAlreadyThere (event->id);
}

void VocalEditorAudioProcessor::setEventGain (const juce::Uuid& id, float gainDb)
{
    if (auto* event = eventList.getById (id))
        event->gainDb = juce::jlimit (-60.0f, 24.0f, gainDb);
}

void VocalEditorAudioProcessor::setEventFades (const juce::Uuid& id, double fadeInMs, double fadeOutMs)
{
    if (auto* event = eventList.getById (id))
    {
        const double maxFadeMs = event->getLength() * 500.0;
        event->fadeInMs = juce::jlimit (0.0, maxFadeMs, fadeInMs);
        event->fadeOutMs = juce::jlimit (0.0, maxFadeMs, fadeOutMs);
    }
}

void VocalEditorAudioProcessor::deleteEvent (const juce::Uuid& id)
{
    if (selectedEvent != nullptr && selectedEvent->id == id)
        selectedEvent = nullptr;
    selectedEventIds.removeAllInstancesOf (id);
    eventList.removeById (id);
}

void VocalEditorAudioProcessor::splitEvent (const juce::Uuid& id, double splitTime)
{
    auto* event = eventList.getById (id);
    if (event == nullptr || splitTime <= event->startTime || splitTime >= event->endTime)
        return;

    const double oldEnd = event->endTime;
    const auto oldType = event->type;
    const float oldGain = event->gainDb;
    const double oldFadeOut = event->fadeOutMs;
    event->endTime = splitTime;
    event->fadeOutMs = 0.0;

    auto* second = eventList.add (splitTime, oldEnd, oldType);
    second->gainDb = oldGain;
    second->fadeOutMs = oldFadeOut;
    setSelectedEvent (second);
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalEditorAudioProcessor();
}
