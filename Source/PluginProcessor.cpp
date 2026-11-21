#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace
{
    using APF = AudioParameterFloat;
    using APC = AudioParameterChoice;
    using APB = AudioParameterBool;

    AudioParameterFloatAttributes fmt (std::function<String (float)> f)
    {
        return AudioParameterFloatAttributes().withStringFromValueFunction ([f] (float v, int) { return f (v); });
    }
    auto pct  = fmt ([] (float v) { return String (roundToInt (v * 100.f)) + " %"; });
    auto ms   = fmt ([] (float v) { return v < 10.f ? String (v, 1) + " ms" : (v < 1000.f ? String (roundToInt (v)) + " ms" : String (v / 1000.f, 2) + " s"); });
    auto db   = fmt ([] (float v) { return (v > 0.f ? "+" : "") + String (v, 1) + " dB"; });
    auto semi = fmt ([] (float v) { return (v > 0.f ? "+" : "") + String (v, 1) + " st"; });
    auto cent = fmt ([] (float v) { return String (roundToInt (v)) + " ct"; });

    NormalisableRange<float> skewed (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }
}

AudioProcessorValueTreeState::ParameterLayout VckProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> v;
    auto add = [&] (auto* p) { v.push_back (std::unique_ptr<RangedAudioParameter> (p)); };
    auto id = [] (const char* s) { return ParameterID { s, 1 }; };

    // ---- vocoder
    add (new APC (id ("bands"), "Bands", StringArray { "8", "12", "16", "24", "32", "48", "64" }, 3));
    add (new APF (id ("formant"), "Formant", NormalisableRange<float> (-12.f, 12.f, 0.01f), 0.f, semi));
    add (new APF (id ("attack"), "Attack", skewed (0.5f, 50.f, 5.f), 3.f, ms));
    add (new APF (id ("release"), "Release", skewed (5.f, 500.f, 60.f), 45.f, ms));
    add (new APF (id ("sharp"), "Sharpness", NormalisableRange<float> (0.f, 1.f), 0.55f, pct));
    add (new APF (id ("sibil"), "Sibilance", NormalisableRange<float> (0.f, 1.f), 0.5f, pct));
    add (new APF (id ("bright"), "Brightness", NormalisableRange<float> (-6.f, 12.f, 0.01f), 3.f, db));
    add (new APF (id ("gate"), "Gate", NormalisableRange<float> (-90.f, -20.f, 0.1f), -62.f, db));
    add (new APB (id ("freeze"), "Freeze", false));

    // ---- porteuse
    add (new APC (id ("source"), "Source", StringArray { "MIDI", "AUTO", "SIDECHAIN" }, 1));
    add (new APC (id ("wave"), "Wave", StringArray { "SAW", "SQUARE", "PULSE", "SUPER", "BUZZ", "NOISE" }, 0));
    add (new AudioParameterInt (id ("unison"), "Unison", 1, 7, 1));
    add (new APF (id ("detune"), "Detune", NormalisableRange<float> (0.f, 1.f), 0.25f, pct));
    add (new APC (id ("octave"), "Octave", StringArray { "-2", "-1", "0", "+1", "+2" }, 2));
    add (new APF (id ("glide"), "Glide", skewed (0.f, 1000.f, 120.f), 0.f, ms));
    add (new APC (id ("mode"), "Voice Mode", StringArray { "POLY", "MONO", "LEGATO" }, 0));
    add (new APF (id ("vibrato"), "Vibrato", NormalisableRange<float> (0.f, 1.f), 0.f, pct));
    add (new APF (id ("noise"), "Noise", NormalisableRange<float> (0.f, 1.f), 0.f, pct));
    add (new APF (id ("envA"), "Env Attack", skewed (1.f, 2000.f, 80.f), 5.f, ms));
    add (new APF (id ("envR"), "Env Release", skewed (5.f, 4000.f, 300.f), 150.f, ms));

    // ---- AUTO (la porteuse suit la voix)
    add (new APC (id ("key"), "Key", StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    add (new APC (id ("scale"), "Scale", StringArray { "LIBRE", "CHROMA", "MAJEUR", "MINEUR", "MIN HARM", "PENTA MAJ", "PENTA MIN", "DORIEN", "DRONE" }, 1));
    add (new APC (id ("harmony"), "Harmony", StringArray { "AUCUNE", "OCTAVE", "QUINTE", "ACCORD", "CHOEUR" }, 0));
    add (new APF (id ("retune"), "Retune", skewed (0.f, 400.f, 60.f), 25.f, ms));

    // ---- clip MIDI
    add (new APB (id ("clipOn"), "Clip Play", true));
    add (new APC (id ("sync"), "Sync", StringArray { "HOST", "FREE" }, 0));
    add (new APF (id ("bpm"), "BPM", NormalisableRange<float> (40.f, 240.f, 0.1f), 120.f,
                  fmt ([] (float x) { return String (x, 1) + " BPM"; })));
    add (new AudioParameterInt (id ("transpose"), "Transpose", -24, 24, 0));
    add (new APC (id ("rate"), "Rate", StringArray { "1/2", "1x", "2x" }, 1));
    add (new APB (id ("loop"), "Loop", true));

    // ---- doubleur
    add (new APC (id ("dblVoices"), "Doubler", StringArray { "OFF", "2 VOIX", "4 VOIX" }, 0));
    add (new APF (id ("dblDetune"), "Dbl Detune", NormalisableRange<float> (0.f, 50.f, 0.1f), 10.f, cent));
    add (new APF (id ("dblDelay"), "Dbl Delay", NormalisableRange<float> (0.f, 60.f, 0.1f), 16.f, ms));
    add (new APF (id ("dblWidth"), "Dbl Width", NormalisableRange<float> (0.f, 1.f), 0.85f, pct));
    add (new APF (id ("dblDrift"), "Dbl Drift", NormalisableRange<float> (0.f, 1.f), 0.5f, pct));
    add (new APF (id ("dblMix"), "Dbl Mix", NormalisableRange<float> (0.f, 1.f), 0.5f, pct));

    // ---- effets
    add (new APF (id ("ensemble"), "Ensemble", NormalisableRange<float> (0.f, 1.f), 0.f, pct));
    add (new APF (id ("delayMix"), "Delay", NormalisableRange<float> (0.f, 1.f), 0.f, pct));
    add (new APC (id ("delayTime"), "Delay Time", StringArray { "1/16", "1/8", "1/8 D", "1/4", "1/4 D", "1/2", "1/8 T" }, 2));
    add (new APF (id ("delayFb"), "Feedback", NormalisableRange<float> (0.f, 0.9f), 0.35f, pct));
    add (new APF (id ("reverb"), "Reverb", NormalisableRange<float> (0.f, 1.f), 0.12f, pct));
    add (new APF (id ("revSize"), "Room Size", NormalisableRange<float> (0.f, 1.f), 0.5f, pct));

    // ---- sortie
    add (new APF (id ("robot"), "Robot Level", NormalisableRange<float> (0.f, 1.f), 1.f, pct));
    add (new APF (id ("voice"), "Voice Level", NormalisableRange<float> (0.f, 1.f), 0.f, pct));
    add (new APF (id ("output"), "Output", NormalisableRange<float> (-24.f, 12.f, 0.1f), 0.f, db));
    add (new APB (id ("bypass"), "Bypass", false));

    return { v.begin(), v.end() };
}

VckProcessor::VckProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "VCK", createLayout()),
      presets (apvts)
{
    P = { p ("bands"), p ("formant"), p ("attack"), p ("release"), p ("sharp"), p ("sibil"), p ("bright"), p ("gate"), p ("freeze"),
          p ("source"), p ("wave"), p ("unison"), p ("detune"), p ("octave"), p ("glide"), p ("mode"), p ("vibrato"), p ("noise"), p ("envA"), p ("envR"),
          p ("key"), p ("scale"), p ("harmony"), p ("retune"),
          p ("clipOn"), p ("sync"), p ("bpm"), p ("transpose"), p ("rate"), p ("loop"),
          p ("dblVoices"), p ("dblDetune"), p ("dblDelay"), p ("dblWidth"), p ("dblDrift"), p ("dblMix"),
          p ("ensemble"), p ("delayMix"), p ("delayTime"), p ("delayFb"), p ("reverb"), p ("revSize"),
          p ("robot"), p ("voice"), p ("output"), p ("bypass") };
    for (auto& a : uiMod) a = 0.f;
    for (auto& a : uiCar) a = 0.f;
    for (auto& a : uiOut) a = 0.f;
    for (auto& a : uiFreq) a = 0.f;
    for (auto& a : uiNotes) a = 0u;
    for (auto& a : uiInNotes) a = 0u;
    presets.currentInfo = u8 (PresetManager::factory()[0].info);
}

