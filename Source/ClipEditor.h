#pragma once
#include "PluginEditor.h"

// Piano roll de vck (repris d'oraclee), en temps musical : tout reste cale sur le BPM.
//  Crayon    : clic dans le vide = nouvelle note (glisser pour la placer) - clic sur une note = la
//              deplacer - bord droit = longueur - Maj+glisser = dupliquer - clic droit = effacer
//  Selection : glisser = rectangle (Ctrl+glisser au crayon aussi)
//  Regle     : glisser la poignee de fin = longueur de la boucle (en mesures)
//  Molette = hauteur, Maj+molette = temps, Ctrl+molette = zoom temps, Alt+molette = zoom hauteur
//  Touches : Suppr, Ctrl+A, Ctrl+Z / Ctrl+Y, Ctrl+C / Ctrl+V, Ctrl+D, fleches, Q, P / E
class ClipRoll : public juce::Component, private juce::Timer
{
public:
    enum class Tool { draw, select };
    explicit ClipRoll (VckProcessor&);

    void setTool (Tool t) { tool = t; if (onStateChanged) onStateChanged(); }
    Tool getTool() const { return tool; }
    void setSnapBeats (double b) { snapBeats = b; repaint(); }
    double getSnapBeats() const { return snapBeats; }
    void zoomToFit();
    void reload();                       // reprend le clip du processeur (import, undo...)
    void quantize();
    void setLengthBars (int bars);
    int lengthBars() const { return (int) std::round (clip.lengthBeats / 4.0); }
    int numNotes() const { return (int) clip.notes.size(); }
    int numSelected() const;
    void undo();
    void redo();

    std::function<void()> onStateChanged;

    void paint (juce::Graphics&) override;
    void resized() override { if (! fitted) zoomToFit(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverNote = -1; repaint(); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scale) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    enum class Drag { none, move, resize, rubber, erase, velocity, loopEnd, keys };
    static constexpr float keyW = 56.f, rulerH = 26.f, velH = 70.f;
    juce::Rectangle<float> gridArea() const   { return { keyW, rulerH, getWidth() - keyW, getHeight() - rulerH - velH - 4.f }; }
    juce::Rectangle<float> rulerArea() const  { return { keyW, 0.f, getWidth() - keyW, rulerH }; }
    juce::Rectangle<float> keysArea() const   { return { 0.f, rulerH, keyW, getHeight() - rulerH - velH - 4.f }; }
    juce::Rectangle<float> velArea() const    { return { keyW, getHeight() - velH, getWidth() - keyW, velH }; }
    float xOf (double beat) const   { return gridArea().getX() + (float) ((beat - scrollBeat) * pxPerBeat); }
    double beatAt (float x) const   { return scrollBeat + (x - gridArea().getX()) / pxPerBeat; }
    float yOf (int pitch) const     { return gridArea().getY() + (topPitch - (float) pitch - 1.f) * rowH; }
    int pitchAt (float y) const     { return juce::jlimit (0, 127, (int) std::floor (topPitch - (y - gridArea().getY()) / rowH)); }
    juce::Rectangle<float> noteRect (const vck::ClipNote& n) const;
    double totalBeats() const;
    void clampScroll();
    void zoomX (double factor, float anchorX);
    void zoomY (float factor, float anchorY);
    int noteAt (juce::Point<float>) const;
    double snapRound (double b) const { return snapBeats > 0 ? std::round (b / snapBeats) * snapBeats : b; }
    double snapFloor (double b) const { return snapBeats > 0 ? std::floor (b / snapBeats) * snapBeats : b; }
    void selectNone();
    void selectAll();
    void deleteSelected();
    void transposeSelected (int semis);
    void moveSelected (double beats);
    void duplicateSelected();
    void copySelected();
    void paste();
    void audition (const vck::ClipNote& n) { lastAuditionPitch = n.note; proc.previewNote (n.note, n.vel); }
    void begin();
    void commit();
    void timerCallback() override;

    VckProcessor& proc;
    vck::Clip clip;
    int version = -1;
    Tool tool = Tool::draw;
    double snapBeats = 0.25, pxPerBeat = 40.0, scrollBeat = 0.0, cursorBeat = 0.0;
    float rowH = 12.f, topPitch = 84.f;
    bool fitted = false, editOpen = false;
    Drag drag = Drag::none;
    juce::Point<float> downPos;
    double downBeat = 0.0, lastLength = 0.0;
    int downPitch = 60, anchor = -1, lastAuditionPitch = -1, hoverNote = -1;
    float lastVel = 0.8f, lastVelX = 0.f;
    std::vector<vck::ClipNote> dragOrig, clipboard;
    juce::Rectangle<float> rubber;
};

// Page d'edition du clip (s'ouvre par-dessus l'interface, comme la page des presets)
class ClipEditorPage : public juce::Component, private juce::Timer
{
public:
    explicit ClipEditorPage (VckProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void open();
    std::function<void()> onClose, onImport;
    ClipRoll roll;

private:
    void timerCallback() override;
    void refreshButtons();
    VckProcessor& proc;
    juce::TextButton drawBtn { "CRAYON" }, selectBtn { u8 ("SÉLECTION") }, quantBtn { "QUANTIZE" }, lenMinus { "-" }, lenPlus { "+" },
                     importBtn { "IMPORT" }, exportBtn { "EXPORT .MID" }, clearBtn { "CLEAR" }, undoBtn { "UNDO" }, redoBtn { "REDO" },
                     closeBtn { "FERMER" };
    juce::ComboBox snapBox;
    ValueBox gate, transpose;
    Pill playPill, loopPill;
    juce::Rectangle<int> panel, lenArea, infoArea;
    std::unique_ptr<juce::FileChooser> chooser;
};
