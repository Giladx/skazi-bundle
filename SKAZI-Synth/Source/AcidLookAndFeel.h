#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Three skins share these colour slots:
//   0 = Neon (dark, glowing), 1 = Vintage (wood, black panel, cream type),
//   2 = Chrome (brushed aluminium with cyan / magenta / UV-purple neon and bloom).
//   3 = Robot (gunmetal mech HUD: chamfered plates, LED rings, cyan + signal orange).
//   4 = Hyper (black + graphite hardware, red LCD displays, red LED buttons, red cheeks).
namespace Acid
{
    enum Skin { Neon = 0, Vintage = 1, Chrome = 2, Robot = 3, Hyper = 4 };
    inline int skin = Hyper;

    inline juce::Colour bg, panel, panel2, line, text, dim, green, cyan, orange, magenta, violet, yellow;
    inline juce::Colour screen, screenDim;   // display glass and the text drawn on it
    const juce::Colour panicRed { 0xffff2a3d };

    inline void applySkin (int s)
    {
        skin = s;
        if (s == Neon)
        {
            bg = juce::Colour (0xff07070b);  panel = juce::Colour (0xff111119); panel2 = juce::Colour (0xff191924);
            line = juce::Colour (0xff2a2a3a); text = juce::Colour (0xffe2e2f0);  dim = juce::Colour (0xff74748c);
            green = juce::Colour (0xffb4ff1a); cyan = juce::Colour (0xff19e6ff); orange = juce::Colour (0xffff8a1f);
            magenta = juce::Colour (0xffff2fd0); violet = juce::Colour (0xff9d6bff); yellow = juce::Colour (0xffffe11a);
            screen = bg; screenDim = dim;
        }
        else if (s == Vintage)
        {
            bg = juce::Colour (0xff121110);  panel = juce::Colour (0xff1a1918); panel2 = juce::Colour (0xff272523);
            line = juce::Colour (0xff45413b); text = juce::Colour (0xfff1e8d4);  dim = juce::Colour (0xffc2b79f);
            green = juce::Colour (0xffeea640); cyan = juce::Colour (0xffe7dcc2); orange = juce::Colour (0xffe07b39);
            magenta = juce::Colour (0xffd2533f); violet = juce::Colour (0xffd8c7a0); yellow = juce::Colour (0xfff3c45c);
            screen = bg; screenDim = dim;
        }
        else if (s == Hyper)
        {
            bg = juce::Colour (0xff0e0e10);  panel = juce::Colour (0xff26262a); panel2 = juce::Colour (0xff3a3a40);
            line = juce::Colour (0xff4c4c55); text = juce::Colour (0xfff4f4f6);  dim = juce::Colour (0xffa9a9b3);
            green = juce::Colour (0xffff2a2a); cyan = juce::Colour (0xffff7a1a); orange = juce::Colour (0xffffc21a);
            magenta = juce::Colour (0xffff2a6a); violet = juce::Colour (0xffff4a3a); yellow = juce::Colour (0xffff2a2a);
            screen = juce::Colour (0xff160606); screenDim = juce::Colour (0xffa04040);
        }
        else if (s == Robot)
        {
            bg = juce::Colour (0xff0b0e12);  panel = juce::Colour (0xff141a21); panel2 = juce::Colour (0xff1b232c);
            line = juce::Colour (0xff2c3946); text = juce::Colour (0xffd9e7f2);  dim = juce::Colour (0xff6f8597);
            green = juce::Colour (0xff00e5ff); cyan = juce::Colour (0xffff6a00); orange = juce::Colour (0xffffd400);
            magenta = juce::Colour (0xffff2e63); violet = juce::Colour (0xff39ff9f); yellow = juce::Colour (0xff00e5ff);
            screen = juce::Colour (0xff04070a); screenDim = juce::Colour (0xff5f7a8e);
        }
        else
        {
            bg = juce::Colour (0xffb9bbbf);  panel = juce::Colour (0xffd6d7d9); panel2 = juce::Colour (0xffc4c6c9);
            line = juce::Colour (0xff8a8d93); text = juce::Colour (0xff17171d);  dim = juce::Colour (0xff4a4c55);
            green = juce::Colour (0xff00ffff); cyan = juce::Colour (0xffff00ff); orange = juce::Colour (0xff8a2be2);
            magenta = juce::Colour (0xffff00ff); violet = juce::Colour (0xff8a2be2); yellow = juce::Colour (0xff00ffff);
            screen = juce::Colour (0xff07070e); screenDim = juce::Colour (0xff8f93a8);
        }
    }

