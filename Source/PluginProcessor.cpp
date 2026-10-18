#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "AudioResampling.h"
#include "AudioProcessingMath.h"

#include <cmath>
#include <limits>
#include <new>
#include <vector>

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
    hostOutputSampleRate.store (sampleRate > 0.0 ? sampleRate : 44100.0, std::memory_order_release);
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

    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    for (int ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (! currentlyPlaying.load (std::memory_order_acquire) || ! hasAudio())
        return;

    // A file/session may have changed while a new render snapshot is pending.
    // Never play the previous file's snapshot against the new session metadata.
    if (processedBufferUpdatePending.load (std::memory_order_acquire))
    {
        buffer.clear();
        return;
    }

    int sourceIndex = 0;
    for (;;)
    {
        sourceIndex = activeProcessedBuffer.load (std::memory_order_acquire);
        processedBufferReaders[static_cast<size_t> (sourceIndex)].fetch_add (1, std::memory_order_acq_rel);
        if (sourceIndex == activeProcessedBuffer.load (std::memory_order_acquire))
            break;

        processedBufferReaders[static_cast<size_t> (sourceIndex)].fetch_sub (1, std::memory_order_release);
    }

    const auto& source = processedBuffers[static_cast<size_t> (sourceIndex)];
    const int sourceLength = source.getNumSamples();
    const int sourceChannels = source.getNumChannels();
    const int outputSamples = buffer.getNumSamples();
    const int outputChannels = buffer.getNumChannels();
    // Read metadata only after pinning and validating the immutable snapshot.
    const double sourceRate = processedBufferSampleRates[static_cast<size_t> (sourceIndex)];
    const double outputRate = hostOutputSampleRate.load (std::memory_order_acquire);
    const double startSeconds = playbackPosition.load (std::memory_order_acquire);

    if (sourceLength <= 0 || sourceChannels <= 0 || sourceRate <= 0.0 || outputRate <= 0.0)
    {
        currentlyPlaying.store (false, std::memory_order_release);
        buffer.clear();
        processedBufferReaders[static_cast<size_t> (sourceIndex)].fetch_sub (1, std::memory_order_release);
        return;
    }

    const double durationSeconds = static_cast<double> (sourceLength) / sourceRate;
    const double secondsPerOutputSample = 1.0 / outputRate;
    int rendered = 0;

    for (int i = 0; i < outputSamples; ++i)
    {
        const double sourcePosition = VocalAudioResampler::sourcePositionForOutputSample (
            startSeconds, i, sourceRate, outputRate);
        if (sourcePosition >= static_cast<double> (sourceLength))
            break;

        for (int outCh = 0; outCh < outputChannels; ++outCh)
        {
            float sample = 0.0f;
            if (sourceChannels == 1)
                sample = VocalAudioResampler::interpolateLinear (source, 0, sourcePosition);
            else if (outputChannels == 1)
            {
                for (int inCh = 0; inCh < sourceChannels; ++inCh)
                    sample += VocalAudioResampler::interpolateLinear (source, inCh, sourcePosition);
                sample /= static_cast<float> (sourceChannels);
            }
            else
                sample = VocalAudioResampler::interpolateLinear (
                    source, juce::jmin (outCh, sourceChannels - 1), sourcePosition);

            buffer.setSample (outCh, i, sample);
        }
        ++rendered;
    }

    if (rendered < outputSamples)
        for (int ch = 0; ch < outputChannels; ++ch)
            buffer.clear (ch, rendered, outputSamples - rendered);

    const double nextPosition = startSeconds + static_cast<double> (rendered) * secondsPerOutputSample;
    if (rendered < outputSamples || nextPosition >= durationSeconds)
    {
        currentlyPlaying.store (false, std::memory_order_release);
        playbackPosition.store (0.0, std::memory_order_release);
        playbackSamplePosition.store (0, std::memory_order_release);
    }
    else
    {
        playbackPosition.store (nextPosition, std::memory_order_release);
        playbackSamplePosition.store (
            VocalAudioMath::timeToSampleIndex (nextPosition, sourceRate, sourceLength),
            std::memory_order_release);
    }

    processedBufferReaders[static_cast<size_t> (sourceIndex)].fetch_sub (1, std::memory_order_release);
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
std::unique_ptr<juce::XmlElement> VocalEditorAudioProcessor::createStateXml() const
{
    auto xml = std::make_unique<juce::XmlElement> ("VocalEditorState");
    xml->setAttribute ("version", 1);
    xml->setAttribute ("audioPath", currentAudioFile.getFullPathName());
    xml->setAttribute ("playbackPosition", playbackPosition);
    xml->setAttribute ("selectionActive", selection.isActive);
    xml->setAttribute ("selectionStart", selection.startTime);
    xml->setAttribute ("selectionEnd", selection.endTime);

    auto* events = xml->createNewChildElement ("Events");
    for (const auto* event : eventList.getAll())
    {
        if (event == nullptr)
            continue;

        auto* item = events->createNewChildElement ("Event");
        item->setAttribute ("id", event->id.toString());
        item->setAttribute ("type", static_cast<int> (event->type));
        item->setAttribute ("start", event->startTime);
        item->setAttribute ("end", event->endTime);
        item->setAttribute ("gainDb", event->gainDb);
        item->setAttribute ("fadeInMs", event->fadeInMs);
        item->setAttribute ("fadeOutMs", event->fadeOutMs);
        item->setAttribute ("fadeInCurve", static_cast<int> (event->fadeInCurve));
        item->setAttribute ("fadeOutCurve", static_cast<int> (event->fadeOutCurve));
        item->setAttribute ("locked", event->locked);

        auto* envelope = item->createNewChildElement ("Envelope");
        for (const auto& point : event->envelope)
        {
            auto* pointXml = envelope->createNewChildElement ("Point");
            pointXml->setAttribute ("t", point.t);
            pointXml->setAttribute ("gainDb", point.gainDb);
        }
    }

    auto* selected = xml->createNewChildElement ("SelectedEvents");
    for (const auto& id : selectedEventIds)
    {
        auto* item = selected->createNewChildElement ("Id");
        item->setAttribute ("value", id.toString());
    }

    return xml;
}

bool VocalEditorAudioProcessor::restoreStateFromXml (const juce::XmlElement& xml,
                                                       juce::String& errorMessage)
{
    if (! xml.hasTagName ("VocalEditorState"))
    {
        errorMessage = "Not a Vocal Editor project/state file.";
        return false;
    }

    const int version = xml.getIntAttribute ("version", 0);
    if (version != 1)
    {
        errorMessage = "Unsupported Vocal Editor state version.";
        return false;
    }

    const double stagedPlaybackPosition = xml.getDoubleAttribute ("playbackPosition", 0.0);
    const double stagedSelectionStart = xml.getDoubleAttribute ("selectionStart", 0.0);
    const double stagedSelectionEnd = xml.getDoubleAttribute ("selectionEnd", 0.0);
    if (! std::isfinite (stagedPlaybackPosition) || stagedPlaybackPosition < 0.0
        || ! std::isfinite (stagedSelectionStart) || stagedSelectionStart < 0.0
        || ! std::isfinite (stagedSelectionEnd) || stagedSelectionEnd < stagedSelectionStart)
    {
        errorMessage = "The project contains invalid playback or selection positions.";
        return false;
    }

    struct EventData
    {
        juce::Uuid id;
        VocalEvent::Type type = VocalEvent::Type::Custom;
        double start = 0.0;
        double end = 0.0;
        float gainDb = 0.0f;
        double fadeInMs = 0.0;
        double fadeOutMs = 0.0;
        VocalEvent::FadeCurve fadeInCurve = VocalEvent::FadeCurve::Smooth;
        VocalEvent::FadeCurve fadeOutCurve = VocalEvent::FadeCurve::Smooth;
        bool locked = false;
        juce::Array<VocalEvent::EnvelopePoint> envelope;
    };

    std::vector<EventData> stagedEvents;
    if (auto* events = xml.getChildByName ("Events"))
    {
        forEachXmlChildElementWithTagName (*events, item, "Event")
        {
            EventData data;
            data.id = juce::Uuid (item->getStringAttribute ("id"));
            const int type = item->getIntAttribute ("type", static_cast<int> (VocalEvent::Type::Custom));
            const int fadeInCurve = item->getIntAttribute ("fadeInCurve", static_cast<int> (VocalEvent::FadeCurve::Smooth));
            const int fadeOutCurve = item->getIntAttribute ("fadeOutCurve", static_cast<int> (VocalEvent::FadeCurve::Smooth));

            data.start = item->getDoubleAttribute ("start", -1.0);
            data.end = item->getDoubleAttribute ("end", -1.0);
            data.gainDb = static_cast<float> (item->getDoubleAttribute ("gainDb", 0.0));
            data.fadeInMs = item->getDoubleAttribute ("fadeInMs", 0.0);
            data.fadeOutMs = item->getDoubleAttribute ("fadeOutMs", 0.0);
            data.locked = item->getBoolAttribute ("locked", false);

            if (data.id.isNull()
                || type < static_cast<int> (VocalEvent::Type::Phrase)
                || type > static_cast<int> (VocalEvent::Type::Custom)
                || fadeInCurve < 0 || fadeInCurve > 1
                || fadeOutCurve < 0 || fadeOutCurve > 1
                || ! std::isfinite (data.start) || ! std::isfinite (data.end)
                || ! std::isfinite (data.gainDb)
                || ! std::isfinite (data.fadeInMs) || ! std::isfinite (data.fadeOutMs)
                || data.start < 0.0 || data.end <= data.start
                || data.gainDb < -96.0f || data.gainDb > 24.0f
                || data.fadeInMs < 0.0 || data.fadeOutMs < 0.0)
            {
                errorMessage = "The project contains an invalid event.";
                return false;
            }

            for (const auto& existing : stagedEvents)
                if (existing.id == data.id)
                {
                    errorMessage = "The project contains duplicate event IDs.";
                    return false;
                }

            data.type = static_cast<VocalEvent::Type> (type);
            data.fadeInCurve = static_cast<VocalEvent::FadeCurve> (fadeInCurve);
            data.fadeOutCurve = static_cast<VocalEvent::FadeCurve> (fadeOutCurve);

            if (auto* envelope = item->getChildByName ("Envelope"))
            {
                forEachXmlChildElementWithTagName (*envelope, point, "Point")
                {
                    const float t = static_cast<float> (point->getDoubleAttribute ("t", -1.0));
                    const float gain = static_cast<float> (point->getDoubleAttribute ("gainDb", 0.0));
                    if (! std::isfinite (t) || ! std::isfinite (gain)
                        || t < 0.0f || t > 1.0f || gain < -120.0f || gain > 24.0f
                        || (! data.envelope.isEmpty() && t <= data.envelope.getLast().t))
                    {
                        errorMessage = "The project contains an invalid envelope.";
                        return false;
                    }
                    data.envelope.add ({ t, gain });
                }
            }
            stagedEvents.push_back (std::move (data));
        }
    }

    const auto audioPath = xml.getStringAttribute ("audioPath");
    juce::AudioBuffer<float> stagedAudio;
    double stagedSampleRate = currentSampleRate;
    juce::File stagedAudioFile;

    if (audioPath.isNotEmpty())
    {
        stagedAudioFile = juce::File (audioPath);
        if (! stagedAudioFile.existsAsFile())
        {
            errorMessage = "The source audio file could not be found: " + audioPath;
            return false;
        }

        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (stagedAudioFile));
        if (reader == nullptr || reader->lengthInSamples <= 0
            || reader->lengthInSamples > static_cast<juce::int64> (std::numeric_limits<int>::max())
            || reader->numChannels <= 0 || reader->numChannels > 64
            || ! std::isfinite (reader->sampleRate) || reader->sampleRate <= 0.0)
        {
            errorMessage = "The source audio file is invalid or unsupported.";
            return false;
        }

        if (! VocalAudioMath::isReasonableAudioAllocation (static_cast<int> (reader->numChannels),
                                                            reader->lengthInSamples)
            || ! VocalAudioMath::isReasonableAudioWorkingSet (static_cast<int> (reader->numChannels),
                                                               reader->lengthInSamples))
        {
            errorMessage = "The source audio file exceeds the safe memory budget (up to 4 GiB for source plus render buffers).";
            return false;
        }

        try
        {
            stagedAudio.setSize (static_cast<int> (reader->numChannels),
                                 static_cast<int> (reader->lengthInSamples));
            if (! reader->read (&stagedAudio, 0, stagedAudio.getNumSamples(), 0, true, true))
            {
                errorMessage = "The source audio file could not be read completely.";
                return false;
            }
        }
        catch (const std::bad_alloc&)
        {
            errorMessage = "There is not enough memory to load the source audio file.";
            return false;
        }
        stagedSampleRate = reader->sampleRate;
    }

    // Validate the complete event timeline against the staged source before
    // committing any project state. This keeps failed loads fully atomic.
    const double stagedDuration = stagedAudio.getNumSamples() > 0
        ? static_cast<double> (stagedAudio.getNumSamples()) / stagedSampleRate : 0.0;
    std::vector<std::pair<double, double>> timelineIntervals;
    timelineIntervals.reserve (stagedEvents.size());

    for (const auto& event : stagedEvents)
    {
        const double length = event.end - event.start;
        if (event.fadeInMs > length * 500.0 + 1.0e-6
            || event.fadeOutMs > length * 500.0 + 1.0e-6)
        {
            errorMessage = "The project contains invalid fades.";
            return false;
        }
        timelineIntervals.emplace_back (event.start, event.end);
    }

    if (! VocalAudioMath::isValidNonOverlappingTimeline (timelineIntervals, stagedDuration))
    {
        errorMessage = "The project contains out-of-range or overlapping events.";
        return false;
    }

    // Commit only after the XML, source audio, and complete event timeline
    // have passed validation. Stop transport and mark the render stale before
    // replacing session data so the callback cannot play the prior snapshot.
    stopPlayback();
    processedBufferUpdatePending.store (true, std::memory_order_release);
    audioBuffer = std::move (stagedAudio);
    audioAvailable.store (audioBuffer.getNumSamples() > 0, std::memory_order_release);
    currentAudioFile = stagedAudioFile;
    currentSampleRate = stagedSampleRate;
    eventList.clear();
    selectedEvent = nullptr;
    selectedEventIds.clear();

    for (const auto& data : stagedEvents)
    {
        auto* event = eventList.add (data.start, data.end, data.type);
        event->id = data.id;
        event->gainDb = data.gainDb;
        event->fadeInMs = juce::jmin (data.fadeInMs, event->getLength() * 500.0);
        event->fadeOutMs = juce::jmin (data.fadeOutMs, event->getLength() * 500.0);
        event->fadeInCurve = data.fadeInCurve;
        event->fadeOutCurve = data.fadeOutCurve;
        event->locked = data.locked;
        event->envelope = data.envelope;
    }

    if (auto* selected = xml.getChildByName ("SelectedEvents"))
    {
        forEachXmlChildElementWithTagName (*selected, item, "Id")
        {
            const juce::Uuid id (item->getStringAttribute ("value"));
            if (eventList.getById (id) != nullptr)
                selectedEventIds.addIfNotAlreadyThere (id);
        }
    }

    if (! selectedEventIds.isEmpty())
        selectedEvent = eventList.getById (selectedEventIds.getFirst());

    const double maxPosition = audioBuffer.getNumSamples() > 0
        ? static_cast<double> (audioBuffer.getNumSamples()) / currentSampleRate : 0.0;
    playbackPosition = juce::jlimit (0.0, maxPosition, stagedPlaybackPosition);
    playbackSamplePosition = static_cast<int> (playbackPosition * currentSampleRate);
    currentlyPlaying = false;

    selection.startTime = stagedSelectionStart;
    selection.endTime = stagedSelectionEnd;
    selection.isActive = xml.getBoolAttribute ("selectionActive", false)
                         && selection.endTime > selection.startTime;
    if (audioBuffer.getNumSamples() > 0)
    {
        const double duration = static_cast<double> (audioBuffer.getNumSamples()) / currentSampleRate;
        selection.startTime = juce::jmin (selection.startTime, duration);
        selection.endTime = juce::jmin (selection.endTime, duration);
        if (selection.endTime <= selection.startTime)
            selection.isActive = false;
    }
    else
    {
        selection = {};
    }

    undoManager.clearUndoHistory();
    lastUndoActionName.clear();
    lastUndoActionTimeMs = 0;
    updateProcessedBuffer();
    errorMessage.clear();
    return true;
}

void VocalEditorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = createStateXml())
        copyXmlToBinary (*xml, destData);
}

void VocalEditorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr)
        return;

    juce::String error;
    restoreStateFromXml (*xml, error);
}

bool VocalEditorAudioProcessor::saveProject (const juce::File& projectFile)
{
    if (projectFile == juce::File() || projectFile.isDirectory())
        return false;

    auto xml = createStateXml();
    if (xml == nullptr)
        return false;

    // Write a complete temporary project first, then replace the destination.
    // If serialization or disk I/O fails, the previous project file remains intact.
    juce::TemporaryFile temporary (projectFile);
    if (! xml->writeTo (temporary.getFile()))
        return false;

    if (! temporary.overwriteTargetFileWithTemporary())
        return false;

    currentProjectFile = projectFile;
    return true;
}

bool VocalEditorAudioProcessor::loadProject (const juce::File& projectFile,
                                              juce::String& errorMessage)
{
    if (! projectFile.existsAsFile())
    {
        errorMessage = "Project file does not exist.";
        return false;
    }

    juce::XmlDocument document (projectFile);
    std::unique_ptr<juce::XmlElement> xml (document.getDocumentElement());
    if (xml == nullptr)
    {
        errorMessage = "The project file is not valid XML.";
        return false;
    }

    if (! restoreStateFromXml (*xml, errorMessage))
        return false;

    currentProjectFile = projectFile;
    return true;
}

