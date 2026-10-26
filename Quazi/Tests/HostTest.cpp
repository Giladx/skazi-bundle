// Loads the QUAZI VST3 like a DAW would and tries to break it: every sample rate, block sizes larger than
// promised, automation on every block, NaN input, mono, state reloads and the editor opening and closing
// while audio runs on another thread. Also measures how much CPU one instance takes.
//
// Usage: QuaziHostTest "<path to QUAZI.vst3>"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <cstdio>
#include <thread>

namespace
{
int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::printf ("[%s] %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
    if (! ok)
        ++failures;
}

void noise (juce::AudioBuffer<float>& b, juce::Random& r, float amp)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            b.setSample (c, i, (r.nextFloat() * 2.0f - 1.0f) * amp);
}

bool allFinite (const juce::AudioBuffer<float>& b, float& peak)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float y = b.getSample (c, i);
            if (! std::isfinite (y)) return false;
            peak = juce::jmax (peak, std::abs (y));
        }
    return true;
}

void randomizeParams (juce::AudioPluginInstance& p, juce::Random& r)
{
    for (auto* prm : p.getParameters())
        if (prm != p.getBypassParameter()) // the host's own bypass would just pass the input through
            prm->setValue (r.nextFloat());
}
} // namespace

int main (int argc, char* argv[])
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2)
    {
        std::printf ("usage: QuaziHostTest <QUAZI.vst3>\n");
        return 2;
    }

    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> found;
    format.findAllTypesForFile (found, juce::String::fromUTF8 (argv[1]));
    check (found.size() == 1, "VST3 scanned (" + juce::String (found.size()) + " plugin found)");
    if (found.isEmpty())
        return 1;

    const auto& desc = *found[0];
    std::printf ("       %s by %s, %s\n", desc.name.toRawUTF8(), desc.manufacturerName.toRawUTF8(), desc.category.toRawUTF8());
    check (desc.name == "QUAZI", "name is QUAZI");
    check (desc.manufacturerName == "SKAZI", "vendor is SKAZI");
    check (! desc.isInstrument, "registered as an effect");

    juce::String error;
    auto plugin = format.createInstanceFromDescription (desc, 48000.0, 512, error);
    check (plugin != nullptr, "instantiated " + error);
    if (plugin == nullptr)
        return 1;
    auto& p = *plugin;
    std::printf ("       %d parameters, %d in / %d out channels, latency %d\n", p.getParameters().size(),
                 p.getTotalNumInputChannels(), p.getTotalNumOutputChannels(), p.getLatencySamples());
    for (auto* prm : p.getParameters()) if (prm->getParameterIndex() < 3) std::printf ("         %s\n", prm->getName (40).toRawUTF8());
    juce::Random rng (7);

    // 1. Every sample rate, odd and oversized blocks, automation on every block
    {
        bool ok = true; float peak = 0.0f; int blocks = 0;
        for (double sr : { 22050.0, 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
            for (int promised : { 32, 128, 512, 2048 })
            {
                p.releaseResources();
                p.setRateAndBufferSizeDetails (sr, promised);
                p.prepareToPlay (sr, promised);
                for (int k = 0; k < 60; ++k, ++blocks)
                {
                    const int n = k % 7 == 0 ? promised : 1 + rng.nextInt (promised); // VST3 hosts never send more than promised
                    juce::AudioBuffer<float> b (2, n);
                    noise (b, rng, k % 10 == 0 ? 4.0f : 0.5f);
                    if (k % 3 == 0) randomizeParams (p, rng);
                    juce::MidiBuffer m;
                    p.processBlock (b, m);
                    float pk = 0.0f;
                    const bool fin = allFinite (b, pk);
                    if (! fin || pk > 1.0f)
                        std::printf ("       %.0f Hz, promised %d, block %d (n %d): %s peak %.3f\n", sr, promised, k, n, fin ? "" : "NaN", pk);
                    ok = ok && fin;
                    peak = juce::jmax (peak, pk);
                }
            }
        check (ok && peak <= 1.0f, juce::String (blocks) + " blocks at 22-192 kHz, sizes 1 to promised, random automation: finite, peak " + juce::String (peak, 3));
    }

    // 2. NaN and infinity from upstream must not break it
    {
        p.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> b (2, 512);
        juce::MidiBuffer m;
        noise (b, rng, 0.5f);
        b.setSample (0, 100, std::numeric_limits<float>::quiet_NaN());
        b.setSample (1, 200, std::numeric_limits<float>::infinity());
        p.processBlock (b, m);
        float peak = 0.0f; bool ok = allFinite (b, peak);
        float after = 0.0f;
        for (int k = 0; k < 20; ++k) { noise (b, rng, 0.5f); p.processBlock (b, m); ok = ok && allFinite (b, after); }
        check (ok && after > 0.01f, "NaN / inf input: output stays finite and keeps playing (peak " + juce::String (after, 3) + ")");
    }

    // 3. Silence after noise dies away (no stuck DC or ringing) and stays fast (no denormal slowdown)
    {
        randomizeParams (p, rng);
        juce::AudioBuffer<float> b (2, 512);
        juce::MidiBuffer m;
        for (int k = 0; k < 50; ++k) { noise (b, rng, 0.8f); p.processBlock (b, m); }
        float tail = 0.0f;
        const auto t0 = juce::Time::getHighResolutionTicks();
        for (int k = 0; k < 400; ++k)
        {
            b.clear(); p.processBlock (b, m);
            if (k >= 300) allFinite (b, tail);
        }
        const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
        check (tail < 1.0e-3f, "falls silent on silence (tail peak " + juce::String (tail, 6) + ")");
        check (secs < 400 * 512 / 48000.0 * 0.5, "silence costs " + juce::String (secs / (400 * 512 / 48000.0) * 100.0, 1) + "% CPU (no denormal slowdown)");
    }

    // 4. CPU: 20 s of loud noise with heavy settings
    {
        for (auto* prm : p.getParameters()) prm->setValue (prm->getDefaultValue());
        juce::AudioBuffer<float> b (2, 512);
        juce::MidiBuffer m;
        const int blocks = (int) (20.0 * 48000.0 / 512);
        const auto t0 = juce::Time::getHighResolutionTicks();
        for (int k = 0; k < blocks; ++k) { noise (b, rng, 0.7f); p.processBlock (b, m); }
        const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
        const double pct = secs / 20.0 * 100.0;
        check (pct < 15.0, "one instance at 48 kHz uses " + juce::String (pct, 2) + "% of one core");
    }

    // 5. Mono track
    {
        juce::AudioProcessor::BusesLayout mono;
        mono.inputBuses.add (juce::AudioChannelSet::mono());
        mono.outputBuses.add (juce::AudioChannelSet::mono());
        p.releaseResources();
        const bool set = p.setBusesLayout (mono);
        p.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> b (1, 512);
        juce::MidiBuffer m;
        bool ok = true; float peak = 0.0f;
        for (int k = 0; k < 100; ++k) { noise (b, rng, 0.5f); if (k % 5 == 0) randomizeParams (p, rng); p.processBlock (b, m); ok = ok && allFinite (b, peak); }
        check (set && ok, "mono in / mono out works");
        juce::AudioProcessor::BusesLayout stereo;
        stereo.inputBuses.add (juce::AudioChannelSet::stereo());
        stereo.outputBuses.add (juce::AudioChannelSet::stereo());
        p.releaseResources();
        p.setBusesLayout (stereo);
        p.prepareToPlay (48000.0, 512);
    }

    // 6. State save / restore keeps every parameter
    {
        // VST3 delivers parameter changes with the next audio block, so run one after each change like a DAW would
        auto settle = [&p]
        {
            juce::AudioBuffer<float> b (2, 64); b.clear();
            juce::MidiBuffer m;
            p.processBlock (b, m);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        };
        randomizeParams (p, rng);
        settle();
        juce::MemoryBlock state;
        p.getStateInformation (state);
        for (auto* prm : p.getParameters()) prm->setValue (prm->getDefaultValue());
        settle();
        juce::MemoryBlock defaults;
        p.getStateInformation (defaults);
        check (defaults != state, "automation reaches the plugin (state changes with the parameters)");
        p.setStateInformation (state.getData(), (int) state.getSize());
        settle();
        juce::MemoryBlock again;
        p.getStateInformation (again);
        check (again == state, "state read back after restoring is byte-identical (what a project reload sees)");
        juce::MidiBuffer m; juce::AudioBuffer<float> b (2, 512);
        p.setStateInformation ("garbage", 7); // a corrupt project must not crash it
        noise (b, rng, 0.5f); p.processBlock (b, m);
        float peak = 0.0f;
        check (allFinite (b, peak), "survives a corrupt state");
    }

    // 7. Audio thread running flat out while the UI opens and closes, automates and reloads state
    {
        std::atomic<bool> run { true }, bad { false };
        std::atomic<int> processed { 0 };
        std::thread audio ([&]
        {
            juce::ScopedNoDenormals nd;
            juce::Random r (11);
            juce::AudioBuffer<float> b (2, 256);
            juce::MidiBuffer m;
            while (run)
            {
                noise (b, r, 0.6f);
                p.processBlock (b, m);
                float pk = 0.0f;
                if (! allFinite (b, pk) || pk > 1.0f) bad = true;
                ++processed;
            }
        });
        juce::MemoryBlock state;
        p.getStateInformation (state);
        bool editorsOk = true;
        for (int k = 0; k < 25; ++k)
        {
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditorIfNeeded());
            editorsOk = editorsOk && ed != nullptr && ed->getWidth() > 0;
            for (int j = 0; j < 5; ++j)
            {
                auto* prm = p.getParameters()[rng.nextInt (p.getParameters().size())];
                if (prm == p.getBypassParameter()) continue;
                prm->beginChangeGesture(); prm->setValueNotifyingHost (rng.nextFloat()); prm->endChangeGesture();
                juce::MessageManager::getInstance()->runDispatchLoopUntil (15);
            }
            if (k % 5 == 0) p.setStateInformation (state.getData(), (int) state.getSize());
            if (ed != nullptr) { auto img = ed->createComponentSnapshot (ed->getLocalBounds()); juce::ignoreUnused (img); }
            ed.reset();
        }
        run = false;
        audio.join();
        check (editorsOk, "editor opened and closed 25 times while audio ran");
        check (! bad && processed > 100, juce::String (processed.load()) + " blocks on a second thread during UI work: no glitch, no NaN");
    }

    plugin->releaseResources();
    plugin.reset();
    std::printf (failures == 0 ? "ALL PASSED\n" : "%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
