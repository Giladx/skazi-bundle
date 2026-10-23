#pragma once

#include "PluginProcessor.h"
#include "../../Common/SkaziUI.h"

// Drive curve (input -> output) with the live input level as a dot
class CurveView : public juce::Component
{
public:
    explicit CurveView (ZhugProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
private:
    ZhugProcessor& proc;
};

// The three bands: how hard each is driven, and how loud each is right now
class BandView : public juce::Component
{
public:
    explicit BandView (ZhugProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
private:
    ZhugProcessor& proc;
};

// Stereo vectorscope: a vertical line is mono, wider is wider
class StereoScope : public juce::Component
{
public:
    explicit StereoScope (ZhugProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
private:
    ZhugProcessor& proc;
};

class ZhugEditor : public skazi::EditorBase
{
public:
    explicit ZhugEditor (ZhugProcessor&);

protected:
    void layout() override;
    void tick() override;

private:
    ZhugProcessor& zp;
    CurveView curve;
    BandView bands;
    StereoScope scope;
    skazi::ChoiceRow* modeRow;
    skazi::ParamToggle *autoGain, *mb;
    skazi::Knob *drive, *tone, *bias, *lowd, *midd, *highd, *xlow, *xhigh, *width, *bassmono, *haas, *mix, *gain;
    skazi::Meter *inMeter, *outMeter;
};