bool VckProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    if (in != out && ! in.isDisabled()) return false;
    if (l.inputBuses.size() > 1)
    {
        const auto sc = l.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != AudioChannelSet::mono() && sc != AudioChannelSet::stereo()) return false;
    }
    return true;
}

void VckProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    vocoder.prepare (sampleRate);
    synth.prepare (sampleRate);
    tracker.prepare (sampleRate);
    doubler.prepare (sampleRate);
    ensemble.prepare (sampleRate);
    delay.prepare (sampleRate);
    reverb.prepare (sampleRate);
    player.reset();
    clipMidi.ensureSize (4096);
    dryBuf.setSize (2, std::max (1, samplesPerBlock));
    for (auto* s : { &outSm, &robotSm, &voiceSm, &bypassSm }) s->reset (sampleRate, 0.03);
    outSm.setCurrentAndTargetValue (vck::dbToGain (P.output->load()));
    robotSm.setCurrentAndTargetValue (P.robot->load());
    voiceSm.setCurrentAndTargetValue (P.voice->load());
    bypassSm.setCurrentAndTargetValue (P.bypass->load() > 0.5f ? 1.f : 0.f);
    ctrlCount = 0; freeBeat = 0.0; heldIn.fill (false);
    updateParams();
}

void VckProcessor::updateParams()
{
    static constexpr int bandCounts[7] = { 8, 12, 16, 24, 32, 48, 64 };
    vck::VocoderParams vp;
    vp.bands = bandCounts[jlimit (0, 6, (int) P.bands->load())];
    vp.formant = P.formant->load();
    vp.attackMs = P.attack->load();
    vp.releaseMs = P.release->load();
    vp.sharp = P.sharp->load();
    vp.sibilance = P.sibil->load();
    vp.bright = P.bright->load();
    vp.gateDb = P.gate->load();
    vp.freeze = P.freeze->load() > 0.5f;
    vocoder.setParams (vp);

    vck::SynthParams sp;
    sp.wave = (int) P.wave->load();
    sp.unison = (int) P.unison->load();
    sp.detune = P.detune->load();
    sp.octave = 12.f * ((float) (int) P.octave->load() - 2.f);
    sp.glideMs = P.glide->load();
    sp.mode = (int) P.mode->load();
    sp.vibrato = P.vibrato->load();
    sp.noise = P.noise->load();
    sp.attackMs = P.envA->load();
    sp.releaseMs = P.envR->load();
    synth.setParams (sp);
    synth.autoGlideMs = P.retune->load();

    vck::DoublerParams dp;
    dp.voices = (int) P.dblVoices->load();
    dp.detuneCents = P.dblDetune->load();
    dp.delayMs = P.dblDelay->load();
    dp.width = P.dblWidth->load();
    dp.drift = P.dblDrift->load();
    dp.mix = P.dblMix->load();
    doubler.setParams (dp);

    reverb.setSize (P.revSize->load());
}

