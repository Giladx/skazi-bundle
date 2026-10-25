#pragma once

// Molten-metal look: a matte graphite chassis, recessed black screen and knobs
// with a glowing lava-orange value ring. One accent colour, used sparingly.

#include <juce_gui_basics/juce_gui_basics.h>

namespace skin
{
const juce::Colour chassisTop  { 0xff1e1e23 };
const juce::Colour chassisBot  { 0xff111114 };
const juce::Colour plate       { 0xff18181c };   // section plates
const juce::Colour plateEdge   { 0xff2c2c33 };
const juce::Colour screen      { 0xff07070a };
const juce::Colour grid        { 0xff1b1b22 };
const juce::Colour gridStrong  { 0xff2a2a34 };
const juce::Colour text        { 0xffe9e6e1 };
const juce::Colour textSoft    { 0xff85838c };
const juce::Colour lava        { 0xffff4a14 };   // main accent
const juce::Colour lavaHot     { 0xffffa21a };   // hot end of the gradient
const juce::Colour lavaDeep    { 0xffb3160c };
const juce::Colour ice         { 0xff6fe3ff };   // click layer, the one cool colour on the panel

inline juce::Font logoFont (float h)
{
    return juce::Font (juce::FontOptions ("Arial Black", h, juce::Font::plain)).withExtraKerningFactor (0.04f);
}

inline juce::Font labelFont (float h, bool bold = true)
{
    return juce::Font (juce::FontOptions ("Arial Narrow", h, bold ? juce::Font::bold : juce::Font::plain))
        .withExtraKerningFactor (0.12f);
}

inline juce::Font valueFont (float h)
{
    return juce::Font (juce::FontOptions ("Consolas", h, juce::Font::bold));
}

/** Lava gradient running across r, cold to hot. */
inline juce::ColourGradient lavaGradient (juce::Rectangle<float> r, bool vertical = false)
{
    juce::ColourGradient grad (lavaHot, r.getX(), r.getY(),
                               lavaDeep, vertical ? r.getX() : r.getRight(), vertical ? r.getBottom() : r.getY(), false);
    grad.addColour (0.45, lava);
    return grad;
}

/** Raised plate with a hairline edge and a top highlight. */
inline void drawPlate (juce::Graphics& g, juce::Rectangle<float> r, float corner = 8.0f)
{
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), corner);
    g.setGradientFill (juce::ColourGradient (plate.brighter (0.06f), 0.0f, r.getY(),
                                             plate.darker (0.15f), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, corner);
    g.setColour (plateEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + corner, r.getRight() - corner);
}

/** Recessed well, for the screen. */
inline void drawWell (juce::Graphics& g, juce::Rectangle<float> r, float corner = 6.0f)
{
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (r.expanded (1.5f).translated (0.0f, 1.0f), corner + 1.5f, 1.0f);
    g.setColour (screen);
    g.fillRoundedRectangle (r, corner);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r, corner, 1.5f);
}

/** Small section title with a lava tick in front: "▌ PITCH". */
inline void drawSectionTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title)
{
    g.setColour (lava);
    g.fillRect (r.getX(), r.getCentreY() - 5, 3, 10);
    g.setColour (textSoft);
    g.setFont (labelFont (12.5f));
    g.drawText (title, r.withTrimmedLeft (9), juce::Justification::centredLeft);
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, text);
        setColour (juce::TooltipWindow::backgroundColourId, plate);
        setColour (juce::TooltipWindow::textColourId, text);
        setColour (juce::TooltipWindow::outlineColourId, plateEdge);
        setColour (juce::BubbleComponent::backgroundColourId, plate);
        setColour (juce::BubbleComponent::outlineColourId, lava.withAlpha (0.6f));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float size = juce::jmin (area.getWidth(), area.getHeight());
        const auto c = area.getCentre();
        const float ringR = size * 0.5f - 4.0f;
        const float angle = startAngle + pos * (endAngle - startAngle);
        const bool big = s.getProperties()["big"];

        // Track and lava value ring with a soft glow
        juce::Path track;
        track.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (plateEdge);
        g.strokePath (track, juce::PathStrokeType (1.0f));

        if (pos > 0.001f)
        {
            juce::Path val;
            val.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, startAngle, angle, true);
            g.setColour (lava.withAlpha (0.18f));
            g.strokePath (val, juce::PathStrokeType (11.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setGradientFill (juce::ColourGradient (lavaDeep, c.x - ringR, c.y + ringR,
                                                     lavaHot, c.x + ringR, c.y - ringR, false));
            g.strokePath (val, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Matte knob body
        const float bodyR = ringR - (big ? 9.0f : 8.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (c.x - bodyR, c.y - bodyR + 3.0f, bodyR * 2.0f, bodyR * 2.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a42), c.x, c.y - bodyR,
                                                 juce::Colour (0xff131317), c.x, c.y + bodyR, false));
        g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);

        // Inner face, slightly dished
        const float faceR = bodyR * 0.78f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1a1f), c.x, c.y - faceR,
                                                 juce::Colour (0xff2a2a31), c.x, c.y + faceR, false));
        g.fillEllipse (c.x - faceR, c.y - faceR, faceR * 2.0f, faceR * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawEllipse (c.x - bodyR + 0.5f, c.y - bodyR + 0.5f, bodyR * 2.0f - 1.0f, bodyR * 2.0f - 1.0f, 1.0f);

        // Pointer: a lava line from the face edge outwards
        const auto p1 = c.getPointOnCircumference (faceR * 0.35f, angle);
        const auto p2 = c.getPointOnCircumference (bodyR * 0.92f, angle);
        g.setColour (lava.withAlpha (0.35f));
        g.drawLine (p1.x, p1.y, p2.x, p2.y, 5.0f);
        g.setColour (lavaHot.brighter (0.3f));
        g.drawLine (p1.x, p1.y, p2.x, p2.y, 2.2f);
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (valueFont (12.0f));
        l->setColour (juce::Label::textWhenEditingColourId, text);
        l->setColour (juce::TextEditor::backgroundColourId, screen);
        l->setColour (juce::TextEditor::highlightColourId, lava.withAlpha (0.4f));
        l->setColour (juce::CaretComponent::caretColourId, lava);
        return l;
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.5f);
        const float corner = r.getHeight() * 0.5f;

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (r.translated (0.0f, down ? 1.0f : 2.5f), corner);

        // A switch that's off is a dark plate; on (or a plain button) it's lava.
        const bool dark = b.getClickingTogglesState() && ! b.getToggleState();
        auto top = dark ? plate.brighter (over ? 0.25f : 0.15f) : over ? lavaHot.brighter (0.15f) : lavaHot;
        auto bot = dark ? plate.darker (0.3f) : lavaDeep;
        if (down) std::swap (top, bot);
        g.setGradientFill (juce::ColourGradient (top, 0.0f, r.getY(), bot, 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r.translated (0.0f, down ? 1.0f : 0.0f), corner);

        g.setColour (juce::Colours::white.withAlpha (down ? 0.05f : 0.25f));
        g.drawHorizontalLine ((int) r.getY() + 2, r.getX() + corner, r.getRight() - corner);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (r.translated (0.0f, down ? 1.0f : 0.0f), corner, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down) override
    {
        g.setFont (b.getProperties()["small"] ? labelFont (12.5f) : logoFont (15.0f));
        g.setColour (b.getClickingTogglesState() && ! b.getToggleState() ? textSoft : juce::Colour (0xff1a0602));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().translated (0, down ? 1 : 0),
                          juce::Justification::centred, 1);
    }
};
} // namespace skin
