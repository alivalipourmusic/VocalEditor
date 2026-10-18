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
    // Store non-destructive edits in the host session. Audio remains an external
    // source file so DAW project state does not balloon with long recordings.
    juce::ValueTree state ("VocalEditorState");
    state.setProperty ("version", 1, nullptr);
    state.setProperty ("audioFile", currentAudioFile.getFullPathName(), nullptr);
    state.setProperty ("deEsserEnabled", deEsserEnabled, nullptr);
    state.setProperty ("deEsserFrequencyHz", deEsserFrequencyHz, nullptr);
    state.setProperty ("deEsserThresholdDb", deEsserThresholdDb, nullptr);
    state.setProperty ("deEsserMaxReductionDb", deEsserMaxReductionDb, nullptr);
    state.setProperty ("dePlosiveEnabled", dePlosiveEnabled, nullptr);
    state.setProperty ("dePlosiveCutoffHz", dePlosiveCutoffHz, nullptr);
    state.setProperty ("dePlosiveThresholdDb", dePlosiveThresholdDb, nullptr);
    state.setProperty ("dePlosiveMaxReductionDb", dePlosiveMaxReductionDb, nullptr);
    state.setProperty ("noiseReductionEnabled", noiseReductionEnabled, nullptr);
    state.setProperty ("noiseReductionThresholdDb", noiseReductionThresholdDb, nullptr);
    state.setProperty ("noiseReductionAmountDb", noiseReductionAmountDb, nullptr);
    state.setProperty ("noiseReductionReleaseMs", noiseReductionReleaseMs, nullptr);
    state.setProperty ("roomTailReductionEnabled", roomTailReductionEnabled, nullptr);
    state.setProperty ("roomTailThresholdDb", roomTailThresholdDb, nullptr);
    state.setProperty ("roomTailMaxReductionDb", roomTailMaxReductionDb, nullptr);
    state.setProperty ("roomTailSensitivity", roomTailSensitivity, nullptr);
    state.setProperty ("spectralBalanceEnabled", spectralBalanceEnabled, nullptr);
    state.setProperty ("spectralLowDb", spectralLowDb, nullptr);
    state.setProperty ("spectralMidDb", spectralMidDb, nullptr);
    state.setProperty ("spectralHighDb", spectralHighDb, nullptr);

    for (const auto* event : eventList.getAll())
    {
        if (event == nullptr)
            continue;

        juce::ValueTree eventState ("Event");
        eventState.setProperty ("id", event->id.toString(), nullptr);
        eventState.setProperty ("type", static_cast<int> (event->type), nullptr);
        eventState.setProperty ("start", event->startTime, nullptr);
        eventState.setProperty ("end", event->endTime, nullptr);
        eventState.setProperty ("gainDb", event->gainDb, nullptr);
        eventState.setProperty ("fadeInMs", event->fadeInMs, nullptr);
        eventState.setProperty ("fadeOutMs", event->fadeOutMs, nullptr);
        eventState.setProperty ("fadeInCurve", static_cast<int> (event->fadeInCurve), nullptr);
        eventState.setProperty ("fadeOutCurve", static_cast<int> (event->fadeOutCurve), nullptr);
        eventState.setProperty ("locked", event->locked, nullptr);

        for (const auto& point : event->envelope)
        {
            juce::ValueTree pointState ("EnvelopePoint");
            pointState.setProperty ("t", point.t, nullptr);
            pointState.setProperty ("gainDb", point.gainDb, nullptr);
            eventState.addChild (pointState, -1, nullptr);
        }
        state.addChild (eventState, -1, nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VocalEditorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("VocalEditorState"))
        return;

    const auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid() || state.getType() != juce::Identifier ("VocalEditorState"))
        return;

    deEsserEnabled = static_cast<bool> (state.getProperty ("deEsserEnabled", false));
    deEsserFrequencyHz = juce::jlimit (2500.0f, 12000.0f, static_cast<float> (state.getProperty ("deEsserFrequencyHz", 6500.0f)));
    deEsserThresholdDb = juce::jlimit (-60.0f, -6.0f, static_cast<float> (state.getProperty ("deEsserThresholdDb", -28.0f)));
    deEsserMaxReductionDb = juce::jlimit (0.0f, 18.0f, static_cast<float> (state.getProperty ("deEsserMaxReductionDb", 6.0f)));

    dePlosiveEnabled = static_cast<bool> (state.getProperty ("dePlosiveEnabled", false));
    dePlosiveCutoffHz = juce::jlimit (60.0f, 300.0f, static_cast<float> (state.getProperty ("dePlosiveCutoffHz", 140.0f)));
    dePlosiveThresholdDb = juce::jlimit (-60.0f, -6.0f, static_cast<float> (state.getProperty ("dePlosiveThresholdDb", -24.0f)));
    dePlosiveMaxReductionDb = juce::jlimit (0.0f, 24.0f, static_cast<float> (state.getProperty ("dePlosiveMaxReductionDb", 9.0f)));

    noiseReductionEnabled = static_cast<bool> (state.getProperty ("noiseReductionEnabled", false));
    noiseReductionThresholdDb = juce::jlimit (-80.0f, -18.0f, static_cast<float> (state.getProperty ("noiseReductionThresholdDb", -48.0f)));
    noiseReductionAmountDb = juce::jlimit (0.0f, 36.0f, static_cast<float> (state.getProperty ("noiseReductionAmountDb", 18.0f)));
    noiseReductionReleaseMs = juce::jlimit (20.0f, 1000.0f, static_cast<float> (state.getProperty ("noiseReductionReleaseMs", 180.0f)));
    roomTailReductionEnabled = static_cast<bool> (state.getProperty ("roomTailReductionEnabled", false));
    roomTailThresholdDb = juce::jlimit (-60.0f, -12.0f, static_cast<float> (state.getProperty ("roomTailThresholdDb", -36.0f)));
    roomTailMaxReductionDb = juce::jlimit (0.0f, 24.0f, static_cast<float> (state.getProperty ("roomTailMaxReductionDb", 8.0f)));
    roomTailSensitivity = juce::jlimit (0.1f, 1.0f, static_cast<float> (state.getProperty ("roomTailSensitivity", 0.65f)));
    spectralBalanceEnabled = static_cast<bool> (state.getProperty ("spectralBalanceEnabled", false));
    spectralLowDb = juce::jlimit (-12.0f, 12.0f, static_cast<float> (state.getProperty ("spectralLowDb", 0.0f)));
    spectralMidDb = juce::jlimit (-12.0f, 12.0f, static_cast<float> (state.getProperty ("spectralMidDb", 0.0f)));
    spectralHighDb = juce::jlimit (-12.0f, 12.0f, static_cast<float> (state.getProperty ("spectralHighDb", 0.0f)));

    const auto audioPath = state.getProperty ("audioFile").toString();
    const juce::File audioFile (audioPath);

    // Avoid destroying the current session if the saved source has been moved.
    if (audioPath.isNotEmpty() && ! audioFile.existsAsFile())
        return;

    if (audioFile.existsAsFile())
        loadAudioFile (audioFile);
    else
        clearAudio();

    eventList.clear();
    selectedEvent = nullptr;
    selectedEventIds.clear();

    const double duration = getDuration();
    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        const auto eventState = state.getChild (i);
        if (! eventState.hasType ("Event"))
            continue;

        const int rawType = static_cast<int> (eventState.getProperty ("type", 4));
        const double rawStart = static_cast<double> (eventState.getProperty ("start", 0.0));
        const double rawEnd = static_cast<double> (eventState.getProperty ("end", 0.0));
        if (! std::isfinite (rawStart) || ! std::isfinite (rawEnd)
            || rawEnd <= rawStart || rawEnd <= 0.0 || rawStart >= duration)
            continue;

        const double start = juce::jlimit (0.0, duration, rawStart);
        const double end = juce::jlimit (start, duration, rawEnd);
        if (end - start < 0.001)
            continue;

        const auto type = static_cast<VocalEvent::Type> (juce::jlimit (0, 4, rawType));
        auto* event = eventList.add (start, end, type);
        const auto storedId = eventState.getProperty ("id").toString();
        if (storedId.isNotEmpty())
            event->id = juce::Uuid (storedId);

        const double rawGain = static_cast<double> (eventState.getProperty ("gainDb", 0.0));
        const double rawFadeIn = static_cast<double> (eventState.getProperty ("fadeInMs", 0.0));
        const double rawFadeOut = static_cast<double> (eventState.getProperty ("fadeOutMs", 0.0));
        event->gainDb = static_cast<float> (std::isfinite (rawGain)
            ? juce::jlimit (-96.0, 24.0, rawGain) : 0.0);
        event->fadeInMs = std::isfinite (rawFadeIn)
            ? juce::jlimit (0.0, event->getLength() * 500.0, rawFadeIn) : 0.0;
        event->fadeOutMs = std::isfinite (rawFadeOut)
            ? juce::jlimit (0.0, event->getLength() * 500.0, rawFadeOut) : 0.0;
        event->fadeInCurve = static_cast<VocalEvent::FadeCurve> (
            juce::jlimit (0, 1, static_cast<int> (eventState.getProperty ("fadeInCurve", 1))));
        event->fadeOutCurve = static_cast<VocalEvent::FadeCurve> (
            juce::jlimit (0, 1, static_cast<int> (eventState.getProperty ("fadeOutCurve", 1))));
        event->locked = static_cast<bool> (eventState.getProperty ("locked", false));

        for (int child = 0; child < eventState.getNumChildren(); ++child)
        {
            const auto pointState = eventState.getChild (child);
            if (! pointState.hasType ("EnvelopePoint"))
                continue;

            const double rawT = static_cast<double> (pointState.getProperty ("t", 0.0));
            const double rawPointGain = static_cast<double> (pointState.getProperty ("gainDb", event->gainDb));
            if (! std::isfinite (rawT) || ! std::isfinite (rawPointGain))
                continue;

            event->envelope.add ({
                static_cast<float> (juce::jlimit (0.0, 1.0, rawT)),
                static_cast<float> (juce::jlimit (-120.0, 24.0, rawPointGain))
            });
        }

        std::sort (event->envelope.begin(), event->envelope.end(),
            [] (const VocalEvent::EnvelopePoint& a, const VocalEvent::EnvelopePoint& b)
            {
                return a.t < b.t;
            });
    }

    if (eventList.size() > 0)
        setSelectedEvent (eventList.get (0));

    updateProcessedBuffer();
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

