#include "ClipEditor.h"
#include "UiDraw.h"
using namespace uidraw;

namespace
{
    juce::String T (const char* s) { return juce::String::fromUTF8 (s); }
}

ClipRoll::ClipRoll (VckProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);
    reload();
    startTimerHz (30);
}

juce::Rectangle<float> ClipRoll::noteRect (const vck::ClipNote& n) const
{
    const float x0 = xOf (n.start), x1 = xOf (n.start + n.len);
    return { x0, yOf (n.note) + 1.f, std::max (3.f, x1 - x0), std::max (2.f, rowH - 2.f) };
}

double ClipRoll::totalBeats() const
{
    double t = std::max (16.0, clip.lengthBeats + 8.0);
    for (auto& n : clip.notes) t = std::max (t, n.start + n.len + 8.0);
    return t;
}

void ClipRoll::clampScroll()
{
    const auto g = gridArea();
    pxPerBeat = juce::jlimit (4.0, 600.0, pxPerBeat);
    rowH = juce::jlimit (5.f, 40.f, rowH);
    scrollBeat = juce::jlimit (0.0, std::max (0.0, totalBeats() - g.getWidth() / pxPerBeat * 0.5), scrollBeat);
    topPitch = juce::jlimit (std::min (128.f, g.getHeight() / rowH), 128.f, topPitch);
}

void ClipRoll::zoomX (double factor, float anchorX)
{
    const double b = beatAt (anchorX);
    pxPerBeat *= factor; clampScroll();
    scrollBeat = b - (anchorX - gridArea().getX()) / pxPerBeat; clampScroll();
    repaint();
}

void ClipRoll::zoomY (float factor, float anchorY)
{
    const float p = topPitch - (anchorY - gridArea().getY()) / rowH;
    rowH *= factor; clampScroll();
    topPitch = p + (anchorY - gridArea().getY()) / rowH; clampScroll();
    repaint();
}

void ClipRoll::zoomToFit()
{
    const auto g = gridArea();
    if (g.getWidth() <= 10.f || g.getHeight() <= 10.f) return;
    const double len = std::max (8.0, std::max (clip.lengthBeats, clip.computeLength()));
    pxPerBeat = g.getWidth() / (len + 1.0);
    scrollBeat = 0.0;
    int lo = 127, hi = 0;
    for (auto& n : clip.notes) { lo = std::min (lo, n.note); hi = std::max (hi, n.note); }
    if (lo > hi) { lo = 48; hi = 72; }
    const int span = juce::jmax (24, hi - lo + 6);
    rowH = juce::jlimit (7.f, 22.f, g.getHeight() / (float) span);
    topPitch = (float) (lo + hi) * 0.5f + g.getHeight() / rowH * 0.5f + 0.5f;
    clampScroll();
    fitted = true;
    repaint();
}

void ClipRoll::reload()
{
    version = proc.clipVersion.load();
    clip = proc.getClipCopy();
    if (clip.lengthBeats <= 0) clip.lengthBeats = 16.0;   // clip vide : 4 mesures pour ecrire
    repaint();
    if (onStateChanged) onStateChanged();
}

void ClipRoll::begin()
{
    if (editOpen) return;
    proc.pushClipUndo();
    editOpen = true;
}

void ClipRoll::commit()
{
    if (editOpen)
    {
        editOpen = false;
        const bool wasEmpty = proc.getClipCopy().empty();
        auto out = clip;
        for (auto& n : out.notes) n.selected = false;
        std::sort (out.notes.begin(), out.notes.end(), [] (auto& a, auto& b) { return a.start < b.start; });
        if (out.name.isEmpty()) out.name = "clip";
        proc.setClip (out);
        version = proc.clipVersion.load();
        // premieres notes ecrites : on veut les entendre (source MIDI + lecture)
        if (wasEmpty && ! out.empty())
        {
            proc.undo.beginNewTransaction ("MIDI");
            proc.presets.setTreeValue ("source", 0.f, &proc.undo);
            proc.presets.setTreeValue ("clipOn", 1.f, &proc.undo);
            proc.undo.beginNewTransaction();
        }
    }
    repaint();
    if (onStateChanged) onStateChanged();
}

void ClipRoll::undo() { if (proc.clipUndo()) reload(); }
void ClipRoll::redo() { if (proc.clipRedo()) reload(); }

int ClipRoll::noteAt (juce::Point<float> pos) const
{
    for (int i = (int) clip.notes.size(); --i >= 0;)
        if (noteRect (clip.notes[(size_t) i]).expanded (0.f, 0.5f).contains (pos)) return i;
    return -1;
}

int ClipRoll::numSelected() const
{
    int c = 0;
    for (auto& n : clip.notes) c += n.selected ? 1 : 0;
    return c;
}

