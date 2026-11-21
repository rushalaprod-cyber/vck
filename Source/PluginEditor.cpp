#include "PluginEditor.h"
#include <BinaryData.h>
#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "dev"
#endif

namespace
{
    constexpr float baseW = 1240.f, baseH = 880.f;

    // Cadre "fer forge" de ravenn : fond translucide, fer brosse, filet lumineux, equerres
    void drawPanelFrame (juce::Graphics& g, juce::Rectangle<float> a, juce::Colour accent, float s)
    {
        using namespace juce;
        const float rad = 3.f * s;
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (a.translated (0, 3.f * s), rad);
        g.setGradientFill (ColourGradient (Colour (0xff0b0f15).withAlpha (0.74f), a.getX(), a.getY(),
                                           Colour (0xff040508).withAlpha (0.86f), a.getX(), a.getBottom(), false));
        g.fillRoundedRectangle (a, rad);
        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (a.reduced (1.f).toNearestInt());
            uint32_t seed = (uint32_t) (a.getX() * 13.f + a.getY() * 7.f) + 1u;
            for (float y = a.getY(); y < a.getBottom(); y += 2.f)
            {
                seed = seed * 1664525u + 1013904223u;
                const float v = (float) (seed >> 8) / 16777216.f;
                g.setColour ((v > 0.5f ? Colours::white : Colours::black).withAlpha (0.012f + 0.02f * std::abs (v - 0.5f)));
                g.fillRect (a.getX(), y, a.getWidth(), 1.f);
            }
            g.setColour (Colours::white.withAlpha (0.045f));
            g.fillRect (a.getX() + 2.f, a.getY() + 1.f, a.getWidth() - 4.f, 1.f);
            g.setColour (Colours::black.withAlpha (0.5f));
            g.fillRect (a.getX() + 2.f, a.getBottom() - 2.f, a.getWidth() - 4.f, 1.f);
        }
        g.setColour (ui::edge.withAlpha (0.9f));
        g.drawRoundedRectangle (a.reduced (0.5f), rad, 1.f);
        g.setGradientFill (ColourGradient (accent.withAlpha (0.0f), a.getX(), a.getY(),
                                           accent.withAlpha (0.55f), a.getCentreX(), a.getY(), true));
        g.fillRect (a.withHeight (1.f).reduced (a.getWidth() * 0.08f, 0));
        const float L = 10.f * s, t = 1.4f * s;
        g.setColour (Colour (0xffc7d2dd).withAlpha (0.55f));
        for (auto c : { a.getTopLeft(), a.getTopRight(), a.getBottomLeft(), a.getBottomRight() })
        {
            const float sx = c.x < a.getCentreX() ? 1.f : -1.f, sy = c.y < a.getCentreY() ? 1.f : -1.f;
            g.fillRect (Rectangle<float> (c.x, c.y, L * sx, t * sy).withPosition (jmin (c.x, c.x + L * sx), jmin (c.y, c.y + t * sy)));
            g.fillRect (Rectangle<float> (c.x, c.y, t * sx, L * sy).withPosition (jmin (c.x, c.x + t * sx), jmin (c.y, c.y + L * sy)));
        }
    }

    void drawPanelTitle (juce::Graphics& g, juce::Rectangle<float> a, const juce::String& t, juce::Colour accent, float s)
    {
        using namespace juce;
        auto r = a.reduced (14.f * s, 10.f * s).removeFromTop (16.f * s);
        const float d = 7.f * s;
        Path diamond;
        diamond.addPolygon ({ r.getX() + d * 0.5f, r.getCentreY() }, 4, d * 0.5f, 0.f);
        g.setColour (accent);
        g.fillPath (diamond);
        g.setColour (ui::text.withAlpha (0.9f));
        const auto f = ui::font (11.f * s, true, 0.32f);
        g.setFont (f);
        auto tr2 = r.withTrimmedLeft (d + 8.f * s);
        g.drawText (t, tr2, Justification::centredLeft);
        const float tw = GlyphArrangement::getStringWidth (f, t);
        const float x0 = tr2.getX() + tw + 10.f * s, x1 = r.getRight();
        if (x1 - x0 > 20.f * s)
        {
            g.setGradientFill (ColourGradient (ui::edge.brighter (0.6f).withAlpha (0.7f), x0, 0, ui::edge.withAlpha (0.0f), x1, 0, false));
            g.fillRect (Rectangle<float> (x0, r.getCentreY(), x1 - x0, 1.f));
        }
    }

    juce::String noteName (float n)
    {
        if (n < 0.f) return "-";
        return juce::MidiMessage::getMidiNoteName (juce::roundToInt (n), true, true, 3);
    }
}

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
    if (b.getToggleState() && b.findColour (TextButton::buttonOnColourId).getPerceivedBrightness() > 0.7f)
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
    if (source == 1)
    {
        const float hz = proc.uiPitchHz.load(), tn = proc.uiTargetNote.load();
        left = hz > 0.f ? "AUTO   VOIX " + noteName (vck::ftom (hz)) + "  " + String (hz, 0) + " Hz   >   ROBOT " + noteName (tn)
                        : String ("AUTO   chante : la porteuse suit ta voix");
    }
    else if (source == 0)
    {
        int cnt = 0;
        for (auto& w : proc.uiNotes) { auto b = w.load(); while (b) { cnt += (int) (b & 1u); b >>= 1; } }
        left = "MIDI   " + String (cnt) + (cnt > 1 ? " notes" : " note");
        if (cnt == 0) hint = u8 ("Joue des notes MIDI sur la piste de vck, ou glisse un fichier .mid");
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

void MidiRoll::mouseDown (const juce::MouseEvent&) { proc.restartFree = true; }

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
        g.drawText (u8 ("il se cale tout seul sur le BPM de ton morceau"), r.withTrimmedTop (r.getHeight() * 0.3f), Justification::centred);
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
namespace tips
{
    static juce::String t (const char* s) { return u8 (s); }
}

VckEditor::VckEditor (VckProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      bands (p.apvts, "bands", ui::violet, tips::t ("Nombre de bandes : peu = robot rétro, beaucoup = voix précise et intelligible.")),
      formant (p.apvts, "formant", "FORMANT", ui::violet, tips::t ("Déplace les formants : + = voix plus petite / aiguë, - = voix plus grosse / grave, sans changer la note.")),
      sharp (p.apvts, "sharp", "SHARP", ui::violet, tips::t ("Finesse des bandes : bas = doux et fondu, haut = précis et métallique.")),
      bright (p.apvts, "bright", "BRIGHT", ui::violet, tips::t ("Brillance : remonte les aigus de la voix pour plus d'intelligibilité.")),
      gate (p.apvts, "gate", "GATE", ui::violet, tips::t ("Seuil du gate : coupe le robot quand tu ne chantes pas (souffle, bruit de pièce).")),
      attack (p.apvts, "attack", "ATTACK", ui::cyan, tips::t ("Vitesse de réaction du robot aux syllabes.")),
      release (p.apvts, "release", "RELEASE", ui::cyan, tips::t ("Durée de retombée : court = haché et robotique, long = lié et fluide.")),
      sibil (p.apvts, "sibil", "SIBILANCE", ui::cyan, tips::t ("Consonnes (s, ch, t, f) : les laisse passer pour qu'on comprenne les paroles.")),
      freeze (p.apvts, "freeze", "FREEZE", ui::cyan, tips::t ("Fige la voyelle en cours : le robot tient le son pendant que la porteuse joue.")),
      source (p.apvts, "source", ui::cyan, tips::t ("Ce qui joue les notes du robot : MIDI (clavier ou fichier .mid), AUTO (suit ta voix, sans MIDI) ou SIDECHAIN (un synthé externe)."),
              juce::StringArray { "MIDI", "AUTO  (suit la voix)", "SIDECHAIN" }),
      wave (p.apvts, "wave", ui::cyan, tips::t ("Forme d'onde de la porteuse : SAW = classique, SQUARE = creux, PULSE = nasal, SUPER = large, BUZZ = robot dur, NOISE = chuchoté.")),
      mode (p.apvts, "mode", ui::cyan, tips::t ("POLY = accords, MONO = une note (glide toujours), LEGATO = glide seulement sur les notes liées (talkbox).")),
      octave (p.apvts, "octave", ui::cyan, tips::t ("Octave de la porteuse.")),
      unison (p.apvts, "unison", "UNISON", ui::cyan, tips::t ("Nombre d'oscillateurs par note : épaissit le son.")),
      detune (p.apvts, "detune", "DETUNE", ui::cyan, tips::t ("Écart entre les oscillateurs de l'unisson.")),
      glide (p.apvts, "glide", "GLIDE", ui::cyan, tips::t ("Glissement entre les notes (portamento).")),
      vibrato (p.apvts, "vibrato", "VIBRATO", ui::cyan, tips::t ("Vibrato naturel, qui arrive après l'attaque de la note.")),
      noise (p.apvts, "noise", "NOISE", ui::cyan, tips::t ("Ajoute du souffle à la porteuse : plus aérien.")),
      envA (p.apvts, "envA", "ENV ATT", ui::pink, tips::t ("Attaque des notes de la porteuse.")),
      envR (p.apvts, "envR", "ENV REL", ui::pink, tips::t ("Relâchement des notes de la porteuse.")),
      key (p.apvts, "key", ui::green, tips::t ("Tonalité du morceau (mode AUTO).")),
      harmony (p.apvts, "harmony", ui::green, tips::t ("Harmonies automatiques ajoutées à ta voix, dans la gamme choisie.")),
      scale (p.apvts, "scale", tips::t ("Gamme : la note du robot est calée dessus. LIBRE = suit ta voix exactement, DRONE = toujours la tonique (robot monotone).")),
      retune (p.apvts, "retune", "RETUNE", ui::green, tips::t ("Vitesse de correction : 0 = effet autotune dur, plus haut = plus naturel.")),
      roll (p),
      clipOn (p.apvts, "clipOn", "PLAY", ui::pink, tips::t ("Joue le clip MIDI importé.")),
      loop (p.apvts, "loop", "LOOP", ui::pink, tips::t ("Boucle le clip MIDI.")),
      sync (p.apvts, "sync", ui::pink, tips::t ("HOST : le clip suit la lecture et le BPM du DAW. FREE : il tourne en boucle au BPM réglé à côté.")),
      rate (p.apvts, "rate", ui::pink, tips::t ("Vitesse du clip : moitié, normale, double.")),
      bpm (p.apvts, "bpm", tips::t ("BPM utilisé en mode FREE (ou sans DAW). Glisse pour changer, double-clic = 120.")),
      transpose (p.apvts, "transpose", tips::t ("Transposition du clip MIDI en demi-tons.")),
      dblVoices (p.apvts, "dblVoices", ui::violet, tips::t ("Doubleur façon Waves Doubler : 2 ou 4 voix décalées, désaccordées et placées dans l'espace.")),
      dblDetune (p.apvts, "dblDetune", "DETUNE", ui::violet, tips::t ("Désaccord des voix doublées (cents).")),
      dblDelay (p.apvts, "dblDelay", "DELAY", ui::violet, tips::t ("Retard des voix doublées : court = serré, long = deux chanteurs.")),
      dblWidth (p.apvts, "dblWidth", "WIDTH", ui::violet, tips::t ("Largeur stéréo des voix doublées.")),
      dblDrift (p.apvts, "dblDrift", "DRIFT", ui::violet, tips::t ("Dérive naturelle de la hauteur et du placement, comme un vrai double.")),
      dblMix (p.apvts, "dblMix", "MIX", ui::violet, tips::t ("Niveau des voix doublées.")),
      ensemble (p.apvts, "ensemble", "ENSEMBLE", ui::cyan, tips::t ("Chorus à trois phases des string machines : le son vocoder des années 80.")),
      delayMix (p.apvts, "delayMix", "DELAY", ui::cyan, tips::t ("Délai ping-pong calé sur le tempo.")),
      delayFb (p.apvts, "delayFb", "FEEDBACK", ui::cyan, tips::t ("Répétitions du délai.")),
      reverb (p.apvts, "reverb", "REVERB", ui::cyan, tips::t ("Réverbération.")),
      revSize (p.apvts, "revSize", "SIZE", ui::cyan, tips::t ("Taille de la pièce.")),
      delayTime (p.apvts, "delayTime", tips::t ("Division du délai, calée sur le BPM.")),
      robot (p.apvts, "robot", "ROBOT", ui::red, tips::t ("Niveau du robot (vocoder).")),
      voice (p.apvts, "voice", "VOIX", ui::red, tips::t ("Ta voix d'origine, mélangée sous le robot (et doublée si le doubleur est actif).")),
      output (p.apvts, "output", "OUTPUT", ui::red, tips::t ("Volume de sortie (limiteur doux intégré, jamais de saturation).")),
      bypass (p.apvts, "bypass", "BYPASS", ui::red, tips::t ("Écoute le son d'origine.")),
      voiceView (p)
{
    setLookAndFeel (&lnf);
    ravenImg = juce::ImageCache::getFromMemory (BinaryData::raven_png, BinaryData::raven_pngSize);
    coverImg = juce::ImageCache::getFromMemory (BinaryData::cover_jpg, BinaryData::cover_jpgSize);

    key.setColumns (6);
    dblVoices.setColumns (1);
    importBtn.setButtonText ("IMPORT MIDI");
    clearBtn.setButtonText ("CLEAR");
    importBtn.setColour (juce::TextButton::buttonOnColourId, ui::pink);
    clearBtn.setColour (juce::TextButton::buttonOnColourId, ui::pink);
    importBtn.setTooltip (u8 ("Charge un fichier .mid : la mélodie / les accords joueront le robot, calés sur le tempo."));
    clearBtn.setTooltip (u8 ("Retire le clip MIDI."));
    importBtn.onClick = [this] { importMidi(); };
    clearBtn.onClick = [this] { proc.clearMidi(); };

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &bands, &formant, &sharp, &bright, &gate, &attack, &release, &sibil, &freeze,
             &source, &wave, &mode, &octave, &unison, &detune, &glide, &vibrato, &noise, &envA, &envR,
             &key, &harmony, &scale, &retune,
             &roll, &importBtn, &clearBtn, &clipOn, &loop, &sync, &rate, &bpm, &transpose,
             &dblVoices, &dblDetune, &dblDelay, &dblWidth, &dblDrift, &dblMix,
             &ensemble, &delayMix, &delayFb, &reverb, &revSize, &delayTime,
             &robot, &voice, &output, &bypass, &voiceView,
             &presetBox, &prevBtn, &nextBtn, &saveBtn })
        addAndMakeVisible (c);

    autoPanelComps[0] = &key; autoPanelComps[1] = &harmony; autoPanelComps[2] = &scale; autoPanelComps[3] = &retune; autoPanelComps[4] = nullptr;
    juce::Component* mc[] = { &roll, &importBtn, &clearBtn, &clipOn, &loop, &sync, &rate, &bpm, &transpose };
    for (int i = 0; i < 9; ++i) midiPanelComps[i] = mc[i];

    presetBox.setTextWhenNothingSelected ("Init");
    presetBox.setJustificationType (juce::Justification::centred);
    presetBox.setTooltip (u8 ("Presets calés sur des sons célèbres (Daft Punk, Kraftwerk, Imogen Heap...), doubleurs et créatifs."));
    presetBox.onChange = [this] { if (presetBox.getSelectedId() > 0) loadPresetById (presetBox.getSelectedId()); };
    prevBtn.setTooltip (u8 ("Preset précédent"));
    nextBtn.setTooltip (u8 ("Preset suivant"));
    saveBtn.setTooltip (u8 ("Sauve tes réglages comme preset (Documents/vck/Presets)."));
    auto step = [this] (int dir)
    {
        const int n = presetBox.getNumItems();
        if (n == 0) return;
        int idx = presetBox.getSelectedItemIndex();
        idx = (idx < 0 ? 0 : (idx + dir + n) % n);
        presetBox.setSelectedItemIndex (idx, juce::sendNotification);
    };
    prevBtn.onClick = [step] { step (-1); };
    nextBtn.onClick = [step] { step (1); };
    saveBtn.onClick = [this] { savePreset(); };
    rebuildPresetBox();

    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio (baseW / baseH);
        c->setSizeLimits ((int) (baseW * 0.6f), (int) (baseH * 0.6f), (int) (baseW * 1.6f), (int) (baseH * 1.6f));
    }
    setSize ((int) (baseW * 0.88f), (int) (baseH * 0.88f));
    startTimerHz (15);
}