void VocalEditorAudioProcessor::setDeEsserEnabled (bool enabled)
{
    if (deEsserEnabled == enabled)
        return;
    deEsserEnabled = enabled;
    updateProcessedBuffer();
}

void VocalEditorAudioProcessor::setDeEsserParameters (float frequencyHz, float thresholdDb, float maxReductionDb)
{
    deEsserFrequencyHz = juce::jlimit (2500.0f, 12000.0f, frequencyHz);
    deEsserThresholdDb = juce::jlimit (-60.0f, -6.0f, thresholdDb);
    deEsserMaxReductionDb = juce::jlimit (0.0f, 18.0f, maxReductionDb);
    updateProcessedBuffer();
}

void VocalEditorAudioProcessor::applyDeEsserToBuffer (juce::AudioBuffer<float>& buffer)
{
    if (! deEsserEnabled || buffer.getNumSamples() == 0 || currentSampleRate <= 0.0)
        return;

    // Split-band detector. This is deterministic DSP, not a trained AI model.
    const double sr = currentSampleRate;
    const double splitHz = juce::jlimit (1500.0, sr * 0.45, static_cast<double> (deEsserFrequencyHz) * 0.72);
    const float lowAlpha = static_cast<float> (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * splitHz / sr));
    const float attack = static_cast<float> (1.0 - std::exp (-1.0 / (sr * 0.0015)));
    const float release = static_cast<float> (1.0 - std::exp (-1.0 / (sr * 0.045)));
    const float threshold = juce::Decibels::decibelsToGain (deEsserThresholdDb);
    const float maxReduction = juce::Decibels::decibelsToGain (-deEsserMaxReductionDb);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float low = 0.0f;
        float detector = 0.0f;
        auto* samples = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float input = samples[i];
            low += lowAlpha * (input - low);
            const float high = input - low;
            const float magnitude = std::abs (high);
            const float coeff = magnitude > detector ? attack : release;
            detector += coeff * (magnitude - detector);

            float bandGain = 1.0f;
            if (detector > threshold && deEsserMaxReductionDb > 0.0f)
            {
                const float over = juce::jlimit (0.0f, 1.0f, (detector - threshold) / juce::jmax (threshold, 1.0e-5f));
                bandGain = 1.0f + (maxReduction - 1.0f) * over;
            }
            samples[i] = low + high * bandGain;
        }
    }
}