void VckProcessor::handleMidi (const MidiMessage& m)
{
    if (m.isNoteOn())
    {
        heldIn[(size_t) m.getNoteNumber()] = true;
        synth.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    }
    else if (m.isNoteOff())
    {
        heldIn[(size_t) m.getNoteNumber()] = false;
        synth.noteOff (m.getNoteNumber());
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        heldIn.fill (false);
        synth.allNotesOff();
    }
}

void VckProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0) return;

    auto main = getBusBuffer (buffer, false, 0);
    const int nch = main.getNumChannels();
    if (nch == 0) { midi.clear(); return; }

    // sidechain (porteuse externe)
    const float* scL = nullptr; const float* scR = nullptr;
    if (auto* bus = getBus (true, 1); bus != nullptr && bus->isEnabled())
    {
        auto sc = getBusBuffer (buffer, true, 1);
        if (sc.getNumChannels() > 0)
        {
            scL = sc.getReadPointer (0);
            scR = sc.getNumChannels() > 1 ? sc.getReadPointer (1) : scL;
        }
    }
    uiScConnected = scL != nullptr;

    updateParams();
    const int source = (int) P.source->load();
    if (source != lastSource)
    {
        synth.setAuto (source == 1);
        synth.allNotesOff();
        lastSource = source;
    }

    // ---- tempo / transport
    double hostBpm = 0.0, ppq = 0.0; bool hasPpq = false, hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) hostBpm = *b;
            if (auto q = pos->getPpqPosition()) { ppq = *q; hasPpq = true; }
            hostPlaying = pos->getIsPlaying();
        }
    const bool syncHost = (int) P.sync->load() == 0;
    const double bpm = (syncHost && hostBpm > 1.0) ? hostBpm : (double) P.bpm->load();
    static constexpr double rates[3] = { 0.5, 1.0, 2.0 };
    const double rate = rates[jlimit (0, 2, (int) P.rate->load())];
    const bool useHostClock = syncHost && hasPpq;
    uiBpm = (float) bpm;
    uiHostSync = useHostClock;
    uiHostPlaying = hostPlaying;
    if (restartFree.exchange (false)) { freeBeat = 0.0; player.stopAll (clipMidi); }

    // ---- clip MIDI -> evenements
    clipMidi.clear();
    {
        const bool want = P.clipOn->load() > 0.5f && source == 0;
        const SpinLock::ScopedTryLockType lock (clipLock);
        if (lock.isLocked() && want && ! clip.empty())
        {
            const bool playing = useHostClock ? hostPlaying : true;
            const double beat = useHostClock ? ppq * rate : freeBeat;
            const double bps = bpm / 60.0 / fs * rate;
            if (playing)
            {
                player.process (clip, beat, bps, n, (int) P.transpose->load(), P.loop->load() > 0.5f, clipMidi);
                const bool loop = P.loop->load() > 0.5f;
                playBeat = (float) (loop ? std::fmod (std::max (0.0, beat), clip.lengthBeats) : beat);
            }
            else
            {
                if (wasPlaying) player.stopAll (clipMidi);
                playBeat = useHostClock ? (float) std::fmod (std::max (0.0, ppq * rate), clip.lengthBeats) : -1.f;
            }
            wasPlaying = playing;
        }
        else if (lock.isLocked() && wasPlaying)
        {
            player.stopAll (clipMidi);
            wasPlaying = false;
            playBeat = -1.f;
        }
    }
    freeBeat += (double) n * bpm / 60.0 / fs * rate;
    if (source == 0) clipMidi.addEvents (midi, 0, n, 0);
    else for (const auto meta : midi) { auto m = meta.getMessage(); if (m.isNoteOnOrOff()) heldIn[(size_t) m.getNoteNumber()] = m.isNoteOn(); }
    midi.clear();

    // ---- copie seche
    if (dryBuf.getNumSamples() < n) dryBuf.setSize (2, n, false, false, true);
    dryBuf.copyFrom (0, 0, main, 0, 0, n);
    dryBuf.copyFrom (1, 0, main, nch > 1 ? 1 : 0, 0, n);
    const float* dL = dryBuf.getReadPointer (0);
    const float* dR = dryBuf.getReadPointer (1);
    float* oL = main.getWritePointer (0);
    float* oR = nch > 1 ? main.getWritePointer (1) : nullptr;

    outSm.setTargetValue (vck::dbToGain (P.output->load()));
    robotSm.setTargetValue (P.robot->load());
    voiceSm.setTargetValue (P.voice->load());
    bypassSm.setTargetValue (P.bypass->load() > 0.5f ? 1.f : 0.f);

    static constexpr float divBeats[7] = { 0.25f, 0.5f, 0.75f, 1.f, 1.5f, 2.f, 1.f / 3.f };
    const float delaySec = divBeats[jlimit (0, 6, (int) P.delayTime->load())] * 60.f / (float) bpm;
    const float delayMix = P.delayMix->load(), delayFb = P.delayFb->load();
    const float ens = P.ensemble->load(), rev = P.reverb->load();
    const int key = (int) P.key->load(), scale = (int) P.scale->load(), harmony = (int) P.harmony->load();
    const bool noiseWave = (int) P.wave->load() == 5;
    const float gateLin = vck::dbToGain (P.gate->load());

    auto it = clipMidi.cbegin();
    const auto end = clipMidi.cend();
    float inPeak = 0.f, outPeak = 0.f;

    for (int i = 0; i < n; ++i)
    {
        while (it != end && (*it).samplePosition <= i) { handleMidi ((*it).getMessage()); ++it; }

        const float xL = dL[i], xR = dR[i];
        const float voice = nch > 1 ? 0.5f * (xL + xR) : xL;
        inPeak = std::max (inPeak, std::abs (voice));
        voiceLev = std::max (std::abs (voice), voiceLev * 0.9995f);

        // ---- AUTO : la porteuse suit la hauteur de la voix
        if (source == 1 && tracker.push (voice))
        {
            const bool loud = voiceLev > gateLin * 2.f;
            int count = 0;
            if (tracker.voiced && loud)
            {
                const float m = vck::ftom (tracker.freq);
                float q = vck::scales::quantize (m, key, scale);
                // hysteresis : on ne change de note que si on s'en eloigne vraiment
                if (scale != 0 && lastAutoNote >= 0.f && q != lastAutoNote && std::abs (m - lastAutoNote) < 0.62f) q = lastAutoNote;
                lastAutoNote = q;
                autoNotes[count++] = q;
                switch (harmony)
                {
                    case 1: autoNotes[count++] = q - 12.f; break;
                    case 2: autoNotes[count++] = vck::scales::degreeUp (q, 4, key, scale); break;
                    case 3: autoNotes[count++] = vck::scales::degreeUp (q, 2, key, scale);
                            autoNotes[count++] = vck::scales::degreeUp (q, 4, key, scale); break;
                    case 4: autoNotes[count++] = q - 12.f;
                            autoNotes[count++] = vck::scales::degreeUp (q, 2, key, scale);
                            autoNotes[count++] = vck::scales::degreeUp (q, 4, key, scale);
                            autoNotes[count++] = q + 12.f; break;
                    default: break;
                }
                autoHold = 0.09f;
                synth.setAutoTargets (autoNotes, count, true);
                uiTargetNote = q;
                uiPitchHz = tracker.freq;
            }
            else
            {
                autoHold -= 96.f * 4.f / (float) fs;
                if (noiseWave && loud)
                {   // souffle : suit le volume de la voix meme sans hauteur
                    autoNotes[0] = lastAutoNote >= 0.f ? lastAutoNote : 60.f;
                    synth.setAutoTargets (autoNotes, 1, true);
                }
                else if (autoHold <= 0.f) { synth.setAutoTargets (autoNotes, 0, false); uiPitchHz = 0.f; uiTargetNote = -1.f; }
            }
        }

        if (ctrlCount == 0) synth.control (vck::Vocoder::ctrl);
        if (++ctrlCount >= vck::Vocoder::ctrl) ctrlCount = 0;

        // ---- porteuse
        float cL = 0.f, cR = 0.f, clvl = 0.f;
        if (source == 2)
        {
            if (scL != nullptr) { cL = scL[i]; cR = scR[i]; }
            const float a = std::abs (0.5f * (cL + cR));
            scLevel = a + (a > scLevel ? 0.995f : 0.9997f) * (scLevel - a);
            scPeak = std::max (scLevel, scPeak * 0.99998f);
            clvl = std::min (1.f, scLevel / (scPeak * 0.5f + 1e-6f));
        }
        else
        {
            synth.render (cL, cR);
            clvl = synth.level();
        }

        // ---- vocoder
        float vL, vR;
        vocoder.process (voice, cL, cR, clvl, vL, vR);

        const float rob = robotSm.getNextValue(), vox = voiceSm.getNextValue();
        float wL = vL * rob + xL * vox;
        float wR = vR * rob + xR * vox;

        doubler.process (wL, wR);
        ensemble.process (wL, wR, ens);
        delay.process (wL, wR, delaySec, delayFb, delayMix);
        reverb.process (wL, wR, rev);

        const float g = outSm.getNextValue();
        wL *= g; wR *= g;
        // limiteur doux a 0 dBFS (jamais de saturation numerique)
        auto soft = [] (float x)
        {
            const float a = std::abs (x);
            if (a < 0.85f) return x;
            const float y = 0.85f + 0.15f * std::tanh ((a - 0.85f) / 0.15f);
            return std::copysign (y, x);
        };
        wL = soft (wL); wR = soft (wR);

        const float byp = bypassSm.getNextValue();
        const float yL = wL + (xL - wL) * byp, yR = wR + (xR - wR) * byp;
        outPeak = std::max (outPeak, std::max (std::abs (yL), std::abs (yR)));
        if (oR != nullptr) { oL[i] = yL; oR[i] = yR; }
        else oL[i] = 0.5f * (yL + yR);
    }
    while (it != end) { handleMidi ((*it).getMessage()); ++it; }

    // ---- donnees pour l'interface
    const int nb = vocoder.numBands();
    uiBands = nb;
    for (int b = 0; b < nb; ++b)
    {
        uiMod[(size_t) b] = vocoder.modLevel (b);
        uiCar[(size_t) b] = vocoder.carLevel (b);
        uiOut[(size_t) b] = vocoder.outGain (b);
        uiFreq[(size_t) b] = vocoder.bandFreq (b);
    }
    uiInLevel = std::max (inPeak, uiInLevel.load() * 0.85f);
    uiOutLevel = std::max (outPeak, uiOutLevel.load() * 0.85f);
    uiUnvoiced = vocoder.unvoiced;
    std::array<bool, 128> act {};
    synth.activeNotes (act);
    for (int w = 0; w < 4; ++w)
    {
        uint32_t bits = 0, inBits = 0;
        for (int k = 0; k < 32; ++k)
        {
            if (act[(size_t) (w * 32 + k)]) bits |= (1u << k);
            if (heldIn[(size_t) (w * 32 + k)]) inBits |= (1u << k);
        }
        uiNotes[(size_t) w] = bits;
        uiInNotes[(size_t) w] = inBits;
    }
    if (source != 1) { uiPitchHz = 0.f; uiTargetNote = -1.f; }
}

