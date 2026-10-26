#include "StepGrid.h"
#include "Skin.h"

namespace
{
bool isBlackKey (int semitone)
{
    const int n = semitone % 12;
    return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}

bool samePattern (const s303::Pattern& a, const s303::Pattern& b)
{
    if (a.length != b.length)
        return false;
    for (int i = 0; i < s303::kMaxSteps; ++i)
    {
        const auto& x = a.steps[i];
        const auto& y = b.steps[i];
        if (x.note != y.note || x.octave != y.octave || x.gate != y.gate || x.accent != y.accent || x.slide != y.slide)
            return false;
    }
    return true;
}
} // namespace

StepGrid::StepGrid (Skazi303Processor& p) : processor (p)
{
    shown = processor.readPattern();
    startTimerHz (30);
}

void StepGrid::timerCallback()
{
    const auto pat = processor.readPattern();
    const int step = processor.getCurrentStep();
    if (step != shownStep || ! samePattern (pat, shown))
    {
        shown = pat;
        shownStep = step;
        repaint();
    }
}

juce::Rectangle<float> StepGrid::cell (int step, Row row) const
{
    const float colW = ((float) getWidth() - labelW) / (float) s303::kMaxSteps;
    const float x = labelW + colW * (float) step + 2.0f;
    const float w = colW - 4.0f;

    float y = 0.0f;
    auto take = [&] (float h) { auto r = juce::Rectangle<float> (x, y, w, h); y += h + gap; return r; };

    const auto roll = take (rollH);
    const auto name = take (14.0f);
    const auto oct = take (rowH);
    const auto gate = take (rowH);
    const auto acc = take (rowH);
    const auto sld = take (rowH);
    const auto key = take (keyH);

    switch (row)
    {
        case Row::Roll:   return roll;
        case Row::Name:   return name;
        case Row::Octave: return oct;
        case Row::Gate:   return gate;
        case Row::Accent: return acc;
        case Row::Slide:  return sld;
        case Row::Key:    return key;
        case Row::None:   break;
    }
    return {};
}

std::pair<int, StepGrid::Row> StepGrid::hitTest (juce::Point<float> pt) const
{
    const float colW = ((float) getWidth() - labelW) / (float) s303::kMaxSteps;
    if (pt.x < labelW)
        return { -1, Row::None };
    const int step = juce::jlimit (0, s303::kMaxSteps - 1, (int) ((pt.x - labelW) / colW));

    for (auto row : { Row::Roll, Row::Name, Row::Octave, Row::Gate, Row::Accent, Row::Slide, Row::Key })
    {
        const auto r = cell (step, row);
        if (pt.y >= r.getY() - gap * 0.5f && pt.y < r.getBottom() + gap * 0.5f)
            return { step, row };
    }
    return { step, Row::None };
}

int StepGrid::noteAt (int step, float y) const
{
    const auto r = cell (step, Row::Roll);
    const float rowHeight = r.getHeight() / 13.0f;
    return juce::jlimit (0, 12, 12 - (int) ((y - r.getY()) / rowHeight));
}