//==============================================================================
void VocalEditorAudioProcessor::applyEventToBuffer (const VocalEvent& event, juce::AudioBuffer<float>& target)
{
    VocalAudioMath::applyEventToBuffer (event, target, currentSampleRate.load (std::memory_order_acquire));
}

void VocalEditorAudioProcessor::updateProcessedBuffer()
{
    // The message thread is the only writer. Never overwrite the published
    // buffer or the buffer currently being read by processBlock().
    const int active = activeProcessedBuffer.load (std::memory_order_acquire);
    int targetIndex = -1;
    for (int i = 0; i < static_cast<int> (processedBuffers.size()); ++i)
        if (i != active
            && processedBufferReaders[static_cast<size_t> (i)].load (std::memory_order_acquire) == 0)
        {
            targetIndex = i;
            break;
        }

    if (targetIndex < 0)
    {
        // Temporary reader contention is safe to retry from the editor timer.
        processedBufferUpdateRetryBlocked.store (false, std::memory_order_release);
        processedBufferUpdatePending.store (true, std::memory_order_release);
        return;
    }

    auto& target = processedBuffers[static_cast<size_t> (targetIndex)];
    if (audioBuffer.getNumSamples() <= 0 || audioBuffer.getNumChannels() <= 0)
    {
        target.setSize (0, 0);
        processedBufferSampleRates[static_cast<size_t> (targetIndex)] =
            currentSampleRate.load (std::memory_order_acquire);
        activeProcessedBuffer.store (targetIndex, std::memory_order_release);
        processedBufferUpdateRetryBlocked.store (false, std::memory_order_release);
        processedBufferUpdatePending.store (false, std::memory_order_release);
        return;
    }

    // The rendered copy is another full-size float buffer. Preflight its
    // allocation independently, then publish only after the entire render succeeds.
    if (! VocalAudioMath::isReasonableAudioAllocation (audioBuffer.getNumChannels(),
                                                        audioBuffer.getNumSamples())
        || ! VocalAudioMath::isReasonableAudioWorkingSet (audioBuffer.getNumChannels(),
                                                           audioBuffer.getNumSamples()))
    {
        processedBufferUpdateRetryBlocked.store (true, std::memory_order_release);
        processedBufferUpdatePending.store (true, std::memory_order_release);
        return;
    }

    try
    {
        target.setSize (audioBuffer.getNumChannels(), audioBuffer.getNumSamples(), false, false, true);
        target.makeCopyOf (audioBuffer, true);
        for (const auto* event : eventList.getAll())
            if (event != nullptr)
                applyEventToBuffer (*event, target);
    }
    catch (const std::bad_alloc&)
    {
        // Keep the previous snapshot valid and wait for an explicit retry; do not
        // spin on repeated large allocations from the 15 ms UI timer.
        processedBufferUpdateRetryBlocked.store (true, std::memory_order_release);
        processedBufferUpdatePending.store (true, std::memory_order_release);
        return;
    }

    processedBufferSampleRates[static_cast<size_t> (targetIndex)] =
        currentSampleRate.load (std::memory_order_acquire);
    activeProcessedBuffer.store (targetIndex, std::memory_order_release);
    processedBufferUpdateRetryBlocked.store (false, std::memory_order_release);
    processedBufferUpdatePending.store (false, std::memory_order_release);
}

