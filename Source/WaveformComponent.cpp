#include "WaveformComponent.h"

//==============================================================================
WaveformComponent::WaveformComponent (VocalEditorAudioProcessor& p)
    : processor (p)
{
    thumbnail = std::make_unique<juce::AudioThumbnail> (512, 
                                                         processor.getFormatManager(), 
                                                         processor.getThumbnailCache());
    thumbnail->addChangeListener (this);
    
    setOpaque (true);
    startTimerHz (30); // for playhead updates
}

WaveformComponent::~WaveformComponent()
{
    thumbnail->removeChangeListener (this);
    stopTimer();
}

//==============================================================================
void WaveformComponent::paint (juce::Graphics& g)
{
    g.fillAll (backgroundColour);
    
    auto bounds = getLocalBounds().toFloat();
    
    // Border
    g.setColour (gridColour);
    g.drawRect (bounds, 1.0f);
    
    if (! processor.hasAudio() || thumbnail->getTotalLength() <= 0.0)
    {
        g.setColour (textColour);
        g.setFont (16.0f);
        g.drawText ("Drop an audio file or use Open...", 
                    getLocalBounds(), 
                    juce::Justification::centred);
        return;
    }
    
    drawGrid (g);
    drawWaveform (g);
    drawSelection (g);
    drawPlayhead (g);
}

void WaveformComponent::resized()
{
    updateVisibleRange();
}

//==============================================================================
void WaveformComponent::drawWaveform (juce::Graphics& g)
{
    if (thumbnail->getTotalLength() <= 0.0)
        return;
    
    auto bounds = getLocalBounds().reduced (1).toFloat();
    
    g.setColour (waveformColour.withAlpha (0.85f));
    
    thumbnail->drawChannels (g,
                             bounds.toNearestInt(),
                             visibleStartTime,
                             visibleEndTime,
                             1.0f);
}

void WaveformComponent::drawSelection (juce::Graphics& g)
{
    const auto& sel = processor.getSelection();
    if (! sel.isActive)
        return;
    
    float x1 = static_cast<float> (timeToX (sel.startTime));
    float x2 = static_cast<float> (timeToX (sel.endTime));
    
    if (x2 < 0 || x1 > getWidth())
        return;
    
    x1 = juce::jlimit (0.0f, static_cast<float> (getWidth()), x1);
    x2 = juce::jlimit (0.0f, static_cast<float> (getWidth()), x2);
    
    g.setColour (selectionColour);
    g.fillRect (x1, 0.0f, x2 - x1, static_cast<float> (getHeight()));
    
    // Selection borders
    g.setColour (juce::Colour (0xff22d3ee));
    g.drawVerticalLine (static_cast<int> (x1), 0.0f, static_cast<float> (getHeight()));
    g.drawVerticalLine (static_cast<int> (x2), 0.0f, static_cast<float> (getHeight()));
}

void WaveformComponent::drawPlayhead (juce::Graphics& g)
{
    double pos = processor.getPlaybackPosition();
    
    if (pos < visibleStartTime || pos > visibleEndTime)
        return;
    
    float x = static_cast<float> (timeToX (pos));
    
    g.setColour (playheadColour);
    g.drawVerticalLine (static_cast<int> (x), 0.0f, static_cast<float> (getHeight()));
    
    // Small triangle at top
    juce::Path triangle;
    triangle.addTriangle (x - 5.0f, 0.0f, x + 5.0f, 0.0f, x, 8.0f);
    g.fillPath (triangle);
}

void WaveformComponent::drawGrid (juce::Graphics& g)
{
    g.setColour (gridColour.withAlpha (0.4f));
    
    const double duration = getVisibleDuration();
    if (duration <= 0.0)
        return;
    
    // Simple time grid (roughly every 1s or adjusted by zoom)
    double step = 1.0;
    if (duration > 30.0) step = 5.0;
    else if (duration > 10.0) step = 2.0;
    else if (duration < 2.0) step = 0.25;
    else if (duration < 5.0) step = 0.5;
    
    double t = std::ceil (visibleStartTime / step) * step;
    
    while (t < visibleEndTime)
    {
        float x = static_cast<float> (timeToX (t));
        g.drawVerticalLine (static_cast<int> (x), 0.0f, static_cast<float> (getHeight()));
        t += step;
    }
}

//==============================================================================
double WaveformComponent::timeToX (double time) const
{
    if (getVisibleDuration() <= 0.0)
        return 0.0;
    
    return (time - visibleStartTime) / getVisibleDuration() * getWidth();
}

double WaveformComponent::xToTime (float x) const
{
    if (getWidth() <= 0)
        return visibleStartTime;
    
    return visibleStartTime + (static_cast<double> (x) / getWidth()) * getVisibleDuration();
}

