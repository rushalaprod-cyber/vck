#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "PluginEditor.h"

// Dessin commun (cadres "fer forge" de ravenn, titres de panneaux)
namespace uidraw
{
    constexpr float baseW = 1240.f, baseH = 880.f;

    // Cadre "fer forge" de ravenn : fond translucide, fer brosse, filet lumineux, equerres
    inline void drawPanelFrame (juce::Graphics& g, juce::Rectangle<float> a, juce::Colour accent, float s)
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

    inline void drawPanelTitle (juce::Graphics& g, juce::Rectangle<float> a, const juce::String& t, juce::Colour accent, float s)
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

    inline juce::String noteName (float n)
    {
        if (n < 0.f) return "-";
        return juce::MidiMessage::getMidiNoteName (juce::roundToInt (n), true, true, 3);
    }
}
