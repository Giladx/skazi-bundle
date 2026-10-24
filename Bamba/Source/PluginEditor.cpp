#include "PluginEditor.h"

using namespace juce;
using namespace bamba;

void BambaDisplay::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    skin::drawInsetPanel (g, b, 6.0f);
    auto in = b.reduced (7.0f);
    g.setColour (Colour (0xff070707));
    g.fillRoundedRectangle (in, 4.0f);

    const auto s = proc.settings();
    const int bars = s.bars;
    const double bpm = 120.0; // the drawing only needs the shape, not the real tempo
    const float lenSec = (float) (bars * 4 * 60.0 / bpm);
    const int shownType = s.type == RandomFx ? proc.engine.uiType.load() : s.type;

    // bar grid with labels
    const int beats = bars * 4;
    g.setFont (skin::font (9.5f));
    for (int i = 0; i <= beats; ++i)
    {
        const float x = in.getX() + in.getWidth() * i / beats;
        const bool bar = i % 4 == 0;
        if (beats > 32 && ! bar) continue;
        g.setColour (Colours::white.withAlpha (bar ? 0.25f : 0.07f));
        g.drawVerticalLine ((int) x, in.getY(), in.getBottom());
        if (bar && i < beats && (bars <= 8 || (i / 4) % 2 == 0))
        {
            g.setColour (skin::textDim);
            g.drawText ("BAR " + String (i / 4 + 1), (int) x + 4, (int) in.getBottom() - 15, 60, 12, Justification::left);
        }
    }

    // curves
    const int w = (int) in.getWidth();
    const float top = in.getY() + 26, bot = in.getBottom() - 18, h = bot - top;
    Path amp, ampFill, filt, pitch;
    double rollPh = 0;
    const float rollAmt = shownType == SnareRoll ? 1.0f : (s.type == RandomFx ? 0.0f : s.roll);
    for (int x = 0; x <= w; ++x)
    {
        const float p = (float) x / w;
        const auto sh = shapeAt (shownType, p, s.curve, lenSec);
        float a = sh.amp;
        if (rollAmt > 0.0f)
        {
            const float rise = std::pow (p, std::pow (2.0f, (s.curve - 0.5f) * 4.0f));
            rollPh += 2.0 * std::pow (4.0, (double) rise) * beats / (double) w;
            a *= 1.0f - rollAmt * (1.0f - (float) std::exp (-(rollPh - std::floor (rollPh)) * 6.0));
        }
        const float ya = bot - a * h, yf = bot - (sh.filter * s.sweep + (1.0f - s.sweep)) * h, yp = bot - sh.pitch * h;
        const float px = in.getX() + x;
        if (x == 0)
        {
            amp.startNewSubPath (px, ya); ampFill.startNewSubPath (px, bot); ampFill.lineTo (px, ya);
            filt.startNewSubPath (px, yf); pitch.startNewSubPath (px, yp);
        }
        else { amp.lineTo (px, ya); ampFill.lineTo (px, ya); filt.lineTo (px, yf); pitch.lineTo (px, yp); }
    }
    ampFill.lineTo (in.getRight(), bot);
    ampFill.closeSubPath();
    g.setGradientFill (ColourGradient (skin::red2.withAlpha (0.3f), 0, top, skin::red2.withAlpha (0.03f), 0, bot, false));
    g.fillPath (ampFill);
    g.setColour (skin::red2.withAlpha (0.25f));
    g.strokePath (amp, PathStrokeType (6.0f));
    g.setColour (skin::red2);
    g.strokePath (amp, PathStrokeType (2.2f));

    Path dashed;
    const float dashes[] = { 6.0f, 5.0f };
    PathStrokeType (1.6f).createDashedStroke (dashed, filt, dashes, 2);
    g.setColour (Colour (0xff22e3a5));
    g.fillPath (dashed);
    g.setColour (Colours::white.withAlpha (0.45f));
    g.strokePath (pitch, PathStrokeType (1.0f));

    // playhead
    const float prog = proc.engine.uiProgress.load();
    if (prog >= 0.0f)
    {
        const float px = in.getX() + prog * in.getWidth();
        for (int i = 3; i > 0; --i)
        {
            g.setColour (Colours::white.withAlpha (0.08f));
            g.fillRect (px - i * 2.0f, in.getY(), 2.0f + i * 4.0f, in.getHeight());
        }
        g.setColour (Colours::white);
        g.fillRect (px - 1.0f, in.getY(), 2.0f, in.getHeight());
    }

    // info + legend
    static const char* names[] = { "UPLIFTER", "DOWNLIFTER", "NOISE SWEEP", "IMPACT", "SNARE ROLL", "REVERSE CRASH", "LASER", "SIREN", "SUB DROP", "RANDOM FX" };
    const String dot = String::fromUTF8 ("  \xc2\xb7  ");
    String info = String (names[s.type]);
    if (s.type == RandomFx) info << " (" << names[shownType] << ")";
    info << dot << bars << (bars == 1 ? " BAR" : " BARS") << dot << (s.trigger == Sync ? "EACH NOTE PLAYS THE FULL LENGTH" : "PLAYS WHILE THE NOTE IS HELD");
    g.setFont (skin::font (10.5f));
    const float tw = GlyphArrangement::getStringWidth (g.getCurrentFont(), info) + 14.0f;
    g.setColour (Colours::black.withAlpha (0.65f));
    g.fillRoundedRectangle (in.getX() + 6, in.getY() + 6, tw, 17, 4);
    g.setColour (skin::red2);
    g.drawText (info, (int) in.getX() + 13, (int) in.getY() + 6, (int) tw, 17, Justification::left);

    g.setFont (skin::font (9.5f));
    const int lx = (int) in.getRight() - 190, ly = (int) in.getY() + 8;
    g.setColour (skin::red2);              g.drawText ("LEVEL", lx, ly, 50, 12, Justification::left);
    g.setColour (Colour (0xff22e3a5));     g.drawText ("FILTER", lx + 54, ly, 50, 12, Justification::left);
    g.setColour (Colours::white.withAlpha (0.6f)); g.drawText ("PITCH", lx + 112, ly, 50, 12, Justification::left);
}

