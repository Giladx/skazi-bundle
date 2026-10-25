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

    // the Splice sample, drawn where it plays inside the effect
    const auto smp = proc.getSample();
    if (smp != nullptr && s.sampleLevel > 0.0f && ! smp->overview.empty())
    {
        const double secs = s.sampleFit == FitOriginal ? smp->seconds() / std::exp2 (s.sampleTune / 12.0) : smp->seconds();
        const auto place = placeSample (s, secs, lenSec);
        const float x0 = in.getX() + (float) (place.start / lenSec) * in.getWidth();
        const float pw = (float) (place.length / lenSec) * in.getWidth();
        const float mid = in.getCentreY() + 4.0f, half = in.getHeight() * 0.36f;
        const int n = (int) smp->overview.size();
        Path wave;
        for (int i = 0; i < n; ++i)
        {
            const float x = x0 + pw * i / (n - 1);
            const float pk = smp->overview[(size_t) (s.sampleReverse ? n - 1 - i : i)] * half;
            if (x < in.getX() || x > in.getRight()) continue;
            wave.addRectangle (x, mid - pk, juce::jmax (1.0f, pw / n), pk * 2.0f);
        }
        g.setColour (Colour (0xff6fc8ff).withAlpha (0.10f + 0.22f * s.sampleLevel));
        g.fillPath (wave);
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
            double phase;
            if (s.rollRate == RollFree)
            {
                const float rise = std::pow (p, std::pow (2.0f, (s.curve - 0.5f) * 4.0f));
                rollPh += 2.0 * std::pow (4.0, (double) rise) * beats / (double) w;
                phase = rollPh - std::floor (rollPh);
            }
            else
            {
                const double hits = p * beats * rollHitsPerBeat (s.rollRate, p);
                phase = hits - std::floor (hits);
            }
            const double k = 6.0 * std::pow (5.0, (s.rollDecay - 0.5) * 2.0);
            a *= 1.0f - rollAmt * (1.0f - (float) std::exp (-phase * k));
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
    if (smp != nullptr)
    {
        g.setColour (Colour (0xff6fc8ff));
        g.drawText ("SAMPLE", lx - 60, ly, 54, 12, Justification::left);
    }
}

//==============================================================================
void Chips::paint (Graphics& g)
{
    const int n = labels.size();
    const float gap = 3.0f, w = (getWidth() - gap * (n - 1)) / n;
    for (int i = 0; i < n; ++i)
    {
        auto r = Rectangle<float> (i * (w + gap), 0, w, (float) getHeight()).reduced (0.5f);
        const bool on = i == selected;
        g.setColour (on ? Colour (0xff10161e) : skin::panel2);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (on ? Colour (0xff6fc8ff) : skin::line);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        g.setColour (on ? skin::white : skin::textDim);
        g.setFont (skin::font (jmin (10.0f, r.getHeight() * 0.45f)));
        g.drawFittedText (labels[i], r.toNearestInt().reduced (2, 0), Justification::centred, 1);
    }
}

void Chips::mouseUp (const MouseEvent& e)
{
    if (labels.isEmpty() || ! getLocalBounds().contains (e.getPosition())) return;
    const int i = jlimit (0, labels.size() - 1, e.x * labels.size() / jmax (1, getWidth()));
    setSelected (i);
    if (onPick) onPick (i);
}

//==============================================================================
void SampleBrowser::refresh (bool isScanning)
{
    list = proc.browseList();
    scanning = isScanning;
    repaint();
}

void SampleBrowser::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Colour (0xff070707));
    g.fillRoundedRectangle (b, 4.0f);
    g.setColour (skin::line);
    g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);
    g.drawVerticalLine (24, 4.0f, b.getBottom() - 4.0f);
    g.drawVerticalLine (getWidth() - 25, 4.0f, b.getBottom() - 4.0f);
    g.setColour (skin::white);
    g.setFont (skin::font (13.0f));
    g.drawText ("<", 0, 0, 24, getHeight(), Justification::centred);
    g.drawText (">", getWidth() - 24, 0, 24, getHeight(), Justification::centred);

    auto text = getLocalBounds().reduced (32, 0);
    const String count = scanning ? String ("SCANNING...") : String ((int) list.size()) + " FOUND";
    g.setFont (skin::font (9.5f));
    g.setColour (skin::textDim);
    g.drawText (count, text, Justification::centredRight);

    const auto file = proc.getSampleFile();
    String name = "SYNTH ONLY  -  PICK A SPLICE RISER WITH < >", info;
    if (proc.getSample() != nullptr && file != File())
    {
        name = file.getFileNameWithoutExtension();
        for (auto& e : proc.library->getEntries())
            if (e.file == file)
                info = String (RiserLibrary::kindName (e.kind)) + String::fromUTF8 ("  \xc2\xb7  ") + String (e.seconds, 1) + " s";
        if (info.isEmpty())
            info = String (proc.getSample()->seconds(), 1) + " s";
    }
    text = text.withTrimmedRight (78);
    if (info.isNotEmpty())
    {
        g.setColour (Colour (0xff6fc8ff));
        g.drawText (info, text, Justification::centredRight);
        text = text.withTrimmedRight ((int) GlyphArrangement::getStringWidth (g.getCurrentFont(), info) + 10);
    }
    g.setFont (skin::font (11.0f));
    g.setColour (proc.getSample() != nullptr ? skin::white : skin::textDim);
    g.drawFittedText (name.toUpperCase(), text, Justification::centredLeft, 1);
}

