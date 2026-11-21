#include "PluginEditor.h"
#include <BinaryData.h>
#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "dev"
#endif

#include "UiDraw.h"
using namespace uidraw;

//==============================================================================
VckLookAndFeel::VckLookAndFeel()
{
    using namespace juce;
    setColour (ResizableWindow::backgroundColourId, ui::bg);
    setColour (Label::textColourId, ui::text);
    setColour (Slider::rotarySliderFillColourId, ui::violet);
    setColour (ComboBox::backgroundColourId, ui::panel);
    setColour (ComboBox::outlineColourId, ui::edge);
    setColour (ComboBox::textColourId, ui::text);
    setColour (ComboBox::arrowColourId, ui::dim);
    setColour (PopupMenu::backgroundColourId, Colour (0xff0b0f16));
    setColour (PopupMenu::highlightedBackgroundColourId, ui::violet.withAlpha (0.35f));
    setColour (PopupMenu::textColourId, ui::text);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (PopupMenu::headerTextColourId, ui::cyan);
    setColour (TextButton::buttonColourId, ui::panel);
    setColour (TextButton::buttonOnColourId, ui::violet);
    setColour (TextButton::textColourOffId, ui::text.withAlpha (0.8f));
    setColour (TextButton::textColourOnId, Colours::white);
    setColour (TooltipWindow::backgroundColourId, Colour (0xff0b0f16));
    setColour (TooltipWindow::textColourId, ui::text);
    setColour (TooltipWindow::outlineColourId, ui::edge);
    setColour (AlertWindow::backgroundColourId, Colour (0xff0b0f16));
    setColour (AlertWindow::textColourId, ui::text);
    setColour (AlertWindow::outlineColourId, ui::edge);
    setColour (TextEditor::backgroundColourId, ui::bg);
    setColour (TextEditor::textColourId, ui::text);
    setColour (TextEditor::outlineColourId, ui::edge);
    setColour (TextEditor::focusedOutlineColourId, ui::violet);
}

void VckLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                       float startAngle, float endAngle, juce::Slider& slider)
{
    using namespace juce;
    const float size = (float) jmin (w, h);
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).withSizeKeepingCentre (size, size).reduced (size * 0.05f);
    const float radius = b.getWidth() * 0.5f;
    if (radius < 8.f) return;
    const auto c = b.getCentre();
    const float lw = jmax (2.5f, radius * 0.085f);
    const float arcR = radius - lw;
    const auto fill = slider.findColour (Slider::rotarySliderFillColourId);
    const auto fill2 = fill.withRotatedHue (-0.09f).brighter (0.25f);
    const PathStrokeType stroke (lw, PathStrokeType::curved, PathStrokeType::rounded);

    for (int i = 0; i <= 30; ++i)
    {
        const float a = startAngle + (float) i / 30.f * (endAngle - startAngle);
        const auto pt = c.getPointOnCircumference (radius - lw * 0.1f + lw * 0.55f, a);
        const float d = (i % 5 == 0) ? lw * 0.30f : lw * 0.16f;
        g.setColour (ui::dim.withAlpha (i % 5 == 0 ? 0.55f : 0.3f));
        g.fillEllipse (pt.x - d, pt.y - d, d * 2.f, d * 2.f);
    }
    Path trackPath;
    trackPath.addCentredArc (c.x, c.y, arcR, arcR, 0.f, startAngle, endAngle, true);
    g.setColour (ui::track);
    g.strokePath (trackPath, stroke);

    float zero = 0.f;
    if (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0)
        zero = (float) slider.valueToProportionOfLength (0.0);
    const float a0 = startAngle + zero * (endAngle - startAngle);
    const float a1 = startAngle + pos * (endAngle - startAngle);
    if (std::abs (a1 - a0) > 0.002f)
    {
        Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.f, jmin (a0, a1), jmax (a0, a1), true);
        for (int i = 4; i >= 1; --i)
        {
            g.setColour (fill.withAlpha (0.055f));
            g.strokePath (val, PathStrokeType (lw * (1.f + (float) i * 0.9f), PathStrokeType::curved, PathStrokeType::rounded));
        }
        g.setGradientFill (ColourGradient (fill, b.getX(), b.getBottom(), fill2, b.getRight(), b.getY(), false));
        g.strokePath (val, stroke);
    }
    const float kr = arcR - lw * 1.75f;
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (c.x - kr, c.y - kr + kr * 0.08f, kr * 2.f, kr * 2.f);
    g.setGradientFill (ColourGradient (Colour (0xff27303c), c.x - kr * 0.3f, c.y - kr * 0.7f,
                                       Colour (0xff05070a), c.x + kr * 0.4f, c.y + kr, true));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.f, kr * 2.f);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.18f), c.x, c.y - kr,
                                       Colours::white.withAlpha (0.0f), c.x, c.y + kr, false));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.f, kr * 2.f, 1.f);
    g.setColour (fill.withAlpha (0.10f));
    g.drawEllipse (c.x - kr * 0.80f, c.y - kr * 0.80f, kr * 1.6f, kr * 1.6f, 1.f);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawEllipse (c.x - kr - 1.5f, c.y - kr - 1.5f, kr * 2.f + 3.f, kr * 2.f + 3.f, 1.2f);
    const auto p0 = c.getPointOnCircumference (kr * 0.62f, a1), p1 = c.getPointOnCircumference (kr * 0.93f, a1);
    const float bw = jmax (1.6f, lw * 0.32f);
    g.setColour (fill.withAlpha (0.30f));
    g.drawLine ({ p0, p1 }, bw * 3.2f);
    g.setColour (Colours::white.withAlpha (0.95f));
    g.drawLine ({ p0, p1 }, bw);
    const auto txt = slider.getTextFromValue (slider.getValue());
    g.setColour (ui::text);
    g.setFont (ui::font (jmax (8.5f, kr * 0.30f), true));
    g.drawFittedText (txt, Rectangle<float> (kr * 1.5f, kr * 0.6f).withCentre (c).toNearestInt(),
                      Justification::centred, 1, 0.8f);
}

void VckLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                       juce::Slider::SliderStyle, juce::Slider& s)
{
    using namespace juce;
    auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.f);
    const float rad = jmin (4.f, r.getHeight() * 0.2f);
    const auto accent = s.findColour (Slider::rotarySliderFillColourId);
    g.setColour (Colour (0xff0b0f15));
    g.fillRoundedRectangle (r, rad);
    auto fillR = r.withWidth (jmax (0.f, pos - r.getX()));
    g.setColour (accent.withAlpha (0.22f));
    g.fillRoundedRectangle (fillR, rad);
    g.setColour (accent.withAlpha (s.isMouseOverOrDragging() ? 0.8f : 0.4f));
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.f);
    g.setColour (ui::text);
    g.setFont (ui::font (jlimit (8.f, 15.f, r.getHeight() * 0.42f), true, 0.05f));
    g.drawFittedText (s.getTextFromValue (s.getValue()), r.toNearestInt(), Justification::centred, 1, 0.7f);
}

void VckLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down)
{
    using namespace juce;
    auto r = b.getLocalBounds().toFloat().reduced (3.f);
    const float rad = jmin (4.f, r.getHeight() * 0.2f);
    const auto accent = b.findColour (TextButton::buttonOnColourId);
    const float alpha = b.isEnabled() ? 1.f : 0.35f;
    // Onglets : plaques gravees, l'onglet actif est souligne d'un trait de lumiere (ravenn)
    if (b.getProperties().contains ("tab"))
    {
        const bool on = b.getToggleState();
        g.setGradientFill (ColourGradient (Colour (on ? 0xff121a26 : 0xff080b10).withAlpha (0.92f), r.getX(), r.getY(),
                                           Colour (0xff030406).withAlpha (0.92f), r.getX(), r.getBottom(), false));
        g.fillRect (r);
        g.setColour (ui::edge.brighter (hover ? 0.5f : 0.f));
        g.drawRect (r, 1.f);
        if (on)
        {
            auto line = r.removeFromBottom (2.f);
            g.setGradientFill (ColourGradient (accent.withAlpha (0.f), line.getX(), 0, accent, line.getCentreX(), 0, true));
            g.fillRect (line);
            g.setColour (accent.withAlpha (0.12f));
            g.fillRect (r.removeFromBottom (6.f));
        }
        return;
    }
    if (b.getToggleState())
    {
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (accent.withAlpha (0.10f * alpha));
            g.fillRoundedRectangle (r.expanded ((float) i), rad + (float) i);
        }
        g.setGradientFill (ColourGradient (accent.brighter (0.3f).withMultipliedAlpha (alpha), r.getX(), r.getY(),
                                           accent.darker (0.45f).withMultipliedAlpha (alpha), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, rad);
        g.setColour (Colours::white.withAlpha (0.22f * alpha));
        g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.f);
    }
    else
    {
        g.setColour (Colour (0xff0b0f15).brighter (hover ? 0.12f : 0.f).withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (r, rad);
        g.setColour (accent.withAlpha ((hover ? 0.75f : 0.32f) * alpha));
        g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.f);
    }
    if (down)
    {
        g.setColour (Colours::white.withAlpha (0.10f));
        g.fillRoundedRectangle (r, rad);
    }
}

void VckLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    using namespace juce;
    auto col = b.findColour (b.getToggleState() ? TextButton::textColourOnId : TextButton::textColourOffId);
    if (b.getProperties().contains ("tab"))
        col = b.getToggleState() ? Colours::white : ui::dim;
    else if (b.getToggleState() && b.findColour (TextButton::buttonOnColourId).getPerceivedBrightness() > 0.7f)
        col = Colour (0xff05080d);
    col = col.withMultipliedAlpha (b.isEnabled() ? 1.f : 0.4f);
    g.setColour (col);
    const float h = (float) b.getHeight();
    auto f = ui::font (jlimit (8.f, 16.f, h * 0.36f), true, 0.08f);
    const float w = (float) b.getWidth();
    int padX = jmax (6, roundToInt (h * 0.28f));
    const int padY = jmax (2, roundToInt (h * 0.12f));
    const auto text = b.getButtonText();
    auto textW = [&] { GlyphArrangement ga; ga.addLineOfText (f, text, 0.f, 0.f); return ga.getBoundingBox (0, -1, true).getWidth(); };
    float tw = textW();
    if (tw > w - 2.f * (float) padX)
    {
        padX = jmax (3, roundToInt (h * 0.14f));
        const float avail = w - 2.f * (float) padX;
        if (tw > avail && tw > 0.f) f = f.withHeight (jmax (7.f, f.getHeight() * avail / tw));
    }
    g.setFont (f);
    g.drawFittedText (text, b.getLocalBounds().reduced (padX, padY), Justification::centred, 1, 0.7f);
}

void VckLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    using namespace juce;
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (3.f);
    const float rad = jmin (4.f, r.getHeight() * 0.2f);
    g.setColour (Colour (0xff0b0f15));
    g.fillRoundedRectangle (r, rad);
    g.setColour (ui::violet.withAlpha (box.isMouseOver (true) ? 0.7f : 0.35f));
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.f);
    Path arrow;
    const float ax = r.getRight() - r.getHeight() * 0.5f - 6.f, ay = r.getCentreY();
    const float as = jmax (3.f, r.getHeight() * 0.12f);
    arrow.startNewSubPath (ax - as, ay - as * 0.5f);
    arrow.lineTo (ax, ay + as * 0.5f);
    arrow.lineTo (ax + as, ay - as * 0.5f);
    g.setColour (ui::dim);
    g.strokePath (arrow, PathStrokeType (1.5f, PathStrokeType::curved, PathStrokeType::rounded));
}

void VckLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (box.getLocalBounds().reduced (box.getHeight() / 2, 1).withTrimmedRight (box.getHeight() / 3));
    label.setFont (getComboBoxFont (box));
}
juce::Font VckLookAndFeel::getComboBoxFont (juce::ComboBox& b) { return ui::font (juce::jlimit (9.f, 18.f, (float) b.getHeight() * 0.42f), true); }
juce::Font VckLookAndFeel::getPopupMenuFont() { return ui::font (15.f); }
juce::Font VckLookAndFeel::getLabelFont (juce::Label& l) { return ui::font (juce::jlimit (9.f, 22.f, (float) l.getHeight() * 0.7f)); }

//==============================================================================
NeonKnob::NeonKnob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& t,
                    juce::Colour c, const juce::String& tip)
    : title (t)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::rotarySliderFillColourId, c);
    slider.setTooltip (tip);
    slider.setMouseDragSensitivity (220);
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    if (auto* prm = s.getParameter (id))
        slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
}
void NeonKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom (juce::jmax (14, getHeight() / 7));
    slider.setBounds (r);
}
void NeonKnob::paint (juce::Graphics& g)
{
    const int lh = juce::jmax (14, getHeight() / 7);
    auto r = getLocalBounds().removeFromBottom (lh);
    g.setColour (ui::text.withAlpha (0.78f));
    g.setFont (ui::font (juce::jlimit (8.f, 14.f, (float) lh * 0.72f), true, 0.16f));
    g.drawFittedText (title, r, juce::Justification::centred, 1, 0.7f);
}

ValueBox::ValueBox (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& tip)
{
    slider.setSliderStyle (juce::Slider::LinearBar);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::rotarySliderFillColourId, ui::pink);
    slider.setSliderSnapsToMousePosition (false);
    slider.setVelocityBasedMode (false);
    slider.setMouseDragSensitivity (300);
    slider.setTooltip (tip);
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    if (auto* prm = s.getParameter (id))
        slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
}

Segmented::Segmented (juce::AudioProcessorValueTreeState& s, const juce::String& id, juce::Colour c,
                      const juce::String& tip, juce::StringArray labels)
{
    auto* prm = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id));
    jassert (prm != nullptr);
    if (labels.isEmpty()) labels = prm->choices;
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (labels[i]));
        b->setColour (juce::TextButton::buttonOnColourId, c);
        b->setTooltip (tip);
        b->onClick = [this, i] { attachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    attachment = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float v)
    {
        const int idx = juce::roundToInt (v);
        for (int i = 0; i < buttons.size(); ++i) buttons[i]->setToggleState (i == idx, juce::dontSendNotification);
    });
    attachment->sendInitialUpdate();
}
void Segmented::resized()
{
    auto r = getLocalBounds();
    const int n = buttons.size();
    if (n == 0) return;
    const int cols = columns > 0 ? columns : n, rows = (n + cols - 1) / cols, gap = 1;
    const int w = (r.getWidth() - gap * (cols - 1)) / cols, h = (r.getHeight() - gap * (rows - 1)) / rows;
    for (int i = 0; i < n; ++i)
        buttons[i]->setBounds (r.getX() + (i % cols) * (w + gap), r.getY() + (i / cols) * (h + gap), w, h);
}

Pill::Pill (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& text,
            juce::Colour c, const juce::String& tip)
{
    button.setButtonText (text);
    button.setClickingTogglesState (true);
    button.setColour (juce::TextButton::buttonOnColourId, c);
    button.setTooltip (tip);
    addAndMakeVisible (button);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, id, button);
}

ChoiceBox::ChoiceBox (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& tip)
{
    if (auto* prm = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)))
        box.addItemList (prm->choices, 1);
    box.setTooltip (tip);
    box.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (box);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, id, box);
}

