#pragma once

#include <JuceHeader.h>
#include "VocalEvent.h"

//==============================================================================
class VocalEditorAudioProcessor : public juce::AudioProcessor
{
public:
    VocalEditorAudioProcessor();
    ~VocalEditorAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void loadAudioFile (const juce::File& file);
    void clearAudio();
    bool hasAudio() const { return audioBuffer.getNumSamples() > 0; }
    juce::File getCurrentAudioFile() const { return currentAudioFile; }

    bool saveProject (const juce::File& projectFile);
    bool loadProject (const juce::File& projectFile, juce::String& errorMessage);
    juce::File getCurrentProjectFile() const { return currentProjectFile; }

    const juce::AudioBuffer<float>& getAudioBuffer() const { return audioBuffer; }
    double getSampleRate() const { return currentSampleRate; }
    int getNumChannels() const { return audioBuffer.getNumChannels(); }
    int getNumSamples() const { return audioBuffer.getNumSamples(); }
    double getDuration() const
    {
        return hasAudio() ? static_cast<double> (audioBuffer.getNumSamples()) / currentSampleRate : 0.0;
    }

    juce::AudioFormatManager& getFormatManager() { return formatManager; }
    juce::AudioThumbnailCache& getThumbnailCache() { return thumbnailCache; }

    void setPlaybackPosition (double positionInSeconds);
    double getPlaybackPosition() const { return playbackPosition; }
    void startPlayback();
    void stopPlayback();
    bool isPlaying() const { return currentlyPlaying; }

    struct Selection
    {
        double startTime = 0.0;
        double endTime   = 0.0;
        bool isActive    = false;
        double getLength() const { return endTime - startTime; }
        bool contains (double time) const { return isActive && time >= startTime && time <= endTime; }
    };

    Selection& getSelection() { return selection; }
    const Selection& getSelection() const { return selection; }
    void setSelection (double start, double end);
    void clearSelection();

    EventList& getEventList() { return eventList; }
    const EventList& getEventList() const { return eventList; }

    VocalEvent* createEventFromSelection (VocalEvent::Type type = VocalEvent::Type::Custom);
    void setEventGain (const juce::Uuid& id, float gainDb);
    void setEventFades (const juce::Uuid& id, double fadeInMs, double fadeOutMs);
    void splitEvent (const juce::Uuid& id, double splitTime);
    void mergeEvents (const juce::Uuid& idA, const juce::Uuid& idB);
    void deleteEvent (const juce::Uuid& id);

    VocalEvent* getSelectedEvent() { return selectedEvent; }
    void setSelectedEvent (VocalEvent* e) { selectedEvent = e; }

    juce::UndoManager& getUndoManager() { return undoManager; }
    void undo() { undoManager.undo(); }
    void redo() { undoManager.redo(); }
    bool canUndo() const { return undoManager.canUndo(); }
    bool canRedo() const { return undoManager.canRedo(); }

    void updateProcessedBuffer();

private:
    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache { 5 };

    juce::AudioBuffer<float> audioBuffer;
    juce::AudioBuffer<float> processedBuffer;
    bool processedNeedsUpdate = true;

    juce::File currentAudioFile;
    juce::File currentProjectFile;

    double currentSampleRate = 44100.0;

    double playbackPosition = 0.0;
    bool currentlyPlaying = false;
    int playbackSamplePosition = 0;

    Selection selection;
    EventList eventList;
    VocalEvent* selectedEvent = nullptr;

    juce::UndoManager undoManager;

    void applyEventToBuffer (const VocalEvent& e, juce::AudioBuffer<float>& buffer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalEditorAudioProcessor)
};
