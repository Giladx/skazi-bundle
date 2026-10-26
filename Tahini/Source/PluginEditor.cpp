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

    // echo taps: ping-pong alternates sides, DUAL puts the right side at 3/4, MULTI-TAP adds the in-between heads
    auto bar = [&] (double t, float h, bool up, Colour c)
    {
        if (t > span) return;
        const float x = X (t);
        g.setColour (c.withAlpha (0.25f));
        if (up) g.fillRect (x - 4.0f, mid - h, 8.0f, h); else g.fillRect (x - 4.0f, mid, 8.0f, h);
        g.setColour (c);
        if (up) g.fillRect (x - 1.5f, mid - h, 3.0f, h); else g.fillRect (x - 1.5f, mid, 3.0f, h);
    };
    const float full = in.getHeight() * 0.42f;
    bar (0.0, 0.9f * full, true, Colours::white);
    bar (0.0, 0.9f * full, false, Colours::white);
    float amp = 1.0f;
    for (int k = 1; k < 40; ++k)
    {
        if (echoSec * (k - 1) > span || amp < 0.01f) break;
        const float h = amp * s.delayMix * full;
        const double t = echoSec * k;
        if (s.mode == PingPong) bar (t, h, k % 2 == 1, skin::red2);
        else if (s.mode == Dual) { bar (t, h, true, skin::red2); bar (t * 0.75, h, false, skin::red2); }
        else
        {
            bar (t, h, true, skin::red2);
            bar (t, h, false, skin::red2);
            if (s.mode == MultiTap)
            {
                const Colour sub = skin::red2.withMultipliedBrightness (0.7f);
                bar (t - echoSec * 0.75, h * 0.3f, true, sub);
                bar (t - echoSec * 0.5, h * 0.45f, false, sub);
                bar (t - echoSec * 0.25, h * 0.55f, true, sub);
            }
        }
        amp *= s.feedback;
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

    static const char* modes[] = { "PING-PONG", "STEREO", "TAPE", "DIGITAL", "BBD", "MULTI-TAP", "DUAL" };
    static const char* drives[] = { "TAPE", "TUBE", "FUZZ", "CRUSH" };
    const String dot = String::fromUTF8 ("  \xc2\xb7  ");
    String info = "DELAY " + skazi::noteLengthNames()[s.time] + " " + modes[s.mode] + dot + "FB " + String (roundToInt (s.feedback * 100)) + "%"
                + dot + "REVERB " + (s.freeze ? String ("FROZEN") : String (s.decay, 1) + " s") + dot + "DUCK " + String (roundToInt (s.duck * 100)) + "%";
    if (s.drive > 0.005f && (s.driveType != DriveTape || s.drive > 0.3f)) info << dot << drives[jlimit (0, 3, s.driveType)] << " " << roundToInt (s.drive * 100) << "%";
    const float st = s.pitchSnap ? (float) roundToInt (s.pitch) : s.pitch;
    if (std::abs (st) >= 0.05f)
        info << dot << (s.pitchMix < 0.99f ? "SHIMMER " : "PITCH ") << (st > 0 ? "+" : "") << (s.pitchSnap ? String (roundToInt (st)) : String (st, 2)) << " st";
    if (s.reverse > 0.01f) info << dot << "REVERSE";
    if (s.tapeStop > 0.01f) info << dot << "TAPE STOPS";
    g.setFont (skin::font (10.5f));
    const float tw = GlyphArrangement::getStringWidth (g.getCurrentFont(), info) + 14.0f;
    g.setColour (Colours::black.withAlpha (0.65f));
    g.fillRoundedRectangle (in.getX() + 6, in.getY() + 6, tw, 17, 4);
    g.setColour (skin::red2);
    g.drawText (info, (int) in.getX() + 13, (int) in.getY() + 6, (int) tw, 17, Justification::left);
}

void FilterCurve::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    skin::drawInsetPanel (g, b, 5.0f);
    auto in = b.reduced (5.0f);
    g.setColour (Colour (0xff070707));
    g.fillRoundedRectangle (in, 3.0f);

    const auto s = proc.settings();
    constexpr double top = 15.0, bottom = -30.0;
    auto X = [&] (double hz) { return in.getX() + (float) (std::log (hz / 20.0) / std::log (1000.0)) * in.getWidth(); };
    auto Y = [&] (double db) { return in.getY() + (float) ((top - jlimit (bottom, top, db)) / (top - bottom)) * in.getHeight(); };
    g.setColour (Colours::white.withAlpha (0.07f));
    for (double hz : { 100.0, 1000.0, 10000.0 }) g.drawVerticalLine ((int) X (hz), in.getY(), in.getBottom());
    g.setColour (Colours::white.withAlpha (0.15f));
    g.drawHorizontalLine ((int) Y (0.0), in.getX(), in.getRight());

    // repeat 1 bright, repeats 2-4 fading: the filters act again on every trip
    for (int rep = 4; rep >= 1; --rep)
    {
        Path p;
        for (int x = 0; x <= (int) in.getWidth(); ++x)
        {
            const double hz = 20.0 * std::pow (1000.0, x / in.getWidth());
            const float y = Y (rep * 20.0 * std::log10 (std::max (1.0e-6, echoFilterGain (s, hz))));
            if (x == 0) p.startNewSubPath (in.getX() + x, y); else p.lineTo (in.getX() + x, y);
        }
        g.setColour (skin::red2.withAlpha (rep == 1 ? 1.0f : 0.45f / rep));
        g.strokePath (p, PathStrokeType (rep == 1 ? 1.6f : 1.0f));
    }
    g.setFont (skin::font (8.5f));
    g.setColour (skin::textDim);
    g.drawText ("REPEATS 1-4", in.reduced (4.0f, 2.0f), Justification::topRight);
}