VckEditor::~VckEditor() { setLookAndFeel (nullptr); }

void VckEditor::rebuildPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    auto& list = PresetManager::factory();
    for (auto& cat : PresetManager::categories())
    {
        presetBox.addSectionHeading (cat);
        for (int i = 0; i < (int) list.size(); ++i)
            if (cat == list[(size_t) i].category) presetBox.addItem (list[(size_t) i].name, i + 1);
    }
    auto users = proc.presets.userNames();
    if (! users.isEmpty())
    {
        presetBox.addSectionHeading ("MES PRESETS");
        for (int j = 0; j < users.size(); ++j) presetBox.addItem (users[j], 1000 + j);
    }
    shownPreset = {};
}

void VckEditor::loadPresetById (int id)
{
    if (id >= 1000)
    {
        auto users = proc.presets.userNames();
        if (id - 1000 < users.size()) proc.presets.loadUser (users[id - 1000]);
    }
    else proc.presets.loadFactory (id - 1);
    shownPreset = {};
    repaint();
}

void VckEditor::savePreset()
{
    saveDialog = std::make_unique<juce::AlertWindow> ("SAVE PRESET", u8 ("Nom du preset :"), juce::MessageBoxIconType::NoIcon, this);
    saveDialog->setLookAndFeel (&lnf);
    saveDialog->addTextEditor ("name", proc.presets.currentName);
    saveDialog->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("ANNULER", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int r)
    {
        if (r == 1 && saveDialog != nullptr)
        {
            const auto name = saveDialog->getTextEditorContents ("name");
            if (proc.presets.saveUser (name)) { rebuildPresetBox(); showMessage (u8 ("Preset sauvé : ") + name); }
            else showMessage (u8 ("Impossible de sauver ce preset"));
        }
        saveDialog.reset();
    }), false);
}