void ClipRoll::selectNone() { for (auto& n : clip.notes) n.selected = false; repaint(); if (onStateChanged) onStateChanged(); }
void ClipRoll::selectAll()  { for (auto& n : clip.notes) n.selected = true;  repaint(); if (onStateChanged) onStateChanged(); }

void ClipRoll::deleteSelected()
{
    if (numSelected() == 0) return;
    begin();
    clip.notes.erase (std::remove_if (clip.notes.begin(), clip.notes.end(), [] (auto& n) { return n.selected; }), clip.notes.end());
    commit();
}

void ClipRoll::transposeSelected (int semis)
{
    if (numSelected() == 0) return;
    begin();
    const vck::ClipNote* first = nullptr;
    for (auto& n : clip.notes) if (n.selected) { n.note = juce::jlimit (0, 127, n.note + semis); if (first == nullptr) first = &n; }
    if (first != nullptr) audition (*first);
    commit();
}

void ClipRoll::moveSelected (double beats)
{
    if (numSelected() == 0) return;
    double minStart = 1e9;
    for (auto& n : clip.notes) if (n.selected) minStart = std::min (minStart, n.start);
    beats = std::max (beats, -minStart);
    begin();
    for (auto& n : clip.notes) if (n.selected) n.start += beats;
    commit();
}

void ClipRoll::quantize()
{
    const double s = snapBeats > 0 ? snapBeats : 0.25;
    const bool any = numSelected() > 0;
    if (clip.notes.empty()) return;
    begin();
    for (auto& n : clip.notes)
    {
        if (any && ! n.selected) continue;
        const double end = n.start + n.len;
        n.start = std::max (0.0, std::round (n.start / s) * s);
        n.len = std::max (s, std::round (end / s) * s - n.start);
    }
    commit();
}

void ClipRoll::setLengthBars (int bars)
{
    bars = juce::jlimit (1, 128, bars);
    if (std::abs (clip.lengthBeats - bars * 4.0) < 1e-6) return;
    begin();
    clip.lengthBeats = bars * 4.0;
    commit();
}

void ClipRoll::duplicateSelected()
{
    std::vector<vck::ClipNote> sel;
    for (auto& n : clip.notes) if (n.selected) sel.push_back (n);
    if (sel.empty()) return;
    double a = 1e9, b = 0;
    for (auto& n : sel) { a = std::min (a, n.start); b = std::max (b, n.start + n.len); }
    const double s = snapBeats > 0 ? snapBeats : 1.0;
    const double offset = std::ceil ((b - a) / s - 1e-6) * s;
    begin();
    for (auto& n : clip.notes) n.selected = false;
    for (auto n : sel) { n.start += offset; n.selected = true; clip.notes.push_back (n); }
    commit();
}

void ClipRoll::copySelected()
{
    clipboard.clear();
    for (auto& n : clip.notes) if (n.selected) clipboard.push_back (n);
}

void ClipRoll::paste()
{
    if (clipboard.empty()) return;
    double a = 1e9;
    for (auto& n : clipboard) a = std::min (a, n.start);
    const double offset = snapRound (cursorBeat) - a;
    begin();
    for (auto& n : clip.notes) n.selected = false;
    for (auto n : clipboard) { n.start += offset; n.selected = true; clip.notes.push_back (n); }
    commit();
}

