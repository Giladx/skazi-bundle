#pragma once

#include "PluginProcessor.h"
#include "../../Common/SkaziUI.h"

// The effect's loudness (red), filter (green) and pitch (white) across its bars, with a playhead,
// and the Splice sample's waveform where it lands in the effect
class BambaDisplay : public juce::Component
{
public:
    explicit BambaDisplay (BambaProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
private:
    BambaProcessor& proc;
};

// Buttons in the ChoiceRow style for something that isn't a parameter (the browser's filters)
class Chips : public juce::Component
{
public:
    explicit Chips (juce::StringArray l) : labels (std::move (l)) {}
    void setSelected (int i) { if (i != selected) { selected = i; repaint(); } }
    std::function<void (int)> onPick;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    juce::StringArray labels;
    int selected = 0;
};

// < sample name - kind - length   N FOUND >: the arrows load and play the next riser, the middle opens a menu
class SampleBrowser : public juce::Component
{
public:
    explicit SampleBrowser (BambaProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh (bool scanning);
private:
    void showMenu();
    BambaProcessor& proc;
    std::vector<RiserLibrary::Entry> list; // the filtered list as last shown
    bool scanning = false;
    std::unique_ptr<juce::FileChooser> chooser;
};

class BambaEditor : public skazi::EditorBase
{
public:
    explicit BambaEditor (BambaProcessor&);

protected:
    void layout() override;
    void tick() override;

private:
    BambaProcessor& bp;
    BambaDisplay display;
    SampleBrowser browser;
    Chips kindChips { { "ALL", "RISE", "DOWN", "SWEEP", "IMPACT", "REVERSE" } }, lengthChips { { "ANY", "SHORT", "LONG" } };
    juce::TextButton playBtn { "PLAY" };
    skazi::ChoiceRow *typeRow, *triggerRow;
    juce::ComboBox *barsBox, *fitBox, *alignBox;
    skazi::ParamToggle* revBtn;
    skazi::Knob *curve, *offset, *noise, *color, *tone, *pfrom, *pto, *sweep, *reso, *roll, *width, *phaser, *reverb, *delay, *gain;
    skazi::Knob *smix, *synth, *stune, *sshape;
    skazi::Meter* outMeter;
    int shownVersion = -1, shownKind = -2, shownLength = -1;
    bool shownScanning = false;
    juce::File shownFile;
};
