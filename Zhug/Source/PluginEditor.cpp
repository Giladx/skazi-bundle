#include "PluginEditor.h"

using namespace juce;
using namespace zhug;

namespace
{
    const Colour kBand[3] = { Colour (0xffffb000), Colour (0xffff5a1f), Colour (0xffff2d7a) };

    Rectangle<float> screen (Graphics& g, Rectangle<float> b)
    {
        skin::drawInsetPanel (g, b, 6.0f);
        auto in = b.reduced (7.0f);
        g.setColour (Colour (0xff070707));
        g.fillRoundedRectangle (in, 4.0f);
        return in;
    }

    void tag (Graphics& g, Rectangle<float> in, const String& text)
    {
        g.setFont (skin::font (10.5f));
        const float tw = GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 14.0f;
        g.setColour (Colours::black.withAlpha (0.65f));
        g.fillRoundedRectangle (in.getX() + 6, in.getY() + 6, tw, 17, 4);
        g.setColour (skin::red2);
        g.drawText (text, (int) in.getX() + 13, (int) in.getY() + 6, (int) tw, 17, Justification::left);
    }
}

void CurveView::paint (Graphics& g)
{
    auto in = screen (g, getLocalBounds().toFloat());
    const auto s = proc.settings();
    auto area = in.reduced (14.0f, 26.0f).withTrimmedBottom (-14.0f);
    const float cx = area.getCentreX(), cy = area.getCentreY(), half = jmin (area.getWidth(), area.getHeight()) * 0.5f;
    g.setColour (Colours::white.withAlpha (0.09f));
    g.drawHorizontalLine ((int) cy, cx - half, cx + half);
    g.drawVerticalLine ((int) cx, cy - half, cy + half);
    g.drawLine (cx - half, cy + half, cx + half, cy - half, 1.0f);

    const float drive = s.mode == BitCrush ? 1.0f : Decibels::decibelsToGain (s.driveDb);
    const float q = std::pow (2.0f, 16.0f - s.driveDb / 36.0f * 14.0f - 1.0f);
    auto out = [&] (float x)
    {
        if (s.mode == BitCrush) return std::round (x * q) / q;
        return shape (s.mode, x * drive, s.bias);
    };
    // normalise like auto gain would, so the shape fills the box
    float peak = 0.05f;
    for (int i = 0; i <= 64; ++i) peak = jmax (peak, std::abs (out (-1.0f + i / 32.0f)));
    Path p;
    for (int i = 0; i <= 200; ++i)
    {
        const float x = -1.0f + i / 100.0f;
        const float y = out (x) / peak;
        const float px = cx + x * half, py = cy - jlimit (-1.1f, 1.1f, y) * half;
        if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
    }
    for (int i = 3; i > 0; --i)
    {
        g.setColour (skin::red2.withAlpha (0.1f));
        g.strokePath (p, PathStrokeType (2.0f + i * 3.0f));
    }
    g.setColour (skin::red2);
    g.strokePath (p, PathStrokeType (2.2f));

    const float lvl = jlimit (0.0f, 1.0f, proc.engine.uiDriven.load() / jmax (1.0f, drive));
    g.setColour (Colours::white);
    g.fillEllipse (cx + lvl * half - 5.0f, cy - jlimit (-1.1f, 1.1f, out (lvl) / peak) * half - 5.0f, 10.0f, 10.0f);

    static const char* names[] = { "TUBE", "TAPE", "TRANSISTOR", "DIGITAL", "FOLD", "FUZZ", "BIT CRUSH", "RECTIFY" };
    tag (g, in, String (names[s.mode]) + String::fromUTF8 ("  \xc2\xb7  ")
                    + (s.mode == BitCrush ? String (roundToInt (16.0f - s.driveDb / 36.0f * 14.0f)) + " BITS" : "DRIVE +" + String (s.driveDb, 1) + " dB"));
}