//==============================================================================
void ClipRoll::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const auto pos = e.position;
    downPos = pos; downBeat = beatAt (pos.x); downPitch = pitchAt (pos.y);
    drag = Drag::none; anchor = -1;

    if (rulerArea().contains (pos))
    {
        // poignee de fin de boucle : on la saisit si on clique pres d'elle, sinon on place le curseur
        if (std::abs (pos.x - xOf (clip.lengthBeats)) < 10.f) { drag = Drag::loopEnd; begin(); return; }
        cursorBeat = juce::jlimit (0.0, totalBeats(), snapRound (downBeat));
        proc.restartFree = true;   // en mode FREE, le clip repart du debut
        repaint();
        return;
    }
    if (keysArea().contains (pos)) { drag = Drag::keys; lastAuditionPitch = downPitch; proc.previewNote (downPitch, 0.8f); repaint(); return; }
    if (velArea().contains (pos)) { drag = Drag::velocity; lastVelX = pos.x; mouseDrag (e); return; }
    if (! gridArea().contains (pos)) return;

    auto& notes = clip.notes;
    if (e.mods.isPopupMenu()) { drag = Drag::erase; mouseDrag (e); return; }

    const int hit = noteAt (pos);
    const bool rubberWanted = tool == Tool::select ? hit < 0 : (e.mods.isCommandDown() || e.mods.isCtrlDown());
    if (rubberWanted)
    {
        drag = Drag::rubber;
        if (! e.mods.isShiftDown()) for (auto& n : notes) n.selected = false;
        dragOrig = notes;
        rubber = { pos, pos };
        repaint();
        return;
    }
    if (hit >= 0)
    {
        auto& n = notes[(size_t) hit];
        if (tool == Tool::select && e.mods.isShiftDown() && n.selected) { n.selected = false; repaint(); return; }
        if (! n.selected)
        {
            if (! e.mods.isShiftDown() || tool == Tool::draw) for (auto& x : notes) x.selected = false;
            n.selected = true;
        }
        lastLength = n.len; lastVel = n.vel; anchor = hit;
        audition (n);
        const auto r = noteRect (n);
        drag = pos.x > r.getRight() - std::min (9.f, r.getWidth() * 0.35f) ? Drag::resize : Drag::move;
        if (drag == Drag::move && tool == Tool::draw && e.mods.isShiftDown())   // Maj+glisser = dupliquer
        {
            begin();
            const size_t count = notes.size();
            for (size_t i = 0; i < count; ++i)
                if (notes[i].selected)
                {
                    auto c = notes[i];
                    notes[i].selected = false;
                    notes.push_back (c);
                    if ((int) i == hit) anchor = (int) notes.size() - 1;
                }
        }
        dragOrig = notes;
        repaint();
        if (onStateChanged) onStateChanged();
        return;
    }
    if (tool == Tool::draw)
    {
        begin();
        for (auto& x : notes) x.selected = false;
        vck::ClipNote n;
        n.start = std::max (0.0, snapFloor (downBeat));
        n.len = lastLength > 0 ? lastLength : (snapBeats > 0 ? snapBeats : 0.25);
        n.note = downPitch; n.vel = lastVel; n.selected = true;
        notes.push_back (n);
        anchor = (int) notes.size() - 1;
        audition (n);
        drag = Drag::move;
        dragOrig = notes;
        repaint();
    }
}

void ClipRoll::mouseDrag (const juce::MouseEvent& e)
{
    const auto pos = e.position;
    auto& notes = clip.notes;
    switch (drag)
    {
        case Drag::loopEnd:
        {
            const double b = std::max (4.0, std::round (beatAt (pos.x) / 4.0) * 4.0);
            clip.lengthBeats = std::min (512.0, b);
            repaint();
            if (onStateChanged) onStateChanged();
            break;
        }
        case Drag::keys:
        {
            const int p = pitchAt (pos.y);
            if (p != lastAuditionPitch) { lastAuditionPitch = p; proc.previewNote (p, 0.8f); repaint(); }
            break;
        }
        case Drag::velocity:
        {
            const auto va = velArea();
            const float v = juce::jlimit (0.02f, 1.f, (va.getBottom() - 3.f - pos.y) / (va.getHeight() - 8.f));
            const float x0 = std::min (lastVelX, pos.x) - 4.f, x1 = std::max (lastVelX, pos.x) + 4.f;
            const bool onlySel = numSelected() > 0;
            for (auto& n : notes)
            {
                if (onlySel && ! n.selected) continue;
                const float x = xOf (n.start);
                if (x >= x0 && x <= x1 && std::abs (n.vel - v) > 1e-3f) { begin(); n.vel = v; lastVel = v; }
            }
            lastVelX = pos.x;
            repaint();
            break;
        }
        case Drag::erase:
        {
            const int hit = noteAt (pos);
            if (hit >= 0) { begin(); notes.erase (notes.begin() + hit); repaint(); }
            break;
        }
        case Drag::rubber:
        {
            if (notes.size() != dragOrig.size()) { drag = Drag::none; break; }
            rubber = juce::Rectangle<float> (downPos, pos);
            for (size_t i = 0; i < notes.size(); ++i)
                notes[i].selected = dragOrig[i].selected || noteRect (notes[i]).intersects (rubber);
            repaint();
            break;
        }
        case Drag::move:
        {
            if (anchor < 0 || notes.size() != dragOrig.size()) { drag = Drag::none; break; }
            if (! editOpen && e.getDistanceFromDragStart() < 3) break;
            begin();
            const auto& a = dragOrig[(size_t) anchor];
            const double dt = beatAt (pos.x) - downBeat;
            double delta = (snapBeats > 0 ? snapRound (a.start + dt) : a.start + dt) - a.start;
            int dp = pitchAt (pos.y) - downPitch;
            double minStart = 1e9; int minP = 127, maxP = 0;
            for (auto& n : dragOrig) if (n.selected) { minStart = std::min (minStart, n.start); minP = std::min (minP, n.note); maxP = std::max (maxP, n.note); }
            delta = std::max (delta, -minStart);
            dp = juce::jlimit (-minP, 127 - maxP, dp);
            for (size_t i = 0; i < notes.size(); ++i)
                if (dragOrig[i].selected) { notes[i] = dragOrig[i]; notes[i].start += delta; notes[i].note += dp; }
            if (notes[(size_t) anchor].note != lastAuditionPitch) audition (notes[(size_t) anchor]);
            repaint();
            break;
        }
        case Drag::resize:
        {
            if (anchor < 0 || notes.size() != dragOrig.size()) { drag = Drag::none; break; }
            begin();
            const auto& a = dragOrig[(size_t) anchor];
            const double dt = beatAt (pos.x) - downBeat;
            const double aEnd = a.start + a.len;
            const double newEnd = snapBeats > 0 ? snapRound (aEnd + dt) : aEnd + dt;
            const double dEnd = newEnd - aEnd;
            const double minLen = snapBeats > 0 ? snapBeats * 0.5 : 0.03;
            for (size_t i = 0; i < notes.size(); ++i)
                if (dragOrig[i].selected) { notes[i] = dragOrig[i]; notes[i].len = std::max (minLen, dragOrig[i].len + dEnd); }
            lastLength = notes[(size_t) anchor].len;
            repaint();
            break;
        }
        case Drag::none: break;
    }
}

