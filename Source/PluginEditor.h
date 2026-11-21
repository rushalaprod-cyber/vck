#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// Interface de vck : meme base visuelle que ravenn 1.8 (fer forge, acier, glace, corbeau)
namespace ui
{
    inline juce::Font font (float h, bool bold = false, float kerning = 0.f)
    {
        return juce::Font (juce::FontOptions (juce::jmax (6.f, h), bold ? juce::Font::bold : juce::Font::plain)
                               .withKerningFactor (kerning));
    }
    const juce::Colour bg      { 0xff030406 };
    const juce::Colour panel   { 0xff07090d };
    const juce::Colour edge    { 0xff1d2733 };
    const juce::Colour track   { 0xff121821 };
    const juce::Colour text    { 0xffe9eef3 };
    const juce::Colour dim     { 0xff738191 };
    const juce::Colour violet  { 0xff3d7bff };
    const juce::Colour cyan    { 0xff9ccfff };
    const juce::Colour pink    { 0xff6f93c4 };
    const juce::Colour red     { 0xfff4f7fb };
    const juce::Colour amber   { 0xffc9d6e3 };
    const juce::Colour green   { 0xff5fb4ff };
    const juce::Colour blue    { 0xff2f5fd0 };
    juce::Image blurred (const juce::Image& src, int w, int h, int smallSize = 40);
}

//==============================================================================
class VckLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VckLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool hover, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool hover, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
};

//==============================================================================
class NeonKnob : public juce::Component
{
public:
    NeonKnob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& title,
              juce::Colour c, const juce::String& tip);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
private:
    juce::String title;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class ValueBox : public juce::Component
{
public:
    ValueBox (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& tip);
    void resized() override { slider.setBounds (getLocalBounds()); }
    juce::Slider slider;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class Segmented : public juce::Component
{
public:
    Segmented (juce::AudioProcessorValueTreeState& s, const juce::String& id, juce::Colour c,
               const juce::String& tip, juce::StringArray labels = {});
    void resized() override;
    void setColumns (int c) { columns = c; resized(); }
private:
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int columns = 0;
};

class Pill : public juce::Component
{
public:
    Pill (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& text,
          juce::Colour c, const juce::String& tip);
    void resized() override { button.setBounds (getLocalBounds()); }
    juce::TextButton button;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

class ChoiceBox : public juce::Component
{
public:
    ChoiceBox (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& tip);
    void resized() override { box.setBounds (getLocalBounds()); }
    juce::ComboBox box;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

//==============================================================================
class VoiceView : public juce::Component, private juce::Timer
{
public:
    explicit VoiceView (VckProcessor& p);
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    VckProcessor& proc;
    std::array<float, VckProcessor::maxBands> mod {}, out {}, peak {};
    float inL = 0.f, outL = 0.f, sib = 0.f;
};

class MidiRoll : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit MidiRoll (VckProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool dropHover = false;
    std::function<void()> onClick;
private:
    void timerCallback() override;
    VckProcessor& proc;
    vck::Clip clip;
    int version = -1;
};

// Graphe de hauteur (facon correcteur) : voix detectee et note du robot
class PitchGraph : public juce::Component, private juce::Timer
{
public:
    explicit PitchGraph (VckProcessor& p) : proc (p) { startTimerHz (30); setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    VckProcessor& proc;
    float centre = 60.f;
};

// Reponse du banc de filtres (bandes, plage, finesse) + niveau de chaque bande
class BankView : public juce::Component, private juce::Timer
{
public:
    explicit BankView (VckProcessor& p) : proc (p) { startTimerHz (25); setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    VckProcessor& proc;
};

// Page des presets (comme ravenn) : categories, recherche, tuiles
class PresetBrowser : public juce::Component
{
public:
    explicit PresetBrowser (VckProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    void syncSelection();
    std::function<void()> onClose, onSave, onImport, onExport;
private:
    void rebuildTiles();
    VckProcessor& proc;
    juce::TextEditor search;
    juce::TextButton closeBtn, saveBtn { "SAVE" }, importBtn { "IMPORT" }, folderBtn, exportBtn { "EXPORT" };
    juce::OwnedArray<juce::TextButton> catBtns, tiles;
    juce::String category { "TOUS" };
    juce::Rectangle<int> panel, tileArea;
};

class ClipEditorPage;

//==============================================================================
class VckEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit VckEditor (VckProcessor&);
    ~VckEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void setPage (int page);

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { roll.dropHover = dragHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { roll.dropHover = dragHover = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;
    void showBrowser (bool show) { browser.refresh(); browser.setVisible (show); browser.toFront (false); }
    void openClipEditor();

private:
    void timerCallback() override;
    void refreshPresets();
    void savePreset();
    void importPresets();
    void importMidi();
    void showMessage (const juce::String& m);
    void buildBackground (float pxScale);
    void place (juce::Component& c, int page);

    struct Panel { juce::String title; juce::Colour colour; juce::Rectangle<int> area; int page; };

    VckProcessor& proc;
    VckLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 500 };

    // VOCODER
    Segmented bands;
    NeonKnob loFreq, hiFreq, sharp, spread, inGain, gate, attack, release, formant, bright, character, sibil, breath;
    Pill freeze;
    BankView bankView;
    // CARRIER
    Segmented wave, mode, octave;
    NeonKnob unison, detune, sub, noise, glide, vibrato, vibRate, velSens, envA, envR, bendRange;
    // AUTO
    Segmented key, voiceRange, harmony;
    ChoiceBox scale;
    Pill drone;
    NeonKnob retune, humanize, tracking, hold, harmLevel, harmWidth, autoTrans;
    PitchGraph pitchGraph;
    // DOUBLER
    Segmented dblVoices;
    Pill dblLink;
    NeonKnob dblMix, dblDirect, dblWidth, dblDetune, dblDelay, dblDrift, dblRate, dblHpf, dblLpf;
    juce::OwnedArray<NeonKnob> dblVoiceKnobs;   // 4 voix x (delay, detune, pan, gain)
    // EFFECTS
    NeonKnob drive, lowCut, highCut, comp, ensemble, ensRate, phaser, phRate, phDepth,
             delayMix, delayFb, delayTone, reverb, revSize, revPre, revDamp, revWidth;
    ChoiceBox delayTime;
    Pill pingPong;
    // OUTPUT (toujours visible)
    Segmented source;
    NeonKnob robot, voice, width, output;
    // MIDI
    MidiRoll roll;
    juce::TextButton importMidiBtn, clearMidiBtn, editMidiBtn;
    Pill clipOn, loop;
    Segmented sync, rate;
    ValueBox bpm, transpose, clipGate;
    VoiceView voiceView;

    // en-tete facon ravenn
    Pill noLatency, bypass;
    juce::TextButton undoBtn, redoBtn, resetBtn, prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" }, folderBtn, importBtn, copyBtn, presetNameBtn;
    juce::OwnedArray<juce::TextButton> slotBtns, tabs;
    PresetBrowser browser;
    std::unique_ptr<ClipEditorPage> clipPage;
    std::unique_ptr<juce::FileChooser> chooser;

    std::vector<Panel> panels;
    std::vector<std::pair<juce::Component*, int>> pageOf;
    juce::Rectangle<int> header, logoArea, infoArea, autoInfoArea, midiInfoArea, msgArea, statusArea, dblColsArea, carrierInfoArea;
    juce::Image background, ravenImg, ravenCrisp, coverImg;
    juce::String shownPreset, message;
    int messageTicks = 0, page = 0, bgPage = -1, shownAB = -1, shownVoices = -1;
    float bgScale = 0.f;
    bool dragHover = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VckEditor)
};
