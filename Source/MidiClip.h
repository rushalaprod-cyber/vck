#pragma once
// Clip MIDI importe : les notes sont stockees en TEMPS (noires), pas en secondes.
// Le tempo du fichier est donc ignore : le clip suit toujours le BPM du DAW
// (ou le BPM interne en mode FREE), et peut etre joue a demi / double vitesse.
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <algorithm>

namespace vck
{
struct ClipNote { double start = 0, len = 0.25; int note = 60; float vel = 0.8f; bool selected = false; };

struct Clip
{
    std::vector<ClipNote> notes;
    double lengthBeats = 0;
    juce::String name;
    bool empty() const { return notes.empty(); }

    juce::String serialise() const
    {
        juce::String s;
        for (auto& n : notes)
            s << juce::String (n.start, 4) << "," << juce::String (n.len, 4) << "," << n.note << "," << juce::String (n.vel, 3) << ";";
        return s;
    }
    void deserialise (const juce::String& s, double len, const juce::String& nm)
    {
        notes.clear(); name = nm;
        for (auto& tok : juce::StringArray::fromTokens (s, ";", ""))
        {
            auto p = juce::StringArray::fromTokens (tok, ",", "");
            if (p.size() != 4) continue;
            notes.push_back ({ p[0].getDoubleValue(), p[1].getDoubleValue(), p[2].getIntValue(), (float) p[3].getDoubleValue(), false });
        }
        lengthBeats = len > 0 ? len : computeLength();
    }
    double computeLength() const
    {
        double end = 0;
        for (auto& n : notes) end = std::max (end, n.start + n.len);
        return std::max (4.0, std::ceil (end / 4.0 - 1e-6) * 4.0);   // arrondi a la mesure (4/4)
    }

    // Ecriture d'un .mid (960 ticks par noire, tempo du morceau)
    bool save (const juce::File& f, double bpm) const
    {
        juce::MidiMessageSequence seq;
        const double tpq = 960.0;
        auto tempo = juce::MidiMessage::tempoMetaEvent ((int) std::round (60000000.0 / std::max (1.0, bpm)));
        tempo.setTimeStamp (0);
        seq.addEvent (tempo);
        for (auto& n : notes)
        {
            seq.addEvent (juce::MidiMessage::noteOn (1, n.note, juce::jlimit (0.01f, 1.f, n.vel)), n.start * tpq);
            seq.addEvent (juce::MidiMessage::noteOff (1, n.note), (n.start + n.len) * tpq - 1.0);
        }
        seq.updateMatchedPairs();
        juce::MidiFile mf;
        mf.setTicksPerQuarterNote ((int) tpq);
        mf.addTrack (seq);
        f.deleteFile();
        juce::FileOutputStream os (f);
        return os.openedOk() && mf.writeTo (os);
    }

    // Lecture d'un .mid    // (toutes les pistes fusionnees, calees sur la premiere mesure)
    static bool load (const juce::File& f, Clip& out, juce::String& error)
    {
        juce::FileInputStream in (f);
        if (! in.openedOk()) { error = "Impossible d'ouvrir le fichier"; return false; }
        juce::MidiFile mf;
        if (! mf.readFrom (in)) { error = "Fichier MIDI invalide"; return false; }
        const short tf = mf.getTimeFormat();
        const bool smpte = tf <= 0;
        if (smpte) mf.convertTimestampTicksToSeconds();
        const double tpq = smpte ? 1.0 : (double) tf;

        Clip c; c.name = f.getFileNameWithoutExtension();
        for (int t = 0; t < mf.getNumTracks(); ++t)
        {
            juce::MidiMessageSequence seq (*mf.getTrack (t));
            seq.updateMatchedPairs();
            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                auto* e = seq.getEventPointer (i);
                if (! e->message.isNoteOn()) continue;
                const double on = e->message.getTimeStamp();
                double off = e->noteOffObject != nullptr ? e->noteOffObject->message.getTimeStamp() : on + tpq * 0.5;
                // en SMPTE, on suppose 120 BPM pour revenir en temps
                const double s = smpte ? on * 2.0 : on / tpq;
                const double l = smpte ? (off - on) * 2.0 : (off - on) / tpq;
                c.notes.push_back ({ s, std::max (0.02, l), e->message.getNoteNumber(), e->message.getFloatVelocity(), false });
            }
        }
        if (c.notes.empty()) { error = "Aucune note dans ce fichier"; return false; }
        double first = 1e9;
        for (auto& n : c.notes) first = std::min (first, n.start);
        const double shift = std::floor (first / 4.0 + 1e-6) * 4.0;
        for (auto& n : c.notes) n.start -= shift;
        std::sort (c.notes.begin(), c.notes.end(), [] (auto& a, auto& b) { return a.start < b.start; });
        c.lengthBeats = c.computeLength();
        out = std::move (c);
        return true;
    }
};

