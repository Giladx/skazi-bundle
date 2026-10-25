#pragma once

#include "PluginProcessor.h"

/**
    The 16-step editor: a small piano roll per step over rows of LED toggles
    (octave, gate, accent, slide) and a row of 303-style step keys that light
    up under the playhead. Everything reads and writes the step parameters.
*/
class StepGrid : public juce::Component, private juce::Timer
{
public:
    explicit StepGrid (Skazi303Processor&);
    ~StepGrid() override { stopTimer(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    enum class Row { Roll, Name, Octave, Gate, Accent, Slide, Key, None };

    void timerCallback() override;
    juce::Rectangle<float> cell (int step, Row row) const;
    std::pair<int, Row> hitTest (juce::Point<float>) const;
    int noteAt (int step, float y) const;
    void setStepValue (int step, const char* field, float value);

    Skazi303Processor& processor;
    s303::Pattern shown;
    int shownStep = -1;
    int dragStep = -1;

    static constexpr float labelW = 52.0f;
    static constexpr float rollH = 146.0f;
    static constexpr float rowH = 18.0f;
    static constexpr float keyH = 48.0f;
    static constexpr float gap = 4.0f;
};
