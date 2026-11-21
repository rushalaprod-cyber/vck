#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp.h"
#include "MidiClip.h"
#include "Presets.h"

class VckProcessor : public juce::AudioProcessor
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
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;

    // ---- clip MIDI
    bool loadMidi (const juce::File& f, juce::String& error);
    void clearMidi();
    vck::Clip getClipCopy() const { const juce::SpinLock::ScopedLockType l (clipLock); return clip; }
    std::atomic<int> clipVersion { 0 };
    std::atomic<float> playBeat { -1.f };     // position dans le clip (pour l'affichage)

    // ---- affichage
    static constexpr int maxBands = vck::Vocoder::maxBands;
    std::array<std::atomic<float>, maxBands> uiMod {}, uiCar {}, uiOut {}, uiFreq {};
    std::atomic<int> uiBands { 24 };
    std::atomic<float> uiPitchHz { 0.f }, uiTargetNote { -1.f }, uiBpm { 120.f }, uiInLevel { 0.f }, uiOutLevel { 0.f }, uiUnvoiced { 0.f };
    std::atomic<bool> uiHostSync { false }, uiHostPlaying { false }, uiScConnected { false };
    std::array<std::atomic<uint32_t>, 4> uiNotes {};   // 128 bits : notes de la porteuse
    std::array<std::atomic<uint32_t>, 4> uiInNotes {}; // 128 bits : notes MIDI recues (clavier / clip)

private:
    void updateParams();
    void handleMidi (const juce::MidiMessage& m);
    float autoNotes[5] {};

    vck::Vocoder vocoder;
    vck::CarrierSynth synth;
    vck::PitchTracker tracker;
    vck::Doubler doubler;
    vck::Ensemble ensemble;
    vck::TempoDelay delay;
    vck::Reverb reverb;
    vck::ClipPlayer player;
    vck::Clip clip;
    juce::SpinLock clipLock;
    juce::MidiBuffer clipMidi;
    std::array<bool, 128> heldIn {};

    double fs = 48000.0, freeBeat = 0.0;
    int ctrlCount = 0;
    float scLevel = 0.f, scPeak = 1e-3f, voiceLev = 0.f;
    juce::SmoothedValue<float> outSm, robotSm, voiceSm, bypassSm;
    bool wasPlaying = false;
    int lastSource = -1, lastPresetLoaded = -2;
    float autoHold = 0.f, lastAutoNote = -1.f;
    juce::AudioBuffer<float> dryBuf;
public:
    std::atomic<bool> restartFree { false };
private:

    std::atomic<float>* p (const char* id) { return apvts.getRawParameterValue (id); }
    struct Ptrs
    {
        std::atomic<float> *bands, *formant, *attack, *release, *sharp, *sibil, *bright, *gate, *freeze,
                           *source, *wave, *unison, *detune, *octave, *glide, *mode, *vibrato, *noise, *envA, *envR,
                           *key, *scale, *harmony, *retune,
                           *clipOn, *sync, *bpm, *transpose, *rate, *loop,
                           *dblVoices, *dblDetune, *dblDelay, *dblWidth, *dblDrift, *dblMix,
                           *ensemble, *delayMix, *delayTime, *delayFb, *reverb, *revSize,
                           *robot, *voice, *output, *bypass;
    } P {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VckProcessor)
};
