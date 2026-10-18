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

    // Both fades reach their exact endpoints, including very short regions.
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
}