//==============================================================================
// Lecteur : genere des note-on / note-off precis a l'echantillon pres
class ClipPlayer
{
public:
    void reset() { sent.clear(); lastEnd = -1.0; events.reserve (8192); }

    // Coupe toutes les notes en cours
    void stopAll (juce::MidiBuffer& out, int pos = 0)
    {
        for (auto& p : sent) if (p >= 0) out.addEvent (juce::MidiMessage::noteOff (1, p), pos);
        std::fill (sent.begin(), sent.end(), -1);
        lastEnd = -1.0;
    }

    // beatStart : position (en noires) du debut du bloc ; beatsPerSample : avance par echantillon
    void process (const Clip& clip, double beatStart, double beatsPerSample, int numSamples,
                  int transpose, bool loop, juce::MidiBuffer& out, double gate = 1.0)
    {
        if (sent.size() != clip.notes.size()) { stopAll (out); sent.assign (clip.notes.size(), -1); }
        const double L = clip.lengthBeats;
        if (clip.empty() || L <= 0) return;
        const double beatEnd = beatStart + beatsPerSample * numSamples;

        events.clear();
        // saut de position (lecture relancee, boucle du DAW...) : on coupe et on rattrape
        const bool jump = lastEnd < 0 || std::abs (beatStart - lastEnd) > beatsPerSample * 4 + 1e-6;
        if (jump)
        {
            for (size_t i = 0; i < sent.size(); ++i)
                if (sent[i] >= 0) { events.push_back ({ 0, true, sent[i], 0.f, (int) i }); }
            if (beatStart >= 0 && (loop || beatStart < L))
            {
                const double p = loop ? std::fmod (beatStart, L) : beatStart;
                for (size_t i = 0; i < clip.notes.size(); ++i)
                {
                    auto& n = clip.notes[i];
                    if (n.start < p - 1e-9 && n.start + n.len * gate > p + 0.02)
                        events.push_back ({ 0, false, n.note, n.vel, (int) i });
                }
            }
        }
        lastEnd = beatEnd;

        // parcours du bloc, decoupe aux fins de boucle
        double a = beatStart;
        // intervalles semi-ouverts [a, b) enchaines sans tolerance : une note pile
        // sur la frontiere d'un bloc ou de la boucle est jouee une fois, jamais zero ni deux
        while (a < beatEnd)
        {
            if (a < 0) { a = std::min (beatEnd, 0.0); continue; }
            double cycle = loop ? std::floor (a / L) * L : 0.0;
            if (loop && cycle + L <= a) cycle += L;
            if (! loop && a >= L) break;
            double b = std::min (beatEnd, cycle + L);
            const double la = a - cycle, lb = b - cycle;
            for (size_t i = 0; i < clip.notes.size(); ++i)
            {
                auto& n = clip.notes[i];
                if (n.start >= la && n.start < lb)
                    events.push_back ({ toSample (cycle + n.start, beatStart, beatsPerSample, numSamples), false, n.note, n.vel, (int) i });
                double e = n.start + std::max (0.01, std::min (n.len * gate, L - 0.001));
                if (loop && e > L) e -= L;          // note qui deborde sur le tour suivant
                if (e >= la && e < lb)
                    events.push_back ({ toSample (cycle + e, beatStart, beatsPerSample, numSamples), true, n.note, 0.f, (int) i });
            }
            a = b;
        }
        if (! loop && beatEnd >= L)
            for (size_t i = 0; i < sent.size(); ++i) if (sent[i] >= 0) events.push_back ({ numSamples - 1, true, 0, 0.f, (int) i });

        std::stable_sort (events.begin(), events.end(), [] (const Ev& x, const Ev& y)
                          { return x.pos != y.pos ? x.pos < y.pos : (x.off && ! y.off); });
        for (auto& e : events)
        {
            auto& s = sent[(size_t) e.idx];
            if (e.off)
            {
                if (s >= 0) { out.addEvent (juce::MidiMessage::noteOff (1, s), e.pos); s = -1; }
            }
            else
            {
                if (s >= 0) out.addEvent (juce::MidiMessage::noteOff (1, s), e.pos);
                s = juce::jlimit (0, 127, e.note + transpose);
                out.addEvent (juce::MidiMessage::noteOn (1, s, juce::jlimit (0.05f, 1.f, e.vel)), e.pos);
            }
        }
    }

private:
    struct Ev { int pos; bool off; int note; float vel; int idx; };
    static int toSample (double beat, double beatStart, double bps, int n)
    {
        return juce::jlimit (0, n - 1, (int) std::floor ((beat - beatStart) / bps));
    }
    std::vector<int> sent;
    std::vector<Ev> events;
    double lastEnd = -1.0;
};
} // namespace vck