juce::Image ui::blurred (const juce::Image& src, int w, int h, int smallSize)
{
    using namespace juce;
    if (! src.isValid() || w <= 0 || h <= 0) return {};
    const float sa = (float) src.getWidth() / (float) src.getHeight(), da = (float) w / (float) h;
    Rectangle<int> crop = sa > da ? Rectangle<int> ((int) ((float) src.getHeight() * da), src.getHeight())
                                  : Rectangle<int> (src.getWidth(), (int) ((float) src.getWidth() / da));
    crop = crop.withCentre (src.getBounds().getCentre());
    auto small = src.getClippedImage (crop).rescaled (jmax (4, (int) ((float) smallSize * jmax (1.f, da))), smallSize,
                                                      Graphics::highResamplingQuality);
    auto mid = small.rescaled (small.getWidth() * 4, small.getHeight() * 4, Graphics::highResamplingQuality);
    mid = mid.convertedToFormat (Image::ARGB);
    ImageConvolutionKernel k (9);
    k.createGaussianBlur (3.5f);
    k.applyToImage (mid, mid, mid.getBounds());
    return mid.rescaled (w, h, Graphics::highResamplingQuality);
}

//==============================================================================
VoiceView::VoiceView (VckProcessor& p) : proc (p) { startTimerHz (40); setInterceptsMouseClicks (false, false); }

void VoiceView::timerCallback()
{
    const int n = proc.uiBands.load();
    for (int i = 0; i < n; ++i)
    {
        const float m = proc.uiMod[(size_t) i].load(), o = proc.uiOut[(size_t) i].load();
        mod[(size_t) i] += 0.55f * (m - mod[(size_t) i]);
        out[(size_t) i] += 0.55f * (o - out[(size_t) i]);
        peak[(size_t) i] = std::max (mod[(size_t) i], peak[(size_t) i] * 0.97f);
    }
    inL = std::max (proc.uiInLevel.load(), inL * 0.85f);
    outL = std::max (proc.uiOutLevel.load(), outL * 0.85f);
    sib += 0.4f * (proc.uiUnvoiced.load() - sib);
    repaint();
}