//==============================================================================
TahiniEditor::TahiniEditor (TahiniProcessor& p)
    : EditorBase (p, "TAH", "INI", String::fromUTF8 ("SPACE DELAY + REVERB  \xc2\xb7  BY ASHER SWISSA"),
                  String::fromUTF8 ("TAHINI v1.1  \xc2\xb7  VST3 / AU  \xc2\xb7  EFFECT")),
      tp (p), display (p), curve (p)
{
    addAndMakeVisible (display);
    addAndMakeVisible (curve);
    const auto red = skin::red2, green = Colour (0xff22e3a5);
    modeRow = &addChoiceRow ("dmode");
    timeBox = &addCombo ("dtime");
    freeze = &addToggle ("freeze", "FREEZE");
    freeze->setTooltip ("Hold the reverb tail forever");
    dry = &addToggle ("dry", "DRY");
    dry->setTooltip ("Turn off to use TAHINI on a send (wet only)");

    fb = &addKnob ("fb", "FEEDBACK", red, "%", 66);

    // echo filters
    locut = &addKnob ("locut", "LOW CUT", red, "Hz", 40);
    hpres = &addKnob ("hpres", "RESO", red, "%", 40);
    hicut = &addKnob ("hicut", "HIGH CUT", red, "Hz", 40);
    lpres = &addKnob ("lpres", "RESO", red, "%", 40);
    locut->slider.setTooltip ("Low cut on the echoes, inside the loop: every repeat gets thinner");
    hicut->slider.setTooltip ("High cut on the echoes, inside the loop: every repeat gets darker");

    // drive
    const auto tapeCol = Colour (0xffffb000);
    distRow = &addChoiceRow ("dist", {}, tapeCol);
    drive = &addKnob ("ddrive", "DRIVE", tapeCol, "%", 56);
    drive->slider.setTooltip ("Distortion on the echoes, inside the loop: each pass grinds more. TAPE soft, TUBE warm and lopsided, FUZZ hard, CRUSH bits + sample rate");

    // pitch
    const auto pitchCol = Colour (0xffff2d7a);
    pitch = &addKnob ("pitch", "PITCH", pitchCol, "st", 56);
    pmix = &addKnob ("pmix", "BLEND", pitchCol, "%", 40);
    snap = &addToggle ("psnap", "SNAP");
    pitch->slider.setTooltip ("Every echo is shifted again: + spirals up, - spirals down (up to 2 octaves)");
    pmix->slider.setTooltip ("100% = full spiral. Lower keeps the unshifted echo too: at +12 the octaves stack up (shimmer)");
    snap->setTooltip ("On: whole semitones. Off: free pitch for detune smears");

    // tape FX
    wow = &addKnob ("wow", "WOW", tapeCol, "%", 32);
    age = &addKnob ("age", "AGE", tapeCol, "%", 32);
    reverse = &addKnob ("reverse", "REVERSE", Colour (0xffc084fc), "%", 32);
    tstop = &addKnob ("tstop", "TAPE STOP", Colour (0xff38bdf8), "%", 32);
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

    setSize (1000, 720);
}

void TahiniEditor::layout()
{
    display.setBounds (20, 68, 960, 196);

    panel ({ 20, 272, 960, 36 }, "TYPE");
    modeRow->setBounds (84, 279, 888, 22);

    // echo FX: everything here acts on the repeats, inside the feedback loop
    const int fy = 316, fh = 150;
    panel ({ 20, fy, 440, fh }, "ECHO FILTER");
    curve.setBounds (30, fy + 30, 180, 110);
    int kx = 216;
    for (auto* k : { locut, hpres, hicut, lpres })
    {
        put (*k, kx, fy + 46, 40, 60);
        kx += 60;
    }

    panel ({ 468, fy, 250, fh }, "ECHO DRIVE");
    distRow->setBounds (478, fy + 32, 230, 22);
    put (*drive, 548, fy + 60, 56, 90);

    panel ({ 726, fy, 254, fh }, "ECHO PITCH");
    put (*pitch, 740, fy + 40, 56, 84);
    put (*pmix, 832, fy + 46, 40, 64);
    snap->setBounds (906, fy + 70, 58, 22);

    const int y = 474, h = 196;
    panel ({ 20, y, 336, h }, "DELAY");
    label ({ 32, y + 40, 60, 12 }, "TIME");
    timeBox->setBounds (32, y + 54, 84, 24);
    put (*fb, 122, y + 40, 66, 90);
    label ({ 236, y + 8, 100, 12 }, "TAPE FX");
    put (*wow, 228, y + 30, 32, 56);
    put (*age, 290, y + 30, 32, 56);
    put (*reverse, 228, y + 110, 32, 56);
    put (*tstop, 290, y + 110, 32, 56);

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