    // Vintage uses one amber lamp colour everywhere for a consistent hardware look
    inline juce::Colour accentFor (juce::Colour c) { return skin == Vintage ? green : c; }

    // Wood tones for the vintage cheeks
    const juce::Colour woodDark { 0xff3b2214 }, woodMid { 0xff6b3f22 }, woodLight { 0xff8a5630 };

    inline juce::Font font (float h, bool bold = true)
    {
        if (skin == Hyper)
           #if JUCE_MAC
            return juce::Font (juce::FontOptions ("Helvetica Neue", h, bold ? juce::Font::bold : juce::Font::plain));
           #else
            return juce::Font (juce::FontOptions ("Arial", h, bold ? juce::Font::bold : juce::Font::plain));
           #endif
        if (skin == Robot)
           #if JUCE_MAC
            return juce::Font (juce::FontOptions ("DIN Alternate", h * 1.05f, bold ? juce::Font::bold : juce::Font::plain));
           #else
            return juce::Font (juce::FontOptions ("Bahnschrift", h * 1.05f, bold ? juce::Font::bold : juce::Font::plain));
           #endif
        if (skin == Vintage)
           #if JUCE_MAC
            return juce::Font (juce::FontOptions ("Futura", h, bold ? juce::Font::bold : juce::Font::plain));
           #else
            return juce::Font (juce::FontOptions ("Century Gothic", h * 1.02f, bold ? juce::Font::bold : juce::Font::plain));
           #endif
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }

    // Round indicator LED (Hyper skin)
    inline void led (juce::Graphics& g, float cx, float cy, float r, juce::Colour c, bool on)
    {
        if (on)
        {
            g.setColour (c.withAlpha (0.35f));
            g.fillEllipse (cx - r * 2.2f, cy - r * 2.2f, r * 4.4f, r * 4.4f);
            g.setColour (c);
            g.fillEllipse (cx - r, cy - r, r * 2.f, r * 2.f);
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.fillEllipse (cx - r * 0.4f, cy - r * 0.6f, r * 0.7f, r * 0.6f);
        }
        else
        {
            g.setColour (juce::Colour (0xff3a1414));
            g.fillEllipse (cx - r, cy - r, r * 2.f, r * 2.f);
        }
    }

    // Rubber hardware key with an LED at the left (Hyper skin)
    inline void rubberKey (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour ledColour, bool on, bool hover)
    {
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (r.translated (0.f, 1.5f), 3.f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (hover ? 0xff55555d : 0xff4a4a51), r.getX(), r.getY(),
                                                 juce::Colour (0xff2c2c31), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 3.f, r.getRight() - 3.f);
        if (r.getWidth() > 34.f)
            led (g, r.getX() + 9.f, r.getCentreY(), 2.6f, ledColour, on);
        else if (on)
        {
            g.setColour (ledColour);
            g.fillRect (r.getX() + 3.f, r.getBottom() - 4.f, r.getWidth() - 6.f, 2.f);
        }
    }

    // Plate with the top-left and bottom-right corners cut off (Robot skin)
    inline juce::Path chamfer (juce::Rectangle<float> r, float c)
    {
        c = std::min (c, std::min (r.getWidth(), r.getHeight()) * 0.4f);
        juce::Path p;
        p.startNewSubPath (r.getX() + c, r.getY());
        p.lineTo (r.getRight(), r.getY());
        p.lineTo (r.getRight(), r.getBottom() - c);
        p.lineTo (r.getRight() - c, r.getBottom());
        p.lineTo (r.getX(), r.getBottom());
        p.lineTo (r.getX(), r.getY() + c);
        p.closeSubPath();
        return p;
    }

