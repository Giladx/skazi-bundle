#include "PluginEditor.h"

using namespace juce;
using namespace quazi;

namespace
{
    // Low, low-mid, high-mid, high
    const Colour kBand[NumBands] = { Colour (0xffff2a1f), Colour (0xffffb000), Colour (0xff22e3a5), Colour (0xff7aa2ff) };
    const char* const kBandName[NumBands] = { "LOW", "LOW-MID", "HIGH-MID", "HIGH" };
    const char* const kModeName[NumModes] = { "SOFT", "HARD", "FUZZ", "TUBE" };
    const char* const kSplitId[3] = { "xLo", "xMid", "xHi" };

    constexpr double kMinF = 20.0, kMaxF = 20000.0;
    constexpr float kEqRange = 18.0f; // dB from the centre line to the top of the screen

    Rectangle<float> screen (Graphics& g, Rectangle<float> b)
    {
        skin::drawInsetPanel (g, b, 6.0f);
        auto in = b.reduced (7.0f);
        g.setColour (Colour (0xff070707));
        g.fillRoundedRectangle (in, 4.0f);
        return in;
    }

    // The splits as the engine uses them: always rising
    std::array<float, 3> shownSplits (const Settings& s)
    {
        auto x = s.xover;
        x[1] = jmax (x[1], x[0] * 1.25f);
        x[2] = jmax (x[2], x[1] * 1.25f);
        return x;
    }
}

//==============================================================================
Rectangle<float> BandView::plotArea() const { return getLocalBounds().toFloat().reduced (7.0f).reduced (8.0f, 0.0f).withTrimmedTop (4.0f).withTrimmedBottom (16.0f); }

float BandView::xFor (double f) const
{
    const auto a = plotArea();
    return a.getX() + (float) (std::log (f / kMinF) / std::log (kMaxF / kMinF)) * a.getWidth();
}

double BandView::freqAt (float x) const
{
    const auto a = plotArea();
    return kMinF * std::pow (kMaxF / kMinF, (double) jlimit (0.0f, 1.0f, (x - a.getX()) / a.getWidth()));
}

int BandView::splitNear (float x) const
{
    const auto split = shownSplits (proc.settings());
    int best = -1; float bestD = 9.0f;
    for (int i = 0; i < 3; ++i)
    {
        const float d = std::abs (xFor (split[(size_t) i]) - x);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

void BandView::mouseMove (const MouseEvent& e) { setHover (splitNear ((float) e.x)); }

void BandView::mouseDown (const MouseEvent& e)
{
    dragging = splitNear ((float) e.x);
    if (dragging >= 0)
    {
        proc.undo.beginNewTransaction();
        if (auto* p = proc.apvts.getParameter (kSplitId[dragging])) p->beginChangeGesture();
    }
}

void BandView::mouseDrag (const MouseEvent& e)
{
    if (dragging < 0) return;
    if (auto* p = dynamic_cast<RangedAudioParameter*> (proc.apvts.getParameter (kSplitId[dragging])))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) freqAt ((float) e.x)));
    repaint();
}

void BandView::mouseUp (const MouseEvent&)
{
    if (dragging >= 0)
        if (auto* p = proc.apvts.getParameter (kSplitId[dragging])) p->endChangeGesture();
    dragging = -1;
}