void VocalEditorAudioProcessor::reclaimUnusedProcessedBufferMemory()
{
    // Call only from the message thread. The active snapshot is immutable and
    // must remain allocated; reader counters protect snapshots pinned by the
    // audio callback or export operation.
    const int active = activeProcessedBuffer.load (std::memory_order_acquire);
    const int sourceSamples = audioBuffer.getNumSamples();
    const int sourceChannels = audioBuffer.getNumChannels();

    for (int i = 0; i < static_cast<int> (processedBuffers.size()); ++i)
    {
        if (i == active
            || processedBufferReaders[static_cast<size_t> (i)].load (std::memory_order_acquire) != 0)
            continue;

        auto& buffer = processedBuffers[static_cast<size_t> (i)];
        const bool obsolete = sourceSamples == 0 || sourceChannels == 0
            || buffer.getNumSamples() > sourceSamples
            || buffer.getNumChannels() > sourceChannels;
        if (obsolete)
        {
            buffer.setSize (0, 0);
            processedBufferSampleRates[static_cast<size_t> (i)] =
                currentSampleRate.load (std::memory_order_acquire);
        }
    }
}

bool VocalEditorAudioProcessor::exportProcessedAudio (const juce::File& file, juce::String& errorMessage)
{
    auto fail = [&errorMessage] (const juce::String& message)
    {
        errorMessage = message;
        return false;
    };

    if (! hasAudio() || currentSampleRate <= 0.0)
        return fail ("Load an audio file before exporting.");

    if (processedBufferUpdatePending.load (std::memory_order_acquire))
    {
        updateProcessedBuffer();
        if (processedBufferUpdatePending.load (std::memory_order_acquire))
            return fail ("The processed audio is busy. Retry export in a moment; no stale render was exported.");
    }

    if (file == juce::File() || file.isDirectory())
        return fail ("Choose a valid output file.");

    if (file == currentAudioFile)
        return fail ("Export to a new file; the source audio cannot be overwritten.");

    const auto extension = file.getFileExtension().toLowerCase();
    if (extension != ".wav" && extension != ".aif" && extension != ".aiff")
        return fail ("Export currently supports WAV and AIFF files.");

    // Acquire an immutable processed snapshot without blocking the audio callback.
    int sourceIndex = 0;
    for (;;)
    {
        sourceIndex = activeProcessedBuffer.load (std::memory_order_acquire);
        processedBufferReaders[static_cast<size_t> (sourceIndex)].fetch_add (1, std::memory_order_acq_rel);
        if (sourceIndex == activeProcessedBuffer.load (std::memory_order_acquire))
            break;

        processedBufferReaders[static_cast<size_t> (sourceIndex)].fetch_sub (1, std::memory_order_release);
    }

    juce::AudioBuffer<float> snapshot;
    const auto& source = processedBuffers[static_cast<size_t> (sourceIndex)];
    const bool sourceValid = source.getNumSamples() > 0 && source.getNumChannels() > 0;
    if (sourceValid)
        snapshot.makeCopyOf (source, true);
    processedBufferReaders[static_cast<size_t> (sourceIndex)].fetch_sub (1, std::memory_order_release);

    if (! sourceValid || snapshot.getNumSamples() <= 0)
        return fail ("There is no processed audio available to export.");

    auto* format = formatManager.findFormatForFileExtension (extension);
    if (format == nullptr)
        return fail ("No writer is available for this audio format.");

    auto outputStream = file.createOutputStream();
    if (outputStream == nullptr || ! outputStream->openedOk())
        return fail ("Could not create the output file. Check the folder and file permissions.");

    if (! outputStream->setPosition (0) || ! outputStream->truncate())
        return fail ("Could not prepare the output file for writing.");

    const int bitsPerSample = extension == ".wav" ? 24 : 24;
    // JUCE 8's AudioFormat API accepts a raw stream pointer. Keep the
    // unique_ptr until writer creation succeeds so a failed writer does not
    // leak the output stream.
    auto* rawWriter = format->createWriterFor (
        outputStream.get(),
        currentSampleRate.load (std::memory_order_acquire),
        static_cast<unsigned int> (snapshot.getNumChannels()),
        bitsPerSample, {}, 0);
    if (rawWriter != nullptr)
        outputStream.release();
    auto writer = std::unique_ptr<juce::AudioFormatWriter> (rawWriter);
    if (writer == nullptr)
        return fail ("Could not create an audio writer for the selected format.");

    if (! writer->writeFromAudioSampleBuffer (snapshot, 0, snapshot.getNumSamples()))
        return fail ("Audio export failed while writing samples.");

    writer.reset();
    errorMessage.clear();
    return true;
}

