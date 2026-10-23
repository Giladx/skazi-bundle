#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace skin
{
const juce::Colour red     { 0xffe10600 };
const juce::Colour red2    { 0xffff2a1f };
const juce::Colour panel   { 0xff141416 };
const juce::Colour panel2  { 0xff1b1b1e };
const juce::Colour line    { 0xff2a2a2e };
const juce::Colour textDim { 0xff8b8b92 };
const juce::Colour white   { 0xfff4f4f4 };

inline juce::Colour fxColour (int fx)
{
    static const juce::uint32 c[] = { 0xffe10600, 0xffff5a1f, 0xffffb000, 0xffff2d7a, 0xffc084fc, 0xff38bdf8,
                                      0xfff4f4f4, 0xff22e3a5, 0xffffe14d, 0xffff7ad9, 0xff7aa2ff };
    return juce::Colour (c[fx]);
}

inline juce::Font font (float size, bool bold = true)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

inline juce::Font titleFont (float size)
{
    return juce::Font (juce::FontOptions ("Impact", size, juce::Font::plain));
}

// A machined screw head: recessed hole, domed head, cross-head slot
inline void drawScrew (juce::Graphics& g, juce::Point<float> c, float r, float angle)
{
    using namespace juce;
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillEllipse (c.x - r - 1.5f, c.y - r - 0.5f, (r + 1.5f) * 2, (r + 1.5f) * 2);
    g.setGradientFill (ColourGradient (Colour (0xff6b6e73), c.x, c.y - r, Colour (0xfff2f3f5), c.x, c.y + r, false));
    g.fillEllipse (c.x - r - 1, c.y - r - 1, (r + 1) * 2, (r + 1) * 2);

    ColourGradient head (Colour (0xfffbfbfc), c.x - r * 0.4f, c.y - r * 0.5f, Colour (0xff7d8086), c.x + r, c.y + r, true);
    head.addColour (0.55, Colour (0xffc3c6cb));
    g.setGradientFill (head);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (Colour (0xff4a4c50).withAlpha (0.7f));
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 0.8f);

    for (int i = 0; i < 2; ++i)
    {
        const float a = angle + i * MathConstants<float>::halfPi;
        const auto d = Point<float> (std::cos (a), std::sin (a)) * (r * 0.72f);
        g.setColour (Colour (0xff2b2c2f));
        g.drawLine ({ c - d, c + d }, r * 0.28f);
        g.setColour (Colours::white.withAlpha (0.55f));
        g.drawLine ({ c - d + Point<float> (0.6f, 0.9f), c + d + Point<float> (0.6f, 0.9f) }, 0.7f);
    }
}

// Recessed dark panel set into the metal faceplate
inline void drawInsetPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner = 6.0f)
{
    using namespace juce;
    g.setColour (Colours::white.withAlpha (0.55f));                   // lower bevel catches light
    g.fillRoundedRectangle (r.translated (0, 1.5f).expanded (1.0f), corner + 1);
    g.setColour (Colours::black.withAlpha (0.55f));                   // upper bevel in shadow
    g.fillRoundedRectangle (r.translated (0, -1.0f).expanded (1.0f), corner + 1);
    g.setGradientFill (ColourGradient (Colour (0xff111113), r.getX(), r.getY(), Colour (0xff18181b), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, corner);
    // inner shadow along the top edge
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.6f), r.getX(), r.getY(), Colours::transparentBlack, r.getX(), r.getY() + 10, false));
    g.fillRoundedRectangle (r.withHeight (12.0f), corner);
}

// Text stamped into the metal: a light edge below, then the ink
inline void engrave (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r, const juce::Font& f,
                     juce::Colour ink, juce::Justification just)
{
    g.setFont (f);
    g.setColour (juce::Colours::white.withAlpha (0.65f));
    g.drawText (text, r.translated (0, 1), just);
    g.setColour (ink);
    g.drawText (text, r, just);
}