//==============================================================================
bool VckProcessor::loadMidi (const File& f, String& error)
{
    vck::Clip c;
    if (! vck::Clip::load (f, c, error)) return false;
    {
        const SpinLock::ScopedLockType l (clipLock);
        clip = std::move (c);
    }
    ++clipVersion;
    return true;
}

void VckProcessor::clearMidi()
{
    {
        const SpinLock::ScopedLockType l (clipLock);
        clip = vck::Clip();
    }
    ++clipVersion;
}

void VckProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    auto xml = state.createXml();
    auto c = getClipCopy();
    auto* cx = xml->createNewChildElement ("CLIP");
    cx->setAttribute ("name", c.name);
    cx->setAttribute ("len", c.lengthBeats);
    cx->setAttribute ("notes", c.serialise());
    xml->setAttribute ("presetName", presets.currentName);
    xml->setAttribute ("presetIndex", presets.currentFactory);
    copyXmlToBinary (*xml, dest);
}

void VckProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    if (auto* cx = xml->getChildByName ("CLIP"))
    {
        vck::Clip c;
        c.deserialise (cx->getStringAttribute ("notes"), cx->getDoubleAttribute ("len"), cx->getStringAttribute ("name"));
        {
            const SpinLock::ScopedLockType l (clipLock);
            clip = std::move (c);
        }
        ++clipVersion;
        xml->removeChildElement (cx, true);
    }
    presets.currentName = xml->getStringAttribute ("presetName", "Init");
    presets.currentFactory = xml->getIntAttribute ("presetIndex", -1);
    if (presets.currentFactory >= 0 && presets.currentFactory < (int) PresetManager::factory().size())
        presets.currentInfo = u8 (PresetManager::factory()[(size_t) presets.currentFactory].info);
    if (xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessorEditor* VckProcessor::createEditor() { return new VckEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VckProcessor(); }