    // Neon: stroked several times for a glow. Chrome: real blurred bloom. Vintage: a clean single line.
    inline void glowStroke (juce::Graphics& g, const juce::Path& p, juce::Colour c, float width)
    {
        if (skin == Chrome)
        {
            juce::Path outline;
            juce::PathStrokeType (width * 1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (outline, p);
            juce::DropShadow (c.withAlpha (0.85f), (int) (width * 4.f) + 4, {}).drawForPath (g, outline);
        }
        else if (skin == Neon || skin == Robot || skin == Hyper)
        {
            g.setColour (c.withAlpha (0.10f)); g.strokePath (p, juce::PathStrokeType (width * 3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (c.withAlpha (0.22f)); g.strokePath (p, juce::PathStrokeType (width * 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (skin == Chrome)
        {
            g.setColour (juce::Colours::white.withAlpha (0.55f));   // hot core of the tube
            g.strokePath (p, juce::PathStrokeType (std::max (0.6f, width * 0.35f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    inline void glowRect (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float corner)
    {
        if (skin == Hyper)
        {
            g.setColour (c.withAlpha (0.18f));
            g.fillRoundedRectangle (r.expanded (3.f), 4.f);
            g.setGradientFill (juce::ColourGradient (c.brighter (0.25f), r.getX(), r.getY(), c.darker (0.3f), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 2.f);
            return;
        }
        if (skin == Robot)
        {
            const float cut = r.getHeight() > 10.f ? std::min (7.f, r.getHeight() * 0.3f) : 0.f;
            auto p = chamfer (r, cut);
            g.setColour (c.withAlpha (0.12f));
            g.strokePath (p, juce::PathStrokeType (6.f));
            g.setColour (c.withAlpha (0.25f));
            g.strokePath (p, juce::PathStrokeType (3.f));
            g.setColour (c);
            g.fillPath (p);
            return;
        }
        if (skin == Chrome)
        {
            juce::Path p;
            p.addRoundedRectangle (r, corner);
            juce::DropShadow (c.withAlpha (0.9f), 12, {}).drawForPath (g, p);
        }
        else if (skin == Neon)
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (c.withAlpha (0.07f * (4 - i)));
                g.fillRoundedRectangle (r.expanded ((float) i * 2.f), corner + (float) i * 2.f);
            }
        else
        {
            // Soft lamp halo
            g.setColour (c.withAlpha (0.12f));
            g.fillRoundedRectangle (r.expanded (2.f), corner + 2.f);
        }
        g.setColour (c);
        g.fillRoundedRectangle (r, skin == Vintage ? std::min (corner, 2.f) : corner);
    }

    // Brushed aluminium: vertical gradient plus fine horizontal grain (cached per size by callers)
    inline juce::Image brushedMetal (int w, int h, juce::Colour top = juce::Colour (0xffe0e0e0), juce::Colour bottom = juce::Colour (0xffc0c0c0))
    {
        juce::Image img (juce::Image::RGB, std::max (1, w), std::max (1, h), false);
        juce::Graphics g (img);
        g.setGradientFill (juce::ColourGradient (top, 0.f, 0.f, bottom, 0.f, (float) h, false));
        g.fillAll();
        juce::Random rnd (1234);
        for (int y = 0; y < h; ++y)
        {
            const float a = rnd.nextFloat() * 0.10f;
            g.setColour ((rnd.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (a));
            const float x0 = rnd.nextFloat() * (float) w * 0.3f;
            g.drawHorizontalLine (y, x0, x0 + (float) w * (0.5f + rnd.nextFloat() * 0.7f));
        }
        // Noise speckle
        for (int i = 0; i < w * h / 40; ++i)
        {
            g.setColour (juce::Colours::black.withAlpha (rnd.nextFloat() * 0.06f));
            g.fillRect (rnd.nextInt (w), rnd.nextInt (h), 1, 1);
        }
        return img;
    }

    // Small slotted screw head, used on vintage panels
    inline void screw (juce::Graphics& g, float cx, float cy, float r)
    {
        juce::ColourGradient grad (juce::Colour (0xffd9d6cf), cx - r, cy - r, juce::Colour (0xff6d6a64), cx + r, cy + r, false);
        g.setGradientFill (grad);
        g.fillEllipse (cx - r, cy - r, r * 2.f, r * 2.f);
        g.setColour (juce::Colour (0xff3a3835));
        g.drawLine (cx - r * 0.7f, cy - r * 0.3f, cx + r * 0.7f, cy + r * 0.3f, 1.2f);
    }

    // Walnut side panel with grain
    inline void wood (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setGradientFill (juce::ColourGradient (woodMid, r.getX(), r.getY(), woodDark, r.getRight(), r.getY(), false));
        g.fillRect (r);
        juce::Random rnd (42);
        for (int i = 0; i < 40; ++i)
        {
            const float x = r.getX() + rnd.nextFloat() * r.getWidth();
            juce::Path grain;
            grain.startNewSubPath (x, r.getY());
            for (float y = r.getY(); y < r.getBottom(); y += 40.f)
                grain.lineTo (x + std::sin (y * 0.013f + (float) i) * 2.5f, y);
            g.setColour ((i % 3 == 0 ? woodLight : woodDark).withAlpha (0.35f));
            g.strokePath (grain, juce::PathStrokeType (0.8f + rnd.nextFloat()));
        }
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawRect (r, 1.f);
    }
}

class AcidLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AcidLookAndFeel() { refreshColours(); }

    void refreshColours()
    {
        setColour (juce::Slider::rotarySliderFillColourId, Acid::green);
        setColour (juce::BubbleComponent::backgroundColourId, Acid::panel2);
        setColour (juce::BubbleComponent::outlineColourId, Acid::line);
        setColour (juce::TooltipWindow::backgroundColourId, Acid::panel2);
        setColour (juce::TooltipWindow::textColourId, Acid::text);
        setColour (juce::ComboBox::backgroundColourId, Acid::panel2);
        setColour (juce::ComboBox::textColourId, Acid::text);
        setColour (juce::ComboBox::outlineColourId, Acid::line);
        setColour (juce::ComboBox::arrowColourId, Acid::dim);
        setColour (juce::PopupMenu::backgroundColourId, Acid::panel2);
        setColour (juce::PopupMenu::textColourId, Acid::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Acid::violet.withAlpha (0.5f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::PopupMenu::headerTextColourId, Acid::green);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return Acid::font (13.f); }
    juce::Font getPopupMenuFont() override { return Acid::font (14.f, false); }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0.f, 0.f, (float) w, (float) h).reduced (1.f);
        if (Acid::skin == Acid::Hyper)
        {
            g.setColour (juce::Colour (0xff1a1a1e));
            g.fillRoundedRectangle (r, 3.f);
            g.setColour (Acid::line);
            g.drawRoundedRectangle (r, 3.f, 1.f);
            juce::Path arrow;
            arrow.addTriangle ((float) w - 21.f, (float) h * 0.5f - 3.f, (float) w - 11.f, (float) h * 0.5f - 3.f, (float) w - 16.f, (float) h * 0.5f + 4.f);
            g.setColour (Acid::green);
            g.fillPath (arrow);
            return;
        }
        if (Acid::skin == Acid::Robot)
        {
            auto p = Acid::chamfer (r, 6.f);
            g.setColour (Acid::panel2);
            g.fillPath (p);
            g.setColour (box.hasKeyboardFocus (true) ? Acid::green : Acid::line);
            g.strokePath (p, juce::PathStrokeType (1.f));
            juce::Path arrow;
            arrow.addTriangle ((float) w - 21.f, (float) h * 0.5f - 3.f, (float) w - 11.f, (float) h * 0.5f - 3.f, (float) w - 16.f, (float) h * 0.5f + 4.f);
            g.setColour (Acid::green);
            g.fillPath (arrow);
            return;
        }
        g.setColour (Acid::panel2);
        g.fillRoundedRectangle (r, Acid::skin == Acid::Vintage ? 2.f : 4.f);
        g.setColour (box.hasKeyboardFocus (true) ? Acid::violet : Acid::line);
        g.drawRoundedRectangle (r, Acid::skin == Acid::Vintage ? 2.f : 4.f, 1.f);
        juce::Path arrow;
        const float ax = (float) w - 16.f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 5.f, ay - 3.f, ax + 5.f, ay - 3.f, ax, ay + 4.f);
        g.setColour (Acid::dim);
        g.fillPath (arrow);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (6, 1, box.getWidth() - 28, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        if (Acid::skin == Acid::Vintage)
        {
            drawVintageKnob (g, x, y, w, h, pos, startAngle, endAngle);
            return;
        }
        if (Acid::skin == Acid::Hyper)
        {
            drawHyperKnob (g, x, y, w, h, pos, startAngle, endAngle, s);
            return;
        }
        if (Acid::skin == Acid::Robot)
        {
            drawRobotKnob (g, x, y, w, h, pos, startAngle, endAngle, s);
            return;
        }
        if (Acid::skin == Acid::Chrome)
        {
            drawChromeKnob (g, x, y, w, h, pos, startAngle, endAngle, s);
            return;
        }

        const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
        const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.f);
        const float r = std::min (area.getWidth(), area.getHeight()) * 0.5f;
        const auto c = area.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);

        juce::Path track;
        track.addCentredArc (c.x, c.y, r, r, 0.f, startAngle, endAngle, true);
        g.setColour (Acid::line);
        g.strokePath (track, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
        if (std::abs (angle - from) > 0.01f)
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, r, r, 0.f, std::min (from, angle), std::max (from, angle), true);
            Acid::glowStroke (g, arc, accent, 3.f);
        }

        const float br = r * 0.72f;
        juce::ColourGradient grad (Acid::panel2.brighter (0.25f), c.x, c.y - br, Acid::bg, c.x, c.y + br, false);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - br, c.y - br, br * 2.f, br * 2.f);
        g.setColour (Acid::line.brighter (0.2f));
        g.drawEllipse (c.x - br, c.y - br, br * 2.f, br * 2.f, 1.f);

        juce::Path ptr;
        const float pl = br * 0.9f;
        ptr.startNewSubPath (c.x + std::sin (angle) * br * 0.35f, c.y - std::cos (angle) * br * 0.35f);
        ptr.lineTo (c.x + std::sin (angle) * pl, c.y - std::cos (angle) * pl);
        Acid::glowStroke (g, ptr, accent.brighter (0.4f), 2.2f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float, float,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
        const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const bool vintage = Acid::skin == Acid::Vintage;

        if (style == juce::Slider::LinearHorizontal)
        {
            const float cy = area.getCentreY();
            auto slot = juce::Rectangle<float> (area.getX(), cy - 3.f, area.getWidth(), 6.f);
            g.setColour (Acid::bg);
            g.fillRoundedRectangle (slot, 3.f);
            g.setColour (Acid::line);
            g.drawRoundedRectangle (slot, 3.f, 1.f);
            const float zero = s.getMinimum() < 0.0 ? area.getCentreX() : area.getX();
            auto fill = juce::Rectangle<float> (std::min (zero, sliderPos), cy - 2.f, std::abs (sliderPos - zero), 4.f);
            if (fill.getWidth() > 0.5f) Acid::glowRect (g, fill, accent, 2.f);
            if (s.getMinimum() < 0.0)
            {
                g.setColour (Acid::dim);
                g.fillRect (zero - 0.5f, cy - 7.f, 1.f, 14.f);
            }
            auto thumb = juce::Rectangle<float> (sliderPos - 7.f, cy - 10.f, 14.f, 20.f);
            if (vintage)
            {
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe4e1da), thumb.getX(), thumb.getY(),
                                                         juce::Colour (0xff7d7a73), thumb.getRight(), thumb.getBottom(), false));
                g.fillRoundedRectangle (thumb, 2.f);
                g.setColour (juce::Colours::black);
                g.fillRect (thumb.withSizeKeepingCentre (2.f, 12.f));
            }
            else
            {
                g.setColour (Acid::panel2.brighter (0.25f));
                g.fillRoundedRectangle (thumb, 3.f);
                g.setColour (accent);
                g.fillRect (thumb.withSizeKeepingCentre (2.f, 10.f));
            }
            return;
        }

        const float cx = area.getCentreX();
        auto slot = juce::Rectangle<float> (cx - 3.f, area.getY(), 6.f, area.getHeight());
        g.setColour (Acid::bg);
        g.fillRoundedRectangle (slot, 3.f);
        g.setColour (Acid::line);
        g.drawRoundedRectangle (slot, 3.f, 1.f);
        Acid::glowRect (g, slot.withTop (sliderPos).reduced (1.f), accent, 2.f);
        auto thumb = juce::Rectangle<float> (cx - 16.f, sliderPos - 7.f, 32.f, 14.f);
        g.setColour (Acid::panel2.brighter (0.2f));
        g.fillRoundedRectangle (thumb, 3.f);
        g.setColour (accent);
        g.fillRect (thumb.withSizeKeepingCentre (24.f, 2.f));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        const auto accent = Acid::accentFor (b.findColour (juce::TextButton::buttonOnColourId));
        auto r = b.getLocalBounds().toFloat().reduced (2.f);
        const bool on = b.getToggleState();

        if (Acid::skin == Acid::Hyper)
        {
            Acid::rubberKey (g, r, accent, on, over || down);
            return;
        }

        if (Acid::skin == Acid::Robot)
        {
            auto p = Acid::chamfer (r, std::min (7.f, r.getHeight() * 0.3f));
            if (on)
                Acid::glowRect (g, r, accent, 3.f);
            else
            {
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff222b35), r.getX(), r.getY(),
                                                         juce::Colour (0xff151b22), r.getX(), r.getBottom(), false));
                g.fillPath (p);
                if (over || down)
                {
                    g.setColour (accent.withAlpha (down ? 0.3f : 0.14f));
                    g.fillPath (p);
                }
                g.setColour (over ? accent.withAlpha (0.8f) : Acid::line);
                g.strokePath (p, juce::PathStrokeType (1.f));
            }
            return;
        }

        if (Acid::skin == Acid::Chrome)
        {
            if (on)
                Acid::glowRect (g, r, accent, 3.f);
            else
            {
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeeeff1), r.getX(), r.getY(),
                                                         juce::Colour (0xffa9acb1), r.getX(), r.getBottom(), false));
                g.fillRoundedRectangle (r, 3.f);
                if (over || down)
                {
                    g.setColour (accent.withAlpha (down ? 0.35f : 0.18f));
                    g.fillRoundedRectangle (r, 3.f);
                }
            }
            g.setColour (juce::Colour (0xff55585e));
            g.drawRoundedRectangle (r, 3.f, 1.f);
            g.setColour (juce::Colours::white.withAlpha (0.6f));
            g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 3.f, r.getRight() - 3.f);
            return;
        }

        if (Acid::skin == Acid::Vintage)
        {
            // Dark bakelite key with a warm lamp when on
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3734), r.getX(), r.getY(),
                                                     juce::Colour (0xff1c1a18), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 2.f);
            g.setColour (juce::Colours::black.withAlpha (0.8f));
            g.drawRoundedRectangle (r, 2.f, 1.f);
            if (on)
            {
                auto lamp = r.reduced (3.f);
                g.setColour (accent.withAlpha (0.18f));
                g.fillRoundedRectangle (lamp.expanded (2.f), 2.f);
                g.setGradientFill (juce::ColourGradient (accent.brighter (0.3f), lamp.getCentreX(), lamp.getY(),
                                                         accent.darker (0.35f), lamp.getCentreX(), lamp.getBottom(), false));
                g.fillRoundedRectangle (lamp, 1.5f);
            }
            else if (over || down)
            {
                g.setColour (Acid::text.withAlpha (down ? 0.12f : 0.06f));
                g.fillRoundedRectangle (r, 2.f);
            }
            return;
        }

        if (on)
            Acid::glowRect (g, r, accent.withAlpha (0.9f), 4.f);
        else
        {
            g.setColour (down ? Acid::panel2.brighter (0.2f) : (over ? Acid::panel2.brighter (0.1f) : Acid::panel2));
            g.fillRoundedRectangle (r, 4.f);
            g.setColour (over ? accent.withAlpha (0.6f) : Acid::line);
            g.drawRoundedRectangle (r, 4.f, 1.f);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setFont (Acid::font (std::min (13.f, b.getHeight() * 0.45f)));
        g.setColour (Acid::skin == Acid::Hyper ? Acid::text : (b.getToggleState() ? juce::Colour (0xff101014) : Acid::text));
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
    }

    juce::Font getSliderPopupFont (juce::Slider&) override { return Acid::font (13.f); }