void VoiceView::paint (juce::Graphics& g)
{
    using namespace juce;
    const float s = (float) getWidth() / 768.f;
    auto r = getLocalBounds().toFloat();
    auto meters = r.removeFromRight (44.f * s);
    auto keys = r.removeFromBottom (30.f * s);
    r.removeFromBottom (16.f * s);
    auto plot = r.withTrimmedTop (22.f * s);

    // fond + grille
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (plot.expanded (2.f), 3.f);
    auto yOf = [&] (float db) { return plot.getBottom() - plot.getHeight() * jlimit (0.f, 1.f, (db + 66.f) / 66.f); };
    g.setFont (ui::font (8.5f * s, true));
    for (float db : { -12.f, -24.f, -36.f, -48.f, -60.f })
    {
        const float y = yOf (db);
        g.setColour (ui::edge.withAlpha (0.45f));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (ui::dim.withAlpha (0.6f));
        g.drawText (String ((int) db), Rectangle<float> (plot.getRight() - 26.f * s, y - 10.f * s, 24.f * s, 10.f * s), Justification::centredRight);
    }

    const int n = proc.uiBands.load();
    if (n > 0)
    {
        const float bw = plot.getWidth() / (float) n;
        const float gap = jmax (1.f, bw * 0.16f);
        for (int i = 0; i < n; ++i)
        {
            const float x = plot.getX() + (float) i * bw;
            const float dbM = vck::gainToDb (mod[(size_t) i] * 2.4f);
            const float dbO = vck::gainToDb (out[(size_t) i] * 2.4f);
            const float yM = yOf (dbM), yO = yOf (dbO), yP = yOf (vck::gainToDb (peak[(size_t) i] * 2.4f));
            auto bar = Rectangle<float> (x + gap * 0.5f, yM, bw - gap, plot.getBottom() - yM);
            g.setGradientFill (ColourGradient (ui::blue.withAlpha (0.55f), 0, plot.getBottom(), ui::cyan.withAlpha (0.9f), 0, plot.getY(), false));
            g.fillRect (bar);
            g.setColour (Colours::white.withAlpha (0.65f));
            g.fillRect (x + gap * 0.5f, yP - 1.f, bw - gap, 1.5f);
            // sortie du robot : trait lumineux
            if (dbO > -66.f)
            {
                g.setColour (ui::violet.withAlpha (0.35f));
                g.fillRect (x + gap * 0.5f, yO - 3.f, bw - gap, 6.f);
                g.setColour (Colours::white);
                g.fillRect (x + gap * 0.5f, yO - 0.75f, bw - gap, 1.5f);
            }
        }
        // reperes de frequence
        g.setFont (ui::font (9.f * s, true, 0.1f));
        g.setColour (ui::dim);
        const float f0 = proc.uiFreq[0].load(), f1 = proc.uiFreq[(size_t) n - 1].load();
        if (f0 > 0.f && f1 > f0)
            for (float f : { 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
            {
                if (f < f0 || f > f1) continue;
                const float t = std::log (f / f0) / std::log (f1 / f0);
                const float x = plot.getX() + bw * 0.5f + t * (plot.getWidth() - bw);
                g.drawText (f >= 1000.f ? String ((int) (f / 1000.f)) + "k" : String ((int) f),
                            Rectangle<float> (x - 20.f * s, plot.getBottom() + 2.f * s, 40.f * s, 12.f * s), Justification::centred);
            }
    }

    // en-tete : etat de la porteuse
    const int source = (int) proc.apvts.getRawParameterValue ("source")->load();
    auto head = getLocalBounds().toFloat().withTrimmedRight (40.f * s).removeFromTop (18.f * s);
    g.setFont (ui::font (11.f * s, true, 0.12f));
    String left, hint;
    const bool autoOn = proc.uiAutoActive.load();
    if (autoOn && source != 2)
    {
        const float hz = proc.uiPitchHz.load(), tn = proc.uiTargetNote.load();
        const String pre = source == 0 ? u8 ("MIDI > AUTO (pas de note)   ") : String ("AUTO   ");
        left = hz > 0.f ? pre + "VOIX " + noteName (vck::ftom (hz)) + "  " + String (hz, 0) + " Hz   >   ROBOT " + noteName (tn)
                        : pre + u8 ("chante : la porteuse suit ta voix");
    }
    else if (source == 0)
    {
        int cnt = 0;
        for (auto& w : proc.uiNotes) { auto b = w.load(); while (b) { cnt += (int) (b & 1u); b >>= 1; } }
        left = "MIDI   " + String (cnt) + (cnt > 1 ? " notes" : " note");
    }
    else
    {
        left = "SIDECHAIN";
        if (! proc.uiScConnected.load()) hint = u8 ("Active l'entrée sidechain de vck dans ton DAW et envoie-y un synthé");
    }
    g.setColour (ui::text);
    g.drawText (left, head, Justification::centredLeft);
    if (sib > 0.25f)
    {
        g.setColour (ui::cyan.withAlpha (jlimit (0.f, 1.f, sib)));
        g.drawText ("CONSONNE", head, Justification::centredRight);
    }
    if (hint.isNotEmpty())
    {
        g.setColour (ui::dim);
        g.setFont (ui::font (12.f * s, true, 0.05f));
        g.drawFittedText (hint, plot.reduced (20.f * s).toNearestInt(), Justification::centredTop, 2);
    }

    // clavier : notes de la porteuse (glace) et notes MIDI recues (bleu)
    {
        const int lo = 24, hi = 96;
        int whites = 0;
        for (int k = lo; k <= hi; ++k) if (! MidiMessage::isMidiNoteBlack (k)) ++whites;
        const float ww = keys.getWidth() / (float) whites;
        auto on = [&] (const std::array<std::atomic<uint32_t>, 4>& a, int k) { return (a[(size_t) (k >> 5)].load() >> (k & 31)) & 1u; };
        float x = keys.getX();
        std::vector<std::pair<float, int>> blacks;
        for (int k = lo; k <= hi; ++k)
        {
            if (MidiMessage::isMidiNoteBlack (k)) { blacks.push_back ({ x, k }); continue; }
            auto kr = Rectangle<float> (x, keys.getY(), ww - 1.f, keys.getHeight());
            g.setColour (on (proc.uiNotes, k) ? ui::cyan : (on (proc.uiInNotes, k) ? ui::violet : Colour (0xffc9d2dc).withAlpha (0.82f)));
            g.fillRoundedRectangle (kr, 1.5f);
            if (k % 12 == 0)
            {
                g.setColour (Colours::black.withAlpha (0.55f));
                g.setFont (ui::font (7.5f * s, true));
                g.drawText ("C" + String (k / 12 - 1), kr.removeFromBottom (11.f * s), Justification::centred);
            }
            x += ww;
        }
        for (auto& b : blacks)
        {
            auto kr = Rectangle<float> (b.first - ww * 0.32f, keys.getY(), ww * 0.64f, keys.getHeight() * 0.6f);
            g.setColour (on (proc.uiNotes, b.second) ? ui::cyan : (on (proc.uiInNotes, b.second) ? ui::violet : Colour (0xff0a0d12)));
            g.fillRoundedRectangle (kr, 1.5f);
        }
    }

    // vu-metres entree / sortie
    auto drawMeter = [&] (Rectangle<float> m, float lv, const String& t)
    {
        auto lab = m.removeFromBottom (12.f * s).withSizeKeepingCentre (24.f * s, 12.f * s);
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (m, 2.f);
        const float h = m.getHeight() * jlimit (0.f, 1.f, (vck::gainToDb (lv) + 60.f) / 60.f);
        g.setGradientFill (ColourGradient (ui::blue, 0, m.getBottom(), ui::red, 0, m.getY(), false));
        g.fillRoundedRectangle (m.withTop (m.getBottom() - h), 2.f);
        g.setColour (ui::dim);
        g.setFont (ui::font (8.f * s, true));
        g.drawText (t, lab, Justification::centred);
    };
    meters.removeFromLeft (8.f * s);
    drawMeter (meters.removeFromLeft (16.f * s).withTrimmedTop (22.f * s), inL, "IN");
    meters.removeFromLeft (4.f * s);
    drawMeter (meters.removeFromLeft (16.f * s).withTrimmedTop (22.f * s), outL, "OUT");
}

//==============================================================================
MidiRoll::MidiRoll (VckProcessor& p) : proc (p)
{
    startTimerHz (30);
    setTooltip (u8 ("Clip MIDI : glisse un .mid ici. Les notes suivent le tempo du DAW (ou le BPM en FREE). Clic : relancer le clip en mode FREE."));
}

void MidiRoll::timerCallback()
{
    if (proc.clipVersion.load() != version) { version = proc.clipVersion.load(); clip = proc.getClipCopy(); }
    repaint();
}

void MidiRoll::mouseDown (const juce::MouseEvent&) { if (onClick) onClick(); else proc.restartFree = true; }

void MidiRoll::paint (juce::Graphics& g)
{
    using namespace juce;
    const float s = (float) getWidth() / 380.f;
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r, 3.f);
    if (clip.empty())
    {
        Path border; border.addRoundedRectangle (r.reduced (2.f), 4.f);
        Path dashed; const float dl[] = { 5.f, 4.f };
        PathStrokeType (1.f).createDashedStroke (dashed, border, dl, 2);
        g.setColour ((dropHover ? ui::cyan : ui::dim).withAlpha (0.8f));
        g.fillPath (dashed);
        g.setFont (ui::font (13.f * s, true, 0.15f));
        g.setColour (dropHover ? ui::cyan : ui::text);
        g.drawText (u8 ("GLISSE UN FICHIER .MID ICI"), r.withTrimmedBottom (r.getHeight() * 0.25f), Justification::centred);
        g.setFont (ui::font (10.f * s));
        g.setColour (ui::dim);
        g.drawText (u8 ("ou clique pour écrire tes notes (piano roll)"), r.withTrimmedTop (r.getHeight() * 0.3f), Justification::centred);
        return;
    }
    const double L = clip.lengthBeats;
    int lo = 127, hi = 0;
    for (auto& n : clip.notes) { lo = std::min (lo, n.note); hi = std::max (hi, n.note); }
    lo -= 2; hi += 2;
    auto plot = r.reduced (4.f).withTrimmedTop (14.f * s);
    const float nh = plot.getHeight() / (float) (hi - lo + 1);
    auto xOf = [&] (double b) { return plot.getX() + (float) (b / L) * plot.getWidth(); };

    for (int b = 0; b <= (int) L; ++b)
    {
        const bool bar = b % 4 == 0;
        g.setColour (ui::edge.withAlpha (bar ? 0.9f : 0.35f));
        g.drawVerticalLine ((int) xOf (b), plot.getY(), plot.getBottom());
        if (bar && b < (int) L)
        {
            g.setColour (ui::dim);
            g.setFont (ui::font (8.f * s, true));
            g.drawText (String (b / 4 + 1), Rectangle<float> (xOf (b) + 2.f, r.getY() + 2.f, 20.f, 11.f * s), Justification::centredLeft);
        }
    }
    const float pb = proc.playBeat.load();
    for (auto& n : clip.notes)
    {
        auto nr = Rectangle<float> (xOf (n.start), plot.getBottom() - (float) (n.note - lo + 1) * nh,
                                    jmax (2.f, xOf (n.start + n.len) - xOf (n.start) - 1.f), jmax (2.f, nh - 1.f));
        const bool playing = pb >= 0.f && pb >= (float) n.start && pb < (float) (n.start + n.len);
        g.setColour ((playing ? ui::red : ui::cyan.interpolatedWith (ui::blue, 1.f - n.vel)).withAlpha (playing ? 1.f : 0.85f));
        g.fillRoundedRectangle (nr, 1.5f);
    }
    if (pb >= 0.f)
    {
        const float x = xOf (pb);
        g.setColour (ui::red.withAlpha (0.25f));
        g.fillRect (x - 3.f, plot.getY(), 6.f, plot.getHeight());
        g.setColour (ui::red);
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
    }
    g.setColour (ui::text);
    g.setFont (ui::font (9.f * s, true, 0.08f));
    g.drawText (clip.name + "  -  " + String ((int) (L / 4)) + " mes.  " + String ((int) clip.notes.size()) + " notes",
                Rectangle<float> (r.getX() + 30.f * s, r.getY() + 1.f, r.getWidth() - 34.f * s, 12.f * s), Justification::centredRight);
    if (dropHover)
    {
        g.setColour (ui::cyan.withAlpha (0.25f));
        g.fillRoundedRectangle (r, 3.f);
    }
}

//==============================================================================

//==============================================================================
void PitchGraph::paint (juce::Graphics& g)
{
    using namespace juce;
    auto r = getLocalBounds().toFloat();
    const float s = (float) getWidth() / 460.f;
    g.setColour (Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (r, 3.f);
    const int N = VckProcessor::pitchHist, w0 = proc.uiPitchWrite.load();
    // centre de la vue : suit doucement la voix
    float sum = 0.f; int cnt = 0;
    for (int i = 0; i < N; ++i) { const float v = proc.uiPitchIn[(size_t) i].load(); if (v > 0.f) { sum += v; ++cnt; } }
    if (cnt > 0) centre += 0.15f * (sum / (float) cnt - centre);
    const float span = 14.f, lo = std::round (centre) - span * 0.5f;
    auto plot = r.reduced (4.f).withTrimmedLeft (30.f * s);
    auto yOf = [&] (float m) { return plot.getBottom() - (m - lo) / span * plot.getHeight(); };
    const int key = (int) proc.apvts.getRawParameterValue ("key")->load(), scale = (int) proc.apvts.getRawParameterValue ("scale")->load();
    const int mask = vck::scales::mask (scale == 0 || scale == 8 ? 1 : scale);
    for (int m = (int) std::ceil (lo); m <= (int) (lo + span); ++m)
    {
        const bool inScale = (mask >> (((m - key) % 12 + 12) % 12)) & 1;
        const float y = yOf ((float) m);
        g.setColour ((inScale ? ui::edge.brighter (0.4f) : ui::edge).withAlpha (inScale ? 0.8f : 0.3f));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        if (inScale)
        {
            g.setColour (((m - key) % 12 + 12) % 12 == 0 ? ui::green : ui::dim);
            g.setFont (ui::font (8.5f * s, true));
            g.drawText (noteName ((float) m), Rectangle<float> (r.getX() + 2.f, y - 6.f, 28.f * s, 12.f), Justification::centredLeft);
        }
    }
    const float dx = plot.getWidth() / (float) (N - 1);
    auto drawLine = [&] (const std::array<std::atomic<float>, VckProcessor::pitchHist>& a, Colour c, float th)
    {
        Path p; bool open = false;
        for (int i = 0; i < N; ++i)
        {
            const float v = a[(size_t) ((w0 + i) % N)].load();
            const float x = plot.getX() + (float) i * dx;
            if (v <= 0.f || v < lo - 2.f || v > lo + span + 2.f) { open = false; continue; }
            const float y = jlimit (plot.getY(), plot.getBottom(), yOf (v));
            if (! open) { p.startNewSubPath (x, y); open = true; } else p.lineTo (x, y);
        }
        g.setColour (c);
        g.strokePath (p, PathStrokeType (th, PathStrokeType::curved, PathStrokeType::rounded));
    };
    drawLine (proc.uiPitchIn, ui::dim.withAlpha (0.9f), 1.4f * s);
    drawLine (proc.uiPitchOut, ui::green, 2.4f * s);
    g.setFont (ui::font (9.f * s, true, 0.1f));
    g.setColour (ui::dim);
    g.drawText (u8 ("gris : ta voix    bleu : note du robot"), plot.reduced (6.f).removeFromBottom (14.f * s), Justification::bottomRight);
}

//==============================================================================
void BankView::paint (juce::Graphics& g)
{
    using namespace juce;
    auto r = getLocalBounds().toFloat();
    const float s = (float) getWidth() / 456.f;
    g.setColour (Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (r, 3.f);
    auto plot = r.reduced (6.f).withTrimmedBottom (14.f * s);
    auto xOf = [&] (float f) { return plot.getX() + plot.getWidth() * std::log (f / 30.f) / std::log (20000.f / 30.f); };
    g.setFont (ui::font (8.5f * s, true));
    for (float f : { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
    {
        const float x = xOf (f);
        g.setColour (ui::edge.withAlpha (0.5f));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (ui::dim);
        g.drawText (f >= 1000.f ? String ((int) (f / 1000.f)) + "k" : String ((int) f), Rectangle<float> (x - 16.f, plot.getBottom() + 1.f, 32.f, 12.f * s), Justification::centred);
    }
    const int n = proc.uiBands.load();
    if (n < 2) return;
    const float sharp = proc.apvts.getRawParameterValue ("sharp")->load();
    const float f0 = proc.uiFreq[0].load(), f1 = proc.uiFreq[(size_t) n - 1].load();
    if (f0 <= 0.f || f1 <= f0) return;
    const float spacing = std::log2 (f1 / f0) / (float) (n - 1);
    const float bw = spacing * (1.3f - 0.5f * sharp);
    for (int i = 0; i < n; ++i)
    {
        const float fc = proc.uiFreq[(size_t) i].load();
        const float lvl = jlimit (0.f, 1.f, (vck::gainToDb (proc.uiMod[(size_t) i].load() * 2.4f) + 60.f) / 60.f);
        Path p;
        for (int k = 0; k <= 40; ++k)
        {
            const float o = ((float) k / 40.f - 0.5f) * bw * 4.f;
            const float x = 2.f * o / bw;
            const float mag = 1.f / std::sqrt (1.f + std::pow (x * x, 4.f));   // Butterworth 8e ordre
            const float px = xOf (fc * std::exp2 (o)), py = plot.getBottom() - mag * plot.getHeight() * (0.25f + 0.7f * lvl);
            if (k == 0) p.startNewSubPath (px, plot.getBottom()); 
            p.lineTo (px, py);
        }
        p.lineTo (xOf (fc * std::exp2 (bw * 2.f)), plot.getBottom());
        p.closeSubPath();
        const auto c = (i & 1) ? ui::cyan : ui::violet;
        g.setColour (c.withAlpha (0.10f + 0.25f * lvl));
        g.fillPath (p);
        g.setColour (c.withAlpha (0.5f + 0.4f * lvl));
        g.strokePath (p, PathStrokeType (1.f));
    }
}

//==============================================================================
PresetBrowser::PresetBrowser (VckProcessor& p) : proc (p)
{
    search.setTextToShowWhenEmpty (u8 ("Rechercher un preset…"), ui::dim);
    search.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0b0f15));
    search.setColour (juce::TextEditor::outlineColourId, ui::edge);
    search.setFont (ui::font (15.f));
    search.onTextChange = [this] { rebuildTiles(); };
    addAndMakeVisible (search);
    closeBtn.onClick = [this] { if (onClose) onClose(); };
    saveBtn.onClick = [this] { if (onSave) onSave(); };
    importBtn.onClick = [this] { if (onImport) onImport(); };
    folderBtn.onClick = [this] { proc.presets.openFolder(); };
    exportBtn.onClick = [this] { if (onExport) onExport(); };
    closeBtn.setButtonText (u8 ("FERMER"));
    folderBtn.setButtonText (u8 ("DOSSIER"));
    exportBtn.setTooltip (u8 ("Exporte le réglage actuel dans un fichier .preset, où tu veux (pour le partager)."));
    importBtn.setTooltip (u8 ("Importe des fichiers .preset (copiés dans ton dossier de presets)"));
    folderBtn.setTooltip (u8 ("Ouvre le dossier des presets (Documents/vck/Presets)"));
    for (auto* b : { &closeBtn, &saveBtn, &importBtn, &folderBtn, &exportBtn }) addAndMakeVisible (b);
    closeBtn.setColour (juce::TextButton::buttonOnColourId, ui::cyan);
    closeBtn.setToggleState (true, juce::dontSendNotification);
    refresh();
}

void PresetBrowser::syncSelection()
{
    const auto cur = proc.presets.getCurrent();
    for (auto* t : tiles) t->setToggleState (t->getButtonText() == cur, juce::dontSendNotification);
}

void PresetBrowser::refresh()
{
    catBtns.clear();
    juce::StringArray cats { "TOUS" };
    for (auto& [c, names] : proc.presets.getFactoryByCategory()) cats.add (c);
    if (! proc.presets.getUserNames().isEmpty()) cats.add ("MES PRESETS");
    for (auto& c : cats)
    {
        auto* b = catBtns.add (new juce::TextButton (c));
        b->getProperties().set ("tab", true);
        b->setColour (juce::TextButton::buttonOnColourId, ui::violet);
        b->setToggleState (c == category, juce::dontSendNotification);
        b->onClick = [this, c] { category = c; for (auto* x : catBtns) x->setToggleState (x->getButtonText() == c, juce::dontSendNotification); rebuildTiles(); };
        addAndMakeVisible (b);
    }
    rebuildTiles();
}

void PresetBrowser::rebuildTiles()
{
    tiles.clear();
    const auto q = search.getText().trim().toLowerCase();
    auto addTile = [&] (const juce::String& name, const juce::String& cat)
    {
        if (category != "TOUS" && category != cat) return;
        if (q.isNotEmpty() && ! name.toLowerCase().contains (q) && ! proc.presets.getInfo (name).toLowerCase().contains (q)) return;
        auto* b = tiles.add (new juce::TextButton (name));
        b->setColour (juce::TextButton::buttonOnColourId, ui::cyan);
        b->setToggleState (name == proc.presets.getCurrent(), juce::dontSendNotification);
        b->setTooltip (cat + "  -  " + proc.presets.getInfo (name));
        b->onClick = [this, name]
        {
            proc.presets.load (name);
            for (auto* t : tiles) t->setToggleState (t->getButtonText() == name, juce::dontSendNotification);
        };
        addAndMakeVisible (b);
    };
    for (auto& [c, names] : proc.presets.getFactoryByCategory())
        for (auto& n : names) addTile (n, c);
    for (auto& n : proc.presets.getUserNames()) addTile (n, "MES PRESETS");
    resized();
}

void PresetBrowser::resized()
{
    const float s = (float) getWidth() / 1240.f;
    auto px = [s] (float v) { return juce::roundToInt (v * s); };
    panel = getLocalBounds().reduced (px (60), px (24));
    auto a = panel.reduced (px (18)).withTrimmedTop (px (26));
    auto top = a.removeFromTop (px (36));
    closeBtn.setBounds (top.removeFromRight (px (100)));
    top.removeFromRight (px (8));
    folderBtn.setBounds (top.removeFromRight (px (90)));
    exportBtn.setBounds (top.removeFromRight (px (76)));
    importBtn.setBounds (top.removeFromRight (px (80)));
    saveBtn.setBounds (top.removeFromRight (px (70)));
    top.removeFromRight (px (12));
    search.setBounds (top);
    a.removeFromTop (px (12));
    auto left = a.removeFromLeft (px (200));
    for (auto* b : catBtns) { b->setBounds (left.removeFromTop (px (34))); left.removeFromTop (px (4)); }
    a.removeFromLeft (px (16));
    tileArea = a;
    const int cols = 3, th = px (38), gap = px (8);
    const int tw = (a.getWidth() - gap * (cols - 1)) / cols;
    for (int i = 0; i < tiles.size(); ++i)
        tiles[i]->setBounds (a.getX() + (i % cols) * (tw + gap), a.getY() + (i / cols) * (th + gap), tw, th);
}

void PresetBrowser::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.72f));
    const float s = (float) getWidth() / 1240.f;
    drawPanelFrame (g, panel.toFloat(), ui::cyan, s);
    drawPanelTitle (g, panel.toFloat(), "PRESETS", ui::cyan, s);
    if (tiles.isEmpty())
    {
        g.setColour (ui::dim);
        g.setFont (ui::font (13.f * s));
        g.drawText (u8 ("Aucun preset ne correspond."), tileArea, juce::Justification::centredTop);
    }
}