void VocalEditorAudioProcessor::setDePlosiveEnabled (bool enabled)
{
    if (dePlosiveEnabled == enabled)
        return;
    dePlosiveEnabled = enabled;
    updateProcessedBuffer();
}

void VocalEditorAudioProcessor::setDePlosiveParameters (float cutoffHz, float thresholdDb, float maxReductionDb)
{
    dePlosiveCutoffHz = juce::jlimit (60.0f, 300.0f, cutoffHz);
    dePlosiveThresholdDb = juce::jlimit (-60.0f, -6.0f, thresholdDb);
    dePlosiveMaxReductionDb = juce::jlimit (0.0f, 24.0f, maxReductionDb);
    updateProcessedBuffer();
}

void VocalEditorAudioProcessor::applyDePlosiveToBuffer (juce::AudioBuffer<float>& buffer)
{
    if (! dePlosiveEnabled || buffer.getNumSamples() == 0 || currentSampleRate <= 0.0)
        return;

    // High-pass/low-band attenuation aimed at close-mic plosive bursts.
    // The detector is smoothed and attenuation is capped; this is conventional
    // DSP, not speech recognition or a trained AI model.
    const double sr = currentSampleRate;
    const float alpha = static_cast<float> (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi
                                                            * dePlosiveCutoffHz / sr));
    const float attack = static_cast<float> (1.0 - std::exp (-1.0 / (sr * 0.002)));
    const float release = static_cast<float> (1.0 - std::exp (-1.0 / (sr * 0.070)));
    const float threshold = juce::Decibels::decibelsToGain (dePlosiveThresholdDb);
    const float minGain = juce::Decibels::decibelsToGain (-dePlosiveMaxReductionDb);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float low = 0.0f;
        float detector = 0.0f;
        auto* samples = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float input = samples[i];
            low += alpha * (input - low);
            const float magnitude = std::abs (low);
            detector += (magnitude > detector ? attack : release) * (magnitude - detector);

            float lowGain = 1.0f;
            if (detector > threshold && dePlosiveMaxReductionDb > 0.0f)
            {
                const float over = juce::jlimit (0.0f, 1.0f,
                    (detector - threshold) / juce::jmax (threshold, 1.0e-5f));
                lowGain = 1.0f + (minGain - 1.0f) * over;
            }
            samples[i] = input - low * (1.0f - lowGain);
        }
    }
}

