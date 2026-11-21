#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace
{
    using APF = AudioParameterFloat;
    using APC = AudioParameterChoice;
    using APB = AudioParameterBool;
    using API = AudioParameterInt;

    AudioParameterFloatAttributes fmt (std::function<String (float)> f)
    {
        return AudioParameterFloatAttributes().withStringFromValueFunction ([f] (float v, int) { return f (v); });
    }
    auto pct  = fmt ([] (float v) { return String (roundToInt (v * 100.f)) + " %"; });
    auto ms   = fmt ([] (float v) { return v < 10.f ? String (v, 1) + " ms" : (v < 1000.f ? String (roundToInt (v)) + " ms" : String (v / 1000.f, 2) + " s"); });
    auto db   = fmt ([] (float v) { return (v > 0.f ? "+" : "") + String (v, 1) + " dB"; });
    auto semi = fmt ([] (float v) { return (v > 0.f ? "+" : "") + String (v, 1) + " st"; });
    auto cent = fmt ([] (float v) { return (v > 0.f ? "+" : "") + String (v, 1) + " ct"; });
    auto hz   = fmt ([] (float v) { return v < 1000.f ? String (roundToInt (v)) + " Hz" : String (v / 1000.f, v < 10000.f ? 2 : 1) + " kHz"; });
    auto lfo  = fmt ([] (float v) { return String (v, 2) + " Hz"; });
    auto panf = fmt ([] (float v) { const int p = roundToInt (v * 100.f); return p == 0 ? String ("C") : (p < 0 ? "L" + String (-p) : "R" + String (p)); });
    auto offHz (float offAt, bool below)
    {
        return fmt ([offAt, below] (float v) { if (below ? v <= offAt : v >= offAt) return String ("OFF");
                                               return v < 1000.f ? String (roundToInt (v)) + " Hz" : String (v / 1000.f, 1) + " kHz"; });
    }

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
    auto unit = NormalisableRange<float> (0.f, 1.f);

    // ---- vocoder
    add (new APC (id ("bands"), "Bands", StringArray { "8", "12", "16", "24", "32", "48", "64" }, 3));
    add (new APF (id ("formant"), "Formant", NormalisableRange<float> (-12.f, 12.f, 0.01f), 0.f, semi));
    add (new APF (id ("attack"), "Attack", skewed (0.5f, 50.f, 5.f), 3.f, ms));
    add (new APF (id ("release"), "Release", skewed (5.f, 500.f, 60.f), 45.f, ms));
    add (new APF (id ("sharp"), "Sharpness", unit, 0.55f, pct));
    add (new APF (id ("sibil"), "Consonants", unit, 0.5f, pct));
    add (new APF (id ("breath"), "Breath", unit, 0.45f, pct));
    add (new APF (id ("bright"), "Brightness", NormalisableRange<float> (-6.f, 12.f, 0.01f), 3.f, db));
    add (new APF (id ("gate"), "Gate", NormalisableRange<float> (-90.f, -20.f, 0.1f), -62.f, db));
    add (new APB (id ("freeze"), "Freeze", false));
    add (new APF (id ("inGain"), "Input", NormalisableRange<float> (-24.f, 24.f, 0.1f), 0.f, db));
    add (new APF (id ("character"), "Character", unit, 0.1f, pct));
    add (new APF (id ("spread"), "Band Spread", unit, 0.f, pct));
    add (new APF (id ("loFreq"), "Low Band", skewed (40.f, 400.f, 100.f), 80.f, hz));
    add (new APF (id ("hiFreq"), "High Band", skewed (3000.f, 16000.f, 9000.f), 12000.f, hz));

    // ---- porteuse
    add (new APC (id ("source"), "Source", StringArray { "MIDI", "AUTO", "SIDECHAIN" }, 1));
    add (new APC (id ("wave"), "Wave", StringArray { "SAW", "SQUARE", "PULSE", "SUPER", "BUZZ", "NOISE" }, 0));
    add (new API (id ("unison"), "Unison", 1, 7, 1));
    add (new APF (id ("detune"), "Detune", unit, 0.25f, pct));
    add (new APC (id ("octave"), "Octave", StringArray { "-2", "-1", "0", "+1", "+2" }, 2));
    add (new APF (id ("glide"), "Glide", skewed (0.f, 1000.f, 120.f), 0.f, ms));
    add (new APC (id ("mode"), "Voice Mode", StringArray { "POLY", "MONO", "LEGATO" }, 0));
    add (new APF (id ("vibrato"), "Vibrato", unit, 0.f, pct));
    add (new APF (id ("vibRate"), "Vibrato Rate", NormalisableRange<float> (2.f, 9.f, 0.01f), 5.6f, lfo));
    add (new APF (id ("noise"), "Noise", unit, 0.f, pct));
    add (new APF (id ("envA"), "Env Attack", skewed (1.f, 2000.f, 80.f), 5.f, ms));
    add (new APF (id ("envR"), "Env Release", skewed (5.f, 4000.f, 300.f), 150.f, ms));
    add (new APF (id ("sub"), "Sub", unit, 0.f, pct));
    add (new APF (id ("velSens"), "Velocity", unit, 0.6f, pct));
    add (new API (id ("bendRange"), "Bend Range", 0, 12, 2));

    // ---- AUTO
    add (new APC (id ("key"), "Key", StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    add (new APC (id ("scale"), "Scale", StringArray { "LIBRE", "CHROMA", "MAJEUR", "MINEUR", "MIN HARM", "PENTA MAJ", "PENTA MIN", "DORIEN", "DRONE" }, 1));
    add (new APC (id ("harmony"), "Harmony", StringArray { "AUCUNE", "OCTAVE", "QUINTE", "ACCORD", "CHOEUR" }, 0));
    add (new APF (id ("retune"), "Retune", skewed (0.f, 400.f, 60.f), 25.f, ms));
    add (new APF (id ("humanize"), "Humanize", unit, 0.2f, pct));
    add (new API (id ("autoTrans"), "Auto Transpose", -12, 12, 0));
    add (new APF (id ("harmLevel"), "Harmony Level", unit, 0.7f, pct));
    add (new APF (id ("tracking"), "Tracking", unit, 0.4f, pct));
    add (new APF (id ("hold"), "Hold", NormalisableRange<float> (0.f, 500.f, 1.f), 90.f, ms));
    add (new APC (id ("voiceRange"), "Voice Range", StringArray { "GRAVE", "MEDIUM", "AIGUE", "LARGE" }, 1));
    add (new APB (id ("drone"), "Drone", false));
    add (new APF (id ("harmWidth"), "Harmony Width", unit, 0.6f, pct));

    // ---- clip MIDI
    add (new APB (id ("clipOn"), "Clip Play", true));
    add (new APC (id ("sync"), "Sync", StringArray { "HOST", "FREE" }, 0));
    add (new APF (id ("bpm"), "BPM", NormalisableRange<float> (40.f, 240.f, 0.1f), 120.f, fmt ([] (float x) { return String (x, 1) + " BPM"; })));
    add (new API (id ("transpose"), "Transpose", -24, 24, 0));
    add (new APC (id ("rate"), "Rate", StringArray { "1/2", "1x", "2x" }, 1));
    add (new APB (id ("loop"), "Loop", true));
    add (new APF (id ("clipGate"), "Clip Gate", NormalisableRange<float> (0.1f, 2.f, 0.01f), 1.f, pct));

    // ---- doubleur (facon Waves Doubler)
    add (new APC (id ("dblVoices"), "Doubler", StringArray { "OFF", "2 VOIX", "4 VOIX" }, 0));
    add (new APF (id ("dblMix"), "Dbl Mix", unit, 0.5f, pct));
    add (new APF (id ("dblDirect"), "Dbl Direct", unit, 1.f, pct));
    add (new APF (id ("dblWidth"), "Dbl Width", NormalisableRange<float> (0.f, 2.f), 1.f, pct));
    add (new APF (id ("dblDetune"), "Dbl Detune x", NormalisableRange<float> (0.f, 2.f), 1.f, pct));
    add (new APF (id ("dblDelay"), "Dbl Delay x", NormalisableRange<float> (0.25f, 2.f), 1.f, pct));
    add (new APF (id ("dblDrift"), "Dbl Drift", unit, 0.5f, pct));
    add (new APF (id ("dblRate"), "Dbl Rate", skewed (0.05f, 5.f, 0.8f), 0.8f, lfo));
    add (new APF (id ("dblHpf"), "Dbl HPF", skewed (20.f, 1000.f, 150.f), 120.f, offHz (20.f, true)));
    add (new APB (id ("dblLink"), "Dbl Link", false));
    add (new APF (id ("dblLpf"), "Dbl LPF", skewed (2000.f, 20000.f, 8000.f), 16000.f, offHz (20000.f, false)));
    {
        const float dly[4] = { 11.f, 17.f, 23.f, 7.f }, det[4] = { 9.f, -11.f, 5.f, -6.f }, pan[4] = { -1.f, 1.f, -0.55f, 0.55f }, gn[4] = { 0.f, 0.f, -2.f, -2.f };
        for (int i = 0; i < 4; ++i)
        {
            const String n (i + 1);
            add (new APF (ParameterID { "dblDelay" + n, 1 }, "Dbl V" + n + " Delay", skewed (1.f, 100.f, 20.f), dly[i], ms));
            add (new APF (ParameterID { "dblDetune" + n, 1 }, "Dbl V" + n + " Detune", NormalisableRange<float> (-50.f, 50.f, 0.1f), det[i], cent));
            add (new APF (ParameterID { "dblPan" + n, 1 }, "Dbl V" + n + " Pan", NormalisableRange<float> (-1.f, 1.f, 0.01f), pan[i], panf));
            add (new APF (ParameterID { "dblGain" + n, 1 }, "Dbl V" + n + " Gain", NormalisableRange<float> (-24.f, 6.f, 0.1f), gn[i], db));
        }
    }

    // ---- effets
    add (new APF (id ("drive"), "Drive", unit, 0.f, pct));
    add (new APF (id ("lowCut"), "Low Cut", skewed (20.f, 1000.f, 150.f), 20.f, offHz (20.f, true)));
    add (new APF (id ("highCut"), "High Cut", skewed (1000.f, 20000.f, 6000.f), 20000.f, offHz (20000.f, false)));
    add (new APF (id ("comp"), "Comp", unit, 0.f, pct));
    add (new APF (id ("ensemble"), "Ensemble", unit, 0.f, pct));
    add (new APF (id ("ensRate"), "Ensemble Rate", NormalisableRange<float> (0.2f, 3.f, 0.01f), 1.f, fmt ([] (float x) { return "x" + String (x, 2); })));
    add (new APF (id ("phaser"), "Phaser", unit, 0.f, pct));
    add (new APF (id ("phRate"), "Phaser Rate", skewed (0.02f, 4.f, 0.4f), 0.4f, lfo));
    add (new APF (id ("phDepth"), "Phaser Depth", unit, 0.6f, pct));
    add (new APF (id ("delayMix"), "Delay", unit, 0.f, pct));
    add (new APC (id ("delayTime"), "Delay Time", StringArray { "1/16", "1/8", "1/8 D", "1/4", "1/4 D", "1/2", "1/8 T" }, 2));
    add (new APF (id ("delayFb"), "Feedback", NormalisableRange<float> (0.f, 0.9f), 0.35f, pct));
    add (new APF (id ("delayTone"), "Delay Tone", skewed (1000.f, 16000.f, 5000.f), 6000.f, hz));
    add (new APB (id ("pingPong"), "Ping Pong", true));
    add (new APF (id ("reverb"), "Reverb", unit, 0.12f, pct));
    add (new APF (id ("revSize"), "Room Size", unit, 0.5f, pct));
    add (new APF (id ("revPre"), "Pre-Delay", NormalisableRange<float> (0.f, 150.f, 0.1f), 12.f, ms));
    add (new APF (id ("revDamp"), "Damping", unit, 0.4f, pct));
    add (new APF (id ("revWidth"), "Reverb Width", NormalisableRange<float> (0.f, 1.5f), 1.f, pct));

    // ---- sortie
    add (new APF (id ("robot"), "Robot Level", unit, 1.f, pct));
    add (new APF (id ("voice"), "Voice Level", unit, 0.f, pct));
    add (new APF (id ("width"), "Width", NormalisableRange<float> (0.f, 2.f), 1.f, pct));
    add (new APF (id ("output"), "Output", NormalisableRange<float> (-24.f, 12.f, 0.1f), 0.f, db));
    add (new APB (id ("bypass"), "Bypass", false));
    add (new APB (id ("noLatency"), "No Latency", false));

    return { v.begin(), v.end() };
}

VckProcessor::VckProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", AudioChannelSet::stereo(), false)),
      apvts (*this, &undo, "VCK", createLayout()),
      presets (apvts, undo)
{
    P.bands = p ("bands"); P.formant = p ("formant"); P.attack = p ("attack"); P.release = p ("release"); P.sharp = p ("sharp");
    P.sibil = p ("sibil"); P.breath = p ("breath"); P.bright = p ("bright"); P.gate = p ("gate"); P.freeze = p ("freeze");
    P.inGain = p ("inGain"); P.character = p ("character"); P.spread = p ("spread"); P.loFreq = p ("loFreq"); P.hiFreq = p ("hiFreq");
    P.source = p ("source"); P.wave = p ("wave"); P.unison = p ("unison"); P.detune = p ("detune"); P.octave = p ("octave");
    P.glide = p ("glide"); P.mode = p ("mode"); P.vibrato = p ("vibrato"); P.vibRate = p ("vibRate"); P.noise = p ("noise");
    P.envA = p ("envA"); P.envR = p ("envR"); P.sub = p ("sub"); P.velSens = p ("velSens"); P.bendRange = p ("bendRange");
    P.key = p ("key"); P.scale = p ("scale"); P.harmony = p ("harmony"); P.retune = p ("retune"); P.humanize = p ("humanize");
    P.autoTrans = p ("autoTrans"); P.harmLevel = p ("harmLevel"); P.tracking = p ("tracking"); P.hold = p ("hold");
    P.voiceRange = p ("voiceRange"); P.drone = p ("drone");
    P.clipOn = p ("clipOn"); P.sync = p ("sync"); P.bpm = p ("bpm"); P.transpose = p ("transpose"); P.rate = p ("rate"); P.loop = p ("loop"); P.clipGate = p ("clipGate"); P.harmWidth = p ("harmWidth"); P.dblLink = p ("dblLink");
    P.dblVoices = p ("dblVoices"); P.dblMix = p ("dblMix"); P.dblDirect = p ("dblDirect"); P.dblWidth = p ("dblWidth");
    P.dblDetune = p ("dblDetune"); P.dblDelay = p ("dblDelay"); P.dblDrift = p ("dblDrift"); P.dblRate = p ("dblRate");
    P.dblHpf = p ("dblHpf"); P.dblLpf = p ("dblLpf");
    for (int i = 0; i < 4; ++i)
    {
        const String n (i + 1);
        P.dblVDelay[i] = apvts.getRawParameterValue ("dblDelay" + n);
        P.dblVDetune[i] = apvts.getRawParameterValue ("dblDetune" + n);
        P.dblVPan[i] = apvts.getRawParameterValue ("dblPan" + n);
        P.dblVGain[i] = apvts.getRawParameterValue ("dblGain" + n);
    }
    P.drive = p ("drive"); P.lowCut = p ("lowCut"); P.highCut = p ("highCut"); P.comp = p ("comp");
    P.ensemble = p ("ensemble"); P.ensRate = p ("ensRate"); P.phaser = p ("phaser"); P.phRate = p ("phRate"); P.phDepth = p ("phDepth");
    P.delayMix = p ("delayMix"); P.delayTime = p ("delayTime"); P.delayFb = p ("delayFb"); P.delayTone = p ("delayTone"); P.pingPong = p ("pingPong");
    P.reverb = p ("reverb"); P.revSize = p ("revSize"); P.revPre = p ("revPre"); P.revDamp = p ("revDamp"); P.revWidth = p ("revWidth");
    P.robot = p ("robot"); P.voice = p ("voice"); P.width = p ("width"); P.output = p ("output"); P.bypass = p ("bypass");
    P.noLatency = p ("noLatency");

    for (auto* arr : { &uiMod, &uiCar, &uiOut, &uiFreq }) for (auto& a : *arr) a = 0.f;
    for (auto& a : uiNotes) a = 0u;
    for (auto& a : uiInNotes) a = 0u;
    for (auto& a : uiPitchIn) a = -1.f;
    for (auto& a : uiPitchOut) a = -1.f;
    startTimerHz (30);
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

void VckProcessor::prepareToPlay (double sampleRate, int)
{
    fs = sampleRate;
    const float f = (float) fs;
    vocoder.prepare (fs);
    synth.prepare (fs);
    previewSynth.prepare (fs);
    previewLeft.fill (0);
    tracker.prepare (fs);
    doubler.prepare (fs);
    ensemble.prepare (fs);
    phaser.prepare (fs);
    delay.prepare (fs);
    reverb.prepare (fs);
    comp.prepare (fs);
    drive = {};
    for (auto& s : lowCut) s.reset();
    for (auto& s : highCut) s.reset();
    lowCutSet = highCutSet = -1.f;
    player.reset();
    clipMidi.ensureSize (4096);
    pending.clear(); pending.reserve (8192);
    for (auto* d : { &dlyL, &dlyR, &dlyScL, &dlyScR }) d->prepare ((int) (0.06 * fs));
    robotS.init (P.robot->load(), 30.f, f); voiceS.init (P.voice->load(), 30.f, f);
    outS.init (vck::dbToGain (P.output->load()), 30.f, f); bypassS.init (P.bypass->load() > 0.5f ? 1.f : 0.f, 20.f, f);
    widthS.init (P.width->load(), 40.f, f); driveS.init (P.drive->load(), 40.f, f); inS.init (vck::dbToGain (P.inGain->load()), 30.f, f);
    ctrlCount = 0; freeBeat = 0.0; heldIn.fill (false); heldCount = 0; sampleClock = 0; lastNoteTime = -100.0;
    lastSource = -1;
    latencyInit = false;
    applyLatencyMode (P.noLatency->load() > 0.5f);
    updateParams();
}

void VckProcessor::applyLatencyMode (bool noLat)
{
    // HQ : 16 ms d'anticipation (la hauteur est connue avant que la voix n'arrive)
    //      + 1,5 ms d'anticipation du limiteur. NO LATENCY : zero.
    lookD = noLat ? 0 : (int) std::round (0.016 * fs);
    limLA = noLat ? 0 : (int) std::round (0.0015 * fs);
    limiter.prepare (fs, limLA);
    for (auto* d : { &dlyL, &dlyR, &dlyScL, &dlyScR }) d->reset();
    pending.clear();
    latencyNoLat = noLat;
    latencyInit = true;
    setLatencySamples (lookD + limLA);
    uiLatencyMs = (float) (1000.0 * (lookD + limLA) / fs);
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
    vp.breath = P.breath->load();
    vp.bright = P.bright->load();
    vp.gateDb = P.gate->load();
    vp.freeze = P.freeze->load() > 0.5f;
    vp.character = P.character->load();
    vp.spread = P.spread->load();
    vp.lo = P.loFreq->load();
    vp.hi = P.hiFreq->load();
    vocoder.setParams (vp);

    vck::SynthParams sp;
    sp.wave = (int) P.wave->load();
    sp.unison = (int) P.unison->load();
    sp.detune = P.detune->load();
    sp.octave = 12.f * ((float) (int) P.octave->load() - 2.f);
    sp.glideMs = P.glide->load();
    sp.mode = (int) P.mode->load();
    sp.vibrato = P.vibrato->load();
    sp.vibRate = P.vibRate->load();
    sp.noise = P.noise->load();
    sp.attackMs = P.envA->load();
    sp.releaseMs = P.envR->load();
    sp.sub = P.sub->load();
    sp.velSens = P.velSens->load();
    sp.bend = bendSemis;
    sp.harmLevel = P.harmLevel->load();
    sp.harmWidth = P.harmWidth->load();
    synth.setParams (sp);
    {
        auto pp = sp; pp.mode = 0; pp.glideMs = 0.f; pp.vibrato = 0.f; pp.attackMs = 3.f; pp.releaseMs = 80.f; pp.bend = 0.f;
        previewSynth.setParams (pp);
    }
    synth.autoGlideMs = P.retune->load();

    static constexpr float rangeLo[4] = { 50.f, 75.f, 140.f, 50.f }, rangeHi[4] = { 450.f, 900.f, 1200.f, 1200.f };
    const int vr = jlimit (0, 3, (int) P.voiceRange->load());
    tracker.setRange (rangeLo[vr], rangeHi[vr]);
    tracker.setThreshold (0.08f + 0.17f * P.tracking->load());

    vck::DoublerParams dp;
    dp.voices = (int) P.dblVoices->load();
    dp.mix = P.dblMix->load();
    dp.direct = P.dblDirect->load();
    dp.width = P.dblWidth->load();
    dp.detuneScale = P.dblDetune->load();
    dp.delayScale = P.dblDelay->load();
    dp.drift = P.dblDrift->load();
    dp.rate = P.dblRate->load();
    dp.hpf = P.dblHpf->load();
    dp.lpf = P.dblLpf->load();
    for (int i = 0; i < 4; ++i)
        dp.v[(size_t) i] = { P.dblVDelay[i]->load(), P.dblVDetune[i]->load(), P.dblVPan[i]->load(), P.dblVGain[i]->load() };
    doubler.setParams (dp);

    reverb.set (P.revSize->load(), P.revDamp->load());
}

void VckProcessor::handleMidi (const MidiMessage& m)
{
    if (m.isNoteOn())
    {
        const int n = m.getNoteNumber();
        if (! heldIn[(size_t) n]) ++heldCount;
        heldIn[(size_t) n] = true;
        lastNoteTime = (double) sampleClock / fs;
        if (synth.isAuto() && (int) P.source->load() == 0) synth.setAuto (false);   // une note arrive : le MIDI reprend la main
        synth.noteOn (n, m.getFloatVelocity());
    }
    else if (m.isNoteOff())
    {
        const int n = m.getNoteNumber();
        if (heldIn[(size_t) n]) --heldCount;
        heldIn[(size_t) n] = false;
        lastNoteTime = (double) sampleClock / fs;
        synth.noteOff (n);
    }
    else if (m.isPitchWheel())
    {
        bendSemis = (float) (m.getPitchWheelValue() - 8192) / 8192.f * P.bendRange->load();
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        heldIn.fill (false); heldCount = 0;
        synth.allNotesOff();
    }
}

void VckProcessor::autoTargets (float voiceLevel)
{
    const int key = (int) P.key->load(), scale = (int) P.scale->load(), harmony = (int) P.harmony->load();
    const bool drone = P.drone->load() > 0.5f;
    const float humanize = P.humanize->load();
    const bool loud = voiceLevel > vck::dbToGain (P.gate->load()) * 2.f;
    const bool noiseWave = (int) P.wave->load() == 5;
    const int w = uiPitchWrite.load();
    int count = 0;
    if (tracker.voiced && loud)
    {
        const float m = vck::ftom (tracker.freq) + (float) (int) P.autoTrans->load();
        const int sc = drone ? 8 : scale;
        float q = vck::scales::quantize (m, key, sc);
        if (sc != 0 && sc != 8 && lastAutoNote >= 0.f && q != lastAutoNote && std::abs (m - lastAutoNote) < 0.62f) q = lastAutoNote;
        lastAutoNote = q;
        const float human = (sc != 0 && sc != 8) ? humanize * std::clamp (m - q, -0.6f, 0.6f) : 0.f;
        autoNotes[count++] = q + human;
        switch (harmony)
        {
            case 1: autoNotes[count++] = q - 12.f + human; break;
            case 2: autoNotes[count++] = vck::scales::degreeUp (q, 4, key, sc) + human; break;
            case 3: autoNotes[count++] = vck::scales::degreeUp (q, 2, key, sc) + human;
                    autoNotes[count++] = vck::scales::degreeUp (q, 4, key, sc) + human; break;
            case 4: autoNotes[count++] = q - 12.f + human;
                    autoNotes[count++] = vck::scales::degreeUp (q, 2, key, sc) + human;
                    autoNotes[count++] = vck::scales::degreeUp (q, 4, key, sc) + human;
                    autoNotes[count++] = q + 12.f + human; break;
            default: break;
        }
        autoHold = P.hold->load() * 0.001f;
        synth.setAutoTargets (autoNotes, count, true);
        uiTargetNote = q;
        uiPitchHz = tracker.freq;
        uiPitchIn[(size_t) w] = m;
        uiPitchOut[(size_t) w] = q + human;
    }
    else
    {
        autoHold -= tracker.hopSeconds();
        if (noiseWave && loud)
        {
            autoNotes[0] = lastAutoNote >= 0.f ? lastAutoNote : 60.f;
            synth.setAutoTargets (autoNotes, 1, true);
        }
        else if (autoHold <= 0.f) { synth.setAutoTargets (autoNotes, 0, false); uiPitchHz = 0.f; uiTargetNote = -1.f; }
        uiPitchIn[(size_t) w] = -1.f;
        uiPitchOut[(size_t) w] = autoHold > 0.f ? lastAutoNote : -1.f;
    }
    uiPitchWrite = (w + 1) % pitchHist;
}

void VckProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0) return;
    auto main = getBusBuffer (buffer, false, 0);
    const int nch = main.getNumChannels();
    if (nch == 0) { midi.clear(); return; }

    const float* scL = nullptr; const float* scR = nullptr;
    if (auto* bus = getBus (true, 1); bus != nullptr && bus->isEnabled())
    {
        auto sc = getBusBuffer (buffer, true, 1);
        if (sc.getNumChannels() > 0) { scL = sc.getReadPointer (0); scR = sc.getNumChannels() > 1 ? sc.getReadPointer (1) : scL; }
    }
    uiScConnected = scL != nullptr;

    const bool noLat = P.noLatency->load() > 0.5f;
    if (! latencyInit || noLat != latencyNoLat) applyLatencyMode (noLat);
    updateParams();
    const int source = (int) P.source->load();
    if (source != lastSource) { synth.allNotesOff(); lastSource = source; }

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
    uiBpm = (float) bpm; uiHostSync = useHostClock; uiHostPlaying = hostPlaying;
    clipMidi.clear();
    if (restartFree.exchange (false)) { freeBeat = 0.0; player.stopAll (clipMidi); }

    // ---- clip MIDI
    clipActive = false;
    {
        const bool want = P.clipOn->load() > 0.5f && source == 0;
        const SpinLock::ScopedTryLockType lock (clipLock);
        if (lock.isLocked() && want && ! clip.empty())
        {
            const bool playing = useHostClock ? hostPlaying : true;
            const double beat = useHostClock ? ppq * rate : freeBeat;
            const double bps = bpm / 60.0 / fs * rate;
            const bool loop = P.loop->load() > 0.5f;
            if (playing)
            {
                player.process (clip, beat, bps, n, (int) P.transpose->load(), loop, clipMidi, P.clipGate->load());
                playBeat = (float) (loop ? std::fmod (std::max (0.0, beat), clip.lengthBeats) : beat);
                clipActive = loop || beat < clip.lengthBeats;
            }
            else
            {
                if (wasPlaying) player.stopAll (clipMidi);
                playBeat = useHostClock ? (float) std::fmod (std::max (0.0, ppq * rate), clip.lengthBeats) : -1.f;
            }
            wasPlaying = playing;
        }
        else if (lock.isLocked() && wasPlaying) { player.stopAll (clipMidi); wasPlaying = false; playBeat = -1.f; }
    }
    freeBeat += (double) n * bpm / 60.0 / fs * rate;
    if (source == 0) clipMidi.addEvents (midi, 0, n, 0);
    midi.clear();
    // tout le MIDI est retarde de l'anticipation (aligne sur la voix retardee)
    for (const auto meta : clipMidi)
    {
        Pending ev { sampleClock + meta.samplePosition + lookD, meta.getMessage() };
        pending.insert (std::upper_bound (pending.begin(), pending.end(), ev, [] (const Pending& a, const Pending& b) { return a.due < b.due; }), ev);
    }
    // ecoute des notes ecrites dans le piano roll (synthe a part, directement en sortie)
    {
        int s1, n1, s2, n2;
        previewFifo.prepareToRead (previewFifo.getNumReady(), s1, n1, s2, n2);
        auto take = [&] (int start, int count)
        {
            for (int k = 0; k < count; ++k)
            {
                const auto [note, vel] = previewBuf[(size_t) (start + k)];
                if (previewLeft[(size_t) note] > 0) previewSynth.noteOff (note);
                previewSynth.noteOn (note, vel);
                previewLeft[(size_t) note] = (int) (0.35 * fs);
            }
        };
        take (s1, n1); take (s2, n2);
        previewFifo.finishedRead (n1 + n2);
    }

    // ---- MIDI sans notes : vck suit la voix (jamais de silence)
    const double now = (double) sampleClock / fs;
    const bool midiIdle = heldCount == 0 && ! clipActive && (now - lastNoteTime) > 0.8;
    const bool autoNow = source == 1 || (source == 0 && midiIdle);
    if (autoNow != synth.isAuto()) synth.setAuto (autoNow);
    uiAutoActive = autoNow;

    float* oL = main.getWritePointer (0);
    float* oR = nch > 1 ? main.getWritePointer (1) : nullptr;
    const float* iL = main.getReadPointer (0);
    const float* iR = nch > 1 ? main.getReadPointer (1) : iL;

    robotS.t = P.robot->load(); voiceS.t = P.voice->load(); outS.t = vck::dbToGain (P.output->load());
    bypassS.t = P.bypass->load() > 0.5f ? 1.f : 0.f; widthS.t = P.width->load(); driveS.t = P.drive->load();
    inS.t = vck::dbToGain (P.inGain->load());

    static constexpr float divBeats[7] = { 0.25f, 0.5f, 0.75f, 1.f, 1.5f, 2.f, 1.f / 3.f };
    const float delaySec = divBeats[jlimit (0, 6, (int) P.delayTime->load())] * 60.f / (float) bpm;
    const float delayMix = P.delayMix->load(), delayFb = P.delayFb->load(), delayTone = P.delayTone->load();
    const bool pingPong = P.pingPong->load() > 0.5f;
    const float ens = P.ensemble->load(), ensRate = P.ensRate->load();
    const float phMix = P.phaser->load(), phRate = P.phRate->load(), phDepth = P.phDepth->load();
    const float rev = P.reverb->load(), revPre = P.revPre->load(), revWidth = P.revWidth->load();
    const float compAmt = P.comp->load();
    const float lc = P.lowCut->load(), hc = P.highCut->load();
    if (lc != lowCutSet) { lowCutSet = lc; for (auto& s : lowCut) s.set (lc, 0.707f, (float) fs); }
    if (hc != highCutSet) { highCutSet = hc; for (auto& s : highCut) s.set (std::min (hc, 0.45f * (float) fs), 0.707f, (float) fs); }
    const bool useLC = lc > 21.f, useHC = hc < 19900.f;

    size_t pIdx = 0;
    float inPeak = 0.f, outPeak = 0.f;
    for (int i = 0; i < n; ++i)
    {
        const int64_t t = sampleClock + i;
        while (pIdx < pending.size() && pending[pIdx].due <= t) handleMidi (pending[pIdx++].msg);

        const float ing = inS.next();
        const float rawL = iL[i], rawR = iR[i];
        const float voiceNow = (nch > 1 ? 0.5f * (rawL + rawR) : rawL) * ing;
        inPeak = std::max (inPeak, std::abs (voiceNow));
        voiceLev = std::max (std::abs (voiceNow), voiceLev * 0.9995f);

        // retard d'anticipation (HQ)
        float xL = rawL, xR = rawR, byL = rawL, byR = rawR;
        if (lookD > 0 || limLA > 0)
        {
            dlyL.push (rawL); dlyR.push (rawR);
            xL = dlyL.readInt (lookD); xR = dlyR.readInt (lookD);
            byL = dlyL.readInt (lookD + limLA); byR = dlyR.readInt (lookD + limLA);
        }
        const float voice = (nch > 1 ? 0.5f * (xL + xR) : xL) * ing;

        // AUTO : la hauteur est lue sur la voix non retardee
        if (source != 2 && tracker.push (voiceNow) && synth.isAuto()) autoTargets (voiceLev);

        if (ctrlCount == 0) synth.control (vck::Vocoder::ctrl);
        if (++ctrlCount >= vck::Vocoder::ctrl) ctrlCount = 0;

        float cL = 0.f, cR = 0.f, clvl = 0.f;
        if (source == 2)
        {
            if (scL != nullptr)
            {
                cL = scL[i]; cR = scR[i];
                if (lookD > 0) { dlyScL.push (cL); dlyScR.push (cR); cL = dlyScL.readInt (lookD); cR = dlyScR.readInt (lookD); }
            }
            const float a = std::abs (0.5f * (cL + cR));
            scLevel = a + (a > scLevel ? 0.995f : 0.9997f) * (scLevel - a);
            scPeak = std::max (scLevel, scPeak * 0.99998f);
            clvl = std::min (1.f, scLevel / (scPeak * 0.5f + 1e-6f));
        }
        else { synth.render (cL, cR); clvl = synth.level(); }
        float pvL = 0.f, pvR = 0.f;
        if (ctrlCount == 1) previewSynth.control (vck::Vocoder::ctrl);
        previewSynth.render (pvL, pvR);

        float vL, vR;
        vocoder.process (voice, cL, cR, clvl, vL, vR);

        const float rob = robotS.next(), vox = voiceS.next();
        float wL = vL * rob + xL * ing * vox;
        float wR = vR * rob + xR * ing * vox;

        // couleur : saturation, EQ, compresseur
        const float drv = driveS.next();
        if (drv > 0.001f)
        {
            const float g = vck::dbToGain (24.f * drv), mk = std::pow (g, -0.85f), mx = std::min (1.f, drv * 4.f);
            wL += (drive[0].process (wL * g) * mk - wL) * mx;
            wR += (drive[1].process (wR * g) * mk - wR) * mx;
        }
        if (useLC) { wL = lowCut[0].hp (wL); wR = lowCut[1].hp (wR); }
        if (useHC) { wL = highCut[0].lp (wL); wR = highCut[1].lp (wR); }
        comp.process (wL, wR, compAmt);

        doubler.process (wL, wR);
        ensemble.process (wL, wR, ens, ensRate);
        phaser.process (wL, wR, phMix, phRate, phDepth);
        delay.process (wL, wR, delaySec, delayFb, delayMix, delayTone, pingPong);
        reverb.process (wL, wR, rev, revPre, revWidth);

        const float wd = widthS.next();
        if (std::abs (wd - 1.f) > 0.001f)
        {
            const float mid = 0.5f * (wL + wR), side = 0.5f * (wL - wR) * wd;
            wL = mid + side; wR = mid - side;
        }
        const float g = outS.next();
        wL = wL * g + pvL * 0.5f; wR = wR * g + pvR * 0.5f;
        limiter.process (wL, wR);

        const float byp = bypassS.next();
        const float yL = wL + (byL - wL) * byp, yR = wR + (byR - wR) * byp;
        outPeak = std::max (outPeak, std::max (std::abs (yL), std::abs (yR)));
        if (oR != nullptr) { oL[i] = yL; oR[i] = yR; }
        else oL[i] = 0.5f * (yL + yR);
    }
    // evenements restants (dus dans ce bloc mais pas encore lus) puis menage de la file
    pending.erase (pending.begin(), pending.begin() + (std::ptrdiff_t) pIdx);
    for (int k = 0; k < 128; ++k)
        if (previewLeft[(size_t) k] > 0 && (previewLeft[(size_t) k] -= n) <= 0) { previewLeft[(size_t) k] = 0; previewSynth.noteOff (k); }
    sampleClock += n;

    // ---- interface
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
    uiGate = vocoder.gateIsOpen();
    uiLimGr = limiter.grDb;
    uiCompGr = comp.grDb;
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
    if (! autoNow) { uiPitchHz = 0.f; uiTargetNote = -1.f; }
}

//==============================================================================
void VckProcessor::resetAllParameters()
{
    undo.beginNewTransaction ("Reset");
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
        {
            const auto pid = rp->getParameterID();
            if (pid == "key" || pid == "scale") continue;   // la gamme du morceau reste
            presets.setTreeValue (pid, rp->convertFrom0to1 (rp->getDefaultValue()), &undo);
        }
    undo.beginNewTransaction();
    presets.setCurrentName ("Init");
}

void VckProcessor::copySlotTo (int dest)
{
    if (dest < 0 || dest > 3 || dest == abSlot) return;
    abState[(size_t) dest] = apvts.copyState();
}

bool VckProcessor::slotHasState (int slot) const
{
    return slot == abSlot || (slot >= 0 && slot < 4 && abState[(size_t) slot].isValid());
}

void VckProcessor::selectSlot (int slot)
{
    slot = jlimit (0, 3, slot);
    if (slot == abSlot) return;
    abState[(size_t) abSlot] = apvts.copyState();
    abSlot = slot;
    if (abState[(size_t) abSlot].isValid()) apvts.replaceState (abState[(size_t) abSlot].createCopy());
}

bool VckProcessor::loadMidi (const File& f, String& error)
{
    vck::Clip c;
    if (! vck::Clip::load (f, c, error)) return false;
    pushClipUndo();
    { const SpinLock::ScopedLockType l (clipLock); clip = std::move (c); }
    ++clipVersion;
    return true;
}

void VckProcessor::setClip (const vck::Clip& c)
{
    { const SpinLock::ScopedLockType l (clipLock); clip = c; }
    ++clipVersion;
}

void VckProcessor::pushClipUndo()
{
    clipUndoStack.push_back (getClipCopy());
    if (clipUndoStack.size() > 200) clipUndoStack.erase (clipUndoStack.begin());
    clipRedoStack.clear();
}

bool VckProcessor::clipUndo()
{
    if (clipUndoStack.empty()) return false;
    clipRedoStack.push_back (getClipCopy());
    setClip (clipUndoStack.back());
    clipUndoStack.pop_back();
    return true;
}

bool VckProcessor::clipRedo()
{
    if (clipRedoStack.empty()) return false;
    clipUndoStack.push_back (getClipCopy());
    setClip (clipRedoStack.back());
    clipRedoStack.pop_back();
    return true;
}

void VckProcessor::previewNote (int note, float vel)
{
    int s1, n1, s2, n2;
    previewFifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 > 0) previewBuf[(size_t) s1] = { jlimit (0, 127, note), vel };
    else if (n2 > 0) previewBuf[(size_t) s2] = { jlimit (0, 127, note), vel };
    previewFifo.finishedWrite (n1 + n2);
}