void ClipRoll::mouseUp (const juce::MouseEvent&)
{
    if (drag == Drag::rubber) { rubber = {}; if (onStateChanged) onStateChanged(); }
    if (drag == Drag::keys) { lastAuditionPitch = -1; }
    drag = Drag::none; anchor = -1;
    commit();
}

void ClipRoll::mouseMove (const juce::MouseEvent& e)
{
    const auto pos = e.position;
    auto c = juce::MouseCursor::NormalCursor;
    if (gridArea().contains (pos))
    {
        const int hit = noteAt (pos);
        if (hit >= 0)
        {
            const auto r = noteRect (clip.notes[(size_t) hit]);
            c = pos.x > r.getRight() - std::min (9.f, r.getWidth() * 0.35f) ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::DraggingHandCursor;
        }
        else if (tool == Tool::draw) c = juce::MouseCursor::CrosshairCursor;
        if (hit != hoverNote) { hoverNote = hit; repaint(); }
    }
    else if (rulerArea().contains (pos) && std::abs (pos.x - xOf (clip.lengthBeats)) < 10.f) c = juce::MouseCursor::LeftRightResizeCursor;
    else if (keysArea().contains (pos) || rulerArea().contains (pos)) c = juce::MouseCursor::PointingHandCursor;
    else if (velArea().contains (pos)) c = juce::MouseCursor::UpDownResizeCursor;
    setMouseCursor (c);
}

void ClipRoll::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown()) zoomX (std::pow (1.0015, (w.deltaY + w.deltaX) * 300.0), e.position.x);
    else if (e.mods.isAltDown()) zoomY (std::pow (1.0015f, w.deltaY * 300.f), e.position.y);
    else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
    {
        const float d = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
        scrollBeat -= d * 400.0 / pxPerBeat; clampScroll(); repaint();
    }
    else { topPitch += w.deltaY * 30.f; clampScroll(); repaint(); }
}

void ClipRoll::mouseMagnify (const juce::MouseEvent& e, float scale) { zoomX (scale, e.position.x); }

bool ClipRoll::keyPressed (const juce::KeyPress& k)
{
    const auto m = k.getModifiers();
    const int code = k.getKeyCode();
    const bool cmd = m.isCommandDown() || m.isCtrlDown();
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey) { deleteSelected(); return true; }
    if (code == juce::KeyPress::escapeKey) { selectNone(); return true; }
    if (code == juce::KeyPress::upKey) { transposeSelected (m.isShiftDown() ? 12 : 1); return true; }
    if (code == juce::KeyPress::downKey) { transposeSelected (m.isShiftDown() ? -12 : -1); return true; }
    const double nudge = snapBeats > 0 ? snapBeats : 0.25;
    if (code == juce::KeyPress::leftKey) { moveSelected (-nudge); return true; }
    if (code == juce::KeyPress::rightKey) { moveSelected (nudge); return true; }
    const int ch = juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) code);
    if (cmd)
    {
        if (ch == 'A') { selectAll(); return true; }
        if (ch == 'Z') { m.isShiftDown() ? redo() : undo(); return true; }
        if (ch == 'Y') { redo(); return true; }
        if (ch == 'C') { copySelected(); return true; }
        if (ch == 'V') { paste(); return true; }
        if (ch == 'D' || ch == 'B') { duplicateSelected(); return true; }
        return false;
    }
    if (ch == 'Q') { quantize(); return true; }
    if (ch == 'P' || ch == 'B') { setTool (Tool::draw); return true; }
    if (ch == 'E') { setTool (Tool::select); return true; }
    return false;
}

