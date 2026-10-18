#pragma once

#include <JuceHeader.h>
#include "VocalEvent.h"
#include <cmath>

namespace VocalAudioMath
{
    inline bool intervalsOverlap (double startA, double endA,
                                  double startB, double endB,
                                  double tolerance = 1.0e-6) noexcept
    {
        if (! std::isfinite (startA) || ! std::isfinite (endA)
            || ! std::isfinite (startB) || ! std::isfinite (endB)
            || endA <= startA || endB <= startB)
            return false;

        return startA < endB - tolerance && startB < endA - tolerance;
    }

    inline double fadeCurveValue (double progress, VocalEvent::FadeCurve curve) noexcept
    {
        const double x = juce::jlimit (0.0, 1.0, progress);
        return curve == VocalEvent::FadeCurve::Smooth ? x * x * (3.0 - 2.0 * x) : x;
    }

    inline int clampFadeSamples (int requestedSamples, int eventLength) noexcept
    {
        return juce::jlimit (0, juce::jmax (0, eventLength), requestedSamples);
    }

    inline void fitFadesToEvent (int eventLength, int& fadeInSamples, int& fadeOutSamples) noexcept
    {
        eventLength = juce::jmax (0, eventLength);
        fadeInSamples = clampFadeSamples (fadeInSamples, eventLength);
        fadeOutSamples = clampFadeSamples (fadeOutSamples, eventLength);

        const int total = fadeInSamples + fadeOutSamples;
        if (total > eventLength && total > 0)
        {
            const double scale = static_cast<double> (eventLength) / static_cast<double> (total);
            fadeInSamples = static_cast<int> (std::floor (fadeInSamples * scale));
            fadeOutSamples = eventLength - fadeInSamples;
        }
    }

    inline float fadeGainAtSample (int localSample,
                                   int eventLength,
                                   int fadeInSamples,
                                   int fadeOutSamples,
                                   VocalEvent::FadeCurve fadeInCurve,
                                   VocalEvent::FadeCurve fadeOutCurve) noexcept
    {
        if (eventLength <= 0 || localSample < 0 || localSample >= eventLength)
            return 0.0f;

        double gain = 1.0;
        if (fadeInSamples > 0 && localSample < fadeInSamples)
        {
            const double progress = fadeInSamples <= 1
                ? 0.0
                : static_cast<double> (localSample) / static_cast<double> (fadeInSamples - 1);
            gain *= fadeCurveValue (progress, fadeInCurve);
        }

        if (fadeOutSamples > 0 && localSample >= eventLength - fadeOutSamples)
        {
            const int remaining = eventLength - 1 - localSample;
            const double progress = fadeOutSamples <= 1
                ? 0.0
                : static_cast<double> (remaining) / static_cast<double> (fadeOutSamples - 1);
            gain *= fadeCurveValue (progress, fadeOutCurve);
        }

        return static_cast<float> (gain);
    }

    // Applies a single event to an already-copied audio buffer. This function
    // is shared by the processor and signal-level tests.
    inline void applyEventToBuffer (const VocalEvent& event,
                                    juce::AudioBuffer<float>& target,
                                    double sampleRate) noexcept
    {
        if (target.getNumSamples() <= 0 || target.getNumChannels() <= 0
            || ! std::isfinite (sampleRate) || sampleRate <= 0.0
            || ! std::isfinite (event.startTime) || ! std::isfinite (event.endTime)
            || event.endTime <= event.startTime)
            return;

        const int totalSamples = target.getNumSamples();
        const int startSample = juce::jlimit (
            0, totalSamples, static_cast<int> (std::floor (event.startTime * sampleRate)));
        const int endSample = juce::jlimit (
            startSample, totalSamples, static_cast<int> (std::ceil (event.endTime * sampleRate)));
        const int eventLength = endSample - startSample;
        if (eventLength <= 0)
            return;

        int fadeInSamples = static_cast<int> (std::round (event.fadeInMs * 0.001 * sampleRate));
        int fadeOutSamples = static_cast<int> (std::round (event.fadeOutMs * 0.001 * sampleRate));
        fitFadesToEvent (eventLength, fadeInSamples, fadeOutSamples);

        for (int sample = startSample; sample < endSample; ++sample)
        {
            const int localSample = sample - startSample;
            const double normalized = eventLength > 1
                ? static_cast<double> (localSample) / static_cast<double> (eventLength - 1)
                : 0.0;
            const float envelopeDb = event.envelope.isEmpty()
                ? event.gainDb
                : event.getEnvelopeGainDb (static_cast<float> (normalized));
            const float gain = juce::Decibels::decibelsToGain (envelopeDb);
            const float fadeGain = fadeGainAtSample (
                localSample, eventLength, fadeInSamples, fadeOutSamples,
                event.fadeInCurve, event.fadeOutCurve);
            const float totalGain = gain * fadeGain;

            for (int channel = 0; channel < target.getNumChannels(); ++channel)
                target.setSample (channel, sample, target.getSample (channel, sample) * totalGain);
        }
    }
}