void VckEditor::importMidi()
{
    chooser = std::make_unique<juce::FileChooser> (u8 ("Choisis un fichier MIDI"), juce::File(), "*.mid;*.midi");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f.existsAsFile()) filesDropped ({ f.getFullPathName() }, 0, 0);
    });
}

bool VckEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".midi")) return true;
    return false;
}

void VckEditor::filesDropped (const juce::StringArray& files, int, int)
{
    roll.dropHover = dragHover = false;
    for (auto& path : files)
    {
        if (! (path.endsWithIgnoreCase (".mid") || path.endsWithIgnoreCase (".midi"))) continue;
        juce::String err;
        if (proc.loadMidi (juce::File (path), err))
        {
            // importer un MIDI = vouloir l'entendre : source MIDI + lecture
            for (auto [id, v] : std::initializer_list<std::pair<const char*, float>> { { "source", 0.f }, { "clipOn", 1.f } })
                if (auto* prm = proc.apvts.getParameter (id))
                {
                    prm->beginChangeGesture();
                    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
                    prm->endChangeGesture();
                }
            showMessage (u8 ("MIDI chargé : ") + juce::File (path).getFileNameWithoutExtension() + u8 ("  -  calé sur le tempo"));
        }
        else showMessage (err);
        break;
    }
    repaint();
}

