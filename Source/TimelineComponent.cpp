#include "TimelineComponent.h"

//==============================================================================
TimelineComponent::TimelineComponent (VocalEditorAudioProcessor& p)
    : processor (p)
{
    setOpaque (true);
}

void TimelineComponent::paint (juce::Graphics& g)
{
    g.fillAll (backgroundColour);
    
    // Bottom border
    g.setColour (tickColour);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));
    
    if (getVisibleDuration() <= 0.0)
        return;
    
    const double duration = visibleEndTime - visibleStartTime;
    
    // Choose nice step
    double step = 1.0;
    if (duration > 60.0)      step = 10.0;
    else if (duration > 30.0) step = 5.0;
    else if (duration > 10.0) step = 2.0;
    else if (duration < 2.0)  step = 0.25;
    else if (duration < 5.0)  step = 0.5;
    
    g.setFont (11.0f);
    
    double t = std::floor (visibleStartTime / step) * step;
    if (t < visibleStartTime)
        t += step;
    
    while (t <= visibleEndTime + 0.001)
    {
        float x = static_cast<float> (timeToX (t));
        
        // Major tick
        g.setColour (tickColour);
        g.drawVerticalLine (static_cast<int> (x), static_cast<float> (getHeight() - 12), 
                            static_cast<float> (getHeight() - 1));
        
        // Label
        g.setColour (textColour);
        auto label = formatTime (t);
        g.drawText (label, static_cast<int> (x) - 30, 2, 60, 14, 
                    juce::Justification::centred);
        
        t += step;
    }
}

void TimelineComponent::resized()
{
}

void TimelineComponent::setVisibleRange (double startTime, double endTime)
{
    visibleStartTime = startTime;
    visibleEndTime = endTime;
    repaint();
}

void TimelineComponent::setTotalDuration (double duration)
{
    totalDuration = duration;
}

double TimelineComponent::timeToX (double time) const
{
    const double dur = visibleEndTime - visibleStartTime;
    if (dur <= 0.0 || getWidth() <= 0)
        return 0.0;
    
    return (time - visibleStartTime) / dur * getWidth();
}

juce::String TimelineComponent::formatTime (double seconds) const
{
    if (seconds < 0.0)
        seconds = 0.0;
    
    int mins = static_cast<int> (seconds) / 60;
    double secs = std::fmod (seconds, 60.0);
    
    if (mins > 0)
        return juce::String::formatted ("%d:%05.2f", mins, secs);
    else
        return juce::String::formatted ("%.2f", secs);
}
