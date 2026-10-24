#include "PluginEditor.h"
#include <cstring>

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& text, bool big)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.getProperties().set ("big", big);
    slider.setColour (juce::Slider::textBoxOutlineColourId, skin::plateEdge);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, skin::screen);
    slider.setColour (juce::Slider::textBoxTextColourId, skin::lavaHot);
    addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setFont (skin::labelFont (big ? 14.0f : 13.0f));
    label.setColour (juce::Label::textColourId, skin::text);
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);

    // Show units after the attachment, which sets up the value text.
    if (auto* p = state.getParameter (paramId))
    {
        const auto unit = p->getLabel();
        slider.textFromValueFunction = [unit] (double v)
        {
            if (unit == "Hz" && v >= 1000.0)
                return juce::String (v / 1000.0, v >= 10000.0 ? 1 : 2) + " kHz";
            const auto num = std::abs (v) >= 100.0 ? juce::String ((int) std::round (v))
                                                   : juce::String (v, unit.isEmpty() ? 2 : 1);
            return unit.isEmpty() ? num : num + " " + unit;
        };
        slider.updateText();
    }
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (18));
    slider.setBounds (r);
}

//==============================================================================
KickPanel::KickPanel (KickassProcessor& p)
    : processor (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);

    knob ("start", "START");
    knob ("punch", "PUNCH");
    knob ("end", "END");
    knob ("snap", "SNAP");
    knob ("sweep", "SWEEP");
    knob ("sub", "SUB");
    knob ("hold", "HOLD");
    knob ("decay", "DECAY");
    knob ("click", "LEVEL");
    knob ("offset", "OFFSET");
    knob ("ctune", "TUNE");
    knob ("chpf", "HPF");
    knob ("clpf", "LPF");
    knob ("slength", "LENGTH");
    knob ("drive", "DRIVE");
    knob ("eqlow", "LOW");
    knob ("eqmid", "MID");
    knob ("eqmidf", "FREQ");
    knob ("eqhigh", "HIGH");
    knob ("level", "LEVEL", true);

    addAndMakeVisible (display);

    hitBtn.onClick = [this] { processor.triggerFromUi(); };
    hitBtn.setTooltip ("Play the kick");
    addAndMakeVisible (hitBtn);

    loadBtn.onClick = [this] { chooseClickSample(); };
    loadBtn.setTooltip ("Load a WAV or AIFF for the click layer (or drop one on the panel)");
    loadBtn.getProperties().set ("small", true);
    addAndMakeVisible (loadBtn);

    keyTrackBtn.setClickingTogglesState (true);
    keyTrackBtn.setTooltip ("Off: every key plays the same kick. On: the kick follows the key (C2 = as designed)");
    keyTrackBtn.getProperties().set ("small", true);
    addAndMakeVisible (keyTrackBtn);
    keyTrackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "keytrack", keyTrackBtn);

    saveBtn.onClick = [this] { savePresetAs(); };
    saveBtn.setTooltip ("Save the current kick as a user preset");
    saveBtn.getProperties().set ("small", true);
    addAndMakeVisible (saveBtn);

    keyboard.setAvailableRange (24, 96);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffd9d6d0));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff111114));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff6a6870));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, skin::lava.withAlpha (0.3f));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, skin::lava);
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha (0.5f));
    addAndMakeVisible (keyboard);

    setSize (designWidth, designHeight);
    startTimerHz (30);
}

KickPanel::~KickPanel()
{
    setLookAndFeel (nullptr);
}

Knob& KickPanel::knob (const juce::String& id, const juce::String& text, bool big)
{
    auto& k = knobs[id];
    k = std::make_unique<Knob> (processor.apvts, id, text, big);
    addAndMakeVisible (*k);
    return *k;
}

