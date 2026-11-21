// Test hors DAW de vck :
//  - rendu de tous les presets sur une voix de synthese (pas de NaN, pas d'ecretage, pas de silence)
//  - lecteur de clip MIDI : notes au bon echantillon quel que soit le BPM
//  - mode AUTO : la note du robot suit la hauteur de la voix
//  - performance (facteur temps reel) et capture de l'interface (dist/apercu-interface.png)
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>

using namespace juce;

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("  ECHEC : %s\n", msg); ++failures; } } while (0)

// Voix de synthese : impulsions glottiques -> formants de voyelles, + consonnes "s"
static bool gWhisper = false;

struct FakeVoice
{
    double fs; double t = 0; float phase = 0; Random rng { 3 };
    std::array<vck::Svf, 3> f;
    float f0Base = 140.f;
    explicit FakeVoice (double sr) : fs (sr) {}
    float next()
    {
        const double sec = t / fs; t += 1;
        const int syl = (int) (sec / 0.32);
        const float within = (float) std::fmod (sec, 0.32) / 0.32f;
        static const float vowels[5][3] = { { 800, 1150, 2900 }, { 400, 2000, 2600 }, { 300, 2300, 3000 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
        const auto& v = vowels[syl % 5];
        for (int k = 0; k < 3; ++k) f[(size_t) k].set (v[k], 9.f, (float) fs);
        const float f0 = f0Base * std::exp2 (((syl * 7) % 12) / 12.f * 0.5f) * (1.f + 0.01f * std::sin ((float) sec * 33.f));
        phase += f0 / (float) fs;
        float src = 0.f;
        if (phase >= 1.f) { phase -= 1.f; src = 1.f; }
        const bool consonant = (syl % 3 == 2) && within < 0.25f;
        const bool silence = within > 0.9f;
        float y = 0.f;
        if (! consonant) { const float e = gWhisper ? (rng.nextFloat() - 0.5f) * 0.35f : src * 0.8f; for (auto& ff : f) y += ff.bp (e); }
        else y = rng.nextFloat() * 0.25f - 0.125f;
        if (silence) y *= 0.02f;
        return y * 0.9f;
    }
};

struct FakePlayHead : AudioPlayHead
{
    double bpm = 120, ppq = 0; bool playing = true;
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setBpm (bpm); p.setPpqPosition (ppq); p.setIsPlaying (playing);
        return p;
    }
};

static void setP (VckProcessor& p, const char* id, float v)
{
    if (auto* prm = dynamic_cast<RangedAudioParameter*> (p.apvts.getParameter (id)))
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static void writeWav (const File& f, const AudioBuffer<float>& b, double fs)
{
    f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<OutputStream> os (f.createOutputStream().release());
    if (os == nullptr) return;
    if (auto w = wav.createWriterFor (os.get(), fs, (unsigned) b.getNumChannels(), 24, {}, 0))
    {
        os.release();
        std::unique_ptr<AudioFormatWriter> writer (w);
        writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
}

// Rendu complet : voix + accord MIDI tenu (source MIDI) ou rien (AUTO)
static AudioBuffer<float> render (VckProcessor& p, double fs, double seconds, bool holdChord, FakePlayHead* ph = nullptr)
{
    const int block = 480, total = (int) (fs * seconds);
    AudioBuffer<float> out (2, total);
    FakeVoice voice (fs);
    AudioBuffer<float> buf (4, block);
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        buf.clear();
        for (int i = 0; i < n; ++i) { const float v = voice.next(); buf.setSample (0, i, v); buf.setSample (1, i, v); }
        MidiBuffer midi;
        if (holdChord && pos == 0) for (int nt : { 48, 55, 60, 64 }) midi.addEvent (MidiMessage::noteOn (1, nt, 0.9f), 0);
        AudioBuffer<float> view (buf.getArrayOfWritePointers(), p.getTotalNumInputChannels(), n);
        p.processBlock (view, midi);
        if (ph != nullptr) ph->ppq += n / fs * ph->bpm / 60.0;
        out.copyFrom (0, pos, buf, 0, 0, n);
        out.copyFrom (1, pos, buf, 1, 0, n);
    }
    return out;
}

// Intelligibilite : correlation moyenne (par trame) entre la forme spectrale de la voix
// et celle du robot, sur 18 bandes en tiers d'octave environ (300 Hz - 8 kHz)
static float formantMatch (const AudioBuffer<float>& out, double fs)
{
    const int order = 11, N = 1 << order;
    dsp::FFT fft (order);
    std::vector<float> win ((size_t) N);
    for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (2.f * vck::pi * (float) i / (float) N);
    FakeVoice v (fs);
    std::vector<float> voice ((size_t) out.getNumSamples());
    for (auto& x : voice) x = v.next();
    auto bandsOf = [&] (const float* x, std::array<float, 12>& b)
    {
        std::vector<float> buf ((size_t) N * 2, 0.f);
        for (int i = 0; i < N; ++i) buf[(size_t) i] = x[i] * win[(size_t) i];
        fft.performFrequencyOnlyForwardTransform (buf.data());
        for (int k = 0; k < 12; ++k)
        {
            // bandes d'une demi-octave : on compare l'enveloppe (les formants), pas les harmoniques
            const double f0 = 250.0 * std::pow (2.0, k * 0.4167), f1 = 250.0 * std::pow (2.0, (k + 1) * 0.4167);
            double e = 1e-12;
            for (int bin = (int) (f0 / fs * N); bin <= (int) (f1 / fs * N); ++bin) e += (double) buf[(size_t) bin] * buf[(size_t) bin];
            b[(size_t) k] = (float) (10.0 * std::log10 (e));
        }
    };
    // enveloppe moyennee sur le coeur de chaque voyelle (les fluctuations aleatoires d'une
    // trame isolee - bruit, battements d'harmoniques - ne disent rien de la qualite)
    const double syl = 0.32;
    const int nSyl = (int) (out.getNumSamples() / fs / syl);
    std::vector<std::array<double, 12>> pa ((size_t) nSyl), pb ((size_t) nSyl);
    for (int pos = (int) fs; pos + N < out.getNumSamples(); pos += N / 4)
    {
        const double c = (pos + N / 2) / fs;
        const int s = (int) (c / syl);
        const double within = std::fmod (c, syl) / syl;
        if (s >= nSyl || within < 0.35 || within > 0.8) continue;
        std::array<float, 12> a {}, b {};
        bandsOf (voice.data() + pos, a);
        bandsOf (out.getReadPointer (0) + pos, b);
        for (int k = 0; k < 12; ++k)
        {
            pa[(size_t) s][(size_t) k] += std::pow (10.0, a[(size_t) k] / 10.0);
            pb[(size_t) s][(size_t) k] += std::pow (10.0, b[(size_t) k] / 10.0);
        }
    }
    double sum = 0; int count = 0;
    for (int s = 0; s < nSyl; ++s)
    {
        if (pa[(size_t) s][0] <= 0.0) continue;
        std::array<double, 12> a {}, b {};
        double ma = 0, mb = 0;
        for (int k = 0; k < 12; ++k)
        {
            a[(size_t) k] = 10.0 * std::log10 (pa[(size_t) s][(size_t) k] + 1e-12); ma += a[(size_t) k];
            b[(size_t) k] = 10.0 * std::log10 (pb[(size_t) s][(size_t) k] + 1e-12); mb += b[(size_t) k];
        }
        ma /= 12; mb /= 12;
        double sab = 0, saa = 0, sbb = 0;
        for (int k = 0; k < 12; ++k)
        {
            const double da = a[(size_t) k] - ma, db = b[(size_t) k] - mb;
            sab += da * db; saa += da * da; sbb += db * db;
        }
        if (saa > 0 && sbb > 0) { sum += sab / std::sqrt (saa * sbb); ++count; }
        if (std::getenv ("VCK_VERBOSE") != nullptr && s < 8)
        {
            std::printf ("    syl %d : ", s);
            for (int k = 0; k < 12; ++k) std::printf ("%4.0f/%-4.0f", a[(size_t) k] - ma, b[(size_t) k] - mb);
            std::printf ("\n");
        }
    }
    return count > 0 ? (float) (sum / count) : 0.f;
}

static bool finiteAll (const AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i) if (! std::isfinite (b.getSample (c, i))) return false;
    return true;
}

int main()
{
    ScopedJuceInitialiser_GUI init;
    const double fs = 48000.0;
    auto outDir = File::getCurrentWorkingDirectory().getChildFile ("test-output");
    outDir.createDirectory();

    // ------------------------------------------------------------------ presets
    std::printf ("== Presets (%d)\n", (int) PresetManager::factory().size());
    {
        auto& list = PresetManager::factory();
        for (int i = 0; i < (int) list.size(); ++i)
        {
            VckProcessor p;
            p.enableAllBuses();
            p.setPlayConfigDetails (2, 2, fs, 480);
            p.prepareToPlay (fs, 480);
            p.presets.loadFactory (i);
            const int src = (int) p.apvts.getRawParameterValue ("source")->load();
            p.prepareToPlay (fs, 480);
            auto out = render (p, fs, 4.0, src == 0);
            const float peak = out.getMagnitude (0, out.getNumSamples());
            const float rms = out.getRMSLevel (0, (int) fs, out.getNumSamples() - (int) fs);
            const float match = formantMatch (out, fs);
            std::printf ("  %-28s src=%d  crete %5.2f dB  rms %6.1f dB  intelligibilite %.2f\n", list[(size_t) i].name, src,
                         vck::gainToDb (peak), vck::gainToDb (rms), match);
            CHECK (finiteAll (out), list[(size_t) i].name);
            CHECK (peak <= 1.0001f, "ecretage");
            CHECK (rms > vck::dbToGain (-40.f), "sortie trop faible / silencieuse");
            CHECK (rms < vck::dbToGain (-3.f), "sortie trop forte");
            const String nm (list[(size_t) i].name);
            if (i < 9 || nm.contains ("Doubleur 4") || nm.contains ("Woods") || nm.contains ("Hide"))
                writeWav (outDir.getChildFile (File::createLegalFileName (nm) + ".wav"), out, fs);
        }
    }

    // ------------------------------------------------------------------ vocoder seul (sans effets)
    std::printf ("== Intelligibilite du vocoder seul\n");
    {
        struct V { const char* name; std::vector<std::pair<const char*, float>> prm; bool chord; };
        const std::vector<V> variants =
        {
            { "MIDI saw 24b",              { { "source", 0 } }, true },
            { "MIDI saw 24b bright0 sib0", { { "source", 0 }, { "bright", 0 }, { "sibil", 0 } }, true },
            { "MIDI saw 48b",              { { "source", 0 }, { "bands", 5 } }, true },
            { "MIDI noise 48b",            { { "source", 0 }, { "bands", 5 }, { "wave", 5 } }, true },
            { "AUTO saw 24b",              { { "source", 1 } }, false },
            { "AUTO saw 64b sharp1",       { { "source", 1 }, { "bands", 6 }, { "sharp", 1 } }, false },
        };
        for (auto& v : variants)
        {
            VckProcessor p;
            p.setPlayConfigDetails (2, 2, fs, 480);
            p.prepareToPlay (fs, 480);
            setP (p, "reverb", 0);
            for (auto& kv : v.prm) setP (p, kv.first, kv.second);
            p.prepareToPlay (fs, 480);
            auto out = render (p, fs, 4.0, v.chord);
            const float m = formantMatch (out, fs);
            gWhisper = true;
            p.prepareToPlay (fs, 480);
            auto outW = render (p, fs, 4.0, v.chord);
            const float mw = formantMatch (outW, fs);
            gWhisper = false;
            std::printf ("  %-28s intelligibilite %.2f   (voix chuchotee : %.2f)\n", v.name, m, mw);
            // voix chuchotee = enveloppe de formants pure : le robot doit la suivre de pres
            if (v.chord) CHECK (mw > 0.8f, "le robot ne suit pas les formants");
            CHECK (m > 0.65f, "intelligibilite (voix chantee)");
        }
    }

    // ------------------------------------------------------------------ AUTO : suivi de la voix
    std::printf ("== AUTO (suivi de hauteur)\n");
    {
        vck::PitchTracker t; t.prepare (fs);
        for (float f0 : { 82.f, 110.f, 196.f, 330.f, 523.f })
        {
            float ph = 0.f, est = 0.f;
            for (int i = 0; i < (int) fs / 2; ++i)
            {
                ph += f0 / (float) fs; if (ph >= 1.f) ph -= 1.f;
                const float x = 0.3f * (std::sin (2.f * vck::pi * ph) + 0.5f * std::sin (4.f * vck::pi * ph) + 0.3f * std::sin (6.f * vck::pi * ph));
                if (t.push (x) && t.voiced) est = t.freq;
            }
            const float err = 1200.f * std::abs (std::log2 (est / f0));
            std::printf ("  %6.1f Hz -> %6.1f Hz (%4.1f cents)\n", f0, est, err);
            CHECK (err < 15.f, "hauteur mal detectee");
        }
        CHECK (vck::scales::quantize (61.4f, 0, 2) == 62.f, "quantification majeur");
        CHECK (vck::scales::quantize (63.2f, 9, 3) == 62.f || vck::scales::quantize (63.2f, 9, 3) == 64.f, "quantification mineur");
        CHECK (vck::scales::degreeUp (60.f, 2, 0, 2) == 64.f, "tierce majeure");
        CHECK (vck::scales::degreeUp (60.f, 4, 0, 2) == 67.f, "quinte");
    }

    // ------------------------------------------------------------------ clip MIDI + BPM
    std::printf ("== Clip MIDI (BPM)\n");
    {
        // fichier .mid de 2 mesures : 8 noires
        MidiMessageSequence seq;
        for (int k = 0; k < 8; ++k)
        {
            seq.addEvent (MidiMessage::noteOn (1, 60 + k, 0.8f), k * 480.0);
            seq.addEvent (MidiMessage::noteOff (1, 60 + k), k * 480.0 + 400.0);
        }
        seq.updateMatchedPairs();
        MidiFile mf; mf.setTicksPerQuarterNote (480); mf.addTrack (seq);
        auto midFile = outDir.getChildFile ("test.mid");
        midFile.deleteFile();
        { FileOutputStream os (midFile); mf.writeTo (os); }

        vck::Clip clip; String err;
        CHECK (vck::Clip::load (midFile, clip, err), "lecture du .mid");
        CHECK ((int) clip.notes.size() == 8, "nombre de notes");
        CHECK (std::abs (clip.lengthBeats - 8.0) < 1e-6, "longueur 2 mesures");

        for (double bpm : { 90.0, 128.0, 174.0 })
        {
            vck::ClipPlayer pl; pl.reset();
            const double bps = bpm / 60.0 / fs;
            std::vector<int> onsets;
            double beat = 0; long long sample = 0;
            const int blocks = (int) std::ceil (20.0 * 60.0 / bpm * fs / 480.0);   // 20 temps = 2,5 tours de clip
            for (int b = 0; b < blocks; ++b)
            {
                MidiBuffer mb;
                pl.process (clip, beat, bps, 480, 0, true, mb);
                for (const auto m : mb) if (m.getMessage().isNoteOn()) onsets.push_back ((int) (sample + m.samplePosition));
                beat += 480 * bps; sample += 480;
            }
            const double spb = 60.0 / bpm * fs;
            int worst = 0;
            for (size_t k = 0; k < onsets.size() && k < 16; ++k)
                worst = std::max (worst, std::abs (onsets[k] - (int) std::llround ((double) k * spb)));
            std::printf ("  %5.1f BPM : %d notes jouees, ecart max %d echantillon(s)\n", bpm, (int) onsets.size(), worst);
            CHECK (onsets.size() >= 16, "boucle du clip");
            CHECK (worst <= 1, "precision des notes");
        }

        // dans le plugin, avec un DAW simule
        VckProcessor p;
        p.setPlayConfigDetails (2, 2, fs, 480);
        p.prepareToPlay (fs, 480);
        FakePlayHead ph; ph.bpm = 100;
        p.setPlayHead (&ph);
        CHECK (p.loadMidi (midFile, err), "chargement dans le plugin");
        setP (p, "source", 0);
        auto out = render (p, fs, 5.0, false, &ph);
        const float rms = out.getRMSLevel (0, 0, out.getNumSamples());
        std::printf ("  plugin + DAW 100 BPM : rms %.1f dB\n", vck::gainToDb (rms));
        CHECK (rms > vck::dbToGain (-40.f), "le clip ne joue pas");
        writeWav (outDir.getChildFile ("clip-midi-100bpm.wav"), out, fs);

        // etat : le clip est sauve avec le projet
        MemoryBlock mb; p.getStateInformation (mb);
        VckProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        CHECK (q.getClipCopy().notes.size() == 8, "clip restaure depuis l'etat");
        p.setPlayHead (nullptr);
    }

    // ------------------------------------------------------------------ performance
    std::printf ("== Performance\n");
    {
        VckProcessor p;
        p.setPlayConfigDetails (2, 2, fs, 480);
        p.prepareToPlay (fs, 480);
        setP (p, "source", 0); setP (p, "bands", 6); setP (p, "wave", 3); setP (p, "unison", 7);
        setP (p, "dblVoices", 2); setP (p, "ensemble", 0.5f); setP (p, "delayMix", 0.3f); setP (p, "reverb", 0.4f);
        const auto t0 = Time::getMillisecondCounterHiRes();
        render (p, fs, 10.0, true);
        const double ms = Time::getMillisecondCounterHiRes() - t0;
        std::printf ("  64 bandes, 4 notes x 7 osc, doubleur 4 voix, effets : 10 s en %.0f ms (x%.0f temps reel)\n", ms, 10000.0 / ms);
        CHECK (ms < 4000.0, "trop lent");
    }

    // ------------------------------------------------------------------ interface
    std::printf ("== Interface\n");
    {
        VckProcessor p;
        p.setPlayConfigDetails (2, 2, fs, 480);
        p.prepareToPlay (fs, 480);
        p.presets.loadFactory (3);
        String err;
        p.loadMidi (outDir.getChildFile ("test.mid"), err);
        setP (p, "source", 0);
        render (p, fs, 1.0, true);
        std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (1240, 880);
        for (int i = 0; i < 30; ++i) MessageManager::getInstance()->runDispatchLoopUntil (20);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto png = File::getCurrentWorkingDirectory().getChildFile ("dist").getChildFile ("apercu-interface.png");
        png.getParentDirectory().createDirectory();
        png.deleteFile();
        FileOutputStream os (png);
        PNGImageFormat().writeImageToStream (img, os);
        std::printf ("  capture : %s\n", png.getFullPathName().toRawUTF8());
        ed.reset();
    }

    std::printf (failures == 0 ? "\nTOUS LES TESTS PASSENT\n" : "\n%d ECHEC(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