void BandView::paint (Graphics& g)
{
    auto in = screen (g, getLocalBounds().toFloat());
    const auto a = plotArea();
    const auto s = proc.settings();
    const auto split = shownSplits (s);
    const float mid = a.getCentreY();

    // grid
    g.setFont (skin::font (9.0f));
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        g.setColour (Colours::white.withAlpha (0.05f));
        g.drawVerticalLine ((int) xFor (f), a.getY(), a.getBottom());
    }
    for (auto [f, t] : { std::pair<double, const char*> { 100.0, "100" }, { 1000.0, "1k" }, { 10000.0, "10k" } })
    {
        g.setColour (skin::textDim);
        g.drawText (t, (int) xFor (f) + 3, (int) in.getBottom() - 14, 30, 12, Justification::left);
    }

    // bands: a dim floor, the drive as a tinted column, the live level as a bright one
    for (int b = 0; b < NumBands; ++b)
    {
        const float x0 = b == 0 ? a.getX() : xFor (split[(size_t) b - 1]);
        const float x1 = b == NumBands - 1 ? a.getRight() : xFor (split[(size_t) b]);
        auto col = Rectangle<float> (x0, a.getY(), x1 - x0, a.getHeight()).reduced (1.5f, 0.0f);
        const auto c = kBand[b];
        const bool on = s.on[(size_t) b];

        g.setColour (c.withAlpha (0.05f));
        g.fillRect (col);
        if (on)
        {
            const float dh = jlimit (0.0f, 1.0f, s.driveDb[(size_t) b] / 48.0f) * col.getHeight();
            g.setGradientFill (ColourGradient (c.withAlpha (0.28f), 0, col.getBottom(), c.withAlpha (0.04f), 0, col.getBottom() - dh, false));
            g.fillRect (col.withTop (col.getBottom() - dh));
        }
        const float pk = proc.engine.uiBand[(size_t) b].load();
        const float lh = jlimit (0.0f, 1.0f, (Decibels::gainToDecibels (pk, -48.0f) + 48.0f) / 48.0f) * col.getHeight();
        auto lvl = col.withTop (col.getBottom() - lh).reduced (col.getWidth() * 0.3f, 0.0f);
        g.setColour (c.withAlpha (on ? 0.75f : 0.3f));
        g.fillRect (lvl);

        g.setColour (on ? c : skin::textDim);
        g.setFont (skin::font (10.5f));
        g.drawText (kBandName[b], col.toNearestInt().withHeight (16).translated (0, 6), Justification::centred);
        g.setColour (on ? skin::white.withAlpha (0.8f) : skin::textDim);
        g.setFont (skin::font (9.0f));
        const String sub = on ? String (kModeName[jlimit (0, NumModes - 1, s.mode[(size_t) b])]) + "  " + String (s.driveDb[(size_t) b], 1) + " dB"
                              : String ("OFF");
        g.drawText (sub, col.toNearestInt().withHeight (14).translated (0, 22), Justification::centred);
    }

    // crossover lines with their frequency
    for (int i = 0; i < 3; ++i)
    {
        const float x = xFor (split[(size_t) i]);
        const bool hot = i == hover || i == dragging;
        g.setColour (Colours::white.withAlpha (hot ? 0.9f : 0.45f));
        g.drawLine (x, a.getY(), x, a.getBottom(), hot ? 2.0f : 1.2f);
        g.fillEllipse (x - 4.0f, a.getBottom() - 9.0f, 8.0f, 8.0f);
        g.setFont (skin::font (9.0f));
        g.drawText (skazi::formatValue (split[(size_t) i], "Hz"), (int) x - 40, (int) a.getBottom() - 24, 80, 12, Justification::centred);
    }

    // post EQ
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawHorizontalLine ((int) mid, a.getX(), a.getRight());
    Path eq;
    const double rate = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
    for (int k = 0; k <= 200; ++k)
    {
        const float x = a.getX() + a.getWidth() * k / 200.0f;
        const float db = Decibels::gainToDecibels (eqMagnitude (s, rate, freqAt (x)), -60.0f);
        const float y = mid - jlimit (-1.0f, 1.0f, db / kEqRange) * a.getHeight() * 0.5f;
        if (k == 0) eq.startNewSubPath (x, y); else eq.lineTo (x, y);
    }
    g.setColour (skin::white);
    g.strokePath (eq, PathStrokeType (1.6f));
    g.setColour (skin::textDim);
    g.drawText ("POST EQ", (int) a.getRight() - 70, (int) a.getY() + 6, 64, 12, Justification::right);
}