void KickPanel::resized()
{
    sections.clear();

    // Header
    logoArea   = { 22, 12, 380, 48 };
    presetArea = { 490, 20, 340, 32 };
    saveBtn.setBounds (838, 23, 64, 26);
    ledArea    = { 1100, 31, 10, 10 };
    hitBtn.setBounds (1122, 18, 142, 38);

    // Screen and the output column next to it
    display.setBounds (16, 80, 836, 300);
    sections.push_back ({ "EQ", { 864, 80, 192, 300 } });
    knobs["eqlow"]->setBounds (872, 108, 88, 132);
    knobs["eqhigh"]->setBounds (960, 108, 88, 132);
    knobs["eqmid"]->setBounds (872, 242, 88, 132);
    knobs["eqmidf"]->setBounds (960, 242, 88, 132);
    sections.push_back ({ "OUTPUT", { 1068, 80, 196, 300 } });
    knobs["level"]->setBounds (1080, 106, 132, 152);
    knobs["drive"]->setBounds (1094, 262, 104, 112);
    meterArea = { 1228, 116, 18, 248 };

    // Knob deck
    const int deckY = 392, deckH = 160;
    auto place = [&] (const juce::String& title, int x, int w, std::initializer_list<const char*> ids)
    {
        sections.push_back ({ title, { x, deckY, w, deckH } });
        const int itemW = (w - 16) / (int) ids.size();
        int ix = x + 8;
        for (auto* id : ids)
        {
            knobs[id]->setBounds (ix, deckY + 32, itemW, deckH - 40);
            ix += itemW;
        }
    };
    place ("PITCH", 16, 436, { "start", "punch", "end", "snap", "sweep" });
    keyTrackBtn.setBounds (352, deckY + 5, 92, 24);
    place ("BODY", 464, 268, { "sub", "hold", "decay" });
    place ("SAMPLE", 744, 520, { "click", "offset", "ctune", "chpf", "clpf", "slength" });
    sampleArea = { 826, deckY + 6, 360, 22 };
    loadBtn.setBounds (1194, deckY + 5, 62, 24);

    // Keyboard
    keyboardFrame = { 16, 566, 1248, 80 };
    keyboard.setBounds (keyboardFrame.reduced (4));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 43.0f); // 43 white keys from C1 to C7
}

void KickPanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Chassis with a faint brushed grain
    g.setGradientFill (juce::ColourGradient (skin::chassisTop, 0.0f, 0.0f, skin::chassisBot, 0.0f, bounds.getBottom(), false));
    g.fillAll();
    for (int y = 0; y < getHeight(); y += 3)
    {
        g.setColour (juce::Colours::white.withAlpha ((y / 3) % 2 == 0 ? 0.012f : 0.0f));
        g.drawHorizontalLine (y, 0.0f, bounds.getRight());
    }

    // Lava strip along the top edge
    g.setGradientFill (skin::lavaGradient (bounds.withHeight (4.0f)));
    g.fillRect (bounds.withHeight (4.0f));

    drawLogo (g, logoArea);

    // Preset LCD: < and > step, the middle opens the browser.
    skin::drawWell (g, presetArea.toFloat(), 5.0f);
    g.setFont (skin::valueFont (14.0f));
    g.setColour (skin::textSoft);
    g.drawText ("<", presetArea.withWidth (24), juce::Justification::centred);
    g.drawText (">", presetArea.withTrimmedLeft (presetArea.getWidth() - 24), juce::Justification::centred);
    g.setColour (skin::plateEdge);
    g.drawVerticalLine (presetArea.getX() + 24, (float) presetArea.getY() + 6.0f, (float) presetArea.getBottom() - 6.0f);
    g.drawVerticalLine (presetArea.getRight() - 25, (float) presetArea.getY() + 6.0f, (float) presetArea.getBottom() - 6.0f);
    g.setColour (skin::lavaHot);
    g.setFont (skin::valueFont (13.0f));
    g.drawFittedText (shownPresetName.toUpperCase(), presetArea.reduced (30, 0), juce::Justification::centred, 1);

    // Hit LED
    const auto led = ledArea.toFloat();
    if (flash > 0.02f)
    {
        g.setColour (skin::lava.withAlpha (0.4f * flash));
        g.fillEllipse (led.expanded (5.0f));
    }
    g.setColour (skin::lavaDeep.darker (0.6f).interpolatedWith (skin::lavaHot, flash));
    g.fillEllipse (led);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (led, 1.0f);

    // Section plates
    for (auto& s : sections)
    {
        skin::drawPlate (g, s.bounds.toFloat());
        skin::drawSectionTitle (g, s.bounds.reduced (12, 0).withHeight (28).translated (0, 2), s.title);
    }

    drawMeter (g, meterArea);

    // Sample browser: < and > step through the kick library and play each one.
    skin::drawWell (g, sampleArea.toFloat(), 4.0f);
    g.setFont (skin::valueFont (12.0f));
    g.setColour (skin::textSoft);
    g.drawText ("<", sampleArea.withWidth (22), juce::Justification::centred);
    g.drawText (">", sampleArea.withTrimmedLeft (sampleArea.getWidth() - 22), juce::Justification::centred);
    g.setColour (skin::plateEdge);
    g.drawVerticalLine (sampleArea.getX() + 22, (float) sampleArea.getY() + 4.0f, (float) sampleArea.getBottom() - 4.0f);
    g.drawVerticalLine (sampleArea.getRight() - 23, (float) sampleArea.getY() + 4.0f, (float) sampleArea.getBottom() - 4.0f);

    const auto name = processor.getClickSampleName();
    const auto text = sampleArea.reduced (28, 0);
    const int count = (int) libraryEntries.size();
    const auto status = library->isScanning() ? juce::String ("SCANNING...")
                                              : juce::String (count) + " KICKS";
    g.setFont (skin::valueFont (11.0f));
    g.setColour (skin::textSoft.withAlpha (0.8f));
    g.drawText (status, text, juce::Justification::centredRight);
    g.setFont (skin::valueFont (11.5f));
    g.setColour (name.isEmpty() ? skin::textSoft.withAlpha (0.7f) : skin::ice);
    g.drawFittedText (name.isEmpty() ? "NOISE  -  BROWSE OR DROP A WAV" : name.toUpperCase(),
                      text.withTrimmedRight (86), juce::Justification::centredLeft, 1);

    // Keyboard bezel
    skin::drawWell (g, keyboardFrame.toFloat(), 6.0f);

    if (dragHover)
    {
        g.setColour (skin::ice.withAlpha (0.08f));
        g.fillAll();
        g.setColour (skin::ice);
        g.drawRect (getLocalBounds(), 3);
    }
}

void KickPanel::mouseDown (const juce::MouseEvent& e)
{
    const auto pos = e.getPosition();
    if (sampleArea.contains (pos))
    {
        if (pos.x < sampleArea.getX() + 22)
            stepKick (-1);
        else if (pos.x >= sampleArea.getRight() - 22)
            stepKick (1);
        else
            showSampleMenu();
    }
    else if (presetArea.contains (pos))
    {
        if (pos.x < presetArea.getX() + 24)
            processor.stepPreset (-1);
        else if (pos.x >= presetArea.getRight() - 24)
            processor.stepPreset (1);
        else
            showPresetMenu();
    }
}