private:
    // Black rubber knob on a dark skirt, white index line, red value arc and scale dots
    static void drawHyperKnob (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider& s)
    {
        const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
        const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.f);
        const float r = std::min (area.getWidth(), area.getHeight()) * 0.5f;
        const auto c = area.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);

        // Scale dots
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * (float) i / 10.f;
            const float d = r * 0.98f, dr = i % 5 == 0 ? 1.6f : 1.f;
            g.fillEllipse (c.x + std::sin (a) * d - dr, c.y - std::cos (a) * d - dr, dr * 2.f, dr * 2.f);
        }

        // Red value arc
        const float ar = r * 0.86f;
        const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
        if (std::abs (angle - from) > 0.01f)
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, ar, ar, 0.f, std::min (from, angle), std::max (from, angle), true);
            g.setColour (accent.withAlpha (0.3f));
            g.strokePath (arc, juce::PathStrokeType (5.f));
            g.setColour (accent);
            g.strokePath (arc, juce::PathStrokeType (2.f));
        }

        // Skirt + rubber cap
        const float sr = r * 0.74f;
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (c.x - sr + 1.f, c.y - sr + 3.f, sr * 2.f, sr * 2.f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4a50), c.x, c.y - sr, juce::Colour (0xff1c1c20), c.x, c.y + sr, false));
        g.fillEllipse (c.x - sr, c.y - sr, sr * 2.f, sr * 2.f);
        const float cr = sr * 0.8f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2e2e33), c.x, c.y - cr, juce::Colour (0xff121214), c.x, c.y + cr, false));
        g.fillEllipse (c.x - cr, c.y - cr, cr * 2.f, cr * 2.f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawEllipse (c.x - cr, c.y - cr, cr * 2.f, cr * 2.f, 1.f);

        juce::Path idx;
        idx.startNewSubPath (c.x + std::sin (angle) * cr * 0.3f, c.y - std::cos (angle) * cr * 0.3f);
        idx.lineTo (c.x + std::sin (angle) * sr * 0.95f, c.y - std::cos (angle) * sr * 0.95f);
        g.setColour (juce::Colours::white);
        g.strokePath (idx, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    // Machined dark hub inside a ring of 28 LED segments that light up to the value
    static void drawRobotKnob (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider& s)
    {
        const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
        const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.f);
        const float r = std::min (area.getWidth(), area.getHeight()) * 0.5f;
        const auto c = area.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
        const float from = bipolar ? 0.5f : 0.f;

        // LED segment ring
        const int segs = 28;
        for (int i = 0; i < segs; ++i)
        {
            const float t = (float) i / (float) (segs - 1);
            const float a = startAngle + t * (endAngle - startAngle);
            const bool lit = bipolar ? ((t >= std::min (from, pos) - 0.001f) && (t <= std::max (from, pos) + 0.001f))
                                     : t <= pos + 0.001f;
            const float r1 = r * 0.80f, r2 = r * 0.98f;
            juce::Path seg;
            seg.startNewSubPath (c.x + std::sin (a) * r1, c.y - std::cos (a) * r1);
            seg.lineTo (c.x + std::sin (a) * r2, c.y - std::cos (a) * r2);
            if (lit)
            {
                g.setColour (accent.withAlpha (0.25f));
                g.strokePath (seg, juce::PathStrokeType (4.5f));
                g.setColour (accent);
            }
            else
                g.setColour (juce::Colour (0xff26313c));
            g.strokePath (seg, juce::PathStrokeType (2.f));
        }

        // Hex-ish machined hub
        const float hr = r * 0.62f;
        juce::Path hub;
        for (int i = 0; i < 8; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 8.f + juce::MathConstants<float>::pi / 8.f;
            const auto pt = juce::Point<float> (c.x + std::sin (a) * hr, c.y - std::cos (a) * hr);
            if (i == 0) hub.startNewSubPath (pt); else hub.lineTo (pt);
        }
        hub.closeSubPath();
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillPath (hub, juce::AffineTransform::translation (0.f, 2.f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a4652), c.x, c.y - hr, juce::Colour (0xff12171d), c.x, c.y + hr, false));
        g.fillPath (hub);
        g.setColour (juce::Colour (0xff4b5a68));
        g.strokePath (hub, juce::PathStrokeType (1.f));
        g.setColour (juce::Colour (0xff0a0d10));
        g.fillEllipse (c.x - hr * 0.42f, c.y - hr * 0.42f, hr * 0.84f, hr * 0.84f);

        // Index notch
        juce::Path idx;
        idx.startNewSubPath (c.x + std::sin (angle) * hr * 0.45f, c.y - std::cos (angle) * hr * 0.45f);
        idx.lineTo (c.x + std::sin (angle) * hr * 0.95f, c.y - std::cos (angle) * hr * 0.95f);
        g.setColour (accent);
        g.strokePath (idx, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    // Recessed dark well, neon value ring with bloom, polished chrome cap
    static void drawChromeKnob (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider& s)
    {
        const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
        const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (5.f);
        const float r = std::min (area.getWidth(), area.getHeight()) * 0.5f;
        const auto c = area.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);

        // Well
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1b20), c.x, c.y - r, juce::Colour (0xff3a3c42), c.x, c.y + r, false));
        g.fillEllipse (c.x - r, c.y - r, r * 2.f, r * 2.f);
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.drawEllipse (c.x - r, c.y - r, r * 2.f, r * 2.f, 1.f);

        // Track + neon value ring
        const float tr = r * 0.82f;
        juce::Path track;
        track.addCentredArc (c.x, c.y, tr, tr, 0.f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff0b0b10));
        g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
        if (std::abs (angle - from) > 0.01f)
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, tr, tr, 0.f, std::min (from, angle), std::max (from, angle), true);
            Acid::glowStroke (g, arc, accent, 2.6f);
        }

        // Chrome cap
        const float cr = r * 0.58f;
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (c.x - cr + 1.f, c.y - cr + 2.5f, cr * 2.f, cr * 2.f);
        juce::ColourGradient cap (juce::Colour (0xfffbfbfc), c.x - cr * 0.6f, c.y - cr * 0.8f, juce::Colour (0xff8d9097), c.x + cr * 0.6f, c.y + cr, true);
        cap.addColour (0.55, juce::Colour (0xffd2d4d8));
        g.setGradientFill (cap);
        g.fillEllipse (c.x - cr, c.y - cr, cr * 2.f, cr * 2.f);
        g.setColour (juce::Colour (0xff5c5f66));
        g.drawEllipse (c.x - cr, c.y - cr, cr * 2.f, cr * 2.f, 1.f);

        juce::Path idx;
        idx.startNewSubPath (c.x + std::sin (angle) * cr * 0.25f, c.y - std::cos (angle) * cr * 0.25f);
        idx.lineTo (c.x + std::sin (angle) * cr * 0.9f, c.y - std::cos (angle) * cr * 0.9f);
        g.setColour (juce::Colour (0xff15161b));
        g.strokePath (idx, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    // Black knurled knob with a brushed aluminium cap and a white index line
    static void drawVintageKnob (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle)
    {
        const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.f);
        const float r = std::min (area.getWidth(), area.getHeight()) * 0.5f;
        const auto c = area.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);

        // Scale ticks
        g.setColour (Acid::text.withAlpha (0.75f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * (float) i / 10.f;
            const float r1 = r * 0.93f, r2 = r * (i % 5 == 0 ? 1.0f : 0.98f);
            g.drawLine (c.x + std::sin (a) * r1, c.y - std::cos (a) * r1, c.x + std::sin (a) * r2, c.y - std::cos (a) * r2,
                        i % 5 == 0 ? 1.6f : 1.f);
        }

        // Skirt with knurling
        const float sr = r * 0.82f;
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (c.x - sr + 1.5f, c.y - sr + 3.f, sr * 2.f, sr * 2.f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b3936), c.x, c.y - sr, juce::Colour (0xff0d0c0b), c.x, c.y + sr, false));
        g.fillEllipse (c.x - sr, c.y - sr, sr * 2.f, sr * 2.f);
        g.setColour (juce::Colour (0xff050505));
        for (int i = 0; i < 36; ++i)
        {
            const float a = angle + juce::MathConstants<float>::twoPi * (float) i / 36.f;
            g.drawLine (c.x + std::sin (a) * sr * 0.84f, c.y - std::cos (a) * sr * 0.84f,
                        c.x + std::sin (a) * sr * 0.99f, c.y - std::cos (a) * sr * 0.99f, 1.f);
        }

        // Aluminium cap
        const float cr = sr * 0.6f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeeece6), c.x - cr, c.y - cr,
                                                 juce::Colour (0xff86837c), c.x + cr, c.y + cr, false));
        g.fillEllipse (c.x - cr, c.y - cr, cr * 2.f, cr * 2.f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        for (float rr = cr * 0.2f; rr < cr; rr += 2.f)
            g.drawEllipse (c.x - rr, c.y - rr, rr * 2.f, rr * 2.f, 0.6f);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawEllipse (c.x - cr, c.y - cr, cr * 2.f, cr * 2.f, 1.f);

        // Index line across skirt and cap
        juce::Path idx;
        idx.startNewSubPath (c.x + std::sin (angle) * cr * 0.25f, c.y - std::cos (angle) * cr * 0.25f);
        idx.lineTo (c.x + std::sin (angle) * sr * 0.95f, c.y - std::cos (angle) * sr * 0.95f);
        g.setColour (juce::Colours::black);
        g.strokePath (idx, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white);
        g.strokePath (idx, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
};
