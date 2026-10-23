#pragma once

#include "PluginProcessor.h"
#include "../../Common/SkaziUI.h"

// The effect's loudness (red), filter (green) and pitch (white) across its bars, with a playhead
class BambaDisplay : public juce::Component
{
public:
    explicit BambaDisplay (BambaProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
private:
    BambaProcessor& proc;
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
    skazi::ChoiceRow *typeRow, *triggerRow;
    juce::ComboBox* barsBox;
    skazi::Knob *curve, *offset, *noise, *color, *tone, *pfrom, *pto, *sweep, *reso, *roll, *width, *phaser, *reverb, *delay, *gain;
    skazi::Meter* outMeter;
};