void StepGrid::setStepValue (int step, const char* field, float value)
{
    if (auto* prm = processor.apvts.getParameter (ids::step (step, field)))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
        prm->endChangeGesture();
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    const auto [step, row] = hitTest (e.position);
    if (step < 0)
        return;

    const auto& s = shown.steps[step];
    dragStep = -1;

    switch (row)
    {
        case Row::Roll:
        {
            const int note = noteAt (step, e.position.y);
            setStepValue (step, "note", (float) note);
            if (! s.gate)
                setStepValue (step, "gate", 1.0f);
            dragStep = step;
            break;
        }
        case Row::Octave:
        {
            // Click cycles 0 -> UP -> DN; right-click goes the other way.
            const int next = e.mods.isPopupMenu() ? (s.octave == -1 ? 1 : s.octave - 1)
                                                  : (s.octave == 1 ? -1 : s.octave + 1);
            setStepValue (step, "oct", (float) next);
            break;
        }
        case Row::Gate:   setStepValue (step, "gate", s.gate ? 0.0f : 1.0f); break;
        case Row::Accent: setStepValue (step, "acc", s.accent ? 0.0f : 1.0f); break;
        case Row::Slide:  setStepValue (step, "sld", s.slide ? 0.0f : 1.0f); break;
        case Row::Key:
        {
            // Clicking a step key sets the loop length to end on that step.
            if (auto* prm = processor.apvts.getParameter ("seq_len"))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (step + 1)));
                prm->endChangeGesture();
            }
            break;
        }
        case Row::Name:
        case Row::None:
            break;
    }

    timerCallback();
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStep < 0)
        return;

    // Dragging sideways paints notes across steps, like drawing a line.
    const auto [step, row] = hitTest (e.position);
    const int target = step >= 0 ? step : dragStep;
    const int note = noteAt (target, juce::jlimit (cell (target, Row::Roll).getY(),
                                                    cell (target, Row::Roll).getBottom() - 1.0f, e.position.y));
    if (shown.steps[target].note != note)
        setStepValue (target, "note", (float) note);
    if (! shown.steps[target].gate)
        setStepValue (target, "gate", 1.0f);
    timerCallback();
}