// Brushed silver faceplate, rendered once per size
inline juce::Image renderBrushedMetal (int w, int h)
{
    using namespace juce;
    Image img (Image::ARGB, w, h, true);
    Graphics g (img);

    // base: satin silver with soft horizontal banding
    ColourGradient base (Colour (0xffc4c7cb), 0, 0, Colour (0xffb9bdc2), 0, (float) h, false);
    base.addColour (0.18, Colour (0xffe9ebed));
    base.addColour (0.45, Colour (0xffc9ccd0));
    base.addColour (0.72, Colour (0xffdfe1e4));
    base.addColour (0.90, Colour (0xffa9adb2));
    g.setGradientFill (base);
    g.fillRect (0, 0, w, h);

    // brushed grain: full-width hairlines plus random streaks
    Random rnd (20261023);
    for (int y = 0; y < h; ++y)
    {
        const float a = rnd.nextFloat() * 0.06f;
        g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (a));
        g.drawHorizontalLine (y, 0.0f, (float) w);
    }
    const int streaks = w * h / 77;
    for (int i = 0; i < streaks; ++i)
    {
        const float y = rnd.nextFloat() * h;
        const float x = rnd.nextFloat() * w;
        const float len = 20.0f + rnd.nextFloat() * 260.0f;
        g.setColour ((rnd.nextFloat() < 0.55f ? Colours::white : Colours::black).withAlpha (0.03f + rnd.nextFloat() * 0.07f));
        g.fillRect (x, y, len, 0.6f);
    }

    // diagonal light sheen
    ColourGradient sheen (Colours::white.withAlpha (0.0f), 0, 0, Colours::white.withAlpha (0.0f), (float) w, (float) h, false);
    sheen.addColour (0.35, Colours::white.withAlpha (0.22f));
    sheen.addColour (0.5, Colours::white.withAlpha (0.0f));
    sheen.addColour (0.7, Colours::white.withAlpha (0.12f));
    g.setGradientFill (sheen);
    g.fillRect (0, 0, w, h);

    // machined edge bevel
    g.setColour (Colours::white.withAlpha (0.8f));
    g.drawHorizontalLine (0, 0.0f, (float) w);
    g.drawVerticalLine (0, 0.0f, (float) h);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.drawHorizontalLine (h - 1, 0.0f, (float) w);
    g.drawVerticalLine (w - 1, 0.0f, (float) h);
    g.setColour (Colours::black.withAlpha (0.18f));
    g.drawRect (2, 2, w - 4, h - 4, 1);
    return img;
}