//==============================================================================
QuaziEditor::QuaziEditor (QuaziProcessor& p)
    : EditorBase (p, "QUA", "ZI", String::fromUTF8 ("4-BAND FUZZ  \xc2\xb7  BY ASHER SWISSA"),
                  String::fromUTF8 ("QUAZI v1.0  \xc2\xb7  4-BAND DISTORTION + WIDTH + POST EQ  \xc2\xb7  VST3 / AU  \xc2\xb7  EFFECT")),
      qp (p), bandView (p)
{
    setSubtitleSize (15.0f);
    addAndMakeVisible (bandView);

    for (int b = 0; b < NumBands; ++b)
    {
        auto& c = bands[(size_t) b];
        c.on = &addToggle (bandId (b, "On"), "ON");
        c.mode = &addChoiceRow (bandId (b, "Mode"), {}, kBand[b]);
        c.drive = &addKnob (bandId (b, "Drive"), "DRIVE", kBand[b], "dB", 50);
        c.level = &addKnob (bandId (b, "Level"), "LEVEL", kBand[b], "dB", 34);
    }

    xLo = &addKnob ("xLo", "LOW", skin::white, "Hz", 34);
    xMid = &addKnob ("xMid", "MID", skin::white, "Hz", 34);
    xHi = &addKnob ("xHi", "HIGH", skin::white, "Hz", 34);

    width = &addKnob ("width", "WIDTH", skin::red2, "%", 44);
    width->setFormatter ([] (double v) { return v < 0.5 ? String ("MONO") : String (roundToInt (v)) + " %"; });
    monoBass = &addToggle ("monoBass", "MONO BASS");
    monoBass->setTooltip ("Keeps the low band in the middle however wide the rest goes");

    const auto eqCol = Colour (0xffd8d8dc);
    lowDb = &addKnob ("eqLowDb", "LOW", eqCol, "dB", 34);
    lowF = &addKnob ("eqLowF", "FREQ", eqCol, "Hz", 34);
    midDb = &addKnob ("eqMidDb", "MID", eqCol, "dB", 34);
    midF = &addKnob ("eqMidF", "FREQ", eqCol, "Hz", 34);
    midQ = &addKnob ("eqMidQ", "Q", eqCol, "", 34);
    midQ->setFormatter ([] (double v) { return String (v, 2); });
    highDb = &addKnob ("eqHighDb", "HIGH", eqCol, "dB", 34);
    highF = &addKnob ("eqHighF", "FREQ", eqCol, "Hz", 34);

    inGain = &addKnob ("in", "INPUT", skin::white, "dB", 34);
    mix = &addKnob ("mix", "MIX", skin::white, "%", 34);
    outGain = &addKnob ("out", "OUTPUT", skin::white, "dB", 34);
    inMeter = &addMeter (p.engine.uiIn);
    outMeter = &addMeter (p.engine.uiOut);

    setSize (1120, 650);
}

void QuaziEditor::layout()
{
    bandView.setBounds (20, 66, 1080, 178);

    int y = 252;
    const int bw = 264, bh = 198;
    for (int b = 0; b < NumBands; ++b)
    {
        const int x = 20 + b * (bw + 8);
        auto& c = bands[(size_t) b];
        panel ({ x, y, bw, bh }, kBandName[b]);
        c.on->setBounds (x + bw - 52, y + 7, 44, 20);
        c.mode->setBounds (x + 10, y + 34, bw - 20, 30);
        put (*c.drive, x + 28, y + 80, 50, 92);
        put (*c.level, x + 150, y + 88, 34, 84);
    }

    y = 458;
    const int h = 148;
    panel ({ 20, y, 236, h }, "CROSSOVER");
    int kx = 30;
    for (auto* k : { xLo, xMid, xHi }) { put (*k, kx, y + 48, 34, 70); kx += 74; }

    panel ({ 264, y, 180, h }, "STEREO WIDTH");
    put (*width, 272, y + 44, 44, 76);
    monoBass->setBounds (356, y + 62, 80, 24);

    panel ({ 452, y, 446, h }, "POST EQ");
    label ({ 466, y + 30, 120, 14 }, "LOW SHELF");
    label ({ 590, y + 30, 120, 14 }, "BELL");
    label ({ 776, y + 30, 120, 14 }, "HIGH SHELF");
    kx = 458;
    for (auto* k : { lowDb, lowF, midDb, midF, midQ, highDb, highF }) { put (*k, kx, y + 48, 34, 60); kx += 62; }

    panel ({ 906, y, 194, h }, "OUTPUT");
    kx = 910;
    for (auto* k : { inGain, mix, outGain }) { put (*k, kx, y + 48, 34, 50); kx += 50; }
    inMeter->setBounds (1064, y + 34, 10, 92);
    outMeter->setBounds (1080, y + 34, 10, 92);
    label ({ 1062, y + 128, 20, 12 }, "IN");
    label ({ 1076, y + 128, 24, 12 }, "OUT");
}

void QuaziEditor::tick()
{
    bandView.repaint();
}