void VocalEditorAudioProcessor::setNoiseReductionEnabled (bool enabled)
{
    if (noiseReductionEnabled == enabled)
        return;
    noiseReductionEnabled = enabled;
    updateProcessedBuffer();
}

void VocalEditorAudioProcessor::setNoiseReductionParameters (float thresholdDb, float reductionDb, float releaseMs)
{
    noiseReductionThresholdDb = juce::jlimit (-80.0f, -18.0f, thresholdDb);
    noiseReductionAmountDb = juce::jlimit (0.0f, 36.0f, reductionDb);
    noiseReductionReleaseMs = juce::jlimit (20.0f, 1000.0f, releaseMs);
    updateProcessedBuffer();
}

void VocalEditorAudioProcessor::applyNoiseReductionToBuffer (juce::AudioBuffer<float>& buffer)
{
    if (! noiseReductionEnabled || buffer.getNumSamples() == 0 || currentSampleRate <= 0.0)
        return;

    // Adaptive downward expander for low-level passages. This is useful for
    // pauses and room noise between phrases, not spectral denoising under speech.
    const double sr = currentSampleRate;
    const float threshold = juce::Decibels::decibelsToGain (noiseReductionThresholdDb);
    const float floorGain = juce::Decibels::decibelsToGain (-noiseReductionAmountDb);
    const float attack = static_cast<float> (1.0 - std::exp (-1.0 / (sr * 0.005)));
    const float release = static_cast<float> (1.0 - std::exp (-1.0 / (sr * noiseReductionReleaseMs * 0.001)));
    const float ratio = 2.5f;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float envelope = 0.0f;
        float gain = 1.0f;
        auto* samples = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float input = samples[i];
            const float magnitude = std::abs (input);
            envelope += (magnitude > envelope ? attack : release) * (magnitude - envelope);

            float targetGain = 1.0f;
            if (envelope < threshold && noiseReductionAmountDb > 0.0f)
            {
                const float level = envelope / juce::jmax (threshold, 1.0e-7f);
                const float attenuation = juce::jlimit (0.0f, 1.0f, (1.0f - level) * ratio);
                targetGain = 1.0f - attenuation * (1.0f - floorGain);
            }

            // Smooth gain changes to avoid abrupt gating clicks.
            gain += (targetGain > gain ? attack : release) * (targetGain - gain);
            samples[i] = input * gain;
        }
    }
}