int VocalEditorAudioProcessor::autoSegmentFromSilence (const SegmentParams& params)
{
    if (! hasAudio() || currentSampleRate <= 0.0)
        return 0;

    const auto before = captureEventState();
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
    if (created > 0 || params.clearExisting)
        recordEventUndo (before, "Auto Segment");
    return created;
}

int VocalEditorAudioProcessor::levelEventsToTarget (float targetDb, bool selectedOnly)
{
    if (! hasAudio() || currentSampleRate <= 0.0 || eventList.isEmpty())
        return 0;

    targetDb = juce::jlimit (-36.0f, -6.0f, targetDb);
    juce::Array<VocalEvent*> targets;

    if (selectedOnly && selectedEventIds.isEmpty())
        return 0;

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

    const auto before = captureEventState();
    int processed = 0;
    for (auto* event : targets)
    {
        if (event == nullptr || event->locked)
            continue;

        // Measure what this event currently outputs, not just the unprocessed
        // source. This includes existing envelope automation and fade curves.
        const double rms = VocalAudioMath::measureRenderedEventRms (
            *event, audioBuffer, currentSampleRate.load (std::memory_order_acquire));
        if (! std::isfinite (rms) || rms < 1.0e-7)
            continue;

        const float effectiveDb = juce::Decibels::gainToDecibels (
            static_cast<float> (rms), -120.0f);
        const float deltaDb = juce::jlimit (-24.0f, 12.0f, targetDb - effectiveDb);
        event->gainDb = juce::jlimit (-96.0f, 24.0f, event->gainDb + deltaDb);
        for (auto& point : event->envelope)
            point.gainDb = juce::jlimit (-120.0f, 24.0f, point.gainDb + deltaDb);
        ++processed;
    }

    if (processed > 0)
    {
        updateProcessedBuffer();
        recordEventUndo (before, "Level Process");
    }
    return processed;
}

juce::XmlElement VocalEditorAudioProcessor::captureEventState() const
{
    juce::XmlElement state ("EventEditState");

    auto* events = state.createNewChildElement ("Events");
    for (const auto* event : eventList.getAll())
    {
        if (event == nullptr)
            continue;

        auto* item = events->createNewChildElement ("Event");
        item->setAttribute ("id", event->id.toString());
        item->setAttribute ("type", static_cast<int> (event->type));
        item->setAttribute ("start", event->startTime);
        item->setAttribute ("end", event->endTime);
        item->setAttribute ("gainDb", event->gainDb);
        item->setAttribute ("fadeInMs", event->fadeInMs);
        item->setAttribute ("fadeOutMs", event->fadeOutMs);
        item->setAttribute ("fadeInCurve", static_cast<int> (event->fadeInCurve));
        item->setAttribute ("fadeOutCurve", static_cast<int> (event->fadeOutCurve));
        item->setAttribute ("locked", event->locked);

        auto* envelope = item->createNewChildElement ("Envelope");
        for (const auto& point : event->envelope)
        {
            auto* pointXml = envelope->createNewChildElement ("Point");
            pointXml->setAttribute ("t", point.t);
            pointXml->setAttribute ("gainDb", point.gainDb);
        }
    }

    auto* selection = state.createNewChildElement ("SelectedEvents");
    for (const auto& id : selectedEventIds)
    {
        auto* item = selection->createNewChildElement ("Id");
        item->setAttribute ("value", id.toString());
    }
    return state;
}