//==============================================================================
BambaEditor::BambaEditor (BambaProcessor& p)
    : EditorBase (p, "BAM", "BA", String::fromUTF8 ("RISER & FX MAKER  \xc2\xb7  BY ASHER SWISSA"),
                  String::fromUTF8 ("BAMBA v1.0  \xc2\xb7  VST3 / AU  \xc2\xb7  INSTRUMENT")),
      bp (p), display (p)
{
    addAndMakeVisible (display);
    typeRow = &addChoiceRow ("type");
    triggerRow = &addChoiceRow ("trigger");
    barsBox = &addCombo ("bars");

    const auto red = skin::red2, green = Colour (0xff22e3a5);
    curve = &addKnob ("curve", "CURVE", skin::white, "%", 38);
    offset = &addKnob ("offset", "OFFSET", skin::white, "", 38);
    offset->setFormatter ([] (double v) { const int b = roundToInt (v); return String (b) + (b == 1 ? " bar" : " bars"); });
    noise = &addKnob ("noise", "NOISE", red, "%", 34);
    color = &addKnob ("color", "COLOR", red, "%", 34);
    color->setFormatter ([] (double v) { return v < 25 ? String ("WHITE") : v < 70 ? String ("PINK") : String ("BROWN"); });
    tone = &addKnob ("tone", "TONE", red, "%", 34);
    pfrom = &addKnob ("pfrom", "PITCH FROM", red, "st", 34);
    pto = &addKnob ("pto", "PITCH TO", red, "st", 34);
    sweep = &addKnob ("sweep", "FILTER SWEEP", red, "%", 66);
    reso = &addKnob ("reso", "RESO", red, "%", 34);
    roll = &addKnob ("roll", "ROLL", red, "%", 34);
    width = &addKnob ("width", "WIDTH", red, "%", 34);
    phaser = &addKnob ("phaser", "PHASER", red, "%", 34);
    reverb = &addKnob ("reverb", "REVERB", green, "%", 34);
    delay = &addKnob ("delay", "DELAY", green, "%", 34);
    gain = &addKnob ("gain", "GAIN", skin::white, "dB", 34);
    outMeter = &addMeter (p.engine.uiOut);

    curve->slider.setTooltip ("How the effect builds: low = fast start, high = holds back then explodes");
    offset->slider.setTooltip ("SYNC mode: which bar the effect starts on");
    roll->slider.setTooltip ("Stutter gate that speeds up from 1/8 to 1/32 as it builds");

    setSize (1000, 544);
}

void BambaEditor::layout()
{
    display.setBounds (20, 68, 960, 204);
    typeRow->setBounds (20, 280, 960, 38);

    const int y = 326, h = 176;
    panel ({ 20, y, 220, h }, "LENGTH");
    triggerRow->setBounds (100, y + 6, 132, 20);
    label ({ 32, y + 38, 80, 12 }, "BARS");
    barsBox->setBounds (32, y + 52, 86, 24);
    put (*curve, 128, y + 32, 38, 56);
    put (*offset, 128, y + 100, 38, 56);

    panel ({ 248, y, 270, h }, "SOURCE");
    put (*noise, 258, y + 32, 34, 78);
    put (*color, 338, y + 32, 34, 78);
    put (*tone, 418, y + 32, 34, 78);
    put (*pfrom, 298, y + 104, 34, 78);
    put (*pto, 378, y + 104, 34, 78);

    panel ({ 526, y, 250, h }, "MOVEMENT");
    put (*sweep, 534, y + 40, 66, 90);
    put (*reso, 626, y + 32, 34, 70);
    put (*roll, 696, y + 32, 34, 70);
    put (*width, 626, y + 104, 34, 70);
    put (*phaser, 696, y + 104, 34, 70);

    panel ({ 784, y, 196, h }, "SPACE + OUT");
    put (*reverb, 790, y + 32, 34, 70);
    put (*delay, 862, y + 32, 34, 70);
    put (*gain, 826, y + 104, 34, 70);
    outMeter->setBounds (950, y + 34, 12, 128);
}

void BambaEditor::tick()
{
    display.repaint();
    offset->setVisible (false); // BAMBA now starts on notes only
}