void SampleBrowser::mouseDown (const MouseEvent& e)
{
    if (e.x < 24)                    proc.stepSample (-1);
    else if (e.x >= getWidth() - 24) proc.stepSample (1);
    else                             showMenu();
    repaint();
}

void SampleBrowser::showMenu()
{
    const auto current = proc.getSampleFile();

    // One submenu per pack, shortest sample first inside each
    PopupMenu menu, packs;
    StringArray packNames;
    for (auto& e : list) packNames.addIfNotAlreadyThere (e.pack);
    packNames.sortNatural();
    for (auto& pack : packNames)
    {
        PopupMenu sub;
        bool here = false;
        for (auto& e : list)
            if (e.pack == pack)
            {
                here = here || e.file == current;
                sub.addItem (e.file.getFileNameWithoutExtension() + "   " + String (e.seconds, 1) + " s", true, e.file == current,
                             [this, f = e.file] { if (proc.loadSample (f)) proc.audition(); repaint(); });
            }
        packs.addSubMenu (pack, sub, true, nullptr, here);
    }

    const auto folder = proc.library->getFolder();
    if (list.empty())
        menu.addItem (scanning ? "Scanning for risers..." : "Nothing here: try ALL / ANY, or another folder", false, false, nullptr);
    else
    {
        menu.addSubMenu ("Splice risers (" + String ((int) list.size()) + ")", packs);
        menu.addItem ("Random one", [this]
        {
            if (! list.empty() && proc.loadSample (list[(size_t) Random::getSystemRandom().nextInt ((int) list.size())].file))
                proc.audition();
            repaint();
        });
    }
    menu.addSeparator();
    menu.addItem ("Load file...", [this]
    {
        chooser = std::make_unique<FileChooser> ("Load a riser, sweep or impact", proc.library->getFolder(), "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
        {
            if (fc.getResult().existsAsFile() && proc.loadSample (fc.getResult()))
                proc.audition();
            repaint();
        });
    });
    menu.addItem ("Synth only (remove sample)", proc.getSample() != nullptr, false, [this] { proc.clearSample(); repaint(); });
    menu.addSeparator();
    menu.addItem ("Folder: " + folder.getFullPathName(), false, false, nullptr);
    menu.addItem ("Change folder...", [this]
    {
        chooser = std::make_unique<FileChooser> ("Folder with your Splice samples", proc.library->getFolder());
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories, [this] (const FileChooser& fc)
        {
            if (fc.getResult().isDirectory())
                proc.library->setFolder (fc.getResult());
        });
    });
    menu.addItem ("Rescan", [this] { proc.library->rescan(); });
    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (this));
}