bool VocalEditorAudioProcessor::restoreEventState (const juce::XmlElement& state)
{
    if (! state.hasTagName ("EventEditState"))
        return false;

    struct EventData
    {
        juce::Uuid id;
        VocalEvent::Type type = VocalEvent::Type::Custom;
        double start = 0.0, end = 0.0, fadeIn = 0.0, fadeOut = 0.0;
        float gain = 0.0f;
        VocalEvent::FadeCurve fadeInCurve = VocalEvent::FadeCurve::Smooth;
        VocalEvent::FadeCurve fadeOutCurve = VocalEvent::FadeCurve::Smooth;
        bool locked = false;
        juce::Array<VocalEvent::EnvelopePoint> envelope;
    };

    std::vector<EventData> staged;
    if (auto* events = state.getChildByName ("Events"))
    {
        forEachXmlChildElementWithTagName (*events, item, "Event")
        {
            EventData data;
            data.id = juce::Uuid (item->getStringAttribute ("id"));
            const int type = item->getIntAttribute ("type", -1);
            const int inCurve = item->getIntAttribute ("fadeInCurve", 1);
            const int outCurve = item->getIntAttribute ("fadeOutCurve", 1);
            data.start = item->getDoubleAttribute ("start", -1.0);
            data.end = item->getDoubleAttribute ("end", -1.0);
            data.gain = static_cast<float> (item->getDoubleAttribute ("gainDb", 0.0));
            data.fadeIn = item->getDoubleAttribute ("fadeInMs", 0.0);
            data.fadeOut = item->getDoubleAttribute ("fadeOutMs", 0.0);
            data.locked = item->getBoolAttribute ("locked", false);

            if (data.id.isNull() || type < 0 || type > 4
                || inCurve < 0 || inCurve > 1 || outCurve < 0 || outCurve > 1
                || ! std::isfinite (data.start) || ! std::isfinite (data.end)
                || ! std::isfinite (data.gain) || ! std::isfinite (data.fadeIn)
                || ! std::isfinite (data.fadeOut) || data.start < 0.0
                || data.end <= data.start || data.gain < -96.0f || data.gain > 24.0f
                || data.fadeIn < 0.0 || data.fadeOut < 0.0
                || data.fadeIn > (data.end - data.start) * 500.0 + 1.0e-6
                || data.fadeOut > (data.end - data.start) * 500.0 + 1.0e-6)
                return false;

            for (const auto& existing : staged)
                if (existing.id == data.id)
                    return false;

            data.type = static_cast<VocalEvent::Type> (type);
            data.fadeInCurve = static_cast<VocalEvent::FadeCurve> (inCurve);
            data.fadeOutCurve = static_cast<VocalEvent::FadeCurve> (outCurve);

            if (auto* envelope = item->getChildByName ("Envelope"))
            {
                forEachXmlChildElementWithTagName (*envelope, point, "Point")
                {
                    const float t = static_cast<float> (point->getDoubleAttribute ("t", -1.0));
                    const float gain = static_cast<float> (point->getDoubleAttribute ("gainDb", 0.0));
                    if (! std::isfinite (t) || ! std::isfinite (gain)
                        || t < 0.0f || t > 1.0f || gain < -120.0f || gain > 24.0f
                        || (! data.envelope.isEmpty() && t <= data.envelope.getLast().t))
                        return false;
                    data.envelope.add ({ t, gain });
                }
            }
            staged.push_back (std::move (data));
        }
    }

    // Validate the complete snapshot before mutating the live event list.
    // Overlaps multiply event gain in the current renderer, so malformed
    // snapshots must fail atomically rather than partially restore.
    const double audioDuration = getDuration();
    std::vector<std::pair<double, double>> snapshotIntervals;
    snapshotIntervals.reserve (staged.size());
    for (const auto& event : staged)
        snapshotIntervals.emplace_back (event.start, event.end);

    if (! VocalAudioMath::isValidNonOverlappingTimeline (snapshotIntervals, audioDuration))
        return false;

    eventList.clear();
    selectedEvent = nullptr;
    selectedEventIds.clear();
    for (const auto& data : staged)
    {
        auto* event = eventList.add (data.start, data.end, data.type);
        event->id = data.id;
        event->gainDb = data.gain;
        event->fadeInMs = juce::jmin (data.fadeIn, event->getLength() * 500.0);
        event->fadeOutMs = juce::jmin (data.fadeOut, event->getLength() * 500.0);
        event->fadeInCurve = data.fadeInCurve;
        event->fadeOutCurve = data.fadeOutCurve;
        event->locked = data.locked;
        event->envelope = data.envelope;
    }

    if (auto* selected = state.getChildByName ("SelectedEvents"))
    {
        forEachXmlChildElementWithTagName (*selected, item, "Id")
        {
            const juce::Uuid id (item->getStringAttribute ("value"));
            if (eventList.getById (id) != nullptr)
                selectedEventIds.addIfNotAlreadyThere (id);
        }
    }
    if (! selectedEventIds.isEmpty())
        selectedEvent = eventList.getById (selectedEventIds.getFirst());

    updateProcessedBuffer();
    return true;
}

class VocalEditorAudioProcessor::EventStateAction final : public juce::UndoableAction
{
public:
    EventStateAction (VocalEditorAudioProcessor& owner,
                      const juce::XmlElement& oldState,
                      const juce::XmlElement& newState,
                      juce::String name)
        : processor (owner), before (oldState), after (newState), actionName (std::move (name))
    {
    }

    bool perform() override
    {
        if (firstPerform)
        {
            firstPerform = false;
            return true;
        }
        return processor.restoreEventState (after);
    }
    bool undo() override { return processor.restoreEventState (before); }
    int getSizeInUnits() override { return before.toString().length() + after.toString().length(); }

private:
    VocalEditorAudioProcessor& processor;
    juce::XmlElement before;
    juce::XmlElement after;
    juce::String actionName;
    bool firstPerform = true;
};

void VocalEditorAudioProcessor::recordEventUndo (const juce::XmlElement& before,
                                                  const juce::String& actionName,
                                                  int coalesceMs)
{
    auto after = captureEventState();
    if (before.toString() == after.toString())
        return;

    // JUCE coalesces actions inside a transaction, not by passing a time
    // interval to perform(). Keep rapid gain/fade drags in one transaction.
    const auto now = juce::Time::getMillisecondCounter();
    const bool canCoalesce = coalesceMs > 0
        && lastUndoActionName == actionName
        && static_cast<juce::uint32> (now - lastUndoActionTimeMs)
               <= static_cast<juce::uint32> (coalesceMs);

    if (! canCoalesce)
        undoManager.beginNewTransaction (actionName);

    undoManager.perform (new EventStateAction (*this, before, after, actionName));
    lastUndoActionName = actionName;
    lastUndoActionTimeMs = now;
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

    // Event gains are applied to the processed signal. Overlapping regions
    // would multiply their gains and create an unintended level change.
    for (const auto* existing : eventList.getAll())
        if (existing != nullptr
            && VocalAudioMath::intervalsOverlap (start, end, existing->startTime, existing->endTime))
            return nullptr;

    const auto before = captureEventState();
    auto* event = eventList.add (start, end, type);
    setSelectedEvent (event);
    updateProcessedBuffer();
    recordEventUndo (before, "Create Event");
    return event;
}

