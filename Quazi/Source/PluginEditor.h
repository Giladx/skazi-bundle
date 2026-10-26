#pragma once

#include "PluginProcessor.h"
#include "../../Common/SkaziUI.h"

// The four bands across the spectrum, lit by how hard each one is working, with the post EQ curve on top.
// Drag a crossover line to move it.
class BandView : public juce::Component
{
public:
    explicit BandView (QuaziProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent&) override { setHover (-1); }
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> plotArea() const;
    float xFor (double freq) const;
    double freqAt (float x) const;
    int splitNear (float x) const;
    void setHover (int h) { if (h != hover) { hover = h; repaint(); } }

    QuaziProcessor& proc;
    int hover = -1, dragging = -1;
};

class QuaziEditor : public skazi::EditorBase
{
public:
    explicit QuaziEditor (QuaziProcessor&);

protected:
    void layout() override;
    void tick() override;

private:
    QuaziProcessor& qp;
    BandView bandView;

    struct BandControls
    {
        skazi::ParamToggle* on = nullptr;
        skazi::ChoiceRow* mode = nullptr;
        skazi::Knob *drive = nullptr, *level = nullptr;
    };
    std::array<BandControls, quazi::NumBands> bands;
    skazi::Knob *xLo, *xMid, *xHi, *width, *lowDb, *lowF, *midDb, *midF, *midQ, *highDb, *highF, *inGain, *mix, *outGain;
    skazi::ParamToggle* monoBass;
    skazi::Meter *inMeter, *outMeter;
};
