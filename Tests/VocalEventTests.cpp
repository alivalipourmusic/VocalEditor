#include "VocalEvent.h"

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

    if (failures == 0)
    {
        std::cout << "All VocalEvent tests passed.\n";
        return 0;
    }

    std::cerr << failures << " VocalEvent test(s) failed.\n";
    return 1;
}