void VckEditor::showMessage (const juce::String& m) { message = m; messageTicks = 15 * 4; repaint (msgArea); }

void VckEditor::timerCallback()
{
    auto& pm = proc.presets;
    if (pm.currentName != shownPreset)
    {
        shownPreset = pm.currentName;
        int id = 0;
        if (pm.currentFactory >= 0) id = pm.currentFactory + 1;
        else
        {
            auto users = pm.userNames();
            const int j = users.indexOf (pm.currentName);
            if (j >= 0) id = 1000 + j;
        }
        if (id > 0) presetBox.setSelectedId (id, juce::dontSendNotification);
        else presetBox.setText (pm.currentName, juce::dontSendNotification);
        repaint (infoArea);
    }
    const int src = (int) proc.apvts.getRawParameterValue ("source")->load();
    if (src != shownSource)
    {
        shownSource = src;
        for (auto* c : autoPanelComps) if (c != nullptr) c->setAlpha (src == 1 ? 1.f : 0.38f);
        for (auto* c : midiPanelComps) if (c != nullptr) c->setAlpha (src == 0 ? 1.f : 0.38f);
        for (juce::Component* c : { (juce::Component*) &wave, (juce::Component*) &mode, (juce::Component*) &octave, (juce::Component*) &unison,
                                    (juce::Component*) &detune, (juce::Component*) &glide, (juce::Component*) &vibrato, (juce::Component*) &noise,
                                    (juce::Component*) &envA, (juce::Component*) &envR })
            c->setAlpha (src == 2 ? 0.38f : 1.f);
        repaint();
    }
    if (messageTicks > 0 && --messageTicks == 0) repaint (msgArea);
    repaint (autoInfoArea);
    repaint (midiInfoArea);
}

