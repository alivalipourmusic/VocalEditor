#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    A single editable vocal region / event.
    Phase 2 core data model.
*/
struct VocalEvent
{
    enum class Type
    {
        Phrase,
        Word,
        Syllable,
        Phoneme,
        Custom
    };

    enum class FadeCurve
    {
        Linear,
        Smooth
    };

    juce::Uuid id;
    Type type = Type::Custom;

    double startTime = 0.0;   // seconds
    double endTime   = 0.0;

    float gainDb = 0.0f;      // relative gain in dB

    double fadeInMs  = 0.0;   // milliseconds
    double fadeOutMs = 0.0;
    FadeCurve fadeInCurve  = FadeCurve::Smooth;
    FadeCurve fadeOutCurve = FadeCurve::Smooth;

    bool locked = false;

    //--------------------------------------------------------------------------
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
/**
    Collection of events belonging to a project / file.
*/
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

    void remove (int index)
    {
        events.remove (index);
    }

    void removeById (const juce::Uuid& id)
    {
        for (int i = events.size(); --i >= 0;)
            if (events[i]->id == id)
                events.remove (i);
    }

    /** Returns events that overlap the given time range (sorted by start). */
    juce::Array<VocalEvent*> getInRange (double start, double end)
    {
        juce::Array<VocalEvent*> result;
        for (auto* e : events)
            if (e->overlaps (start, end))
                result.add (e);

        result.sort ([] (const VocalEvent* a, const VocalEvent* b)
                     { return a->startTime < b->startTime ? -1 : 1; });
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