//==============================================================================
BambaEditor::BambaEditor (BambaProcessor& p)
    : EditorBase (p, "BAM", "BA", String::fromUTF8 ("RISER & FX MAKER  \xc2\xb7  BY ASHER SWISSA"),
                  String::fromUTF8 ("BAMBA v1.1  \xc2\xb7  VST3 / AU  \xc2\xb7  INSTRUMENT")),
      bp (p), display (p), browser (p)
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
    rateRow = &addChoiceRow ("rrate");
    rdecay = &addKnob ("rdecay", "DECAY", red, "%", 34);
    roll->slider.setTooltip ("Roll gate amount (SNARE ROLL always rolls fully)");
    rdecay->slider.setTooltip ("How short each roll hit is: low = smooth pulses, high = tight snare hits");

    // Splice layer
    const auto blue = Colour (0xff6fc8ff);
    smix = &addKnob ("smix", "SAMPLE", blue, "%", 34);
    synth = &addKnob ("synth", "SYNTH", red, "%", 34);
    synfx = &addKnob ("synfx", "SYNTH FX", green, "%", 34);
    smpfx = &addKnob ("smpfx", "SAMPLE FX", green, "%", 34);
    synfx->slider.setTooltip ("How much of the synth goes into the delay and reverb");
    smpfx->slider.setTooltip ("How much of the Splice sample goes into the delay and reverb");
    stune = &addKnob ("stune", "TUNE", blue, "st", 30);
    sshape = &addKnob ("sshape", "FOLLOW", blue, "%", 30);
    fitBox = &addCombo ("sfit");
    alignBox = &addCombo ("salign");
    revBtn = &addToggle ("srev", "REVERSE");
    addAndMakeVisible (browser);
    addAndMakeVisible (kindChips);
    addAndMakeVisible (lengthChips);
    addAndMakeVisible (playBtn);
    kindChips.onPick = [this] (int i) { bp.setBrowseFilter (i - 1, bp.getBrowseLength()); };
    lengthChips.onPick = [this] (int i) { bp.setBrowseFilter (bp.getBrowseKind(), i); };
    playBtn.onClick = [this] { bp.audition(); };
    playBtn.setTooltip ("Play the whole effect once (C3)");
    sshape->slider.setTooltip ("How much the sample follows BAMBA's own level and filter sweep");
    fitBox->setTooltip ("ORIGINAL keeps the sample's speed, STRETCH fits it to the bars at the same pitch, VARISPEED fits it like tape");
    alignBox->setTooltip ("ORIGINAL only: start with the note, or time it to end exactly on the drop");
    stune->slider.setTooltip ("Sample pitch in semitones (not in VARISPEED)");

    curve->slider.setTooltip ("How the effect builds: low = fast start, high = holds back then explodes");
    offset->slider.setTooltip ("SYNC mode: which bar the effect starts on");
    roll->slider.setTooltip ("Stutter gate that speeds up from 1/8 to 1/32 as it builds");

    setSize (1000, 744);
}

void BambaEditor::layout()
{
    display.setBounds (20, 68, 960, 204);
    typeRow->setBounds (20, 280, 960, 38);

    // Splice layer strip
    {
        const int sy = 326;
        panel ({ 20, sy, 960, 88 }, "SPLICE LAYER");
        playBtn.setBounds (32, sy + 34, 96, 22);
        label ({ 32, sy + 62, 100, 12 }, "YOUR RISERS");
        browser.setBounds (140, sy + 8, 500, 28);
        kindChips.setBounds (140, sy + 44, 330, 22);
        lengthChips.setBounds (478, sy + 44, 162, 22);
        put (*stune, 680, sy + 8, 30, 52);
        put (*sshape, 760, sy + 8, 30, 52);
        fitBox->setBounds (858, sy + 8, 112, 22);
        alignBox->setBounds (858, sy + 34, 112, 22);
        revBtn->setBounds (858, sy + 60, 112, 20);
    }

    // Roll with beat quantize, and the mixer between the sources and the FX
    {
        const int ry = 422;
        panel ({ 20, ry, 560, 96 }, "ROLL + QUANTIZE");
        label ({ 32, ry + 30, 300, 12 }, "FREE = SMOOTH SPEED-UP   BUILD = 1/4 > 1/8 > 1/16 > 1/32");
        rateRow->setBounds (32, ry + 48, 420, 34);
        put (*roll, 456, ry + 26, 34, 52);
        put (*rdecay, 514, ry + 26, 34, 52);

        panel ({ 588, ry, 392, 96 }, "MIXER");
        put (*synth, 600, ry + 26, 34, 80);
        put (*smix, 690, ry + 26, 34, 80);
        put (*synfx, 790, ry + 26, 34, 80);
        put (*smpfx, 880, ry + 26, 34, 80);
    }

    const int y = 526, h = 176;
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
    put (*phaser, 696, y + 32, 34, 70);
    put (*width, 660, y + 104, 34, 70);

    panel ({ 784, y, 196, h }, "SPACE + OUT");
    put (*reverb, 790, y + 32, 34, 70);
    put (*delay, 862, y + 32, 34, 70);
    put (*gain, 826, y + 104, 34, 70);
    outMeter->setBounds (950, y + 34, 12, 128);
}

void BambaEditor::tick()
{
    display.repaint();

    // Pick up a finished scan, a new filter or a new sample.
    const bool scanning = bp.library->isScanning();
    const int ver = bp.library->getVersion(), kind = bp.getBrowseKind(), len = bp.getBrowseLength();
    if (ver != shownVersion || scanning != shownScanning || kind != shownKind || len != shownLength)
    {
        shownVersion = ver; shownScanning = scanning; shownKind = kind; shownLength = len;
        kindChips.setSelected (kind + 1);
        lengthChips.setSelected (len);
        browser.refresh (scanning);
    }
    alignBox->setEnabled (bp.settings().sampleFit == FitOriginal); // STRETCH and VARISPEED always fill the bars
    if (bp.getSampleFile() != shownFile)
    {
        shownFile = bp.getSampleFile();
        browser.repaint();
    }
    offset->setVisible (false); // BAMBA now starts on notes only
}