void KickPanel::showPresetMenu()
{
    static constexpr int userBase = 10000, saveId = 20000, revealId = 20001;
    const auto& bank = kick::factoryPresets();
    const int current = processor.getPresetIndex();

    // Factory kicks, one submenu per category in bank order.
    juce::PopupMenu menu;
    juce::String category;
    juce::PopupMenu sub;
    auto flush = [&]
    {
        if (category.isNotEmpty())
            menu.addSubMenu (category, sub);
        sub = {};
    };
    for (int i = 0; i < (int) bank.size(); ++i)
    {
        if (category != bank[(size_t) i].category)
        {
            flush();
            category = bank[(size_t) i].category;
        }
        sub.addItem (i + 1, bank[(size_t) i].name, true, i == current);
    }
    flush();

    const auto userFiles = KickassProcessor::getUserPresets();
    const auto currentFile = processor.getUserPresetFile();
    juce::PopupMenu user;
    for (int i = 0; i < userFiles.size(); ++i)
        user.addItem (userBase + i, userFiles[i].getFileNameWithoutExtension(), true,
                      current < 0 && userFiles[i] == currentFile);
    if (userFiles.isEmpty())
        user.addItem (-1, "(none saved yet)", false);
    menu.addSeparator();
    menu.addSubMenu ("User", user);
    menu.addSeparator();
    menu.addItem (saveId, "Save preset...");
    menu.addItem (revealId, "Show preset folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                                                  .withTargetScreenArea (localAreaToGlobal (presetArea)),
                        [this, userFiles] (int result)
                        {
                            if (result == saveId)
                                savePresetAs();
                            else if (result == revealId)
                            {
                                auto folder = KickassProcessor::getUserPresetFolder();
                                folder.createDirectory();
                                folder.revealToUser();
                            }
                            else if (result >= userBase && result - userBase < userFiles.size())
                                processor.loadUserPreset (userFiles[result - userBase]);
                            else if (result > 0 && result <= kick::numFactoryPresets())
                                processor.loadFactoryPreset (result - 1);
                        });
}

void KickPanel::savePresetAs()
{
    auto folder = KickassProcessor::getUserPresetFolder();
    folder.createDirectory();

    // Suggest the current name (minus its category) as a starting point.
    auto suggestion = processor.getPresetDisplayName().upToFirstOccurrenceOf (" *", false, false)
                                                     .fromLastOccurrenceOf (" / ", false, false);
    suggestion = juce::File::createLegalFileName (suggestion.isEmpty() ? "My Kick" : suggestion);

    fileChooser = std::make_unique<juce::FileChooser> ("Save preset", folder.getChildFile (suggestion + ".xml"), "*.xml");
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f != juce::File())
                                      processor.saveUserPreset (f.withFileExtension ("xml"));
                              });
}

void KickPanel::loadKick (const juce::File& file)
{
    // Load and play it straight away, so browsing is listening.
    if (processor.loadClickSample (file))
        processor.triggerFromUi();
    repaint (sampleArea);
}

void KickPanel::stepKick (int delta)
{
    const int n = (int) libraryEntries.size();
    if (n == 0)
        return;
    const int current = library->indexOf (processor.getClickSampleFile());
    const int next = current < 0 ? (delta > 0 ? 0 : n - 1) : ((current + delta) % n + n) % n;
    loadKick (libraryEntries[(size_t) next].file);
}

