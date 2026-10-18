#include "VocalEvent.h"
#include "AudioResampling.h"
#include "AudioProcessingMath.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace
{
    bool near (float actual, float expected, float tolerance = 1.0e-4f)
    {
        return std::abs (actual - expected) <= tolerance;
    }

    int failures = 0;

    void check (bool condition, const char* message)
    {
        if (! condition)
        {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    }

    void testEnvelopeSlicePreservesInterpolatedBoundaries()
    {
        VocalEvent event;
        event.gainDb = 0.0f;
        event.envelope.add ({ 0.0f, 0.0f });
        event.envelope.add ({ 0.5f, -6.0f });
        event.envelope.add ({ 1.0f, 0.0f });

        const auto slice = event.makeEnvelopeSlice (0.25f, 0.75f);
        check (slice.size() == 3, "slice contains both interpolated boundaries and interior point");
        if (slice.size() == 3)
        {
            check (near (slice[0].t, 0.0f), "left boundary is normalised to zero");
            check (near (slice[0].gainDb, -3.0f), "left boundary gain is interpolated");
            check (near (slice[1].t, 0.5f), "interior point is remapped");
            check (near (slice[1].gainDb, -6.0f), "interior point gain is preserved");
            check (near (slice[2].t, 1.0f), "right boundary is normalised to one");
            check (near (slice[2].gainDb, -3.0f), "right boundary gain is interpolated");
        }
    }

    void testEnvelopeSliceAtExistingPoint()
    {
        VocalEvent event;
        event.envelope.add ({ 0.0f, 0.0f });
        event.envelope.add ({ 0.5f, -12.0f });
        event.envelope.add ({ 1.0f, 3.0f });

        const auto left = event.makeEnvelopeSlice (0.0f, 0.5f);
        const auto right = event.makeEnvelopeSlice (0.5f, 1.0f);
        check (left.size() == 2, "left slice has two endpoints when split at an existing point");
        check (right.size() == 2, "right slice has two endpoints when split at an existing point");

        if (left.size() == 2 && right.size() == 2)
        {
            check (near (left[1].gainDb, -12.0f), "left slice ends at original split gain");
            check (near (right[0].gainDb, -12.0f), "right slice starts at original split gain");
            check (near (left[0].t, 0.0f) && near (left[1].t, 1.0f), "left slice times are ordered and normalised");
            check (near (right[0].t, 0.0f) && near (right[1].t, 1.0f), "right slice times are ordered and normalised");
        }
    }

    void testSampleRateMappingAcrossRates()
    {
        const double sourcePosition = VocalAudioResampler::sourcePositionForOutputSample (
            0.0, 48000, 44100.0, 48000.0);
        check (std::abs (sourcePosition - 44100.0) < 1.0e-6,
               "one second of 48 kHz output maps to one second of 44.1 kHz source");

        const double offsetPosition = VocalAudioResampler::sourcePositionForOutputSample (
            2.0, 24000, 44100.0, 48000.0);
        check (std::abs (offsetPosition - 88200.0) < 1.0e-6,
               "resampling keeps timeline offsets in seconds");
    }

    void testLinearInterpolation()
    {
        juce::AudioBuffer<float> source (1, 3);
        source.setSample (0, 0, 0.0f);
        source.setSample (0, 1, 1.0f);
        source.setSample (0, 2, 3.0f);

        check (near (VocalAudioResampler::interpolateLinear (source, 0, 0.0), 0.0f),
               "interpolation returns the first sample at an integer position");
        check (near (VocalAudioResampler::interpolateLinear (source, 0, 1.5), 2.0f),
               "fractional sample positions are linearly interpolated");
        check (near (VocalAudioResampler::interpolateLinear (source, 0, 8.0), 3.0f),
               "positions past the end clamp to the last sample");
        check (near (VocalAudioResampler::interpolateLinear (source, 2, 1.0), 0.0f),
               "invalid channels return silence");
    }

    void testSignalLevelGainAndFadeRendering()
    {
        constexpr double sampleRate = 48000.0;
        constexpr int numSamples = 4800;
        juce::AudioBuffer<float> audio (1, numSamples);
        audio.clear();
        for (int i = 0; i < numSamples; ++i)
            audio.setSample (0, i, 0.5f);

        VocalEvent event;
        event.startTime = 0.025;
        event.endTime = 0.075;
        event.gainDb = -6.0f;

        VocalAudioMath::applyEventToBuffer (event, audio, sampleRate);

        const int start = 1200;
        const int end = 3600;
        const float expectedGain = juce::Decibels::decibelsToGain (-6.0f);
        check (near (audio.getSample (0, start), 0.5f * expectedGain),
               "event start receives the expected constant gain when fades are disabled");
        check (near (audio.getSample (0, start - 1), 0.5f),
               "samples before the event remain untouched");
        check (near (audio.getSample (0, end), 0.5f),
               "samples after the event remain untouched");

        double sumSquares = 0.0;
        float measuredPeak = 0.0f;
        for (int i = start + 100; i < end - 100; ++i)
        {
            const float value = audio.getSample (0, i);
            sumSquares += static_cast<double> (value) * value;
            measuredPeak = juce::jmax (measuredPeak, std::abs (value));
        }
        const float measuredRms = static_cast<float> (
            std::sqrt (sumSquares / static_cast<double> (end - start - 200)));
        check (near (measuredRms, 0.5f * expectedGain),
               "measured RMS matches the expected -6 dB gain");
        check (near (measuredPeak, 0.5f * expectedGain),
               "measured peak matches the expected -6 dB gain for a constant signal");

        juce::AudioBuffer<float> faded (1, numSamples);
        for (int i = 0; i < numSamples; ++i)
            faded.setSample (0, i, 0.5f);
        event.fadeInMs = 10.0;
        event.fadeOutMs = 10.0;
        VocalAudioMath::applyEventToBuffer (event, faded, sampleRate);

        check (near (faded.getSample (0, start), 0.0f),
               "rendered fade-in starts at silence");
        check (near (faded.getSample (0, start + 479), 0.5f * expectedGain),
               "rendered fade-in reaches target gain at its last sample");
        check (near (faded.getSample (0, end - 1), 0.0f),
               "rendered fade-out ends at silence");
    }

    void testMergeEnvelopeBoundaryCompatibility()
    {
        VocalEvent left;
        VocalEvent right;
        left.gainDb = -3.0f;
        right.gainDb = -3.0f;
        check (VocalAudioMath::canJoinEnvelopeAtBoundary (left, right),
               "flat events with matching gains can join");

        right.gainDb = -9.0f;
        check (! VocalAudioMath::canJoinEnvelopeAtBoundary (left, right),
               "flat events with a gain discontinuity cannot join");

        left.envelope.add ({ 0.0f, -12.0f });
        left.envelope.add ({ 0.5f, -6.0f });
        left.envelope.add ({ 1.0f, -2.0f });
        right.envelope.add ({ 0.0f, -2.0f });
        right.envelope.add ({ 1.0f, -8.0f });
        check (VocalAudioMath::canJoinEnvelopeAtBoundary (left, right),
               "custom envelopes with matching boundary gains can join");
        right.envelope.getReference (0).gainDb = -4.0f;
        check (! VocalAudioMath::canJoinEnvelopeAtBoundary (left, right),
               "custom envelopes with mismatched boundary gains cannot join");

        VocalEvent sparse;
        sparse.gainDb = 0.0f;
        sparse.envelope.add ({ 0.25f, -8.0f });
        sparse.envelope.add ({ 0.75f, -4.0f });
        const auto normalised = sparse.makeEnvelopeSlice (0.0f, 1.0f);
        check (normalised.size() == 4,
               "normalised envelope slice adds interpolated endpoints around sparse points");
        if (normalised.size() == 4)
        {
            check (near (normalised.getFirst().t, 0.0f)
                   && near (normalised.getLast().t, 1.0f),
                   "normalised envelope slice spans the complete event");
        }
    }

    void testRenderedEventRmsIncludesEnvelopeAndFades()
    {
        constexpr double sampleRate = 48000.0;
        juce::AudioBuffer<float> source (1, 4800);
        for (int i = 0; i < source.getNumSamples(); ++i)
            source.setSample (0, i, 0.5f);

        VocalEvent event;
        event.startTime = 0.025;
        event.endTime = 0.075;
        event.gainDb = -6.0f;

        const double flatRms = VocalAudioMath::measureRenderedEventRms (event, source, sampleRate);
        check (std::abs (flatRms - 0.5 * juce::Decibels::decibelsToGain (-6.0f)) < 1.0e-4,
               "rendered RMS includes the event's current constant gain");

        event.envelope.add ({ 0.0f, -12.0f });
        event.envelope.add ({ 1.0f, -12.0f });
        const double envelopeRms = VocalAudioMath::measureRenderedEventRms (event, source, sampleRate);
        check (std::abs (envelopeRms - 0.5 * juce::Decibels::decibelsToGain (-12.0f)) < 1.0e-4,
               "rendered RMS uses custom envelope gain rather than the base gain field");

        event.fadeInMs = 10.0;
        event.fadeOutMs = 10.0;
        const double fadedRms = VocalAudioMath::measureRenderedEventRms (event, source, sampleRate);
        check (fadedRms < envelopeRms,
               "rendered RMS accounts for fade-in and fade-out attenuation");
    }

    void testAudioAllocationPreflight()
    {
        check (VocalAudioMath::isReasonableAudioAllocation (2, 1000, 100000),
               "allocation preflight accepts a small stereo buffer");
        check (! VocalAudioMath::isReasonableAudioAllocation (0, 1000, 100000),
               "allocation preflight rejects zero channels");
        check (! VocalAudioMath::isReasonableAudioAllocation (2, 0, 100000),
               "allocation preflight rejects empty audio");
        check (! VocalAudioMath::isReasonableAudioAllocation (2, 1000, 1000),
               "allocation preflight rejects a buffer over the configured byte limit");
        check (! VocalAudioMath::isReasonableAudioAllocation (2,
                    static_cast<juce::int64> (std::numeric_limits<int>::max()) + 1,
                    std::numeric_limits<size_t>::max()),
               "allocation preflight rejects sample counts beyond AudioBuffer index limits");
    }

    void testAudioWorkingSetBudget()
    {
        check (VocalAudioMath::isReasonableAudioWorkingSet (2, 1000, 4, 100000),
               "working-set preflight accepts a small source plus three render buffers");
        check (! VocalAudioMath::isReasonableAudioWorkingSet (2, 1000, 4, 1000),
               "working-set preflight rejects aggregate buffers over budget");
        check (! VocalAudioMath::isReasonableAudioWorkingSet (2, 1000, 0, 100000),
               "working-set preflight rejects zero buffer copies");
        check (! VocalAudioMath::isReasonableAudioWorkingSet (2,
                    static_cast<juce::int64> (std::numeric_limits<int>::max()) + 1,
                    4, std::numeric_limits<size_t>::max()),
               "working-set preflight rejects invalid sample counts");
    }

    void testTimelineValidation()
    {
        const std::vector<std::pair<double, double>> valid {
            { 0.0, 0.5 }, { 0.5, 1.0 }, { 1.2, 1.5 }
        };
        check (VocalAudioMath::isValidNonOverlappingTimeline (valid, 2.0),
               "timeline accepts valid, touching and separated events");

        const std::vector<std::pair<double, double>> overlapping {
            { 0.0, 0.6 }, { 0.5, 0.9 }
        };
        check (! VocalAudioMath::isValidNonOverlappingTimeline (overlapping, 2.0),
               "timeline rejects overlapping events");

        const std::vector<std::pair<double, double>> outOfRange {
            { 0.0, 1.1 }
        };
        check (! VocalAudioMath::isValidNonOverlappingTimeline (outOfRange, 1.0),
               "timeline rejects events extending beyond source duration");

        const std::vector<std::pair<double, double>> invalid {
            { 0.5, 0.5 }
        };
        check (! VocalAudioMath::isValidNonOverlappingTimeline (invalid, 1.0),
               "timeline rejects zero-length events");

        const std::vector<std::pair<double, double>> nonFinite {
            { 0.0, std::numeric_limits<double>::infinity() }
        };
        check (! VocalAudioMath::isValidNonOverlappingTimeline (nonFinite, 1.0),
               "timeline rejects non-finite event endpoints");
    }

    void testIntervalOverlapRules()
    {
        check (VocalAudioMath::intervalsOverlap (0.0, 1.0, 0.5, 1.5),
               "intersecting event intervals are detected");
        check (! VocalAudioMath::intervalsOverlap (0.0, 1.0, 1.0, 2.0),
               "touching event boundaries are allowed");
        check (! VocalAudioMath::intervalsOverlap (0.0, 1.0, 1.00001, 2.0),
               "separated intervals do not overlap");
        check (! VocalAudioMath::intervalsOverlap (0.0, 0.0, 0.0, 1.0),
               "zero-length intervals are invalid and do not overlap");
    }

    void testFadeEndpointsAndShortEvents()
    {
        int fadeIn = 8, fadeOut = 8;
        VocalAudioMath::fitFadesToEvent (10, fadeIn, fadeOut);
        check (fadeIn + fadeOut == 10, "overlapping fades are proportionally fitted to the event");
        check (fadeIn >= 0 && fadeOut >= 0, "fitted fade lengths never become negative");

        check (near (VocalAudioMath::fadeGainAtSample (
                         0, 8, 4, 3, VocalEvent::FadeCurve::Linear, VocalEvent::FadeCurve::Linear),
                     0.0f),
               "fade-in starts at exact silence");
        check (near (VocalAudioMath::fadeGainAtSample (
                         3, 8, 4, 3, VocalEvent::FadeCurve::Linear, VocalEvent::FadeCurve::Linear),
                     1.0f),
               "fade-in reaches unity at its last sample when not overlapped");
        check (near (VocalAudioMath::fadeGainAtSample (
                         5, 8, 4, 3, VocalEvent::FadeCurve::Linear, VocalEvent::FadeCurve::Linear),
                     1.0f),
               "fade-out begins at unity at its first sample");
        check (near (VocalAudioMath::fadeGainAtSample (
                         7, 8, 4, 3, VocalEvent::FadeCurve::Linear, VocalEvent::FadeCurve::Linear),
                     0.0f),
               "fade-out reaches exact silence at the last sample");

        check (near (VocalAudioMath::fadeGainAtSample (
                         0, 1, 1, 0, VocalEvent::FadeCurve::Smooth, VocalEvent::FadeCurve::Smooth),
                     0.0f),
               "one-sample fade-in avoids division by zero and reaches silence");
        check (near (VocalAudioMath::fadeGainAtSample (
                         0, 1, 0, 1, VocalEvent::FadeCurve::Smooth, VocalEvent::FadeCurve::Smooth),
                     0.0f),
               "one-sample fade-out avoids division by zero and reaches silence");
    }

    void testEmptyEnvelopeAndInvalidRanges()
    {
        VocalEvent event;
        event.gainDb = -4.0f;

        const auto whole = event.makeEnvelopeSlice (0.0f, 1.0f);
        check (whole.size() == 2, "flat event creates two slice endpoints");
        if (whole.size() == 2)
        {
            check (near (whole[0].gainDb, -4.0f), "flat envelope left endpoint uses event gain");
            check (near (whole[1].gainDb, -4.0f), "flat envelope right endpoint uses event gain");
        }

        const auto zeroWidth = event.makeEnvelopeSlice (0.5f, 0.5f);
        check (zeroWidth.isEmpty(), "zero-width range returns an empty slice");

        const auto reversed = event.makeEnvelopeSlice (0.75f, 0.25f);
        check (reversed.isEmpty(), "reversed range returns an empty slice");
    }
}

int main()
{
    testEnvelopeSlicePreservesInterpolatedBoundaries();
    testEnvelopeSliceAtExistingPoint();
    testEmptyEnvelopeAndInvalidRanges();
    testSampleRateMappingAcrossRates();
    testLinearInterpolation();
    testFadeEndpointsAndShortEvents();
    testIntervalOverlapRules();
    testTimelineValidation();
    testAudioAllocationPreflight();
    testAudioWorkingSetBudget();
    testMergeEnvelopeBoundaryCompatibility();
    testSignalLevelGainAndFadeRendering();
    testRenderedEventRmsIncludesEnvelopeAndFades();

    if (failures == 0)
    {
        std::cout << "All VocalEvent tests passed.\n";
        return 0;
    }

    std::cerr << failures << " VocalEvent test(s) failed.\n";
    return 1;
}