void VckEditor::buildBackground (float pxScale)
{
    using namespace juce;
    const int w = jmax (1, getWidth()), h = jmax (1, getHeight());
    background = Image (Image::ARGB, jmax (1, roundToInt (w * pxScale)), jmax (1, roundToInt (h * pxScale)), true);
    bgScale = pxScale;
    Graphics g (background);
    g.addTransform (AffineTransform::scale (pxScale));
    g.fillAll (ui::bg);
    const float s = (float) getWidth() / baseW;
    if (coverImg.isValid())
    {
        auto bl = ui::blurred (coverImg, w, h, 32);
        g.setOpacity (0.95f);
        g.drawImageAt (bl, 0, 0);
        g.setOpacity (1.f);
    }
    g.setColour (Colour (0xff04122a).withAlpha (0.30f));
    g.fillAll();
    g.setGradientFill (ColourGradient (Colours::transparentBlack, (float) w * 0.5f, (float) h * 0.4f,
                                       Colours::black.withAlpha (0.65f), 0.f, (float) h, true));
    g.fillAll();
    for (int y = 0; y < h; y += 3) { g.setColour (Colours::black.withAlpha (0.08f)); g.fillRect (0, y, w, 1); }
    Random rng (1337);
    for (int i = 0, count = w * h / 30; i < count; ++i)
    {
        g.setColour (Colours::white.withAlpha (rng.nextFloat() * 0.035f));
        g.fillRect (rng.nextInt (w), rng.nextInt (h), 1, 1);
    }
    if (ravenImg.isValid())
    {
        const float rh = (float) h * 0.78f, rw = rh * (float) ravenImg.getWidth() / (float) ravenImg.getHeight();
        g.setOpacity (0.07f);
        g.drawImage (ravenImg, Rectangle<float> ((float) w * 0.52f - rw * 0.5f, (float) h * 0.30f, rw, rh), RectanglePlacement::stretchToFit);
        g.setOpacity (1.f);
    }
    for (auto& pnl : panels)
    {
        drawPanelFrame (g, pnl.area.toFloat(), pnl.colour, s);
        drawPanelTitle (g, pnl.area.toFloat(), pnl.title, pnl.colour, s);
    }
}

void VckEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    const float s = (float) getWidth() / baseW;
    const float pxs = jlimit (1.f, 4.f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (background.isNull() || bgScale != pxs
        || background.getWidth() != roundToInt (getWidth() * pxs) || background.getHeight() != roundToInt (getHeight() * pxs))
        buildBackground (pxs);
    g.drawImage (background, getLocalBounds().toFloat());

    // en-tete
    auto hb = header.toFloat();
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.88f), 0, hb.getY(), Colours::black.withAlpha (0.55f), 0, hb.getBottom(), false));
    g.fillRect (hb);
    g.setGradientFill (ColourGradient (ui::cyan.withAlpha (0.0f), 0, 0, ui::cyan.withAlpha (0.6f), (float) getWidth() * 0.5f, 0, true));
    g.fillRect (Rectangle<float> (0, hb.getBottom() - 1.f, (float) getWidth(), 1.f));

    // logo : le corbeau de ravenn, net au pixel pres
    auto la = logoArea.toFloat();
    if (ravenImg.isValid())
    {
        const float rh = la.getHeight() * 1.12f;
        const float rw = rh * (float) ravenImg.getWidth() / (float) ravenImg.getHeight();
        const float px = jmax (1.f, g.getInternalContext().getPhysicalPixelScaleFactor());
        const int tw = jmax (1, roundToInt (rw * px)), th = jmax (1, roundToInt (rh * px));
        auto box = Rectangle<float> (std::round (la.getX() * px) / px, std::round ((la.getCentreY() - rh * 0.5f) * px) / px,
                                     (float) tw / px, (float) th / px);
        if (ravenCrisp.getWidth() != tw || ravenCrisp.getHeight() != th)
        {
            Image src = ravenImg.convertedToFormat (Image::ARGB);
            while (src.getWidth() > tw * 2 && src.getHeight() > th * 2)
                src = src.rescaled (src.getWidth() / 2, src.getHeight() / 2, Graphics::highResamplingQuality);
            ravenCrisp = src.rescaled (tw, th, Graphics::highResamplingQuality).convertedToFormat (Image::ARGB);
            Image::BitmapData bd (ravenCrisp, Image::BitmapData::readWrite);
            for (int y = 0; y < th; ++y)
                for (int x = 0; x < tw; ++x)
                {
                    const float a = bd.getPixelColour (x, y).getFloatAlpha();
                    bd.setPixelColour (x, y, Colours::white.withAlpha (jlimit (0.f, 1.f, (a - 0.12f) * 2.4f)));
                }
        }
        g.drawImageTransformed (ravenCrisp, AffineTransform::scale (1.f / px).translated (box.getX(), box.getY()));
        la.removeFromLeft (rw + 6.f * s);
    }
    auto word = la.removeFromTop (la.getHeight() * 0.66f);
    g.setGradientFill (ColourGradient (Colours::white, word.getX(), word.getY(), ui::cyan, word.getRight(), word.getBottom(), false));
    g.setFont (ui::font (34.f * s, true, 0.06f));
    g.drawText ("vck", word, Justification::bottomLeft);
    g.setColour (ui::dim);
    g.setFont (ui::font (8.f * s, true, 0.5f));
    g.drawText (String ("VOCODER  ") + JucePlugin_VersionString, la.withTrimmedLeft (2.f * s), Justification::topLeft);

    // description du preset
    g.setColour (ui::dim);
    g.setFont (ui::font (10.5f * s));
    g.drawFittedText (proc.presets.currentInfo, infoArea, Justification::centredLeft, 3, 0.9f);

    // etiquettes des selecteurs
    auto tag = [&] (Component& c, const String& t)
    {
        auto r = c.getBounds().toFloat().translated (0, -14.f * s).withHeight (13.f * s);
        g.setColour (ui::dim);
        g.setFont (ui::font (9.f * s, true, 0.3f));
        g.drawText (t, r, Justification::centred);
    };
    tag (bands, "BANDS");
    tag (source, "SOURCE");
    tag (wave, "WAVE");
    tag (mode, "MODE");
    tag (octave, "OCTAVE");
    tag (key, "KEY");
    tag (scale, "SCALE");
    tag (harmony, "HARMONY");
    tag (sync, "SYNC");
    tag (rate, "RATE");
    tag (bpm, "BPM");
    tag (transpose, "TRANSPOSE");

    // infos dynamiques : tempo
    {
        g.setFont (ui::font (10.5f * s, true, 0.1f));
        const bool host = proc.uiHostSync.load();
        g.setColour (host ? (proc.uiHostPlaying.load() ? ui::cyan : ui::dim) : ui::text);
        g.drawText (String (proc.uiBpm.load(), 1) + " BPM  " + (host ? (proc.uiHostPlaying.load() ? "HOST  >" : "HOST  ||") : "FREE"),
                    midiInfoArea, Justification::centredRight);
    }
    // note detectee (AUTO)
    {
        auto a = autoInfoArea.toFloat();
        const float hz = proc.uiPitchHz.load(), tn = proc.uiTargetNote.load();
        const bool on = (int) proc.apvts.getRawParameterValue ("source")->load() == 1;
        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (a, 3.f);
        g.setColour ((hz > 0.f && on) ? ui::green : ui::dim.withAlpha (0.6f));
        g.setFont (ui::font (30.f * s, true, 0.05f));
        g.drawText (on && tn >= 0.f ? noteName (tn) : String ("--"), a.withTrimmedBottom (a.getHeight() * 0.35f), Justification::centred);
        g.setFont (ui::font (9.5f * s, true, 0.15f));
        g.setColour (ui::dim);
        g.drawText (on && hz > 0.f ? "VOIX " + String (hz, 0) + " Hz" : String ("NOTE DU ROBOT"),
                    a.withTrimmedTop (a.getHeight() * 0.62f), Justification::centredTop);
    }

    if (messageTicks > 0)
    {
        g.setColour (Colours::black.withAlpha (0.85f));
        g.fillRoundedRectangle (msgArea.toFloat(), 4.f);
        g.setColour (ui::cyan);
        g.drawRoundedRectangle (msgArea.toFloat().reduced (0.5f), 4.f, 1.f);
        g.setFont (ui::font (12.f * s, true, 0.05f));
        g.setColour (ui::text);
        g.drawFittedText (message, msgArea.reduced (10, 2), Justification::centred, 2);
    }
    if (dragHover)
    {
        g.setColour (ui::cyan);
        g.drawRect (getLocalBounds().reduced (roundToInt (6 * s)), roundToInt (2 * s));
    }
    if (proc.apvts.getRawParameterValue ("bypass")->load() > 0.5f)
    {
        g.setColour (Colours::black.withAlpha (0.3f));
        g.fillRect (getLocalBounds().withTrimmedTop (header.getBottom()));
    }
}