void KickPanel::showSampleMenu()
{
    const auto current = processor.getClickSampleFile();

    // One submenu per pack, in library order.
    juce::String currentPack;
    for (const auto& e : libraryEntries)
        if (e.file == current)
            currentPack = e.pack;

    juce::PopupMenu menu, packs;
    juce::String pack;
    juce::PopupMenu sub;
    auto flush = [&]
    {
        if (pack.isNotEmpty())
            packs.addSubMenu (pack, sub, true, nullptr, pack == currentPack);
        sub = {};
    };
    for (const auto& e : libraryEntries)
    {
        if (e.pack != pack)
        {
            flush();
            pack = e.pack;
        }
        sub.addItem (e.file.getFileNameWithoutExtension(), true, e.file == current, [this, f = e.file] { loadKick (f); });
    }
    flush();

    const auto folder = library->getFolder();
    if (libraryEntries.empty())
        menu.addItem (library->isScanning() ? "Scanning for kicks..." : "No kicks found in this folder", false, false, nullptr);
    else
    {
        menu.addSubMenu ("Kicks (" + juce::String ((int) libraryEntries.size()) + ")", packs);
        menu.addItem ("Random kick", [this]
        {
            const auto n = (int) libraryEntries.size();
            if (n > 0)
                loadKick (libraryEntries[(size_t) juce::Random::getSystemRandom().nextInt (n)].file);
        });
    }
    const bool hasSample = processor.getClickSample() != nullptr;
    menu.addSeparator();
    menu.addItem ("Full sample kick (sub off, no filters)", hasSample, false, [this]
    {
        processor.useSampleAsFullKick();
        processor.triggerFromUi();
    });
    menu.addItem ("Sample top + KICKASS sub (HPF 150 Hz)", hasSample, false, [this]
    {
        processor.useSampleAsTopLayer();
        processor.triggerFromUi();
    });
    menu.addSeparator();
    menu.addItem ("Load file...", [this] { chooseClickSample(); });
    menu.addItem ("Use noise click", processor.getClickSample() != nullptr, false, [this]
    {
        processor.clearClickSample();
        repaint();
    });
    menu.addSeparator();
    menu.addItem ("Kick folder: " + folder.getFullPathName(), false, false, nullptr);
    menu.addItem ("Change kick folder...", [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Folder with your kick samples", library->getFolder());
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                  [this] (const juce::FileChooser& fc)
                                  {
                                      if (fc.getResult().isDirectory())
                                          library->setFolder (fc.getResult());
                                  });
    });
    menu.addItem ("Rescan", [this] { library->rescan(); repaint (sampleArea); });

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                                                  .withTargetScreenArea (localAreaToGlobal (sampleArea)));
}

void KickPanel::chooseClickSample()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Load a kick or click sample", library->getFolder(), "*.wav;*.aif;*.aiff;*.flac");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (fc.getResult().existsAsFile())
                                      processor.loadClickSample (fc.getResult());
                                  repaint();
                              });
}

bool KickPanel::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1 && juce::File (files[0]).hasFileExtension ("wav;aif;aiff;flac");
}

void KickPanel::filesDropped (const juce::StringArray& files, int, int)
{
    dragHover = false;
    processor.loadClickSample (juce::File (files[0]));
    repaint();
}

void KickPanel::drawLogo (juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto r = area.toFloat();
    const auto font = skin::logoFont (40.0f);

    juce::GlyphArrangement kickText, assText;
    kickText.addLineOfText (font, "KICK", r.getX(), r.getY() + 36.0f);
    const float kickW = kickText.getBoundingBox (0, -1, true).getWidth();
    assText.addLineOfText (font, "ASS", r.getX() + kickW + 2.0f, r.getY() + 36.0f);

    juce::Path kickPath, assPath;
    kickText.createPath (kickPath);
    assText.createPath (assPath);

    // Lean the word forward for speed.
    const auto lean = juce::AffineTransform::shear (-0.18f, 0.0f).translated (0.18f * (r.getY() + 36.0f), 0.0f);
    kickPath.applyTransform (lean);
    assPath.applyTransform (lean);

    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillPath (kickPath, juce::AffineTransform::translation (0.0f, 3.0f));
    g.fillPath (assPath, juce::AffineTransform::translation (0.0f, 3.0f));

    g.setColour (skin::text);
    g.fillPath (kickPath);
    g.setGradientFill (skin::lavaGradient (assPath.getBounds(), true));
    g.fillPath (assPath);

    // Byline: a lava divider, a small "BY", then the name big and bright on the logo's baseline.
    const float logoRight = assPath.getBounds().getRight();
    const float baseline = r.getY() + 36.0f;
    g.setGradientFill (skin::lavaGradient ({ logoRight + 12.0f, r.getY() + 8.0f, 3.0f, 30.0f }, true));
    g.fillRect (logoRight + 12.0f, r.getY() + 8.0f, 3.0f, 30.0f);

    g.setColour (skin::textSoft);
    g.setFont (skin::labelFont (13.0f));
    g.drawText ("BY", juce::Rectangle<float> (logoRight + 24.0f, baseline - 14.0f, 22.0f, 14.0f),
                juce::Justification::bottomLeft);

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.setFont (skin::labelFont (24.0f));
    const juce::Rectangle<float> nameBox (logoRight + 48.0f, baseline - 24.0f, 220.0f, 26.0f);
    g.drawText ("ASHER SWISSA", nameBox.translated (0.0f, 2.0f), juce::Justification::bottomLeft);
    g.setColour (skin::text);
    g.drawText ("ASHER SWISSA", nameBox, juce::Justification::bottomLeft);
}

