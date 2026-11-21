#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp.h"
#include "MidiClip.h"
#include "Presets.h"

class VckProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    VckProcessor();
    ~VckProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "vck"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::UndoManager undo { 400000, 100 };
    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;

    // ---- facon ravenn : reset (une etape d'undo), slots A/B/C/D
    void resetAllParameters();
    int getABSlot() const { return abSlot; }
    void selectSlot (int slot);
    void copySlotTo (int dest);
    bool slotHasState (int slot) const;

    // ---- clip MIDI
    bool loadMidi (const juce::File& f, juce::String& error);
    void clearMidi();
    // edition du clip (piano roll) : thread message uniquement
    void setClip (const vck::Clip& c);
    void pushClipUndo();
    bool clipUndo();
    bool clipRedo();
    bool canClipUndo() const { return ! clipUndoStack.empty(); }
    bool canClipRedo() const { return ! clipRedoStack.empty(); }
    void previewNote (int note, float vel);    // ecoute d'une note pendant l'ecriture
    double beatSeconds() const { return 60.0 / std::max (1.f, uiBpm.load()); }
    vck::Clip getClipCopy() const { const juce::SpinLock::ScopedLockType l (clipLock); return clip; }
    std::atomic<int> clipVersion { 0 };
    std::atomic<float> playBeat { -1.f };
    std::atomic<bool> restartFree { false };

    // ---- affichage
    static constexpr int maxBands = vck::Vocoder::maxBands;
    std::array<std::atomic<float>, maxBands> uiMod {}, uiCar {}, uiOut {}, uiFreq {};
    std::atomic<int> uiBands { 24 };
    std::atomic<float> uiPitchHz { 0.f }, uiTargetNote { -1.f }, uiBpm { 120.f }, uiInLevel { 0.f }, uiOutLevel { 0.f },
                       uiUnvoiced { 0.f }, uiLatencyMs { 0.f }, uiLimGr { 0.f }, uiCompGr { 0.f };
    std::atomic<bool> uiHostSync { false }, uiHostPlaying { false }, uiScConnected { false }, uiAutoActive { false }, uiGate { false };
    std::array<std::atomic<uint32_t>, 4> uiNotes {}, uiInNotes {};
    // historique de hauteur (graphe AUTO) : voix detectee / note du robot, en MIDI
    static constexpr int pitchHist = 256;
    std::array<std::atomic<float>, pitchHist> uiPitchIn {}, uiPitchOut {};
    std::atomic<int> uiPitchWrite { 0 };

private:
    void timerCallback() override;    // LINK du doubleur
    std::array<std::array<float, 4>, 4> linkLast {};
    bool linkPrimed = false;
    std::vector<vck::Clip> clipUndoStack, clipRedoStack;
    juce::AbstractFifo previewFifo { 64 };
    std::array<std::pair<int, float>, 64> previewBuf {};
    vck::CarrierSynth previewSynth;
    std::array<int, 128> previewLeft {};
    void updateParams();
    void handleMidi (const juce::MidiMessage& m);
    void applyLatencyMode (bool noLatency);
    void autoTargets (float voiceLevel);

    vck::Vocoder vocoder;
    vck::CarrierSynth synth;
    vck::PitchTracker tracker;
    vck::Doubler doubler;
    vck::Ensemble ensemble;
    vck::Phaser phaser;
    vck::TempoDelay delay;
    vck::Reverb reverb;
    vck::Compressor comp;
    vck::Limiter limiter;
    std::array<vck::AdaaTanh, 2> drive;
    std::array<vck::Svf, 2> lowCut, highCut;
    vck::ClipPlayer player;
    vck::Clip clip;
    juce::SpinLock clipLock;
    juce::MidiBuffer clipMidi;

    // retard d'anticipation (mode HQ) : voix, sec, sidechain et MIDI decales ensemble
    vck::DelayLine dlyL, dlyR, dlyScL, dlyScR;
    struct Pending { int64_t due; juce::MidiMessage msg; };
    std::vector<Pending> pending;
    int64_t sampleClock = 0;
    int lookD = 0, limLA = 0;
    bool latencyNoLat = false, latencyInit = false;

    std::array<bool, 128> heldIn {};
    int heldCount = 0;
    double fs = 48000.0, freeBeat = 0.0, lastNoteTime = -100.0;
    int ctrlCount = 0;
    float scLevel = 0.f, scPeak = 1e-3f, voiceLev = 0.f;
    vck::Smooth robotS, voiceS, outS, bypassS, widthS, driveS, inS;
    float lowCutSet = -1.f, highCutSet = -1.f;
    bool wasPlaying = false, clipActive = false;
    int lastSource = -1;
    float autoHold = 0.f, lastAutoNote = -1.f, autoNotes[5] {};

    int abSlot = 0;
    std::array<juce::ValueTree, 4> abState;

    std::atomic<float>* p (const char* id) { return apvts.getRawParameterValue (id); }
    struct Ptrs
    {
        std::atomic<float> *bands, *formant, *attack, *release, *sharp, *sibil, *breath, *bright, *gate, *freeze,
                           *inGain, *character, *spread, *loFreq, *hiFreq,
                           *source, *wave, *unison, *detune, *octave, *glide, *mode, *vibrato, *vibRate, *noise, *envA, *envR,
                           *sub, *velSens, *bendRange,
                           *key, *scale, *harmony, *retune, *humanize, *autoTrans, *harmLevel, *tracking, *hold, *voiceRange, *drone,
                           *clipOn, *sync, *bpm, *transpose, *rate, *loop, *clipGate, *harmWidth, *dblLink,
                           *dblVoices, *dblMix, *dblDirect, *dblWidth, *dblDetune, *dblDelay, *dblDrift, *dblRate, *dblHpf, *dblLpf,
                           *dblVDelay[4], *dblVDetune[4], *dblVPan[4], *dblVGain[4],
                           *drive, *lowCut, *highCut, *comp,
                           *ensemble, *ensRate, *phaser, *phRate, *phDepth,
                           *delayMix, *delayTime, *delayFb, *delayTone, *pingPong,
                           *reverb, *revSize, *revPre, *revDamp, *revWidth,
                           *robot, *voice, *width, *output, *bypass, *noLatency;
    } P {};
    float bendSemis = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VckProcessor)
};