void StepGrid::paint (juce::Graphics& g)
{
    using namespace skin;

    const int root = (int) processor.apvts.getRawParameterValue ("root")->load();
    const int len = juce::jlimit (1, s303::kMaxSteps, shown.length);

    // Row labels
    g.setFont (labelFont (11.5f));
    g.setColour (cream);
    auto label = [&] (Row row, const char* text)
    {
        const auto r = cell (0, row);
        g.drawText (text, juce::Rectangle<float> (0.0f, r.getY(), labelW - 8.0f, r.getHeight()),
                    juce::Justification::centredRight);
    };
    label (Row::Roll, "NOTE");
    label (Row::Octave, "OCT");
    label (Row::Gate, "GATE");
    label (Row::Accent, "ACCENT");
    label (Row::Slide, "SLIDE");
    label (Row::Key, "STEP");

    for (int i = 0; i < s303::kMaxSteps; ++i)
    {
        const auto& s = shown.steps[i];
        const bool active = i < len;
        const bool playing = i == shownStep;
        const float dim = active ? 1.0f : 0.35f;

        // --- piano roll
        const auto roll = cell (i, Row::Roll);
        const float rh = roll.getHeight() / 13.0f;
        for (int n = 0; n <= 12; ++n)
        {
            auto rr = juce::Rectangle<float> (roll.getX(), roll.getBottom() - rh * (float) (n + 1), roll.getWidth(), rh).reduced (0.0f, 0.5f);
            g.setColour ((isBlackKey (n + root) ? juce::Colour (0xff12051f) : juce::Colour (0xff22093a)).withMultipliedAlpha (dim));
            g.fillRect (rr);
        }

        if (playing)
        {
            g.setColour (acid.withAlpha (0.12f));
            g.fillRect (roll);
        }

        {
            auto nr = juce::Rectangle<float> (roll.getX(), roll.getBottom() - rh * (float) (s.note + 1), roll.getWidth(), rh).reduced (1.0f, 0.5f);
            auto col = s.accent ? accentOrange : acid;
            if (! s.gate)
                col = juce::Colour (0xff3b3e44);
            col = col.withMultipliedAlpha (dim);
            if (s.gate && active)
            {
                g.setColour (col.withAlpha (0.3f));
                g.fillRoundedRectangle (nr.expanded (2.0f, 2.0f), 3.0f);
            }
            g.setColour (col);
            g.fillRoundedRectangle (nr, 2.0f);

            // Slide: a tail reaching into the next step.
            if (s.slide && s.gate && i < s303::kMaxSteps - 1)
            {
                g.setColour (slideCyan.withMultipliedAlpha (dim * 0.8f));
                g.fillRect (nr.getRight() - 1.0f, nr.getCentreY() - 1.0f, 6.0f, 2.0f);
            }
        }

        // --- note name
        g.setFont (juce::Font (juce::FontOptions ("Consolas", 11.0f, juce::Font::bold)));
        g.setColour ((s.gate ? cream : cream.withAlpha (0.35f)).withMultipliedAlpha (dim));
        juce::String name = Skazi303Processor::noteNames[(s.note + root) % 12];
        if (s.note + root >= 12) name << "'";
        g.drawText (name, cell (i, Row::Name), juce::Justification::centred);

        // --- octave pill
        {
            const auto r = cell (i, Row::Octave).reduced (1.0f, 1.0f);
            const bool on = s.octave != 0;
            g.setColour ((on ? juce::Colour (0xff4a0d3a) : deckRaised).withMultipliedAlpha (dim));
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour ((on ? acid : deckText.withAlpha (0.35f)).withMultipliedAlpha (dim));
            g.setFont (labelFont (11.0f));
            g.drawText (s.octave > 0 ? "UP" : s.octave < 0 ? "DN" : "--", r, juce::Justification::centred);
        }

        // --- LED toggles
        auto led = [&] (Row row, bool on, juce::Colour col)
        {
            const auto r = cell (i, row);
            const float d = juce::jmin (r.getHeight(), r.getWidth()) - 4.0f;
            const auto c = r.withSizeKeepingCentre (d, d);
            g.setColour (deckRaised.withMultipliedAlpha (dim));
            g.fillRoundedRectangle (r.reduced (1.0f), 3.0f);
            if (on)
            {
                g.setColour (col.withAlpha (0.35f * dim));
                g.fillEllipse (c.expanded (3.0f));
                g.setColour (col.withMultipliedAlpha (dim));
                g.fillEllipse (c);
                g.setColour (juce::Colours::white.withAlpha (0.5f * dim));
                g.fillEllipse (c.reduced (d * 0.3f).translated (-d * 0.12f, -d * 0.12f));
            }
            else
            {
                g.setColour (juce::Colour (0xff35100c).withMultipliedAlpha (dim));
                g.fillEllipse (c);
            }
        };
        led (Row::Gate, s.gate, ledRed);
        led (Row::Accent, s.accent, accentOrange);
        led (Row::Slide, s.slide, slideCyan);

        // --- 303-style step key with its LED
        {
            const auto k = cell (i, Row::Key);
            auto body = k.withTrimmedTop (12.0f);
            g.setColour (juce::Colour (0x88000000));
            g.fillRoundedRectangle (body.translated (0.0f, 2.0f), 3.0f);
            const auto top = active ? juce::Colour (0xffe9ebee) : juce::Colour (0xff6d7177);
            const auto bot = active ? juce::Colour (0xffb5b9bf) : juce::Colour (0xff4f5257);
            g.setGradientFill (juce::ColourGradient (top, 0.0f, body.getY(), bot, 0.0f, body.getBottom(), false));
            g.fillRoundedRectangle (body, 3.0f);
            g.setColour (juce::Colour (0xff2a2b2e));
            g.drawRoundedRectangle (body, 3.0f, 1.0f);

            g.setColour (ink);
            g.setFont (labelFont (14.0f));
            g.drawText (juce::String (i + 1), body, juce::Justification::centred);

            const auto ledR = juce::Rectangle<float> (k.getCentreX() - 4.0f, k.getY() + 1.0f, 8.0f, 8.0f);
            if (playing)
            {
                g.setColour (ledRed.withAlpha (0.4f));
                g.fillEllipse (ledR.expanded (4.0f));
                g.setColour (ledRed);
            }
            else
            {
                g.setColour (juce::Colour (0xff3a0f0b));
            }
            g.fillEllipse (ledR);

            // Group markers every four steps, like the hardware's beat printing.
            if (i % 4 == 0)
            {
                g.setColour (cream.withAlpha (0.45f));
                g.fillRect (k.getX(), k.getBottom() + 3.0f, (k.getWidth() + 4.0f) * 4.0f - 4.0f, 1.0f);
            }
        }
    }
}