void ClipRoll::timerCallback()
{
    // le clip a change ailleurs (import, undo, clear) : on le reprend
    if (! editOpen && drag == Drag::none && proc.clipVersion.load() != version)
    {
        const bool wasEmpty = clip.notes.empty();
        reload();
        if (wasEmpty || ! fitted) zoomToFit();
    }
    repaint();
}

//==============================================================================
void ClipRoll::paint (juce::Graphics& g)
{
    using namespace juce;
    g.fillAll (Colour (0xff070a0f));
    const auto grid = gridArea();
    auto& notes = clip.notes;

    const int pTop = pitchAt (grid.getY() + 0.5f), pBot = pitchAt (grid.getBottom() - 0.5f);
    for (int p = pBot; p <= pTop; ++p)
    {
        const float y = yOf (p);
        if (MidiMessage::isMidiNoteBlack (p)) { g.setColour (Colours::black.withAlpha (0.45f)); g.fillRect (grid.getX(), y, grid.getWidth(), rowH); }
        if (p % 12 == 0) { g.setColour (ui::edge.brighter (0.4f)); g.drawHorizontalLine ((int) (y + rowH), grid.getX(), grid.getRight()); }
        else if (p % 12 == 5) { g.setColour (ui::edge.withAlpha (0.5f)); g.drawHorizontalLine ((int) (y + rowH), grid.getX(), grid.getRight()); }
    }

    g.saveState();
    g.reduceClipRegion (grid.toNearestInt());
    // zone hors boucle : assombrie (ces notes ne jouent pas)
    {
        const float xe = xOf (clip.lengthBeats);
        if (xe < grid.getRight()) { g.setColour (Colours::black.withAlpha (0.45f)); g.fillRect (Rectangle<float> (xe, grid.getY(), grid.getRight() - xe, grid.getHeight())); }
    }
    auto drawLines = [&] (double unit, Colour c)
    {
        if (unit <= 0 || unit * pxPerBeat < 5.0) return;
        g.setColour (c);
        const double b1 = beatAt (grid.getRight());
        for (double b = std::floor (scrollBeat / unit) * unit; b <= b1; b += unit)
            g.drawVerticalLine ((int) std::round (xOf (b)), grid.getY(), grid.getBottom());
    };
    drawLines (snapBeats, ui::edge.withAlpha (0.35f));
    drawLines (1.0, ui::edge.withAlpha (0.8f));
    drawLines (4.0, ui::dim.withAlpha (0.6f));
    g.setColour (ui::cyan.withAlpha (0.7f));
    { const float xe = xOf (clip.lengthBeats); g.drawLine (xe, grid.getY(), xe, grid.getBottom(), 1.5f); }

    for (size_t i = 0; i < notes.size(); ++i)
    {
        const auto& n = notes[i];
        const auto r = noteRect (n);
        if (r.getRight() < grid.getX() || r.getX() > grid.getRight() || r.getBottom() < grid.getY() || r.getY() > grid.getBottom()) continue;
        const auto c = ui::cyan.interpolatedWith (ui::violet, 1.f - n.vel);
        const bool out = n.start >= clip.lengthBeats;
        const float rad = std::min (3.f, r.getHeight() * 0.3f);
        g.setColour ((n.selected ? c.brighter (0.4f) : c).withAlpha (out ? 0.3f : (0.55f + 0.45f * n.vel)));
        g.fillRoundedRectangle (r, rad);
        g.setColour (Colours::black.withAlpha (0.25f));
        g.fillRect (r.withTrimmedLeft (r.getWidth() * n.vel).reduced (0.f, r.getHeight() * 0.38f));
        g.setColour (n.selected ? Colours::white : c.brighter (0.6f).withAlpha ((int) i == hoverNote ? 1.f : 0.7f));
        g.drawRoundedRectangle (r, rad, n.selected ? 1.6f : ((int) i == hoverNote ? 1.3f : 0.8f));
        if (r.getWidth() > 24.f && r.getHeight() >= 9.f)
        {
            g.setColour (Colour (0xff05080d));
            g.setFont (ui::font (std::min (11.f, r.getHeight() - 1.f), true));
            g.drawText (noteName ((float) n.note), r.reduced (4.f, 0.f), Justification::centredLeft, false);
        }
    }
    if (! rubber.isEmpty())
    {
        g.setColour (ui::cyan.withAlpha (0.12f)); g.fillRect (rubber);
        g.setColour (ui::cyan); g.drawRect (rubber, 1.f);
    }
    g.setColour (ui::green.withAlpha (0.5f));
    g.drawVerticalLine ((int) xOf (cursorBeat), grid.getY(), grid.getBottom());
    const float pb = proc.playBeat.load();
    if (pb >= 0.f)
    {
        const float x = xOf (pb);
        g.setColour (Colours::white);
        g.drawLine (x, grid.getY(), x, grid.getBottom(), 1.5f);
    }
    if (notes.empty())
    {
        g.setColour (ui::text);
        g.setFont (ui::font (15.f, true));
        g.drawText (T ("Écris ta mélodie ou tes accords au crayon, ou glisse un fichier .mid"), grid.withTrimmedBottom (24.f), Justification::centred);
        g.setColour (ui::dim);
        g.setFont (ui::font (12.f));
        g.drawText (T ("clic = note · bord droit = longueur · clic droit = effacer · Ctrl + glisser = sélection · molette = défiler, Ctrl + molette = zoom"),
                    grid.withTrimmedTop (34.f), Justification::centred);
    }
    g.restoreState();

    // regle (mesures) + poignee de fin de boucle
    const auto ru = rulerArea();
    g.setColour (ui::panel); g.fillRect (ru);
    g.setColour (ui::edge); g.drawHorizontalLine ((int) ru.getBottom() - 1, ru.getX(), ru.getRight());
    {
        g.saveState();
        g.reduceClipRegion (ru.toNearestInt());
        const double b1 = beatAt (ru.getRight());
        const long barsPerLabel = std::max (1L, (long) std::ceil (40.0 / std::max (1.0, 4.0 * pxPerBeat)));
        g.setFont (ui::font (11.f, true));
        for (double b = std::floor (scrollBeat); b <= b1; b += 1.0)
        {
            const float x = xOf (b);
            const long bi = std::lround (b);
            const bool isBar = bi % 4 == 0;
            g.setColour (isBar ? ui::dim : ui::edge.brighter (0.3f));
            g.drawVerticalLine ((int) x, isBar ? ru.getY() + 6.f : ru.getBottom() - 7.f, ru.getBottom());
            if (isBar && (bi / 4) % barsPerLabel == 0)
                g.drawText (String (bi / 4 + 1), (int) x + 4, (int) ru.getY() + 2, 40, (int) ru.getHeight() - 4, Justification::centredLeft);
        }
        const float xe = xOf (clip.lengthBeats);
        Path tri;
        tri.addTriangle (xe - 7.f, ru.getY() + 2.f, xe + 7.f, ru.getY() + 2.f, xe, ru.getBottom() - 4.f);
        g.setColour (ui::cyan);
        g.fillPath (tri);
        Path cur;
        const float cx = xOf (cursorBeat);
        cur.addTriangle (cx - 5.f, ru.getY() + 2.f, cx + 5.f, ru.getY() + 2.f, cx, ru.getY() + 10.f);
        g.setColour (ui::green);
        g.fillPath (cur);
        g.restoreState();
    }
    g.setColour (ui::panel); g.fillRect (0.f, 0.f, keyW, rulerH);

    // clavier
    const auto ka = keysArea();
    g.saveState();
    g.reduceClipRegion (ka.toNearestInt());
    g.setColour (ui::panel); g.fillRect (ka);
    for (int p = pBot; p <= pTop; ++p)
    {
        const float y = yOf (p);
        const bool black = MidiMessage::isMidiNoteBlack (p);
        const bool pressed = drag == Drag::keys && p == lastAuditionPitch;
        auto keyR = Rectangle<float> (ka.getX(), y, ka.getWidth() - 2.f, rowH).reduced (0.f, 0.5f);
        if (black)
        {
            g.setColour (pressed ? ui::cyan : Colour (0xff0a0d12)); g.fillRect (keyR.withWidth (keyR.getWidth() * 0.62f));
            g.setColour (Colour (0xffc9d2dc).withAlpha (0.85f)); g.fillRect (keyR.withTrimmedLeft (keyR.getWidth() * 0.62f));
        }
        else { g.setColour (pressed ? ui::cyan : Colour (0xffc9d2dc)); g.fillRect (keyR); }
        if (p % 12 == 0 && rowH >= 7.f)
        {
            g.setColour (Colour (0xff1d2733));
            g.setFont (ui::font (jlimit (8.f, 11.f, rowH * 0.85f), true));
            g.drawText (noteName ((float) p), keyR.reduced (4.f, 0.f), Justification::centredRight);
        }
    }
    g.restoreState();

    // couloir de velocite
    const auto va = velArea();
    g.setColour (ui::panel); g.fillRect (va.withX (0.f).withWidth ((float) getWidth()));
    g.setColour (ui::edge); g.drawHorizontalLine ((int) va.getY(), 0.f, (float) getWidth());
    g.setColour (ui::dim); g.setFont (ui::font (10.f, true));
    g.drawFittedText (T ("VÉLOCITÉ"), Rectangle<int> (4, (int) va.getY() + 4, (int) keyW - 8, 30), Justification::topRight, 2);
    g.saveState();
    g.reduceClipRegion (va.toNearestInt());
    const bool anySel = numSelected() > 0;
    for (auto& n : notes)
    {
        const float x = xOf (n.start);
        if (x < va.getX() - 4.f || x > va.getRight() + 4.f) continue;
        const float h = (va.getHeight() - 8.f) * n.vel;
        g.setColour ((n.selected ? ui::cyan.brighter (0.4f) : ui::cyan).withAlpha (anySel && ! n.selected ? 0.35f : 0.95f));
        g.drawLine (x, va.getBottom() - 3.f, x, va.getBottom() - 3.f - h, 2.f);
        g.fillEllipse (x - 3.f, va.getBottom() - 6.f - h, 6.f, 6.f);
    }
    g.restoreState();
}

