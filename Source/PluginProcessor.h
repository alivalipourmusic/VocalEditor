#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    Vocal Editor - Audio Processor
    Phase 1: Basic structure for Standalone + VST3
*/
class VocalEditorAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    VocalEditorAudioProcessor();
    ~VocalEditorAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Audio file management (Phase 1)
    void loadAudioFile (const juce::File& file);
    void clearAudio();
    bool hasAudio() const { return audioBuffer.getNumSamples() > 0; }
    
    const juce::AudioBuffer<float>& getAudioBuffer() const { return audioBuffer; }
    double getSampleRate() const { return currentSampleRate; }
    int getNumChannels() const { return audioBuffer.getNumChannels(); }
    int getNumSamples() const { return audioBuffer.getNumSamples(); }
    
    juce::AudioFormatManager& getFormatManager() { return formatManager; }
    juce::AudioThumbnailCache& getThumbnailCache() { return thumbnailCache; }

    // Playback control
    void setPlaybackPosition (double positionInSeconds);
    double getPlaybackPosition() const { return playbackPosition; }
    void startPlayback();
    void stopPlayback();
    bool isPlaying() const { return currentlyPlaying; }

    // Selection (basic region)
    struct Selection
    {
        double startTime = 0.0;
        double endTime = 0.0;
        bool isActive = false;
        
        double getLength() const { return endTime - startTime; }
        bool contains (double time) const { return isActive && time >= startTime && time <= endTime; }
    };
    
    Selection& getSelection() { return selection; }
    const Selection& getSelection() const { return selection; }
    void setSelection (double start, double end);
    void clearSelection();

private:
    //==============================================================================
    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache { 5 };
    
    juce::AudioBuffer<float> audioBuffer;
    double currentSampleRate = 44100.0;
    
    // Playback state
    double playbackPosition = 0.0;
    bool currentlyPlaying = false;
    int playbackSamplePosition = 0;
    
    Selection selection;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalEditorAudioProcessor)
};