void VocalEditorAudioProcessor::setRoomTailReductionEnabled (bool enabled)
{
    if (roomTailReductionEnabled == enabled)
        return;
    roomTailReductionEnabled = enabled;
    updateProcessedBuffer();
}

void VocalEditorAudioProcessor::setRoomTailReductionParameters (float thresholdDb, float maxReductionDb, float tailSensitivity)
{
    roomTailThresholdDb = juce::jlimit (-60.0f, -12.0f, thresholdDb);
    roomTailMaxReductionDb = juce::jlimit (0.0f, 24.0f, maxReductionDb);
    roomTailSensitivity = juce::jlimit (0.1f, 1.0f, tailSensitivity);
    updateProcessedBuffer();
}

void VocalEditorAudioProcessor::applyRoomTailReductionToBuffer (juce::AudioBuffer<float>& buffer)
{
    if (! roomTailReductionEnabled || buffer.getNumSamples() == 0 || currentSampleRate <= 0.0)
        return;

    // Conservative decay-aware attenuation: identify low-level tails following
    // louder material, then smoothly attenuate only those tails. This does not
    // estimate or deconvolve a room impulse response.
    const double sr = currentSampleRate;
    const float threshold = juce::Decibels::decibelsToGain (roomTailThresholdDb);
    const float maxAttenuation = juce::Decibels::decibelsToGain (-roomTailMaxReductionDb);
    const float fast = static_cast<float> (1.0 - std::exp (-1.0 / (sr * 0.004)));
    const float slow = static_cast<float> (1.0 - std::exp (-1.0 / (sr * (0.12 + 0.55 * roomTailSensitivity))));
    const float release = static_cast<float> (1.0 - std::exp (-1.0 / (sr * (0.08 + 0.35 * roomTailSensitivity))));

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float envelope = 0.0f;
        float recentPeak = 0.0f;
        float gain = 1.0f;
        auto* samples = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float input = samples[i];
            const float magnitude = std::abs (input);
            envelope += (magnitude > envelope ? fast : slow) * (magnitude - envelope);
            recentPeak = juce::jmax (magnitude, recentPeak * 0.9995f);

            float targetGain = 1.0f;
            const float tailThreshold = threshold * (0.5f + roomTailSensitivity);
            if (recentPeak > threshold * 2.0f && envelope < tailThreshold && roomTailMaxReductionDb > 0.0f)
            {
                const float tailRatio = juce::jlimit (0.0f, 1.0f,
                    (tailThreshold - envelope) / juce::jmax (tailThreshold, 1.0e-7f));
                targetGain = 1.0f - tailRatio * (1.0f - maxAttenuation);
            }

            gain += (targetGain < gain ? fast : release) * (targetGain - gain);
            samples[i] = input * gain;
        }
    }
}

void VocalEditorAudioProcessor::setSpectralBalanceEnabled (bool enabled)
{
    if (spectralBalanceEnabled == enabled)
        return;
    spectralBalanceEnabled = enabled;
    updateProcessedBuffer();
}

void VocalEditorAudioProcessor::setSpectralBalanceParameters (float lowDb, float midDb, float highDb)
{
    spectralLowDb = juce::jlimit (-12.0f, 12.0f, lowDb);
    spectralMidDb = juce::jlimit (-12.0f, 12.0f, midDb);
    spectralHighDb = juce::jlimit (-12.0f, 12.0f, highDb);
    updateProcessedBuffer();
}