// SKAZI logo on a black anodised badge; draws a vector stand-in when no logo image is given
inline void drawLogoBadge (juce::Graphics& g, juce::Rectangle<float> r, const juce::Image& logo)
{
    using namespace juce;
    g.setColour (Colours::white.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (0, 1.5f), 6.0f);
    g.setGradientFill (ColourGradient (Colour (0xff1c1c1e), r.getX(), r.getY(), Colour (0xff000000), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 6.0f);

    auto in = r.reduced (8.0f, 4.0f);
    if (logo.isValid())
    {
        g.drawImage (logo, in, RectanglePlacement::centred);
        return;
    }

    const float h = in.getHeight();
    const auto f = Font (FontOptions ("Arial Black", h * 0.86f, Font::bold));
    g.setFont (f);
    g.setColour (Colours::white);
    const float skW = GlyphArrangement::getStringWidth (f, "SK");
    const float ziW = GlyphArrangement::getStringWidth (f, "ZI");
    const float ring = h * 0.62f;
    const float total = skW + ring * 1.2f + ziW;
    float x = in.getCentreX() - total * 0.5f;
    g.drawText ("SK", Rectangle<float> (x, in.getY(), skW + 2, h), Justification::centredLeft);
    x += skW;
    const auto c = Point<float> (x + ring * 0.6f, in.getCentreY() + h * 0.05f);
    g.setColour (red);
    g.drawEllipse (c.x - ring * 0.5f, c.y - ring * 0.5f, ring, ring, h * 0.09f);
    g.setColour (Colours::white);
    g.drawLine (c.x, in.getY() + 1, c.x - ring * 0.42f, in.getBottom() - 1, h * 0.12f);
    g.drawLine (c.x, in.getY() + 1, c.x + ring * 0.42f, in.getBottom() - 1, h * 0.12f);
    x += ring * 1.2f;
    g.drawText ("ZI", Rectangle<float> (x, in.getY(), ziW + 2, h), Justification::centredLeft);
}

class MetalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MetalLookAndFeel()
    {
        using namespace juce;
        setColour (Slider::textBoxTextColourId, white);
        setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
        setColour (Slider::rotarySliderFillColourId, red);
        setColour (ComboBox::backgroundColourId, Colour (0xff0f0f10));
        setColour (ComboBox::outlineColourId, line);
        setColour (ComboBox::textColourId, white);
        setColour (ComboBox::arrowColourId, textDim);
        setColour (PopupMenu::backgroundColourId, Colour (0xff141416));
        setColour (PopupMenu::textColourId, white);
        setColour (PopupMenu::highlightedBackgroundColourId, red);
        setColour (PopupMenu::highlightedTextColourId, Colours::white);
        setColour (TextButton::textColourOffId, white);
        setColour (TextButton::textColourOnId, Colours::white);
        setColour (Label::textColourId, textDim);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider& s) override
    {
        using namespace juce;
        const auto accent = s.findColour (Slider::rotarySliderFillColourId);
        const float size = (float) jmin (w, h);
        const auto c = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
        const float r = size * 0.5f - 4.0f;
        const float thick = jmax (3.0f, r * 0.13f);
        const float ang = a0 + pos * (a1 - a0);

        Path track;
        track.addCentredArc (c.x, c.y, r, r, 0, a0, a1, true);
        g.setColour (Colour (0xff26262a));
        g.strokePath (track, PathStrokeType (thick, PathStrokeType::curved, PathStrokeType::rounded));

        if (pos > 0.0f)
        {
            Path val;
            val.addCentredArc (c.x, c.y, r, r, 0, a0, ang, true);
            for (int i = 3; i > 0; --i)
            {
                g.setColour (accent.withAlpha (0.10f));
                g.strokePath (val, PathStrokeType (thick + i * 3.0f, PathStrokeType::curved, PathStrokeType::rounded));
            }
            g.setColour (accent);
            g.strokePath (val, PathStrokeType (thick, PathStrokeType::curved, PathStrokeType::rounded));
        }

        const float kr = r * 0.66f;
        g.setColour (Colours::black.withAlpha (0.5f));
        g.fillEllipse (c.x - kr, c.y - kr + 2, kr * 2, kr * 2);
        g.setGradientFill (ColourGradient (Colour (0xff3a3a3f), c.x - kr * 0.3f, c.y - kr * 0.6f, Colour (0xff0d0d0f), c.x + kr, c.y + kr, true));
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
        g.setColour (Colour (0xff333336));
        g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.0f);

        const auto p1 = c.getPointOnCircumference (kr * 0.25f, ang);
        const auto p2 = c.getPointOnCircumference (kr * 0.92f, ang);
        g.setColour (Colours::white);
        g.drawLine ({ p1, p2 }, jmax (2.0f, kr * 0.12f));
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (font (11.5f));
        l->setColour (juce::Label::textColourId, white);
        return l;
    }

    // Only the seed box uses a linear slider: a dark display with a red fill
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle, juce::Slider& s) override
    {
        using namespace juce;
        auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        g.setColour (Colour (0xff0f0f10));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (red.withAlpha (0.35f));
        g.fillRoundedRectangle (r.withRight (pos), 4.0f);
        g.setColour (s.isMouseOver (true) ? red : line);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace juce;
        auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (box.findColour (ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (box.isMouseOver (true) ? red : box.findColour (ComboBox::outlineColourId));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        g.setColour (textDim);
        g.drawText (String::fromUTF8 ("\xe2\x96\xbe"), Rectangle<int> (w - 20, 0, 16, h), Justification::centred);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (12.0f, false); }
    juce::Font getPopupMenuFont() override { return font (13.0f, false); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (6, 0, box.getWidth() - 24, box.getHeight());
        label.setFont (getComboBoxFont (box));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        using namespace juce;
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        if (on)
        {
            g.setColour (red.withAlpha (0.35f));
            g.fillRoundedRectangle (r.expanded (2.0f), 6.0f);
        }
        g.setColour (on ? red : (down ? Colour (0xff26262a) : panel2));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (on ? red2 : (over ? textDim : line));
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (12.0f); }

    // Small pill switch, used for the FX on/off toggles
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
    {
        using namespace juce;
        auto r = b.getLocalBounds().toFloat().withSizeKeepingCentre (28.0f, 12.0f);
        const auto accent = b.findColour (ToggleButton::tickColourId);
        const bool on = b.getToggleState();
        g.setColour (on ? accent : Colour (0xff2a2a2e));
        g.fillRoundedRectangle (r, 6.0f);
        const float kx = on ? r.getRight() - 11.0f : r.getX() + 1.0f;
        g.setColour (on ? Colours::white : Colour (0xff6b6b70));
        g.fillEllipse (kx, r.getY() + 1.0f, 10.0f, 10.0f);
    }
};
} // namespace skin