//==============================================================================
ClipEditorPage::ClipEditorPage (VckProcessor& p)
    : roll (p),
      gate (p.apvts, "clipGate", T ("GATE : longueur jouée de chaque note (100 % = comme écrite, moins = plus piqué, plus = plus lié). Glisse pour changer.")),
      transpose (p.apvts, "transpose", T ("Transposition du clip (demi-tons).")),
      playPill (p.apvts, "clipOn", "PLAY", ui::pink, T ("Joue le clip.")),
      loopPill (p.apvts, "loop", "LOOP", ui::pink, T ("Boucle le clip.")),
      proc (p)
{
    addAndMakeVisible (roll);
    for (auto* b : { &drawBtn, &selectBtn, &quantBtn, &lenMinus, &lenPlus, &importBtn, &exportBtn, &clearBtn, &undoBtn, &redoBtn, &closeBtn })
    {
        b->setColour (juce::TextButton::buttonOnColourId, ui::cyan);
        addAndMakeVisible (b);
    }
    for (juce::Component* c : { (juce::Component*) &snapBox, (juce::Component*) &gate, (juce::Component*) &transpose,
                                (juce::Component*) &playPill, (juce::Component*) &loopPill })
        addAndMakeVisible (c);
    closeBtn.setToggleState (true, juce::dontSendNotification);
    drawBtn.setTooltip (T ("Crayon (P) : clic = note, bord droit = longueur, clic droit = effacer"));
    selectBtn.setTooltip (T ("Sélection (E) : glisser = rectangle"));
    quantBtn.setTooltip (T ("Quantize (Q) : cale les notes sélectionnées (ou toutes) sur la grille"));
    lenMinus.setTooltip (T ("Boucle plus courte (1 mesure)"));
    lenPlus.setTooltip (T ("Boucle plus longue (1 mesure). Tu peux aussi glisser le triangle bleu de la règle."));
    exportBtn.setTooltip (T ("Enregistre le clip en .mid (au tempo du morceau)"));
    drawBtn.onClick = [this] { roll.setTool (ClipRoll::Tool::draw); };
    selectBtn.onClick = [this] { roll.setTool (ClipRoll::Tool::select); };
    quantBtn.onClick = [this] { roll.quantize(); };
    lenMinus.onClick = [this] { roll.setLengthBars (roll.lengthBars() - 1); };
    lenPlus.onClick = [this] { roll.setLengthBars (roll.lengthBars() + 1); };
    undoBtn.onClick = [this] { roll.undo(); };
    redoBtn.onClick = [this] { roll.redo(); };
    clearBtn.onClick = [this] { proc.clearMidi(); };
    importBtn.onClick = [this] { if (onImport) onImport(); };
    closeBtn.onClick = [this] { if (onClose) onClose(); };
    exportBtn.onClick = [this]
    {
        auto c = proc.getClipCopy();
        chooser = std::make_unique<juce::FileChooser> (T ("Exporter le clip"),
                      juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile ((c.name.isEmpty() ? juce::String ("clip") : c.name) + ".mid"), "*.mid");
        juce::Component::SafePointer<ClipEditorPage> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safe] (const juce::FileChooser& fc)
        {
            if (safe == nullptr) return;
            auto f = fc.getResult();
            if (f != juce::File()) safe->proc.getClipCopy().save (f.withFileExtension (".mid"), safe->proc.uiBpm.load());
        });
    };
    const char* snaps[] = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4 T", "1/8 T", "1/16 T", "LIBRE" };
    for (int i = 0; i < 10; ++i) snapBox.addItem (snaps[i], i + 1);
    snapBox.setSelectedId (5, juce::dontSendNotification);
    roll.setSnapBeats (0.25);
    snapBox.setTooltip (T ("Grille (magnétisme) : les notes se calent sur cette division du temps"));
    snapBox.onChange = [this]
    {
        static constexpr double v[10] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 0.0 };
        roll.setSnapBeats (v[juce::jlimit (0, 9, snapBox.getSelectedId() - 1)]);
    };
    roll.onStateChanged = [this] { refreshButtons(); };
    refreshButtons();
    startTimerHz (10);
}