void VocalEditorAudioProcessor::applySpectralBalanceToBuffer (juce::AudioBuffer<float>& buffer)
{
    if (! spectralBalanceEnabled || buffer.getNumSamples() == 0 || currentSampleRate <= 0.0
        || (std::abs (spectralLowDb) < 0.001f && std::abs (spectralMidDb) < 0.001f
            && std::abs (spectralHighDb) < 0.001f))
        return;

    // Three broad, first-order shelves/tilt bands. This is a practical tone
    // control, not AI mastering or automatic spectral correction.
    const float lowGain = juce::Decibels::decibelsToGain (spectralLowDb);
    const float midGain = juce::Decibels::decibelsToGain (spectralMidDb);
    const float highGain = juce::Decibels::decibelsToGain (spectralHighDb);
    const double sr = currentSampleRate;
    const float lowAlpha = static_cast<float> (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 180.0 / sr));
    const float highAlpha = static_cast<float> (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 4500.0 / sr));

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        float lowState = 0.0f;
        float highLowState = 0.0f;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float input = data[i];
            lowState += lowAlpha * (input - lowState);
            const float low = lowState;
            const float highPass = input - highLowState;
            highLowState += highAlpha * (input - highLowState);
            const float high = highPass;
            const float mid = input - low - high;
            data[i] = low * lowGain + mid * midGain + high * highGain;
        }
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

    // Cleanup is applied after event edits so it affects the actual rendered
    // audio rather than being overwritten by event-level sample replacement.
    applyDeEsserToBuffer (target);
    applyDePlosiveToBuffer (target);
    applyNoiseReductionToBuffer (target);
    applyRoomTailReductionToBuffer (target);
    applySpectralBalanceToBuffer (target);

    activeProcessedBuffer.store (targetIndex, std::memory_order_release);
}

int VocalEditorAudioProcessor::autoSegmentFromSilence (const SegmentParams& params)
{
    if (! hasAudio() || currentSampleRate <= 0.0)
        return 0;

    const int totalSamples = audioBuffer.getNumSamples();
    const int frameSamples = juce::jmax (1, static_cast<int> (std::round (currentSampleRate * 0.010)));
    const int numFrames = (totalSamples + frameSamples - 1) / frameSamples;
    const float threshold = juce::Decibels::decibelsToGain (params.silenceThresholdDb);
    const int minSilenceFrames = juce::jmax (1, static_cast<int> (
        std::ceil (params.minSilenceMs / 10.0)));
    const int minEventFrames = juce::jmax (1, static_cast<int> (
        std::ceil (params.minEventMs / 10.0)));
    const int padSamples = juce::jmax (0, static_cast<int> (
        std::round (params.padMs * 0.001 * currentSampleRate)));

    auto frameIsActive = [&] (int frame)
    {
        const int start = frame * frameSamples;
        const int count = juce::jmin (frameSamples, totalSamples - start);
        if (count <= 0)
            return false;

        double sumSquares = 0.0;
        int sampleCount = 0;
        for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
        {
            const float* data = audioBuffer.getReadPointer (ch, start);
            for (int i = 0; i < count; ++i)
            {
                const double sample = data[i];
                sumSquares += sample * sample;
            }
            sampleCount += count;
        }

        const float rms = sampleCount > 0
            ? static_cast<float> (std::sqrt (sumSquares / sampleCount)) : 0.0f;
        return rms >= threshold;
    };

    if (params.clearExisting)
    {
        eventList.clear();
        selectedEvent = nullptr;
        selectedEventIds.clear();
    }

    int created = 0;
    int activeStartFrame = -1;
    int lastActiveEndFrame = 0;
    int lastCreatedEndSample = 0;

    auto addDetectedEvent = [&] (int startFrame, int endFrame)
    {
        // Padding protects consonants and breath tails, but neighbouring events
        // must not overlap or their gain envelopes would multiply.
        int startSample = juce::jmax (lastCreatedEndSample,
                                      juce::jmax (0, startFrame * frameSamples - padSamples));
        int endSample = juce::jmin (totalSamples, endFrame * frameSamples + padSamples);
        if (endSample - startSample < minEventFrames * frameSamples)
            return;

        const double startTime = static_cast<double> (startSample) / currentSampleRate;
        const double endTime = static_cast<double> (endSample) / currentSampleRate;
        if (endTime - startTime < params.minEventMs * 0.001)
            return;

        eventList.add (startTime, endTime, params.eventType);
        lastCreatedEndSample = endSample;
        ++created;
    };

    for (int frame = 0; frame < numFrames; ++frame)
    {
        if (frameIsActive (frame))
        {
            if (activeStartFrame < 0)
                activeStartFrame = frame;
            lastActiveEndFrame = frame + 1;
        }
        else if (activeStartFrame >= 0
                 && (frame + 1 - lastActiveEndFrame) >= minSilenceFrames)
        {
            addDetectedEvent (activeStartFrame, lastActiveEndFrame);
            activeStartFrame = -1;
        }
    }

    if (activeStartFrame >= 0)
        addDetectedEvent (activeStartFrame, lastActiveEndFrame);

    if (created > 0)
        setSelectedEvent (eventList.get (eventList.size() - created));
    updateProcessedBuffer();
    return created;
}