void BandView::paint (Graphics& g)
{
    auto in = screen (g, getLocalBounds().toFloat());
    const auto s = proc.settings();
    auto X = [&] (double f) { return in.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * in.getWidth(); };
    g.setFont (skin::font (9.0f));
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        g.setColour (Colours::white.withAlpha (0.06f));
        g.drawVerticalLine ((int) X (f), in.getY(), in.getBottom());
    }
    for (auto [f, t] : { std::pair<double, const char*> { 100.0, "100" }, { 1000.0, "1k" }, { 10000.0, "10k" } })
    {
        g.setColour (skin::textDim);
        g.drawText (t, (int) X (f) + 3, (int) in.getBottom() - 14, 30, 12, Justification::left);
    }

    const float top = in.getY() + 28, bot = in.getBottom() - 16, h = bot - top;
    const float driveNorm = s.driveDb / 36.0f;
    if (s.multiband)
    {
        const float edges[4] = { in.getX(), X (s.xLow), X (jmax (s.xHigh, s.xLow * 1.5f)), in.getRight() };
        const float amt[3] = { s.lowDrive, s.midDrive, s.highDrive };
        for (int b = 0; b < 3; ++b)
        {
            const float dh = jlimit (0.04f, 1.0f, amt[b] * (0.3f + 0.7f * driveNorm)) * h;
            auto r = Rectangle<float> (edges[b] + 1, bot - dh, edges[b + 1] - edges[b] - 2, dh);
            g.setColour (kBand[b].withAlpha (0.2f));
            g.fillRect (r);
            g.setColour (kBand[b]);
            g.fillRect (r.withHeight (3.0f));
            const float lv = jlimit (0.0f, 1.0f, proc.engine.uiBand[(size_t) b].load() * 2.0f) * h;
            g.setColour (kBand[b].withAlpha (0.55f));
            g.fillRect (r.getCentreX() - 6.0f, bot - lv, 12.0f, lv);
        }
        g.setColour (Colours::white);
        g.fillRect (edges[1] - 1.0f, in.getY(), 2.0f, in.getHeight());
        g.fillRect (edges[2] - 1.0f, in.getY(), 2.0f, in.getHeight());
        tag (g, in, String::fromUTF8 ("3 BANDS  \xc2\xb7  ") + skazi::formatValue (s.xLow, "Hz") + " / " + skazi::formatValue (s.xHigh, "Hz"));
    }
    else
    {
        const float dh = jlimit (0.04f, 1.0f, 0.3f + 0.7f * driveNorm) * h;
        auto r = Rectangle<float> (in.getX() + 1, bot - dh, in.getWidth() - 2, dh);
        g.setColour (kBand[1].withAlpha (0.18f));
        g.fillRect (r);
        g.setColour (kBand[1]);
        g.fillRect (r.withHeight (3.0f));
        const float lv = jlimit (0.0f, 1.0f, proc.engine.uiBand[1].load() * 2.0f) * h;
        g.setColour (kBand[1].withAlpha (0.55f));
        g.fillRect (r.getCentreX() - 8.0f, bot - lv, 16.0f, lv);
        tag (g, in, String::fromUTF8 ("FULL BAND  \xc2\xb7  MULTIBAND OFF"));
    }
}

void StereoScope::paint (Graphics& g)
{
    auto in = screen (g, getLocalBounds().toFloat());
    const auto s = proc.settings();
    const float cx = in.getCentreX(), cy = in.getCentreY() + 8.0f, half = jmin (in.getWidth(), in.getHeight() - 30.0f) * 0.5f;
    g.setColour (Colours::white.withAlpha (0.09f));
    g.drawVerticalLine ((int) cx, cy - half, cy + half);
    g.drawLine (cx - half, cy - half, cx + half, cy + half, 1.0f);
    g.drawLine (cx + half, cy - half, cx - half, cy + half, 1.0f);

    auto& e = proc.engine;
    float peak = 0.05f;
    for (int i = 0; i < Engine::kScope; ++i)
        peak = jmax (peak, std::abs (e.scope[(size_t) i * 2].load()), std::abs (e.scope[(size_t) i * 2 + 1].load()));
    g.setColour (Colour (0xff7aa2ff).withAlpha (0.6f));
    for (int i = 0; i < Engine::kScope; ++i)
    {
        const float l = e.scope[(size_t) i * 2].load() / peak, r = e.scope[(size_t) i * 2 + 1].load() / peak;
        const float x = (l - r) * 0.7071f, y = (l + r) * 0.7071f;
        g.fillRect (cx + x * half * 0.7f, cy - y * half * 0.7f, 1.6f, 1.6f);
    }
    tag (g, in, "WIDTH " + String (roundToInt (s.width * 100)) + "%" + String::fromUTF8 ("  \xc2\xb7  BASS MONO ")
                    + (s.bassMono > 21.0f ? skazi::formatValue (s.bassMono, "Hz") : String ("OFF")));
}