void ClipEditorPage::open()
{
    roll.reload();
    roll.zoomToFit();
    setVisible (true);
    toFront (false);
    roll.grabKeyboardFocus();
}

void ClipEditorPage::refreshButtons()
{
    drawBtn.setToggleState (roll.getTool() == ClipRoll::Tool::draw, juce::dontSendNotification);
    selectBtn.setToggleState (roll.getTool() == ClipRoll::Tool::select, juce::dontSendNotification);
    undoBtn.setEnabled (proc.canClipUndo());
    redoBtn.setEnabled (proc.canClipRedo());
    repaint (lenArea);
    repaint (infoArea);
}

void ClipEditorPage::timerCallback() { refreshButtons(); }

void ClipEditorPage::resized()
{
    const float s = (float) getWidth() / 1240.f;
    auto px = [s] (float v) { return juce::roundToInt (v * s); };
    panel = getLocalBounds().reduced (px (14), px (10));
    auto a = panel.reduced (px (14)).withTrimmedTop (px (24));
    auto top = a.removeFromTop (px (34));
    auto place = [&] (juce::Component& c, float w) { c.setBounds (top.removeFromLeft (px (w))); top.removeFromLeft (px (4)); };
    place (drawBtn, 84); place (selectBtn, 96); top.removeFromLeft (px (8));
    place (snapBox, 86); place (quantBtn, 86); top.removeFromLeft (px (8));
    place (lenMinus, 30); lenArea = top.removeFromLeft (px (80)); top.removeFromLeft (px (4)); place (lenPlus, 30); top.removeFromLeft (px (8));
    place (gate, 74); place (transpose, 64); top.removeFromLeft (px (8));
    place (playPill, 64); place (loopPill, 64);
    closeBtn.setBounds (top.removeFromRight (px (90))); top.removeFromRight (px (6));
    redoBtn.setBounds (top.removeFromRight (px (56))); top.removeFromRight (px (4));
    undoBtn.setBounds (top.removeFromRight (px (56))); top.removeFromRight (px (8));
    clearBtn.setBounds (top.removeFromRight (px (62))); top.removeFromRight (px (4));
    exportBtn.setBounds (top.removeFromRight (px (118))); top.removeFromRight (px (4));
    importBtn.setBounds (top.removeFromRight (px (72)));
    a.removeFromTop (px (6));
    infoArea = a.removeFromBottom (px (18));
    a.removeFromBottom (px (4));
    roll.setBounds (a);
}