void VckEditor::resized()
{
    using namespace juce;
    const float s = (float) getWidth() / baseW;
    auto R = [s] (float x, float y, float w, float h)
    {
        return Rectangle<int> (roundToInt (x * s), roundToInt (y * s), roundToInt (w * s), roundToInt (h * s));
    };

    header = R (0, 0, baseW, 70);
    logoArea = R (14, 6, 240, 58);
    prevBtn.setBounds (R (290, 18, 30, 34));
    presetBox.setBounds (R (322, 18, 250, 34));
    nextBtn.setBounds (R (574, 18, 30, 34));
    saveBtn.setBounds (R (610, 18, 64, 34));
    infoArea = R (690, 10, 410, 50);
    bypass.setBounds (R (1116, 18, 108, 34));
    msgArea = R (370, 74, 500, 30);

    panels.clear();
    auto panel = [&] (const char* t, Colour c, float x, float y, float w, float h) { panels.push_back ({ t, c, R (x, y, w, h) }); };

    // ---- ligne 1
    panel ("VOICE VIEW", ui::cyan, 12, 82, 796, 262);
    voiceView.setBounds (R (26, 108, 770, 228));

    panel ("MIDI", ui::pink, 820, 82, 408, 262);
    midiInfoArea = R (1010, 90, 206, 18);
    roll.setBounds (R (834, 112, 380, 130));
    importBtn.setBounds (R (830, 248, 122, 32));
    clearBtn.setBounds (R (954, 248, 70, 32));
    clipOn.setBounds (R (1026, 248, 96, 32));
    loop.setBounds (R (1124, 248, 96, 32));
    sync.setBounds (R (830, 300, 96, 30));
    rate.setBounds (R (932, 300, 104, 30));
    bpm.setBounds (R (1042, 300, 84, 30));
    transpose.setBounds (R (1132, 300, 86, 30));

    // ---- ligne 2
    panel ("VOCODER", ui::violet, 12, 356, 400, 330);
    bands.setBounds (R (26, 404, 372, 30));
    {
        NeonKnob* row1[] = { &formant, &sharp, &bright, &gate };
        NeonKnob* row2[] = { &attack, &release, &sibil };
        for (int i = 0; i < 4; ++i) row1[i]->setBounds (R (20.f + (float) i * 96.f, 446, 92, 112));
        for (int i = 0; i < 3; ++i) row2[i]->setBounds (R (20.f + (float) i * 96.f, 564, 92, 112));
        freeze.setBounds (R (20 + 3 * 96 + 8, 600, 78, 34));
    }

    panel ("CARRIER", ui::cyan, 424, 356, 500, 330);
    source.setBounds (R (438, 400, 472, 36));
    wave.setBounds (R (438, 460, 300, 28));
    mode.setBounds (R (746, 460, 164, 28));
    {
        NeonKnob* row1[] = { &unison, &detune, &glide, &vibrato };
        NeonKnob* row2[] = { &noise, &envA, &envR };
        for (int i = 0; i < 4; ++i) row1[i]->setBounds (R (434.f + (float) i * 121.f, 494, 116, 94));
        for (int i = 0; i < 3; ++i) row2[i]->setBounds (R (434.f + (float) i * 121.f, 590, 116, 94));
        octave.setBounds (R (434 + 3 * 121 + 4, 626, 110, 28));
    }

    panel ("AUTO", ui::green, 936, 356, 292, 330);
    key.setBounds (R (950, 402, 264, 56));
    scale.setBounds (R (950, 486, 264, 30));
    harmony.setBounds (R (950, 540, 264, 30));
    retune.setBounds (R (944, 578, 118, 104));
    autoInfoArea = R (1068, 584, 146, 90);

    // ---- ligne 3
    panel ("DOUBLER", ui::violet, 12, 698, 520, 170);
    dblVoices.setBounds (R (26, 734, 92, 120));
    {
        NeonKnob* k[] = { &dblDetune, &dblDelay, &dblWidth, &dblDrift, &dblMix };
        for (int i = 0; i < 5; ++i) k[i]->setBounds (R (126.f + (float) i * 80.f, 730, 78, 128));
    }
    panel ("EFFECTS", ui::cyan, 544, 698, 412, 170);
    delayTime.setBounds (R (850, 704, 96, 24));
    {
        NeonKnob* k[] = { &ensemble, &delayMix, &delayFb, &reverb, &revSize };
        for (int i = 0; i < 5; ++i) k[i]->setBounds (R (550.f + (float) i * 80.f, 730, 78, 128));
    }
    panel ("OUTPUT", ui::red, 968, 698, 260, 170);
    {
        NeonKnob* k[] = { &robot, &voice, &output };
        for (int i = 0; i < 3; ++i) k[i]->setBounds (R (974.f + (float) i * 83.f, 730, 81, 128));
    }

    background = {};
    repaint();
}
