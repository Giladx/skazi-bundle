#include "WaveformDisplay.h"
#include "Skin.h"

namespace
{
constexpr float minHz = 20.0f, maxHz = 8000.0f;

/** 0 at maxHz (top) .. 1 at minHz (bottom), log scale. */
float pitchToY01 (float hz)
{
    return 1.0f - std::log (juce::jlimit (minHz, maxHz, hz) / minHz) / std::log (maxHz / minHz);
}

const char* const tabNames[] = { "FULL", "ATTACK" };
} // namespace

void WaveformDisplay::setKick (const kick::Params& p, kick::ClickSamplePtr sample)
{
    params = p;
    hasSample = sample != nullptr;

    // Draw the layers without drive or level, so each one keeps its real shape.
    auto shown = p;
    shown.drive = 0.0f;
    shown.levelDb = 0.0f;

    kick::Engine offline;
    offline.setParams (shown);
    offline.setClickSample (std::move (sample));
    offline.trigger();

    const auto n = (size_t) (renderRate * renderMs / 1000.0);
    sub.resize (n);
    click.resize (n);
    const float dt = (float) (1.0 / renderRate);
    for (size_t i = 0; i < n; ++i)
    {
        const auto f = offline.processFrame (dt);
        sub[i] = f.sub;
        click[i] = f.click;
    }

    rebuildPaths();
    repaint();
}

void WaveformDisplay::setFlash (float amount)
{
    if (std::abs (amount - flash) > 0.01f)
    {
        flash = amount;
        repaint();
    }
}

void WaveformDisplay::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < 2; ++i)
        if (tabArea (i).contains (e.position) && zoomed != (i == 1))
        {
            zoomed = i == 1;
            rebuildPaths();
            repaint();
        }
}

juce::Rectangle<float> WaveformDisplay::plotArea() const
{
    // Leave room for the time ruler at the bottom and the Hz scale on the right.
    return getLocalBounds().toFloat().reduced (10.0f).withTrimmedBottom (16.0f).withTrimmedRight (34.0f);
}

juce::Rectangle<float> WaveformDisplay::tabArea (int index) const
{
    const auto r = plotArea();
    return { r.getRight() - 132.0f + 66.0f * (float) index, r.getY() + 6.0f, 62.0f, 18.0f };
}

juce::Path WaveformDisplay::layerPath (const std::vector<float>& samples, juce::Rectangle<float> r, bool trimSilence) const
{
    juce::Path path;
    const int width = (int) r.getWidth();
    const int numSamples = juce::jmin ((int) samples.size(), (int) (renderRate * viewMs() / 1000.0));
    if (numSamples == 0 || width <= 1)
        return path;

    // A short layer only gets drawn where it makes sound, not as a flat line to the end.
    int firstX = 0, lastX = width - 1;
    if (trimSilence)
    {
        auto loud = [] (float s) { return std::abs (s) > 1.0e-4f; };
        const auto end = samples.begin() + numSamples;
        const auto first = std::find_if (samples.begin(), end, loud);
        if (first == end)
            return path;
        const auto last = std::find_if (std::make_reverse_iterator (end), samples.rend(), loud);
        const float pxPerSample = (float) width / (float) numSamples;
        firstX = (int) ((float) (first - samples.begin()) * pxPerSample);
        lastX = juce::jmin (width - 1, (int) ((float) (samples.rend() - last) * pxPerSample) + 1);
    }

    // Min/max per pixel column, traced as one closed shape. When zoomed in past one
    // sample per pixel, neighbouring columns share a sample, which keeps it continuous.
    const float centreY = r.getCentreY(), halfH = r.getHeight() * 0.5f;
    const float perPixel = (float) numSamples / (float) width;
    std::vector<float> tops ((size_t) width), bottoms ((size_t) width);

    for (int x = 0; x < width; ++x)
    {
        const int start = juce::jmin ((int) (x * perPixel), numSamples - 1);
        const int end = juce::jmin (juce::jmax (start + 1, (int) ((x + 1) * perPixel) + 1), numSamples);

        float lo = samples[(size_t) start], hi = lo;
        for (int i = start + 1; i < end; ++i)
        {
            lo = juce::jmin (lo, samples[(size_t) i]);
            hi = juce::jmax (hi, samples[(size_t) i]);
        }

        float yTop = centreY - juce::jlimit (-1.0f, 1.0f, hi) * halfH;
        float yBot = centreY - juce::jlimit (-1.0f, 1.0f, lo) * halfH;
        if (yBot - yTop < 1.0f) { yTop -= 0.5f; yBot += 0.5f; }
        tops[(size_t) x] = yTop;
        bottoms[(size_t) x] = yBot;
    }

    path.startNewSubPath (r.getX() + (float) firstX, tops[(size_t) firstX]);
    for (int x = firstX + 1; x <= lastX; ++x) path.lineTo (r.getX() + (float) x, tops[(size_t) x]);
    for (int x = lastX; x >= firstX; --x)     path.lineTo (r.getX() + (float) x, bottoms[(size_t) x]);
    path.closeSubPath();
    return path;
}

