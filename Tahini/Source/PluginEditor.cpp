#include "PluginEditor.h"

using namespace juce;
using namespace tahini;

void TahiniDisplay::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    skin::drawInsetPanel (g, b, 6.0f);
    auto in = b.reduced (7.0f);
    g.setColour (Colour (0xff070707));
    g.fillRoundedRectangle (in, 4.0f);

    const auto s = proc.settings();
    const double bpm = proc.engine.uiBpm.load();
    const double echoSec = skazi::noteBeats (s.time) * 60.0 / bpm;
    const double span = jlimit (2.0, 12.0, jmax (echoSec * 9.0, (double) jmin (s.decay, 8.0f) + s.predelayMs * 0.001));
    auto X = [&] (double sec) { return in.getX() + (float) (sec / span) * in.getWidth(); };
    const float mid = in.getCentreY();

    // time grid every beat
    const double beat = 60.0 / bpm;
    for (double t = 0; t <= span; t += beat)
    {
        g.setColour (Colours::white.withAlpha (0.07f));
        g.drawVerticalLine ((int) X (t), in.getY(), in.getBottom());
    }
    g.setColour (Colours::white.withAlpha (0.15f));
    g.drawHorizontalLine ((int) mid, in.getX(), in.getRight());

    // reverb tail: pre-delay, then -60 dB over DECAY
    if (s.reverbMix > 0.0f)
    {
        Path tail;
        tail.startNewSubPath (in.getX(), mid);
        const double pre = s.predelayMs * 0.001;
        for (int x = 0; x <= (int) in.getWidth(); x += 2)
        {
            const double t = span * x / in.getWidth();
            float a = 0.0f;
            if (t > pre)
            {
                const double tt = t - pre;
                a = (float) (std::min (1.0, tt / 0.05) * (s.freeze ? 1.0 : std::pow (10.0, -3.0 * tt / s.decay)));
            }
            tail.lineTo (in.getX() + x, mid - a * s.reverbMix * in.getHeight() * 0.42f);
        }
        tail.lineTo (in.getRight(), mid);
        for (int x = (int) in.getWidth(); x >= 0; x -= 2)
        {
            const double t = span * x / in.getWidth();
            float a = 0.0f;
            if (t > pre)
            {
                const double tt = t - pre;
                a = (float) (std::min (1.0, tt / 0.05) * (s.freeze ? 1.0 : std::pow (10.0, -3.0 * tt / s.decay)));
            }
            tail.lineTo (in.getX() + x, mid + a * s.reverbMix * in.getHeight() * 0.42f);
        }
        tail.closeSubPath();
        g.setColour (Colour (0xff22e3a5).withAlpha (0.18f));
        g.fillPath (tail);
        g.setColour (Colour (0xff22e3a5).withAlpha (0.7f));
        g.strokePath (tail, PathStrokeType (1.2f));
    }

    // echo taps: ping-pong alternates sides, stereo/tape hit both
    float amp = 1.0f;
    for (int k = 0; k < 40; ++k)
    {
        const double t = echoSec * k;
        if (t > span || amp < 0.01f) break;
        const float h = (k == 0 ? 0.9f : amp * s.delayMix) * in.getHeight() * 0.42f;
        const float x = X (t);
        const bool up = s.mode != PingPong || k % 2 == 1 || k == 0;
        const bool down = s.mode != PingPong || k % 2 == 0;
        const Colour c = k == 0 ? Colours::white : skin::red2;
        g.setColour (c.withAlpha (0.25f));
        if (up)   g.fillRect (x - 4.0f, mid - h, 8.0f, h);
        if (down) g.fillRect (x - 4.0f, mid, 8.0f, h);
        g.setColour (c);
        if (up)   g.fillRect (x - 1.5f, mid - h, 3.0f, h);
        if (down) g.fillRect (x - 1.5f, mid, 3.0f, h);
        if (k > 0) amp *= s.feedback;
    }

    // scrolling wet level
    Path trace;
    for (size_t i = 0; i < history.size(); ++i)
    {
        const float x = in.getX() + in.getWidth() * i / (history.size() - 1);
        const float y = in.getBottom() - 4.0f - jmin (1.0f, history[i] * 2.0f) * in.getHeight() * 0.3f;
        if (i == 0) trace.startNewSubPath (x, y); else trace.lineTo (x, y);
    }
    g.setColour (Colours::white.withAlpha (0.35f));
    g.strokePath (trace, PathStrokeType (1.2f));

    g.setFont (skin::font (9.5f));
    g.setColour (skin::textDim);
    g.drawText ("L", (int) in.getX() + 6, (int) in.getY() + 28, 12, 12, Justification::left);
    g.drawText ("R", (int) in.getX() + 6, (int) in.getBottom() - 18, 12, 12, Justification::left);

    static const char* modes[] = { "PING-PONG", "STEREO", "TAPE" };
    const String dot = String::fromUTF8 ("  \xc2\xb7  ");
    String info = "DELAY " + skazi::noteLengthNames()[s.time] + " " + modes[s.mode] + dot + "FB " + String (roundToInt (s.feedback * 100)) + "%"
                + dot + "REVERB " + (s.freeze ? String ("FROZEN") : String (s.decay, 1) + " s") + dot + "DUCK " + String (roundToInt (s.duck * 100)) + "%";
    if (std::abs (s.pitch) >= 0.5f) info << dot << "PITCH " << (s.pitch > 0 ? "+" : "") << roundToInt (s.pitch) << " st";
    if (s.reverse > 0.01f) info << dot << "REVERSE";
    if (s.tapeStop > 0.01f) info << dot << "TAPE STOPS";
    g.setFont (skin::font (10.5f));
    const float tw = GlyphArrangement::getStringWidth (g.getCurrentFont(), info) + 14.0f;
    g.setColour (Colours::black.withAlpha (0.65f));
    g.fillRoundedRectangle (in.getX() + 6, in.getY() + 6, tw, 17, 4);
    g.setColour (skin::red2);
    g.drawText (info, (int) in.getX() + 13, (int) in.getY() + 6, (int) tw, 17, Justification::left);
}

