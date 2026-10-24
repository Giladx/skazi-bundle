#pragma once

// Smooth preset switching for any plugin: the audio fades out for ~8 ms, the new settings are
// applied on the message thread while it is silent, then it fades back in. No clicks, no jumps.
//
//   fader.change ([this] { applyMyPreset(); });   // message thread
//   fader.process (buffer, getSampleRate());       // last line of processBlock
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>
#include <functional>

namespace skazi
{
class PresetFader : private juce::AsyncUpdater, private juce::Timer
{
public:
    ~PresetFader() override { stopTimer(); cancelPendingUpdate(); }

    void change (std::function<void()> apply)
    {
        // no audio running (host stopped, offline render, editor only): change at once
        if (juce::Time::getMillisecondCounter() - lastBlockMs.load() > 150)
        {
            apply();
            return;
        }
        pending = std::move (apply);
        const int s = state.load();
        if (s == Normal || s == FadeIn) state.store (FadeOut);
        else if (s == Silent) triggerAsyncUpdate();
        startTimer (250); // safety net if the host stops processing mid-fade
    }

    void process (juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        lastBlockMs.store (juce::Time::getMillisecondCounter());
        if (state.load() == Normal) return;
        const int n = buffer.getNumSamples(), chans = buffer.getNumChannels();
        const float step = (float) (1.0 / juce::jmax (1.0, 0.008 * sampleRate));
        for (int i = 0; i < n; ++i)
        {
            const int s = state.load();
            if (s == FadeOut)
            {
                gain -= step;
                if (gain <= 0.0f) { gain = 0.0f; state.store (Silent); triggerAsyncUpdate(); }
            }
            else if (s == FadeIn)
            {
                gain += step;
                if (gain >= 1.0f) { gain = 1.0f; state.store (Normal); }
            }
            else if (s == Silent) gain = 0.0f;
            for (int c = 0; c < chans; ++c) buffer.getWritePointer (c)[i] *= gain;
        }
    }

private:
    enum States { Normal, FadeOut, Silent, FadeIn };

    void handleAsyncUpdate() override
    {
        if (pending) { auto f = std::move (pending); pending = nullptr; f(); }
        if (state.load() == Silent) state.store (FadeIn);
    }

    void timerCallback() override
    {
        stopTimer();
        if (pending) handleAsyncUpdate();
        if (state.load() != Normal && juce::Time::getMillisecondCounter() - lastBlockMs.load() > 150) { state.store (Normal); gain = 1.0f; }
    }

    std::function<void()> pending;               // message thread only
    std::atomic<int> state { Normal };
    std::atomic<juce::uint32> lastBlockMs { 0 };
    float gain = 1.0f;                            // audio thread
};
} // namespace skazi