//==============================================================================
ZhugEditor::ZhugEditor (ZhugProcessor& p)
    : EditorBase (p, "ZH", "UG", String::fromUTF8 ("SATURATOR + WIDENER  \xc2\xb7  BY ASHER SWISSA"),
                  String::fromUTF8 ("ZHUG v1.0  \xc2\xb7  4X OVERSAMPLED  \xc2\xb7  VST3 / AU  \xc2\xb7  EFFECT")),
      zp (p), curve (p), bands (p), scope (p)
{
    addAndMakeVisible (curve);
    addAndMakeVisible (bands);
    addAndMakeVisible (scope);
    modeRow = &addChoiceRow ("mode");
    autoGain = &addToggle ("auto", "AUTO GAIN");
    autoGain->setTooltip ("Keeps the output as loud as the input, so you hear the tone, not just more volume");
    mb = &addToggle ("mb", "ON");
    mb->setTooltip ("Drive lows, mids and highs separately");

    const auto red = skin::red2, blue = Colour (0xff7aa2ff);
    drive = &addKnob ("drive", "DRIVE", red, "dB", 70);
    tone = &addKnob ("tone", "TONE", red, "%", 34);
    tone->setFormatter ([] (double v) { return std::abs (v) < 1 ? String ("FLAT") : (v < 0 ? "DARK " : "BRIGHT ") + String (roundToInt (std::abs (v))); });
    bias = &addKnob ("bias", "BIAS", red, "%", 34);
    lowd = &addKnob ("lowd", "LOW", kBand[0], "%", 40);
    midd = &addKnob ("midd", "MID", kBand[1], "%", 40);
    highd = &addKnob ("highd", "HIGH", kBand[2], "%", 40);
    xlow = &addKnob ("xlow", "X LOW", skin::white, "Hz", 34);
    xhigh = &addKnob ("xhigh", "X HIGH", skin::white, "Hz", 34);
    width = &addKnob ("width", "WIDTH", blue, "%", 40);
    bassmono = &addKnob ("bassmono", "BASS MONO", blue, "Hz", 40);
    bassmono->setFormatter ([] (double v) { return v <= 21.0 ? String ("OFF") : skazi::formatValue (v, "Hz"); });
    haas = &addKnob ("haas", "HAAS", blue, "ms", 40);
    mix = &addKnob ("mix", "MIX", skin::white, "%", 38);
    gain = &addKnob ("gain", "GAIN", skin::white, "dB", 38);
    inMeter = &addMeter (p.engine.uiIn);
    outMeter = &addMeter (p.engine.uiOut);

    setSize (1000, 548);
}

void ZhugEditor::layout()
{
    curve.setBounds (20, 68, 300, 196);
    bands.setBounds (328, 68, 344, 196);
    scope.setBounds (680, 68, 300, 196);
    modeRow->setBounds (20, 272, 960, 36);

    const int y = 316, h = 186;
    panel ({ 20, y, 250, h }, "DRIVE");
    autoGain->setBounds (82, y + 6, 92, 20);
    put (*drive, 28, y + 40, 70, 96);
    put (*tone, 150, y + 32, 34, 82);
    put (*bias, 150, y + 108, 34, 82);

    panel ({ 278, y, 330, h }, "MULTIBAND");
    mb->setBounds (378, y + 6, 40, 20);
    put (*lowd, 292, y + 34, 40, 70);
    put (*midd, 392, y + 34, 40, 70);
    put (*highd, 492, y + 34, 40, 70);
    put (*xlow, 344, y + 112, 34, 70);
    put (*xhigh, 444, y + 112, 34, 70);

    panel ({ 616, y, 190, h }, "WIDTH");
    put (*width, 622, y + 34, 40, 84);
    put (*bassmono, 712, y + 34, 40, 84);
    put (*haas, 668, y + 108, 40, 84);

    panel ({ 814, y, 166, h }, "OUTPUT");
    put (*mix, 818, y + 34, 38, 66);
    put (*gain, 818, y + 108, 38, 66);
    inMeter->setBounds (914, y + 36, 12, 128);
    outMeter->setBounds (944, y + 36, 12, 128);
    label ({ 906, y + 166, 30, 12 }, "IN");
    label ({ 935, y + 166, 30, 12 }, "OUT");
}

void ZhugEditor::tick()
{
    curve.repaint();
    bands.repaint();
    scope.repaint();
    const bool on = zp.v ("mb") > 0.5f;
    for (auto* k : { lowd, midd, highd, xlow, xhigh })
        k->setAlpha (on ? 1.0f : 0.4f);
}
