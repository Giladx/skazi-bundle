#pragma once

#include "PluginProcessor.h"
#include "../../Common/SkaziUI.h"

// Echo taps (L up, R down), the reverb tail, and a scrolling trace of the wet signal
class TahiniDisplay : public juce::Component
{
public:
    explicit TahiniDisplay (TahiniProcessor& p) : proc (p) { history.assign (240, 0.0f); }
    void paint (juce::Graphics& g) override;
    void push (float v) { history.erase (history.begin()); history.push_back (v); }
private:
    TahiniProcessor& proc;
    std::vector<float> history;
};

// Echo filter response: the first repeat bright, later repeats fainter as the filters pile up
class FilterCurve : public juce::Component
{
public:
    explicit FilterCurve (TahiniProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
private:
    TahiniProcessor& proc;
};

class TahiniEditor : public skazi::EditorBase
{
public:
    explicit TahiniEditor (TahiniProcessor&);

protected:
    void layout() override;
    void tick() override;

private:
    TahiniProcessor& tp;
    TahiniDisplay display;
    FilterCurve curve;
    skazi::ChoiceRow *modeRow, *distRow;
    juce::ComboBox* timeBox;
    skazi::ParamToggle *freeze, *dry, *snap;
    skazi::Knob *fb, *locut, *hicut, *wow, *drive, *size, *decay, *predelay, *damp, *width, *duck, *duckrel, *dmix, *rmix, *gain;
    skazi::Knob *pitch, *reverse, *tstop, *age, *hpres, *lpres, *pmix;
    skazi::Meter *outMeter, *duckMeter;
};