// LINK du doubleur : on tourne un bouton d'une voix, les autres suivent en proportion
// (retard, desaccord et panoramique gardent leur rapport ; le gain suit en dB)
void VckProcessor::timerCallback()
{
    static const char* kinds[4] = { "dblDelay", "dblDetune", "dblPan", "dblGain" };
    std::array<std::array<float, 4>, 4> cur {};
    for (int t = 0; t < 4; ++t)
        for (int v = 0; v < 4; ++v)
            cur[(size_t) t][(size_t) v] = apvts.getRawParameterValue (String (kinds[t]) + String (v + 1))->load();
    if (linkPrimed && P.dblLink->load() > 0.5f)
    {
        const int nv = (int) P.dblVoices->load() == 2 ? 4 : 2;
        for (int t = 0; t < 4; ++t)
        {
            int changed = -1, count = 0;
            for (int v = 0; v < nv; ++v)
                if (std::abs (cur[(size_t) t][(size_t) v] - linkLast[(size_t) t][(size_t) v]) > 1e-4f) { changed = v; ++count; }
            if (count != 1) continue;
            const float before = linkLast[(size_t) t][(size_t) changed], after = cur[(size_t) t][(size_t) changed];
            const float delta = after - before;
            const bool ratioOk = t != 3 && std::abs (before) > (t == 0 ? 0.5f : 0.05f);
            for (int v = 0; v < nv; ++v)
            {
                if (v == changed) continue;
                const float o = cur[(size_t) t][(size_t) v];
                float nvv = t == 3 ? o + delta
                          : ratioOk ? o * (after / before)
                                    : o + delta * (o < 0.f ? -1.f : 1.f);
                static constexpr float lo[4] = { 1.f, -50.f, -1.f, -24.f }, hi[4] = { 100.f, 50.f, 1.f, 6.f };
                nvv = jlimit (lo[t], hi[t], nvv);
                presets.setTreeValue (String (kinds[t]) + String (v + 1), nvv, &undo);
                cur[(size_t) t][(size_t) v] = apvts.getRawParameterValue (String (kinds[t]) + String (v + 1))->load();
            }
        }
    }
    linkLast = cur;
    linkPrimed = true;
}

void VckProcessor::clearMidi()
{
    pushClipUndo();
    { const SpinLock::ScopedLockType l (clipLock); clip = vck::Clip(); }
    ++clipVersion;
}

void VckProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", presets.getCurrent(), nullptr);
    auto xml = state.createXml();
    auto c = getClipCopy();
    auto* cx = xml->createNewChildElement ("CLIP");
    cx->setAttribute ("name", c.name);
    cx->setAttribute ("len", c.lengthBeats);
    cx->setAttribute ("notes", c.serialise());
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
        { const SpinLock::ScopedLockType l (clipLock); clip = std::move (c); }
        ++clipVersion;
        xml->removeChildElement (cx, true);
    }
    if (xml->hasTagName (apvts.state.getType()))
    {
        apvts.replaceState (ValueTree::fromXml (*xml));
        const auto name = apvts.state.getProperty ("preset").toString();
        presets.setCurrentName (name.isNotEmpty() ? name : String ("Init"));
        undo.clearUndoHistory();
    }
}

AudioProcessorEditor* VckProcessor::createEditor() { return new VckEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VckProcessor(); }
