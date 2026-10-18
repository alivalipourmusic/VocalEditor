#pragma once

#include <JuceHeader.h>
#include "VocalEvent.h"
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace VocalAudioMath
{
    // Convert timeline seconds to a bounded sample index without casting an
    // arbitrarily large finite double to int (which can overflow/trigger UB).
    inline int timeToSampleIndex (double seconds, double sampleRate, int totalSamples,
                                  bool roundUp = false) noexcept
    {
        if (totalSamples <= 0 || ! std::isfinite (seconds) || seconds <= 0.0
            || ! std::isfinite (sampleRate) || sampleRate <= 0.0)
            return 0;

        const double duration = static_cast<double> (totalSamples) / sampleRate;
        if (! std::isfinite (duration) || seconds >= duration)
            return totalSamples;

        const double position = seconds * sampleRate;
        if (! std::isfinite (position) || position <= 0.0)
            return 0;

        const double rounded = roundUp ? std::ceil (position) : std::floor (position);
        return juce::jlimit (0, totalSamples, static_cast<int> (rounded));
    }

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

    inline bool isSupportedSampleRate (double sampleRate) noexcept
    {
        return std::isfinite (sampleRate) && sampleRate >= 1000.0 && sampleRate <= 768000.0;
    }

    // Check full-buffer allocation sizes before asking AudioBuffer to allocate.
    inline bool isReasonableAudioAllocation (int channels,
                                               juce::int64 samples,
                                               size_t maxBytes = (size_t) 2 * 1024 * 1024 * 1024ULL) noexcept
    {
        if (channels <= 0 || samples <= 0
            || samples > static_cast<juce::int64> (std::numeric_limits<int>::max()))
            return false;

        const auto channelCount = static_cast<size_t> (channels);
        const auto sampleCount = static_cast<size_t> (samples);
        if (sampleCount > std::numeric_limits<size_t>::max() / channelCount)
            return false;

        const auto totalSamples = channelCount * sampleCount;
        if (totalSamples > std::numeric_limits<size_t>::max() / sizeof (float))
            return false;

        return totalSamples * sizeof (float) <= maxBytes;
    }

    // The editor can retain one source buffer plus three render snapshots.
    // Bound the aggregate working set, not just one allocation in isolation.
    inline bool isReasonableAudioWorkingSet (int channels,
                                              juce::int64 samples,
                                              size_t bufferCopies = 4,
                                              size_t maxBytes = (size_t) 4 * 1024 * 1024 * 1024ULL) noexcept
    {
        if (bufferCopies == 0
            || ! isReasonableAudioAllocation (channels, samples,
                                               std::numeric_limits<size_t>::max()))
            return false;

        const auto channelCount = static_cast<size_t> (channels);
        const auto sampleCount = static_cast<size_t> (samples);
        const auto totalSamples = channelCount * sampleCount;
        const auto bytesPerBuffer = totalSamples * sizeof (float);

        if (bytesPerBuffer > std::numeric_limits<size_t>::max() / bufferCopies)
            return false;

        return bytesPerBuffer * bufferCopies <= maxBytes;
    }

    inline bool isValidNonOverlappingTimeline (
        const std::vector<std::pair<double, double>>& intervals,
        double duration,
        double tolerance = 1.0e-6) noexcept
    {
        if (! std::isfinite (duration) || duration < 0.0)
            return false;

        for (size_t i = 0; i < intervals.size(); ++i)
        {
            const auto [start, end] = intervals[i];
            if (! std::isfinite (start) || ! std::isfinite (end)
                || start < 0.0 || end <= start || end > duration + tolerance)
                return false;

            for (size_t j = i + 1; j < intervals.size(); ++j)
                if (intervalsOverlap (start, end, intervals[j].first, intervals[j].second, tolerance))
                    return false;
        }
        return true;
    }

    inline bool canJoinEnvelopeAtBoundary (const VocalEvent& left,
                                            const VocalEvent& right,
                                            float tolerance = 1.0e-4f) noexcept
    {
        const float leftGain = left.getEnvelopeGainDb (1.0f);
        const float rightGain = right.getEnvelopeGainDb (0.0f);
        return std::isfinite (leftGain) && std::isfinite (rightGain)
            && std::abs (leftGain - rightGain) <= tolerance;
    }

    inline double fadeCurveValue (double progress, VocalEvent::FadeCurve curve) noexcept
    {
        const double x = juce::jlimit (0.0, 1.0, progress);
        return curve == VocalEvent::FadeCurve::Smooth ? x * x * (3.0 - 2.0 * x) : x;
    }

    inline int millisecondsToSamplesClamped (double milliseconds, double sampleRate,
                                               int maxSamples) noexcept
    {
        if (maxSamples <= 0 || ! std::isfinite (milliseconds) || milliseconds <= 0.0
            || ! std::isfinite (sampleRate) || sampleRate <= 0.0)
            return 0;

        const double samples = milliseconds * 0.001 * sampleRate;
        if (! std::isfinite (samples) || samples >= static_cast<double> (maxSamples))
            return maxSamples;
        return juce::jlimit (0, maxSamples, static_cast<int> (std::round (samples)));
    }

    // Smoothly ramps an additional word-level gain offset at each event edge.
    // A zero transition disables that edge ramp; Smooth uses a cubic S-curve.
    inline float dynamicGainWindowAtSample (int localSample, int eventLength,
                                            int transitionInSamples,
                                            int transitionOutSamples) noexcept
    {
        if (eventLength <= 0 || localSample < 0 || localSample >= eventLength)
            return 0.0f;

        double window = 1.0;
        transitionInSamples = juce::jlimit (0, eventLength, transitionInSamples);
        transitionOutSamples = juce::jlimit (0, eventLength, transitionOutSamples);

        if (transitionInSamples > 0 && localSample < transitionInSamples)
        {
            const double progress = transitionInSamples <= 1 ? 0.0
                : static_cast<double> (localSample) / (transitionInSamples - 1);
            window = juce::jmin (window, fadeCurveValue (progress, VocalEvent::FadeCurve::Smooth));
        }

        if (transitionOutSamples > 0 && localSample >= eventLength - transitionOutSamples)
        {
            const int remaining = eventLength - 1 - localSample;
            const double progress = transitionOutSamples <= 1 ? 0.0
                : static_cast<double> (remaining) / (transitionOutSamples - 1);
            window = juce::jmin (window, fadeCurveValue (progress, VocalEvent::FadeCurve::Smooth));
        }

        return static_cast<float> (window);
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

        // The two individually valid fade lengths can still overflow int when
        // added (e.g. very long events near AudioBuffer's index limit).
        const juce::int64 total = static_cast<juce::int64> (fadeInSamples)
                                + static_cast<juce::int64> (fadeOutSamples);
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

    // Measure the RMS of the signal as this event currently renders, including
    // its envelope and fades. Used by Level Process so repeated passes converge
    // toward the target even when an event already has custom gain automation.
    inline double measureRenderedEventRms (const VocalEvent& event,
                                            const juce::AudioBuffer<float>& source,
                                            double sampleRate) noexcept
    {
        if (source.getNumSamples() <= 0 || source.getNumChannels() <= 0
            || ! std::isfinite (sampleRate) || sampleRate <= 0.0
            || ! std::isfinite (event.startTime) || ! std::isfinite (event.endTime)
            || event.endTime <= event.startTime)
            return 0.0;

        const int totalSamples = source.getNumSamples();
        const int start = timeToSampleIndex (event.startTime, sampleRate, totalSamples);
        const int end = juce::jmax (start,
            timeToSampleIndex (event.endTime, sampleRate, totalSamples, true));
        const int length = end - start;
        if (length <= 0)
            return 0.0;

        int fadeIn = millisecondsToSamplesClamped (event.fadeInMs, sampleRate, length);
        int fadeOut = millisecondsToSamplesClamped (event.fadeOutMs, sampleRate, length);
        fitFadesToEvent (length, fadeIn, fadeOut);

        const int transitionIn = millisecondsToSamplesClamped (event.gainTransitionInMs, sampleRate, length);
        const int transitionOut = millisecondsToSamplesClamped (event.gainTransitionOutMs, sampleRate, length);
        double sumSquares = 0.0;
        const auto channels = source.getNumChannels();
        for (int i = 0; i < length; ++i)
        {
            const double t = length > 1 ? static_cast<double> (i) / (length - 1) : 0.0;
            const float envelopeDb = event.getEnvelopeGainDb (static_cast<float> (t));
            const float dynamicWindow = dynamicGainWindowAtSample (i, length, transitionIn, transitionOut);
            const float gain = juce::Decibels::decibelsToGain (
                envelopeDb + event.dynamicGainDb * dynamicWindow);
            const float fade = fadeGainAtSample (i, length, fadeIn, fadeOut,
                                                  event.fadeInCurve, event.fadeOutCurve);
            const double scale = static_cast<double> (gain) * fade;
            for (int channel = 0; channel < channels; ++channel)
            {
                const double sample = static_cast<double> (source.getSample (channel, start + i)) * scale;
                sumSquares += sample * sample;
            }
        }

        const auto sampleCount = static_cast<double> (length) * channels;
        return sampleCount > 0.0 ? std::sqrt (sumSquares / sampleCount) : 0.0;
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
        const int startSample = timeToSampleIndex (event.startTime, sampleRate, totalSamples);
        const int endSample = juce::jmax (startSample,
            timeToSampleIndex (event.endTime, sampleRate, totalSamples, true));
        const int eventLength = endSample - startSample;
        if (eventLength <= 0)
            return;

        int fadeInSamples = millisecondsToSamplesClamped (event.fadeInMs, sampleRate, eventLength);
        int fadeOutSamples = millisecondsToSamplesClamped (event.fadeOutMs, sampleRate, eventLength);
        fitFadesToEvent (eventLength, fadeInSamples, fadeOutSamples);
        const int transitionInSamples = millisecondsToSamplesClamped (
            event.gainTransitionInMs, sampleRate, eventLength);
        const int transitionOutSamples = millisecondsToSamplesClamped (
            event.gainTransitionOutMs, sampleRate, eventLength);

        for (int sample = startSample; sample < endSample; ++sample)
        {
            const int localSample = sample - startSample;
            const double normalized = eventLength > 1
                ? static_cast<double> (localSample) / static_cast<double> (eventLength - 1)
                : 0.0;
            const float envelopeDb = event.envelope.isEmpty()
                ? event.gainDb
                : event.getEnvelopeGainDb (static_cast<float> (normalized));
            const float dynamicWindow = dynamicGainWindowAtSample (
                localSample, eventLength, transitionInSamples, transitionOutSamples);
            const float gain = juce::Decibels::decibelsToGain (
                envelopeDb + event.dynamicGainDb * dynamicWindow);
            const float fadeGain = fadeGainAtSample (
                localSample, eventLength, fadeInSamples, fadeOutSamples,
                event.fadeInCurve, event.fadeOutCurve);
            const float totalGain = gain * fadeGain;

            for (int channel = 0; channel < target.getNumChannels(); ++channel)
                target.setSample (channel, sample, target.getSample (channel, sample) * totalGain);
        }
    }
}
