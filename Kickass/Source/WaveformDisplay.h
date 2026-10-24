#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include "KickEngine.h"

/** The main screen: the kick rendered offline, the SUB layer filled in lava with
    the CLICK layer drawn on top in ice, and the pitch envelope on a log-frequency
    scale. FULL shows the whole hit; ATTACK zooms in to line the click up with the
    sub. Message thread only. */
class WaveformDisplay : public juce::Component
{
public:
    static constexpr double renderRate = 44100.0;
    static constexpr double renderMs = 600.0;

    /** Re-renders the kick from these settings and redraws. */
    void setKick (const kick::Params&, kick::ClickSamplePtr);

    /** 0..1 brightness boost after a hit; the editor decays it. */
    void setFlash (float amount);

    /** FULL (false) or ATTACK (true) view. */
    void setZoomed (bool shouldZoom) { zoomed = shouldZoom; rebuildPaths(); repaint(); }

    void paint (juce::Graphics&) override;
    void resized() override { rebuildPaths(); }
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> plotArea() const;
    juce::Rectangle<float> tabArea (int index) const;
    double viewMs() const { return zoomed ? 40.0 : renderMs; }
    juce::Path layerPath (const std::vector<float>&, juce::Rectangle<float>, bool trimSilence) const;
    void rebuildPaths();

    kick::Params params;
    bool hasSample = false, zoomed = false;
    std::vector<float> sub, click;
    juce::Path subPath, clickPath, pitchPath;
    std::vector<juce::Point<float>> pitchDots;
    float flash = 0.0f;
};
