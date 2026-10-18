#pragma once

#include <JuceHeader.h>

//==============================================================================
struct VocalEvent
{
    enum class Type { Phrase, Word, Syllable, Phoneme, Custom };
    enum class FadeCurve { Linear, Smooth };

    juce::Uuid id;
    Type type = Type::Custom;

    double startTime = 0.0;
    double endTime   = 0.0;

    float gainDb = 0.0f;

    double fadeInMs  = 0.0;
    double fadeOutMs = 0.0;
    FadeCurve fadeInCurve  = FadeCurve::Smooth;
    FadeCurve fadeOutCurve = FadeCurve::Smooth;

    bool locked = false;

    struct EnvelopePoint
    {
        float t = 0.0f;
        float gainDb = 0.0f;
    };

    juce::Array<EnvelopePoint> envelope;

    double getLength() const { return juce::jmax (0.0, endTime - startTime); }

    bool contains (double time) const
    {
        return time >= startTime && time < endTime;
    }

    bool overlaps (double otherStart, double otherEnd) const
    {
        return startTime < otherEnd && endTime > otherStart;
    }

    float getGainLinear() const
    {
        return juce::Decibels::decibelsToGain (gainDb);
    }

    void ensureEnvelope()
    {
        if (envelope.isEmpty())
        {
            envelope.add ({ 0.0f, gainDb });
            envelope.add ({ 1.0f, gainDb });
        }
    }

    float getEnvelopeGainDb (float t) const
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        if (envelope.isEmpty())
            return gainDb;
        if (t <= envelope.getFirst().t)
            return envelope.getFirst().gainDb;
        if (t >= envelope.getLast().t)
            return envelope.getLast().gainDb;

        for (int i = 1; i < envelope.size(); ++i)
        {
            const auto& a = envelope.getReference (i - 1);
            const auto& b = envelope.getReference (i);
            if (t <= b.t)
            {
                const float span = b.t - a.t;
                if (span < 1.0e-6f)
                    return b.gainDb;
                const float frac = (t - a.t) / span;
                return a.gainDb + frac * (b.gainDb - a.gainDb);
            }
        }
        return gainDb;
    }

    int addOrUpdateEnvelopePoint (float t, float gDb)
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        ensureEnvelope();

        for (int i = 0; i < envelope.size(); ++i)
        {
            if (std::abs (envelope.getReference (i).t - t) < 0.02f)
            {
                envelope.getReference (i).gainDb = gDb;
                return i;
            }
        }

        EnvelopePoint pt { t, gDb };
        int idx = 0;
        while (idx < envelope.size() && envelope.getReference (idx).t < t)
            ++idx;
        envelope.insert (idx, pt);
        return idx;
    }

    void removeEnvelopePoint (int index)
    {
        if (envelope.size() <= 2)
            return;
        if (index > 0 && index < envelope.size() - 1)
            envelope.remove (index);
    }

    juce::String getTypeName() const
    {
        switch (type)
        {
            case Type::Phrase:   return "Phrase";
            case Type::Word:     return "Word";
            case Type::Syllable: return "Syllable";
            case Type::Phoneme:  return "Phoneme";
            default:             return "Custom";
        }
    }
};

//==============================================================================
class EventList
{
public:
    EventList() = default;

    void clear() { events.clear(); }
    int size() const { return events.size(); }
    bool isEmpty() const { return events.isEmpty(); }

    VocalEvent* get (int index) { return events[index]; }
    const VocalEvent* get (int index) const { return events[index]; }

    VocalEvent* getById (const juce::Uuid& id)
    {
        for (auto* e : events)
            if (e->id == id)
                return e;
        return nullptr;
    }

    const juce::OwnedArray<VocalEvent>& getAll() const { return events; }
    juce::OwnedArray<VocalEvent>& getAll() { return events; }

    VocalEvent* add (double start, double end, VocalEvent::Type type = VocalEvent::Type::Custom)
    {
        auto* e = events.add (new VocalEvent());
        e->id = juce::Uuid();
        e->startTime = juce::jmin (start, end);
        e->endTime   = juce::jmax (start, end);
        e->type = type;
        return e;
    }

    void remove (int index) { events.remove (index); }

    void removeById (const juce::Uuid& id)
    {
        for (int i = events.size(); --i >= 0;)
            if (events[i]->id == id)
                events.remove (i);
    }

    juce::Array<VocalEvent*> getInRange (double start, double end)
    {
        juce::Array<VocalEvent*> result;
        for (auto* e : events)
            if (e->overlaps (start, end))
                result.add (e);

        struct
        {
            int compareElements (const VocalEvent* first, const VocalEvent* second) const
            {
                if (first->startTime < second->startTime) return -1;
                if (first->startTime > second->startTime) return 1;
                return 0;
            }
        } comparator;

        result.sort (comparator);
        return result;
    }

    VocalEvent* getAtTime (double time)
    {
        for (auto* e : events)
            if (e->contains (time))
                return e;
        return nullptr;
    }

private:
    juce::OwnedArray<VocalEvent> events;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EventList)
};