int VocalEditorAudioProcessor::levelEventsToTarget (float targetDb, bool selectedOnly)
{
    if (! hasAudio() || currentSampleRate <= 0.0)
        return 0;

    juce::Array<VocalEvent*> targets;
    if (selectedOnly)
    {
        for (const auto& id : selectedEventIds)
            if (auto* event = eventList.getById (id))
                targets.addIfNotAlreadyThere (event);
    }
    else
    {
        for (auto* event : eventList.getAll())
            if (event != nullptr)
                targets.add (event);
    }

    // Level Riding: analyse short RMS windows and build a smoothed gain
    // envelope for each event. This is conventional DSP automation, not AI.
    constexpr double windowMs = 60.0;
    constexpr float maxBoostDb = 6.0f;
    constexpr float maxCutDb = -12.0f;
    int processed = 0;

    for (auto* event : targets)
    {
        if (event == nullptr || event->locked)
            continue;

        const int start = juce::jlimit (0, audioBuffer.getNumSamples(),
            static_cast<int> (std::floor (event->startTime * currentSampleRate)));
        const int end = juce::jlimit (start, audioBuffer.getNumSamples(),
            static_cast<int> (std::ceil (event->endTime * currentSampleRate)));
        if (end - start < 16)
            continue;

        const int windowSamples = juce::jmax (16, static_cast<int> (std::round (currentSampleRate * windowMs * 0.001)));
        juce::Array<float> positions;
        juce::Array<float> correctionsDb;

        for (int windowStart = start; windowStart < end; windowStart += windowSamples)
        {
            const int windowEnd = juce::jmin (end, windowStart + windowSamples);
            double sumSquares = 0.0;
            int sampleCount = 0;
            for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
            {
                const float* data = audioBuffer.getReadPointer (ch, windowStart);
                for (int i = 0; i < windowEnd - windowStart; ++i)
                {
                    const double sample = data[i];
                    sumSquares += sample * sample;
                }
                sampleCount += windowEnd - windowStart;
            }

            if (sampleCount <= 0)
                continue;

            const double rms = std::sqrt (sumSquares / sampleCount);
            const float t = static_cast<float> (static_cast<double> (windowStart + windowEnd) * 0.5 - start)
                          / static_cast<float> (end - start);
            const float existingGainDb = event->getEnvelopeGainDb (t);
            const float measuredDb = juce::Decibels::gainToDecibels (static_cast<float> (rms), -120.0f)
                                   + existingGainDb;
            // Ignore near-silence so breaths and gaps are not boosted into noise.
            if (rms < juce::Decibels::decibelsToGain (-65.0f))
            {
                positions.add (t);
                correctionsDb.add (0.0f);
                continue;
            }

            positions.add (t);
            correctionsDb.add (juce::jlimit (maxCutDb, maxBoostDb, targetDb - measuredDb));
        }

        if (positions.size() < 2)
            continue;

        // Three-point smoothing limits rapid gain swings between adjacent windows.
        juce::Array<float> smoothed;
        for (int i = 0; i < correctionsDb.size(); ++i)
        {
            float sum = correctionsDb[i];
            int count = 1;
            if (i > 0) { sum += correctionsDb[i - 1]; ++count; }
            if (i + 1 < correctionsDb.size()) { sum += correctionsDb[i + 1]; ++count; }
            smoothed.add (sum / static_cast<float> (count));
        }

        juce::Array<VocalEvent::EnvelopePoint> newEnvelope;
        newEnvelope.add ({ 0.0f, juce::jlimit (-120.0f, 24.0f,
            event->getEnvelopeGainDb (0.0f) + smoothed.getFirst()) });
        for (int i = 0; i < positions.size(); ++i)
        {
            const float t = positions[i];
            newEnvelope.add ({ t, juce::jlimit (-120.0f, 24.0f,
                event->getEnvelopeGainDb (t) + smoothed[i]) });
        }
        newEnvelope.add ({ 1.0f, juce::jlimit (-120.0f, 24.0f,
            event->getEnvelopeGainDb (1.0f) + smoothed.getLast()) });

        std::sort (newEnvelope.begin(), newEnvelope.end(),
            [] (const VocalEvent::EnvelopePoint& a, const VocalEvent::EnvelopePoint& b)
            {
                return a.t < b.t;
            });
        event->envelope.clear();
        for (const auto& point : newEnvelope)
        {
            if (! event->envelope.isEmpty()
                && std::abs (event->envelope.getLast().t - point.t) < 0.001f)
                event->envelope.getReference (event->envelope.size() - 1).gainDb = point.gainDb;
            else
                event->envelope.add (point);
        }
        ++processed;
    }

    if (processed > 0)
        updateProcessedBuffer();
    return processed;
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
        const float newGainDb = juce::jlimit (-96.0f, 24.0f, gainDb);
        const float deltaDb = newGainDb - event->gainDb;
        event->gainDb = newGainDb;

        // Preserve any user-drawn envelope shape by shifting every point by
        // the same gain delta instead of flattening the envelope.
        for (auto& point : event->envelope)
            point.gainDb = juce::jlimit (-120.0f, 24.0f, point.gainDb + deltaDb);

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

void VocalEditorAudioProcessor::setEventEnvelopePoint (const juce::Uuid& id, int pointIndex, float t, float gainDb)
{
    if (auto* event = eventList.getById (id))
    {
        if (event->locked || pointIndex < 0 || pointIndex >= event->envelope.size())
            return;

        auto& point = event->envelope.getReference (pointIndex);
        point.t = juce::jlimit (0.0f, 1.0f, t);
        point.gainDb = juce::jlimit (-60.0f, 12.0f, gainDb);
        std::sort (event->envelope.begin(), event->envelope.end(),
            [] (const VocalEvent::EnvelopePoint& a, const VocalEvent::EnvelopePoint& b)
            {
                return a.t < b.t;
            });
        updateProcessedBuffer();
    }
}

void VocalEditorAudioProcessor::removeEventEnvelopePoint (const juce::Uuid& id, int pointIndex)
{
    if (auto* event = eventList.getById (id))
    {
        if (event->locked)
            return;

        event->removeEnvelopePoint (pointIndex);
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

void VocalEditorAudioProcessor::splitEvent (const juce::Uuid& id, double splitTime)
{
    auto* event = eventList.getById (id);
    if (event == nullptr || event->locked)
        return;

    const double split = juce::jlimit (event->startTime, event->endTime, splitTime);
    if (split - event->startTime < 0.005 || event->endTime - split < 0.005)
        return;

    const double originalEnd = event->endTime;
    const double originalFadeOut = event->fadeOutMs;
    const auto originalType = event->type;
    const auto originalFadeOutCurve = event->fadeOutCurve;
    const float originalGain = event->gainDb;

    // Keep the original event as the left-hand segment. The original fade-in
    // remains at the beginning; the original fade-out moves to the right segment.
    event->endTime = split;
    event->fadeOutMs = 0.0;

    auto* right = eventList.add (split, originalEnd, originalType);
    right->gainDb = originalGain;
    right->fadeInMs = 0.0;
    right->fadeOutMs = juce::jmin (originalFadeOut, right->getLength() * 500.0);
    right->fadeOutCurve = originalFadeOutCurve;
    right->fadeInCurve = event->fadeInCurve;
    right->locked = false;

    setSelectedEvent (event);
    updateProcessedBuffer();
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