//==============================================================================
TahiniEditor::TahiniEditor (TahiniProcessor& p)
    : EditorBase (p, "TAH", "INI", String::fromUTF8 ("SPACE DELAY + REVERB  \xc2\xb7  BY ASHER SWISSA"),
                  String::fromUTF8 ("TAHINI v1.0  \xc2\xb7  VST3 / AU  \xc2\xb7  EFFECT")),
      tp (p), display (p)
{
    addAndMakeVisible (display);
    const auto red = skin::red2, green = Colour (0xff22e3a5);
    modeRow = &addChoiceRow ("dmode");
    timeBox = &addCombo ("dtime");
    freeze = &addToggle ("freeze", "FREEZE");
    freeze->setTooltip ("Hold the reverb tail forever");
    dry = &addToggle ("dry", "DRY");
    dry->setTooltip ("Turn off to use TAHINI on a send (wet only)");

    fb = &addKnob ("fb", "FEEDBACK", red, "%", 66);
    locut = &addKnob ("locut", "LOW CUT", red, "Hz", 34);
    hicut = &addKnob ("hicut", "HIGH CUT", red, "Hz", 34);

    // tape FX strip
    const auto tapeCol = Colour (0xffffb000);
    wow = &addKnob ("wow", "WOW", tapeCol, "%", 40);
    age = &addKnob ("age", "AGE", tapeCol, "%", 40);
    drive = &addKnob ("ddrive", "DRIVE", tapeCol, "%", 40);
    pitch = &addKnob ("pitch", "PITCH", Colour (0xffff2d7a), "st", 40);
    reverse = &addKnob ("reverse", "REVERSE", Colour (0xffc084fc), "%", 40);
    tstop = &addKnob ("tstop", "TAPE STOP", Colour (0xff38bdf8), "%", 40);
    pitch->slider.setTooltip ("Every echo is shifted again: + spirals up, - spirals down");
    reverse->slider.setTooltip ("Blend in echoes played backwards");
    tstop->slider.setTooltip ("How often the tape drags to a halt and spins back up");
    age->slider.setTooltip ("Old tape: hiss, dropouts, extra wobble and a darker sound");
    size = &addKnob ("size", "SIZE", green, "%", 66);
    decay = &addKnob ("decay", "DECAY", green, "s", 32);
    predelay = &addKnob ("predelay", "PRE-DELAY", green, "ms", 32);
    damp = &addKnob ("damp", "DAMP", green, "%", 32);
    width = &addKnob ("width", "WIDTH", green, "%", 32);
    duck = &addKnob ("duck", "AMOUNT", skin::white, "%", 40);
    duckrel = &addKnob ("duckrel", "RELEASE", skin::white, "ms", 40);
    dmix = &addKnob ("dmix", "DELAY", red, "%", 36);
    rmix = &addKnob ("rmix", "REVERB", green, "%", 36);
    gain = &addKnob ("gain", "GAIN", skin::white, "dB", 36);
    outMeter = &addMeter (p.engine.uiOut);
    duckMeter = &addMeter (p.engine.uiDuck, true);
    duck->slider.setTooltip ("Pushes the echoes and reverb down while you play, so they swell in the gaps");

    setSize (1000, 624);
}

void TahiniEditor::layout()
{
    display.setBounds (20, 68, 960, 196);

    // TAPE FX strip across the whole width
    panel ({ 20, 272, 960, 96 }, "TAPE FX");
    int tx = 130;
    for (auto* k : { wow, age, drive, pitch, reverse, tstop })
    {
        put (*k, tx, 280, 40, 80);
        tx += 140;
    }

    const int y = 378, h = 196;
    panel ({ 20, y, 336, h }, "DELAY");
    modeRow->setBounds (84, y + 6, 264, 20);
    label ({ 32, y + 40, 60, 12 }, "TIME");
    timeBox->setBounds (32, y + 54, 84, 24);
    put (*fb, 122, y + 40, 66, 90);
    put (*locut, 232, y + 34, 34, 80);
    put (*hicut, 232, y + 112, 34, 80);

    panel ({ 364, y, 300, h }, "REVERB");
    freeze->setBounds (432, y + 6, 66, 20);
    put (*size, 372, y + 40, 66, 90);
    put (*decay, 470, y + 34, 32, 60);
    put (*predelay, 536, y + 34, 32, 64);
    put (*damp, 470, y + 112, 32, 60);
    put (*width, 536, y + 112, 32, 64);

    panel ({ 672, y, 130, h }, "DUCK");
    put (*duck, 678, y + 32, 40, 56);
    put (*duckrel, 678, y + 112, 40, 56);
    duckMeter->setBounds (774, y + 36, 12, 140);

    panel ({ 810, y, 170, h }, "MIX");
    dry->setBounds (858, y + 6, 46, 20);
    put (*dmix, 814, y + 34, 36, 58);
    put (*rmix, 872, y + 34, 36, 58);
    put (*gain, 842, y + 112, 36, 58);
    outMeter->setBounds (950, y + 36, 12, 140);
}

void TahiniEditor::tick()
{
    display.push (tp.engine.uiWet.load());
    display.repaint();
}