void VocalEditorAudioProcessor::setEventGain (const juce::Uuid& id, float gainDb)
{
    if (auto* event = eventList.getById (id))
    {
        if (event->locked)
            return;
        const auto before = captureEventState();
        const float newGainDb = juce::jlimit (-96.0f, 24.0f, gainDb);
        const float deltaDb = newGainDb - event->gainDb;
        event->gainDb = newGainDb;

        // Preserve any user-drawn envelope shape by shifting every point by
        // the same gain delta instead of flattening the envelope.
        for (auto& point : event->envelope)
            point.gainDb = juce::jlimit (-120.0f, 24.0f, point.gainDb + deltaDb);

        updateProcessedBuffer();
        recordEventUndo (before, "Change Event Gain", 300);
    }
}

void VocalEditorAudioProcessor::setEventFades (const juce::Uuid& id, double fadeInMs, double fadeOutMs)
{
    if (auto* event = eventList.getById (id))
    {
        if (event->locked)
            return;
        const auto before = captureEventState();
        const double maxFadeMs = event->getLength() * 500.0;
        event->fadeInMs = juce::jlimit (0.0, maxFadeMs, fadeInMs);
        event->fadeOutMs = juce::jlimit (0.0, maxFadeMs, fadeOutMs);
        updateProcessedBuffer();
        recordEventUndo (before, "Change Event Fades", 300);
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

    const auto before = captureEventState();
    const double split = juce::jlimit (event->startTime, event->endTime, splitTime);
    const double originalStart = event->startTime;
    const double originalEnd = event->endTime;
    if (split - originalStart < 0.005 || originalEnd - split < 0.005)
        return;

    const double originalFadeOut = event->fadeOutMs;
    const auto originalType = event->type;
    const auto originalFadeInCurve = event->fadeInCurve;
    const auto originalFadeOutCurve = event->fadeOutCurve;
    const float originalGain = event->gainDb;
    const float splitNormalised = static_cast<float> (
        (split - originalStart) / (originalEnd - originalStart));
    const bool hadCustomEnvelope = ! event->envelope.isEmpty();

    // Prepare both envelope slices before changing the original event.
    juce::Array<VocalEvent::EnvelopePoint> leftEnvelope;
    juce::Array<VocalEvent::EnvelopePoint> rightEnvelope;
    if (hadCustomEnvelope)
    {
        leftEnvelope = event->makeEnvelopeSlice (0.0f, splitNormalised);
        rightEnvelope = event->makeEnvelopeSlice (splitNormalised, 1.0f);
    }

    // Keep the original event as the left segment. The fade-out remains on the
    // right segment, while the original envelope is split at the exact boundary.
    event->endTime = split;
    event->fadeOutMs = 0.0;
    if (hadCustomEnvelope)
        event->envelope = leftEnvelope;

    auto* right = eventList.add (split, originalEnd, originalType);
    right->gainDb = originalGain;
    right->fadeInMs = 0.0;
    right->fadeOutMs = juce::jmin (originalFadeOut, right->getLength() * 500.0);
    right->fadeInCurve = originalFadeInCurve;
    right->fadeOutCurve = originalFadeOutCurve;
    right->locked = false;
    if (hadCustomEnvelope)
        right->envelope = rightEnvelope;

    setSelectedEvent (event);
    updateProcessedBuffer();
    recordEventUndo (before, "Split Event");
}

void VocalEditorAudioProcessor::deleteEvent (const juce::Uuid& id)
{
    auto* event = eventList.getById (id);
    if (event == nullptr || event->locked)
        return;
    const auto before = captureEventState();
    if (selectedEvent != nullptr && selectedEvent->id == id)
        selectedEvent = nullptr;
    selectedEventIds.removeAllInstancesOf (id);
    eventList.removeById (id);
    updateProcessedBuffer();
    recordEventUndo (before, "Delete Event");
}

void VocalEditorAudioProcessor::deleteSelectedEvents()
{
    if (selectedEventIds.isEmpty())
        return;

    const auto before = captureEventState();
    bool removedAny = false;
    const auto idsToDelete = selectedEventIds;
    for (const auto& id : idsToDelete)
    {
        auto* event = eventList.getById (id);
        if (event != nullptr && ! event->locked)
        {
            eventList.removeById (id);
            removedAny = true;
        }
    }

    if (! removedAny)
        return;

    selectedEvent = nullptr;
    selectedEventIds.clear();
    updateProcessedBuffer();
    recordEventUndo (before, "Delete Selected Events");
}

void VocalEditorAudioProcessor::mergeEvents (const juce::Uuid& idA, const juce::Uuid& idB)
{
    auto* a = eventList.getById (idA);
    auto* b = eventList.getById (idB);
    if (a == nullptr || b == nullptr || a == b || a->locked || b->locked || a->type != b->type)
        return;

    if (b->startTime < a->startTime)
        std::swap (a, b);

    const double gap = b->startTime - a->endTime;

    // A merged event has only one fade-in and one fade-out. Until internal
    // fades can be represented explicitly, reject merges that would erase them.
    if (gap < -1.0e-6 || gap > 1.0e-6
        || a->fadeOutMs > 1.0e-6 || b->fadeInMs > 1.0e-6)
        return;

    // A single merged envelope cannot represent a discontinuity at the join.
    // Refuse that merge instead of creating duplicate-time envelope points
    // which later fail snapshot validation and break Redo.
    if (! VocalAudioMath::canJoinEnvelopeAtBoundary (*a, *b))
        return;

    const auto before = captureEventState();
    const double combinedStart = a->startTime;
    const double combinedEnd = b->endTime;
    const double combinedLength = combinedEnd - combinedStart;
    if (combinedLength <= 0.0)
        return;

    juce::Array<VocalEvent::EnvelopePoint> combinedEnvelope;
    auto appendMapped = [&] (const VocalEvent& source, double sourceStart, double sourceEnd)
    {
        const double sourceLength = sourceEnd - sourceStart;
        if (sourceLength <= 0.0)
            return;

        const auto points = source.makeEnvelopeSlice (0.0f, 1.0f);

        auto addUnique = [&combinedEnvelope] (float t, float value)
        {
            t = juce::jlimit (0.0f, 1.0f, t);
            if (! combinedEnvelope.isEmpty()
                && std::abs (combinedEnvelope.getLast().t - t) <= 1.0e-6f)
            {
                combinedEnvelope.getReference (combinedEnvelope.size() - 1).gainDb = value;
                return;
            }
            combinedEnvelope.add ({ t, value });
        };

        if (points.isEmpty())
        {
            addUnique (static_cast<float> ((sourceStart - combinedStart) / combinedLength), source.gainDb);
            addUnique (static_cast<float> ((sourceEnd - combinedStart) / combinedLength), source.gainDb);
        }
        else
        {
            for (const auto& point : points)
                addUnique (
                    static_cast<float> ((sourceStart + point.t * sourceLength - combinedStart) / combinedLength),
                    point.gainDb);
        }
    };

    appendMapped (*a, a->startTime, a->endTime);
    appendMapped (*b, b->startTime, b->endTime);
    std::sort (combinedEnvelope.begin(), combinedEnvelope.end(),
               [] (const auto& left, const auto& right) { return left.t < right.t; });

    // Collapse duplicate boundary points at the join and keep a deterministic curve.
    juce::Array<VocalEvent::EnvelopePoint> cleanEnvelope;
    for (const auto& point : combinedEnvelope)
    {
        if (! cleanEnvelope.isEmpty() && std::abs (cleanEnvelope.getLast().t - point.t) < 1.0e-6f)
            cleanEnvelope.getReference (cleanEnvelope.size() - 1).gainDb = point.gainDb;
        else
            cleanEnvelope.add (point);
    }

    a->endTime = combinedEnd;
    a->fadeOutMs = b->fadeOutMs;
    a->fadeOutCurve = b->fadeOutCurve;
    a->envelope = cleanEnvelope;
    eventList.removeById (b->id);
    setSelectedEvent (a);
    updateProcessedBuffer();
    recordEventUndo (before, "Merge Events");
}

bool VocalEditorAudioProcessor::loadAudioFile (const juce::File& file, juce::String* errorMessage)
{
    auto fail = [errorMessage] (const juce::String& message)
    {
        if (errorMessage != nullptr)
            *errorMessage = message;
        return false;
    };

    if (! file.existsAsFile())
        return fail ("The selected audio file does not exist.");

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
        return fail ("The selected file is not a supported audio format.");

    if (reader->lengthInSamples <= 0
        || reader->lengthInSamples > static_cast<juce::int64> (std::numeric_limits<int>::max())
        || reader->numChannels <= 0 || reader->numChannels > 64
        || ! std::isfinite (reader->sampleRate) || reader->sampleRate <= 0.0)
        return fail ("The selected audio file has invalid duration, channel count, or sample rate.");

    if (! VocalAudioMath::isReasonableAudioAllocation (static_cast<int> (reader->numChannels),
                                                        reader->lengthInSamples)
        || ! VocalAudioMath::isReasonableAudioWorkingSet (static_cast<int> (reader->numChannels),
                                                           reader->lengthInSamples))
        return fail ("The selected audio file exceeds the safe memory budget (up to 4 GiB for source plus render buffers).");

    juce::AudioBuffer<float> stagedAudio;
    try
    {
        stagedAudio.setSize (static_cast<int> (reader->numChannels),
                             static_cast<int> (reader->lengthInSamples));

        if (! reader->read (&stagedAudio, 0, stagedAudio.getNumSamples(), 0, true, true))
            return fail ("The selected audio file could not be read completely.");
    }
    catch (const std::bad_alloc&)
    {
        return fail ("There is not enough memory to load the selected audio file.");
    }

    // Commit only after opening, validation, allocation, and decoding succeed.
    stopPlayback();
    processedBufferUpdatePending.store (true, std::memory_order_release);
    audioBuffer = std::move (stagedAudio);
    audioAvailable.store (audioBuffer.getNumSamples() > 0, std::memory_order_release);
    currentAudioFile = file;
    currentSampleRate = reader->sampleRate;
    playbackPosition = 0.0;
    playbackSamplePosition = 0;

    clearSelection();
    eventList.clear();
    selectedEvent = nullptr;
    selectedEventIds.clear();
    undoManager.clearUndoHistory();
    lastUndoActionName.clear();
    lastUndoActionTimeMs = 0;
    currentProjectFile = {};
    updateProcessedBuffer();

    if (errorMessage != nullptr)
        errorMessage->clear();
    return true;
}

void VocalEditorAudioProcessor::clearAudio()
{
    // Invalidate history before removing the audio that its event snapshots
    // depend on. Otherwise Undo can attempt to restore events against an
    // empty timeline and fail, leaving a misleading stale history stack.
    stopPlayback();
    undoManager.clearUndoHistory();
    lastUndoActionName.clear();
    lastUndoActionTimeMs = 0;
    processedBufferUpdatePending.store (true, std::memory_order_release);

    audioAvailable.store (false, std::memory_order_release);
    audioBuffer.setSize (0, 0);
    playbackPosition = 0.0;
    playbackSamplePosition = 0;
    clearSelection();
    eventList.clear();
    selectedEvent = nullptr;
    selectedEventIds.clear();
    currentAudioFile = {};
    currentProjectFile = {};
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
    if (! std::isfinite (start) || ! std::isfinite (end))
    {
        clearSelection();
        return;
    }

    if (end < start)
        std::swap (start, end);

    const double duration = getDuration();
    selection.startTime = juce::jlimit (0.0, duration, start);
    selection.endTime = juce::jlimit (selection.startTime, duration, end);
    selection.isActive = hasAudio() && selection.endTime > selection.startTime;
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