void WaveformDisplay::rebuildPaths()
{
    const auto r = plotArea();
    subPath = layerPath (sub, r, false);
    clickPath = layerPath (click, r, true);

    // Pitch envelope, straight from the engine's formula.
    pitchPath.clear();
    pitchDots.clear();
    const auto view = (float) viewMs();
    auto pointAt = [&] (float ms)
    {
        return juce::Point<float> (r.getX() + r.getWidth() * ms / view,
                                   r.getY() + r.getHeight() * pitchToY01 (kick::pitchAt (params, ms)));
    };

    // The snap is over in a few ms, so sample the first pixels finely.
    pitchPath.startNewSubPath (pointAt (0.0f));
    const float msPerPx = view / r.getWidth();
    for (float ms = 0.05f; ms < juce::jmin (view, 4.0f * msPerPx); ms += 0.05f)
        pitchPath.lineTo (pointAt (ms));
    for (int x = 4; x <= (int) r.getWidth(); ++x)
        pitchPath.lineTo (pointAt (view * (float) x / r.getWidth()));

    // Handles: start, end of snap (~punch), end of sweep, and the root at the right edge.
    pitchDots.push_back (pointAt (0.0f));
    for (float ms : { params.snapMs * 2.0f, params.pitchMs })
        if (ms < view)
            pitchDots.push_back (pointAt (ms));
    pitchDots.push_back (pointAt (view));
}

void WaveformDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    skin::drawWell (g, bounds, 8.0f);

    const auto r = plotArea();
    const auto view = viewMs();

    // Grid: time columns, amplitude quarters, and the Hz scale for the pitch curve.
    const int minor = zoomed ? 2 : 50, major = zoomed ? 10 : 100;
    g.setFont (skin::valueFont (10.0f));
    for (int ms = 0; ms <= (int) view; ms += minor)
    {
        const float x = r.getX() + r.getWidth() * (float) (ms / view);
        const bool isMajor = ms % major == 0;
        g.setColour (isMajor ? skin::gridStrong : skin::grid);
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        if (isMajor && ms < (int) view)
        {
            g.setColour (skin::textSoft.withAlpha (0.7f));
            g.drawText (juce::String (ms), juce::Rectangle<float> (x - 20.0f, r.getBottom() + 2.0f, 40.0f, 14.0f),
                        ms == 0 ? juce::Justification::centredLeft : juce::Justification::centred);
        }
    }
    for (int i = 1; i < 4; ++i)
    {
        g.setColour (i == 2 ? skin::gridStrong : skin::grid);
        g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * (float) i / 4.0f), r.getX(), r.getRight());
    }
    for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f })
    {
        const float y = r.getY() + r.getHeight() * pitchToY01 (hz);
        g.setColour (skin::textSoft.withAlpha (0.55f));
        g.drawText (hz >= 1000.0f ? juce::String (hz / 1000.0f, 0) + "k" : juce::String ((int) hz),
                    juce::Rectangle<float> (r.getRight() + 4.0f, y - 7.0f, 30.0f, 14.0f),
                    juce::Justification::centredLeft);
        g.setColour (skin::grid.withAlpha (0.6f));
        g.drawHorizontalLine ((int) y, r.getRight() - 4.0f, r.getRight());
    }
    g.setColour (skin::textSoft.withAlpha (0.55f));
    g.drawText ("ms", juce::Rectangle<float> (r.getRight() + 4.0f, r.getBottom() + 2.0f, 30.0f, 14.0f),
                juce::Justification::centredLeft);

    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (r.toNearestInt());

        // SUB: lava fill, hot at the attack, cooling as it decays, with a glow edge.
        auto grad = skin::lavaGradient (r);
        grad.multiplyOpacity (0.8f + 0.2f * flash);
        g.setGradientFill (grad);
        g.fillPath (subPath);
        g.setColour (skin::lavaHot.withAlpha (0.25f + 0.4f * flash));
        g.strokePath (subPath, juce::PathStrokeType (1.0f));

        // SAMPLE: ice on top, so the transient cuts through the body.
        g.setColour (skin::ice.withAlpha (0.35f));
        g.fillPath (clickPath);
        g.setColour (skin::ice.withAlpha (0.9f + 0.1f * flash));
        g.strokePath (clickPath, juce::PathStrokeType (1.3f));

        // Where the click starts, so the offset can be set by eye.
        const float offX = r.getX() + r.getWidth() * (float) (params.offsetMs / view);
        if (params.offsetMs > 0.0f && offX < r.getRight())
        {
            juce::Path marker;
            marker.startNewSubPath (offX, r.getY());
            marker.lineTo (offX, r.getBottom());
            const float dashes[] = { 3.0f, 3.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (marker, marker, dashes, 2);
            g.setColour (skin::ice.withAlpha (0.6f));
            g.fillPath (marker);
        }

        // Pitch curve on top, with handles at start / sweep / end.
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.strokePath (pitchPath, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (skin::text.withAlpha (0.9f));
        g.strokePath (pitchPath, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        for (auto p : pitchDots)
        {
            g.setColour (skin::screen);
            g.fillEllipse (p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f);
            g.setColour (skin::text);
            g.drawEllipse (p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f, 1.6f);
        }
    }

    // Readout and layer legend in the top-left corner.
    const auto readout = juce::String (juce::roundToInt (params.startHz)) + " > "
                       + juce::String (juce::roundToInt (params.punchHz)) + " > "
                       + juce::String (juce::roundToInt (params.endHz)) + " Hz"
                       + "   HOLD " + juce::String (juce::roundToInt (params.holdMs)) + " ms"
                       + "   DECAY " + juce::String (juce::roundToInt (params.ampMs)) + " ms";
    auto top = r.reduced (8.0f, 6.0f);
    g.setColour (skin::screen.withAlpha (0.75f));
    g.fillRoundedRectangle (top.withSize (340.0f, 34.0f).expanded (5.0f, 3.0f), 4.0f);
    g.setFont (skin::valueFont (11.0f));
    g.setColour (skin::text.withAlpha (0.8f));
    g.drawText (readout, top.removeFromTop (14.0f), juce::Justification::centredLeft);

    auto legend = top.removeFromTop (16.0f);
    g.setColour (skin::lava);
    g.fillRect (legend.removeFromLeft (8.0f).withSizeKeepingCentre (8.0f, 8.0f));
    g.setColour (skin::textSoft);
    g.drawText ("SUB", legend.removeFromLeft (36.0f).withTrimmedLeft (5.0f), juce::Justification::centredLeft);
    g.setColour (skin::ice);
    g.fillRect (legend.removeFromLeft (8.0f).withSizeKeepingCentre (8.0f, 8.0f));
    g.setColour (skin::textSoft);
    g.drawText (hasSample ? "SAMPLE" : "SAMPLE (NOISE)", legend.withTrimmedLeft (5.0f), juce::Justification::centredLeft);

    // View tabs
    for (int i = 0; i < 2; ++i)
    {
        const auto t = tabArea (i);
        const bool on = zoomed == (i == 1);
        g.setColour (on ? skin::lava : skin::plate);
        g.fillRoundedRectangle (t, 4.0f);
        g.setColour (on ? skin::lavaHot : skin::plateEdge);
        g.drawRoundedRectangle (t, 4.0f, 1.0f);
        g.setFont (skin::labelFont (11.5f));
        g.setColour (on ? juce::Colour (0xff1a0602) : skin::textSoft);
        g.drawText (tabNames[i], t, juce::Justification::centred);
    }
}
