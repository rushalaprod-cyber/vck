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
    const juce::Colour violet  { 0xff3d7bff };   // bleu electrique
    const juce::Colour cyan    { 0xff9ccfff };   // glace
    const juce::Colour pink    { 0xff6f93c4 };   // acier bleute
    const juce::Colour red     { 0xfff4f7fb };   // blanc os
    const juce::Colour amber   { 0xffc9d6e3 };   // argent
    const juce::Colour green   { 0xff5fb4ff };   // bleu clair
    const juce::Colour blue    { 0xff2f5fd0 };   // bleu nuit
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

// Petite case de valeur (BPM, transposition) : glisser verticalement, double-clic = defaut
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
// Vue de la voix : enveloppe de chaque bande, porteuse, notes jouees
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

// Clip MIDI : piano roll, tete de lecture, depot de fichier
class MidiRoll : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit MidiRoll (VckProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool dropHover = false;
private:
    void timerCallback() override;
    VckProcessor& proc;
    vck::Clip clip;
    int version = -1;
};

//==============================================================================
class VckEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit VckEditor (VckProcessor&);
    ~VckEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { roll.dropHover = dragHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { roll.dropHover = dragHover = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

    void loadPresetById (int id);

private:
    void timerCallback() override;
    void rebuildPresetBox();
    void savePreset();
    void importMidi();
    void showMessage (const juce::String& m);
    void buildBackground (float pxScale);

    struct Panel { juce::String title; juce::Colour colour; juce::Rectangle<int> area; };

    VckProcessor& proc;
    VckLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 500 };

    // vocoder
    Segmented bands;
    NeonKnob formant, sharp, bright, gate, attack, release, sibil;
    Pill freeze;
    // porteuse
    Segmented source, wave, mode, octave;
    NeonKnob unison, detune, glide, vibrato, noise, envA, envR;
    // auto
    Segmented key, harmony;
    ChoiceBox scale;
    NeonKnob retune;
    // midi
    MidiRoll roll;
    juce::TextButton importBtn, clearBtn;
    Pill clipOn, loop;
    Segmented sync, rate;
    ValueBox bpm, transpose;
    // doubleur / effets / sortie
    Segmented dblVoices;
    NeonKnob dblDetune, dblDelay, dblWidth, dblDrift, dblMix;
    NeonKnob ensemble, delayMix, delayFb, reverb, revSize;
    ChoiceBox delayTime;
    NeonKnob robot, voice, output;
    Pill bypass;
    VoiceView voiceView;

    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" };
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::AlertWindow> saveDialog;

    std::vector<Panel> panels;
    juce::Rectangle<int> header, logoArea, infoArea, autoInfoArea, midiInfoArea, msgArea;
    juce::Image background, ravenImg, ravenCrisp, coverImg;
    juce::String shownPreset, shownInfo, message;
    int messageTicks = 0, shownSource = -1;
    float bgScale = 0.f;
    bool dragHover = false;
    juce::Component *autoPanelComps[5] {}, *midiPanelComps[9] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VckEditor)
};
