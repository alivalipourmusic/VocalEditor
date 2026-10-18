#include "VocalEvent.h"
#include "AudioResampling.h"
#include "AudioProcessingMath.h"

#include <cmath>
#include <iostream>

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

    if (failures == 0)
    {
        std::cout << "All VocalEvent tests passed.\n";
        return 0;
    }

    std::cerr << failures << " VocalEvent test(s) failed.\n";
    return 1;
}
