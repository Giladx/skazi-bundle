#pragma once

#include "PluginProcessor.h"
#include "../../Common/SkaziUI.h"

// The last 16 chop steps, coloured by what happened, over a scrolling picture of the output
class ChopView : public juce::Component
{
public:
    explicit ChopView (SalatProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
private:
    SalatProcessor& proc;
};

// Filter response at the cutoff the LFO has it at right now, with the LFO's sweep shaded
class FilterView : public juce::Component
{
public:
    explicit FilterView (SalatProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
private:
    SalatProcessor& proc;
};

// How far the RISE has got: a filling bar with the current pitch and bar count
class RiseView : public juce::Component
{
public:
    explicit RiseView (SalatProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
private:
    SalatProcessor& proc;
};

// MAKE A SALAT
class SalatButton : public juce::Button
{
public:
    SalatButton() : juce::Button ("MAKE A SALAT") {}
    void paintButton (juce::Graphics& g, bool over, bool down) override;
    float flash = 0.0f;
};

class SalatEditor : public skazi::EditorBase
{
public:
    explicit SalatEditor (SalatProcessor&);

protected:
    void layout() override;
    void tick() override;

private:
    SalatProcessor& sp;
    ChopView chopView;
    FilterView filterView;
    RiseView riseView;
    SalatButton salatBtn;
    juce::TextButton holdBtn { "HOLD STUTTER" };
    bool holdDown = false;

    skazi::ChoiceRow *posRow, *stepRow, *repRow, *driveRow, *filterRow, *modRow, *riseLenRow;
    skazi::ParamToggle *chopOn, *driveOn, *crushOn, *filterOn, *ringOn, *modOn, *delayOn, *reverbOn, *riseOn;
    skazi::Knob* riseAmt;
    juce::TextButton riseQuick[3] { juce::TextButton ("12"), juce::TextButton ("24"), juce::TextButton ("48") };
    skazi::Knob *chance, *wStut, *wRev, *wGate, *wTape, *drive, *bits, *down, *cutoff, *reso, *lfoDepth;
    skazi::Knob *ringFreq, *ringMix, *modRate, *modDepth, *modFb, *modMix, *dlyFb, *dlyTone, *dlyMix;
    skazi::Knob *revSize, *revDamp, *revMix, *inGain, *mix, *outGain;
    juce::ComboBox *lfoRate, *lfoShape, *dlyTime;
    skazi::Meter *outMeter, *grMeter;
};
