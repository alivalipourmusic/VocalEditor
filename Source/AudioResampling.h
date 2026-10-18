#pragma once

#include <JuceHeader.h>
#include <cmath>

// Lightweight, allocation-free helpers for mapping source audio onto the host
// output timeline. Linear interpolation is a safe baseline; a band-limited
// interpolator is still required for production-quality downsampling.
namespace VocalAudioResampler
{
    inline double sourcePositionForOutputSample (double startTimeSeconds,
                                                int outputSampleIndex,
                                                double sourceSampleRate,
                                                double outputSampleRate) noexcept
    {
        if (! std::isfinite (startTimeSeconds) || ! std::isfinite (sourceSampleRate)
            || ! std::isfinite (outputSampleRate) || sourceSampleRate <= 0.0
            || outputSampleRate <= 0.0 || outputSampleIndex < 0)
            return 0.0;

        return (startTimeSeconds + static_cast<double> (outputSampleIndex) / outputSampleRate)
               * sourceSampleRate;
    }

    inline float interpolateLinear (const juce::AudioBuffer<float>& source,
                                    int channel,
                                    double sourcePosition) noexcept
    {
        const int numSamples = source.getNumSamples();
        if (numSamples <= 0 || channel < 0 || channel >= source.getNumChannels()
            || ! std::isfinite (sourcePosition))
            return 0.0f;

        sourcePosition = juce::jlimit (0.0, static_cast<double> (numSamples - 1), sourcePosition);
        const int index0 = static_cast<int> (sourcePosition);
        const int index1 = juce::jmin (index0 + 1, numSamples - 1);
        const float fraction = static_cast<float> (sourcePosition - static_cast<double> (index0));
        const float a = source.getSample (channel, index0);
        const float b = source.getSample (channel, index1);
        return a + (b - a) * fraction;
    }
}