void ClipEditorPage::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.78f));
    const float s = (float) getWidth() / 1240.f;
    drawPanelFrame (g, panel.toFloat(), ui::pink, s);
    drawPanelTitle (g, panel.toFloat(), "PIANO ROLL", ui::pink, s);
    g.setColour (ui::text);
    g.setFont (ui::font (12.f * s, true, 0.08f));
    g.drawText (juce::String (roll.lengthBars()) + (roll.lengthBars() > 1 ? " MESURES" : " MESURE"), lenArea, juce::Justification::centred);
    auto tagFor = [&] (juce::Component& c, const char* t)
    {
        g.setColour (ui::dim);
        g.setFont (ui::font (8.5f * s, true, 0.3f));
        g.drawText (T (t), c.getBounds().translated (0, -juce::roundToInt (12 * s)).withHeight (juce::roundToInt (11 * s)), juce::Justification::centred);
    };
    tagFor (gate, "GATE"); tagFor (transpose, "TRANSP."); tagFor (snapBox, "GRILLE");
    g.setColour (ui::dim);
    g.setFont (ui::font (10.5f * s));
    const auto bpm = proc.uiBpm.load();
    g.drawText (juce::String (roll.numNotes()) + " notes   -   " + juce::String (bpm, 1) + (proc.uiHostSync.load() ? " BPM (DAW)" : " BPM (FREE)")
                    + T ("   -   tout est en mesures : le clip suit le tempo du morceau.   Ctrl+Z annule, Q quantize, P crayon, E sélection, Suppr efface"),
                infoArea, juce::Justification::centredLeft);
}