//==============================================================================
void WaveformComponent::mouseDown (const juce::MouseEvent& e)
{
    if (! processor.hasAudio())
        return;
    
    const double time = xToTime (static_cast<float> (e.x));
    
    if (e.mods.isShiftDown())
    {
        // Extend selection
        const auto& sel = processor.getSelection();
        if (sel.isActive)
        {
            if (time < sel.startTime)
                processor.setSelection (time, sel.endTime);
            else
                processor.setSelection (sel.startTime, time);
        }
        else
        {
            processor.setSelection (time, time);
        }
    }
    else
    {
        // Start new selection or move playhead
        isSelecting = true;
        selectionStartTime = time;
        processor.setSelection (time, time);
        
        // Also set playhead
        processor.setPlaybackPosition (time);
    }
    
    repaint();
}

void WaveformComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (! processor.hasAudio() || ! isSelecting)
        return;
    
    const double time = juce::jlimit (0.0, totalDuration, xToTime (static_cast<float> (e.x)));
    processor.setSelection (selectionStartTime, time);
    
    // Auto-scroll if dragging near edges
    const float edge = 40.0f;
    if (e.x < edge)
    {
        visibleStartTime = juce::jmax (0.0, visibleStartTime - 0.05 * getVisibleDuration());
        visibleEndTime = visibleStartTime + getVisibleDuration();
        updateVisibleRange();
    }
    else if (e.x > getWidth() - edge)
    {
        visibleEndTime = juce::jmin (totalDuration, visibleEndTime + 0.05 * getVisibleDuration());
        visibleStartTime = visibleEndTime - getVisibleDuration();
        updateVisibleRange();
    }
    
    repaint();
}

void WaveformComponent::mouseUp (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
    isSelecting = false;
    
    // Ensure selection has minimum length
    auto& sel = processor.getSelection();
    if (sel.isActive && sel.getLength() < 0.005) // 5ms minimum
    {
        // Treat as click - just place playhead
        processor.clearSelection();
    }
    
    repaint();
}

void WaveformComponent::mouseMove (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
}

void WaveformComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
    // Double-click clears selection
    clearSelection();
    repaint();
}

void WaveformComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (! processor.hasAudio())
        return;
    
    // Zoom around mouse position
    const double mouseTime = xToTime (static_cast<float> (e.x));
    const double oldDuration = getVisibleDuration();
    
    if (wheel.deltaY > 0)
        zoomFactor *= 1.15;
    else
        zoomFactor /= 1.15;
    
    zoomFactor = juce::jlimit (0.05, 100.0, zoomFactor);
    
    const double newDuration = totalDuration / zoomFactor;
    const double ratio = (mouseTime - visibleStartTime) / oldDuration;
    
    visibleStartTime = mouseTime - ratio * newDuration;
    visibleEndTime = visibleStartTime + newDuration;
    
    // Clamp
    if (visibleStartTime < 0.0)
    {
        visibleStartTime = 0.0;
        visibleEndTime = newDuration;
    }
    if (visibleEndTime > totalDuration)
    {
        visibleEndTime = totalDuration;
        visibleStartTime = juce::jmax (0.0, totalDuration - newDuration);
    }
    
    repaint();
}

//==============================================================================
void WaveformComponent::setZoomFactor (double factor)
{
    zoomFactor = juce::jlimit (0.05, 100.0, factor);
    updateVisibleRange();
    repaint();
}

void WaveformComponent::zoomIn()
{
    setZoomFactor (zoomFactor * 1.25);
}

void WaveformComponent::zoomOut()
{
    setZoomFactor (zoomFactor / 1.25);
}

void WaveformComponent::zoomToFit()
{
    zoomFactor = 1.0;
    visibleStartTime = 0.0;
    visibleEndTime = totalDuration > 0.0 ? totalDuration : 10.0;
    repaint();
}

void WaveformComponent::setViewRange (double startTime, double endTime)
{
    visibleStartTime = startTime;
    visibleEndTime = endTime;
    if (totalDuration > 0.0)
        zoomFactor = totalDuration / (endTime - startTime);
    repaint();
}

void WaveformComponent::updateVisibleRange()
{
    if (totalDuration <= 0.0)
    {
        visibleStartTime = 0.0;
        visibleEndTime = 10.0;
        return;
    }
    
    const double duration = totalDuration / zoomFactor;
    visibleEndTime = visibleStartTime + duration;
    
    if (visibleEndTime > totalDuration)
    {
        visibleEndTime = totalDuration;
        visibleStartTime = juce::jmax (0.0, totalDuration - duration);
    }
}

void WaveformComponent::clearSelection()
{
    processor.clearSelection();
}

void WaveformComponent::audioFileLoaded()
{
    if (processor.hasAudio())
    {
        totalDuration = static_cast<double> (processor.getNumSamples()) / processor.getSampleRate();
        
        // Rebuild thumbnail from the in-memory buffer
        thumbnail->clear();
        thumbnail->reset (processor.getNumChannels(),
                          processor.getSampleRate(),
                          static_cast<juce::int64> (processor.getNumSamples()));
        
        thumbnail->addBlock (0, processor.getAudioBuffer(), 0, processor.getNumSamples());
        
        zoomToFit();
    }
    else
    {
        totalDuration = 0.0;
        thumbnail->clear();
    }
    
    repaint();
}

void WaveformComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    juce::ignoreUnused (source);
    repaint();
}

void WaveformComponent::timerCallback()
{
    if (processor.isPlaying())
        repaint();
}