void KickPanel::drawMeter (juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto r = area.toFloat();
    skin::drawWell (g, r, 3.0f);

    // -48..+6 dB, 27 segments of 2 dB.
    const float db = juce::Decibels::gainToDecibels (meterLevel, -60.0f);
    const int numSegs = 27;
    const auto inner = r.reduced (3.0f);
    const float segH = inner.getHeight() / (float) numSegs;
    for (int i = 0; i < numSegs; ++i)
    {
        const float segDb = -48.0f + 2.0f * (float) i;
        const auto seg = juce::Rectangle<float> (inner.getX(), inner.getBottom() - segH * (float) (i + 1),
                                                 inner.getWidth(), segH - 1.5f);
        const auto col = segDb >= 0.0f ? juce::Colour (0xffff2020)
                       : skin::lavaDeep.interpolatedWith (skin::lavaHot, (float) i / (float) numSegs);
        g.setColour (db > segDb ? col : col.withAlpha (0.08f));
        g.fillRect (seg);
    }

    g.setFont (skin::valueFont (9.0f));
    g.setColour (skin::textSoft.withAlpha (0.7f));
    for (int mark : { 0, -12, -24, -36, -48 })
    {
        const float y = inner.getBottom() - inner.getHeight() * (float) (mark + 48) / 54.0f;
        g.drawText (juce::String (mark), juce::Rectangle<float> (r.getX() - 30.0f, y - 6.0f, 26.0f, 12.0f),
                    juce::Justification::centredRight);
    }
}

void KickPanel::timerCallback()
{
    // Redraw the kick only when a knob actually moved.
    const auto p = processor.currentParams();
    const auto sample = processor.getClickSample();
    if (firstRender || sample != shownSample || std::memcmp (&p, &shownParams, sizeof (p)) != 0)
    {
        if (sample != shownSample)
            repaint (sampleArea);
        shownParams = p;
        shownSample = sample;
        firstRender = false;
        display.setKick (p, sample);
    }

    // Pick up a finished library scan.
    if (library->getVersion() != shownLibraryVersion || library->isScanning() != shownScanning)
    {
        shownLibraryVersion = library->getVersion();
        shownScanning = library->isScanning();
        libraryEntries = library->getEntries();
        repaint (sampleArea);
    }

    const auto presetName = processor.getPresetDisplayName();
    if (presetName != shownPresetName)
    {
        shownPresetName = presetName;
        repaint (presetArea);
    }

    const int hits = processor.getHitCount();
    const float newFlash = hits != lastHitCount ? 1.0f : flash * 0.82f;
    lastHitCount = hits;
    if (std::abs (newFlash - flash) > 0.005f)
    {
        flash = newFlash < 0.02f ? 0.0f : newFlash;
        display.setFlash (flash);
        repaint (ledArea.expanded (6));
    }

    const float newLevel = juce::jmax (processor.takePeak(), meterLevel * 0.86f);
    if (std::abs (newLevel - meterLevel) > 0.0005f)
    {
        meterLevel = newLevel;
        repaint (meterArea);
    }
}

//==============================================================================
KickassEditor::KickassEditor (KickassProcessor& p)
    : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (KickPanel::designWidth / 2, KickPanel::designHeight / 2,
                     KickPanel::designWidth * 2, KickPanel::designHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) KickPanel::designWidth / KickPanel::designHeight);
    setSize (KickPanel::designWidth, KickPanel::designHeight);
}

void KickassEditor::resized()
{
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) KickPanel::designWidth));
}
