// Offline checks for ACIDANCE:
//   RenderTest <outDir>              render demo presets to WAV + snapshot every editor tab
//   RenderTest <outDir> --calibrate  play all presets, report health, write PresetGains.h
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/Presets.h"

namespace
{
    constexpr double sr = 48000.0;
    constexpr int block = 256;

    struct Result { float peak = 0.f; double rms = 0.0; bool bad = false; double seconds = 0.0; };

    // Plays a preset with a part that suits its category and returns level stats.
    Result play (int preset, juce::AudioBuffer<float>* capture, double length = 4.0)
    {
        AcidanceProcessor proc;
        proc.setCurrentProgram (preset);
        proc.fader.flush(); // no message loop here: apply the preset now
        proc.setPlayConfigDetails (0, 2, sr, block);
        proc.prepareToPlay (sr, block);

        const auto cat = Presets::category (preset);
        const bool seq = proc.apvts.getRawParameterValue ("seq_on")->load() > 0.5f;
        if (seq) proc.setParam ("run", 1.f);

        const int total = (int) (sr * length);
        if (capture) capture->setSize (2, total);
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        Result r;
        double sumSq = 0.0;
        const auto t0 = juce::Time::getMillisecondCounterHiRes();

        auto addNote = [&midi] (int pos, int note, bool on, int vel = 100)
        {
            midi.addEvent (on ? juce::MidiMessage::noteOn (1, note, (juce::uint8) vel)
                              : juce::MidiMessage::noteOff (1, note), pos);
        };

        for (int pos = 0; pos < total; pos += block)
        {
            const int n = std::min (block, total - pos);
            buf.setSize (2, n, false, false, true);
            midi.clear();
            if (seq)
            {
                // the pattern only plays while a key is held: hold C3 (no transpose) for the whole render
                if (pos == 0) addNote (0, 60, true);
            }
            else
            {
                const double t = pos / sr, t1 = (pos + n) / sr;
                auto at = [&] (double when) { return when >= t && when < t1 ? (int) ((when - t) * sr) : -1; };
                if (cat == "Bass" || cat == "Techno Bass" || cat == "House Bass" || cat == "Deep Bass")
                {
                    for (int k = 0; k < 16; ++k)
                    {
                        const int note = (k % 4 == 3) ? 43 : 36;
                        if (int o = at (k * 0.25); o >= 0) addNote (o, note, true, k % 4 == 0 ? 110 : 90);
                        if (int o = at (k * 0.25 + 0.18); o >= 0) addNote (o, note, false);
                    }
                }
                else if (cat == "Lead" || cat == "Acid Leads" || cat == "Hard Leads")
                {
                    const int mel[] = { 72, 75, 79, 77, 75, 72, 70, 72 };
                    for (int k = 0; k < 8; ++k)
                    {
                        if (int o = at (k * 0.45); o >= 0) addNote (o, mel[k], true);
                        if (int o = at (k * 0.45 + 0.42); o >= 0) addNote (o, mel[k], false);
                    }
                }
                else if (cat == "Pluck" || cat == "FM Plucks")
                {
                    for (int k = 0; k < 8; ++k)
                        for (int nn : { 60, 63, 67 })
                        {
                            if (int o = at (k * 0.5); o >= 0) addNote (o, nn, true);
                            if (int o = at (k * 0.5 + 0.2); o >= 0) addNote (o, nn, false);
                        }
                }
                else
                {
                    for (int nn : { 60, 63, 67 })
                    {
                        if (int o = at (0.0); o >= 0) addNote (o, nn, true);
                        if (int o = at (length * 0.75); o >= 0) addNote (o, nn, false);
                    }
                }
            }
            proc.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c)
            {
                if (capture) capture->copyFrom (c, pos, buf, c, 0, n);
                for (int i = 0; i < n; ++i)
                {
                    const float v = buf.getSample (c, i);
                    if (! std::isfinite (v)) r.bad = true;
                    r.peak = std::max (r.peak, std::abs (v));
                    sumSq += (double) v * v;
                }
            }
        }
        r.seconds = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
        r.rms = std::sqrt (sumSq / (2.0 * total));
        return r;
    }

    void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b)
    {
        f.deleteFile();
        juce::WavAudioFormat wav;
        if (auto stream = std::unique_ptr<juce::OutputStream> (f.createOutputStream()))
            if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), sr, 2, 24, {}, 0)))
            {
                stream.release();
                w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
            }
    }
}

#include "../Source/PresetGains.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    const juce::String mode = argc > 2 ? juce::String (argv[2]) : juce::String();

    if (mode == "--calibrate")
    {
        // Target about -18 dBFS RMS for every preset
        const double target = 0.12;
        constexpr int numGains = (int) (sizeof (presetGains) / sizeof (presetGains[0]));
        juce::String table = "#pragma once\n// Generated by RenderTest --calibrate: per-preset loudness trim\nstatic const float presetGains[] = {";
        int bad = 0;
        double worstRt = 0.0;
        for (int i = 0; i < Presets::count(); ++i)
        {
            auto r = play (i, nullptr);
            double g = r.rms > 1.0e-5 ? target / r.rms : 1.0;
            const double current = juce::jlimit (0.0, 2.0, 0.8 * (i < numGains ? presetGains[i] : 1.f)) / 0.8;
            g = juce::jlimit (0.25, 2.5, current * g);
            worstRt = std::max (worstRt, r.seconds / 4.0);
            if (r.bad || r.rms < 0.005) ++bad;
            std::printf ("%4d %-13s %-24s rms=%.3f peak=%.3f gain=%.2f cpu=%.1f%%%s\n", i + 1,
                         Presets::category (i).toRawUTF8(), Presets::name (i).toRawUTF8(), r.rms, r.peak, g,
                         100.0 * r.seconds / 4.0, r.bad ? " NAN!" : (r.rms < 0.005 ? " SILENT!" : ""));
            table << (i % 10 == 0 ? "\n   " : "") << " " << juce::String (g, 3) << "f,";
        }
        table << "\n};\n";
        outDir.getChildFile ("PresetGains.h").replaceWithText (table);
        std::printf ("problems=%d worst-cpu=%.1f%% of real time\n", bad, worstRt * 100.0);
        return bad > 0 ? 1 : 0;
    }

    if (mode == "--dump")
    {
        // Print every parameter that differs from its default for one preset
        AcidanceProcessor proc;
        proc.setCurrentProgram (juce::String (argv[3]).getIntValue());
        proc.fader.flush(); // no message loop here: apply the preset now
        std::printf ("%s / %s\n", proc.currentPresetFolder.toRawUTF8(), proc.currentPresetName.toRawUTF8());
        for (auto* p : proc.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (std::abs (rp->getValue() - rp->getDefaultValue()) > 1.0e-4f)
                    std::printf ("  %-14s %s\n", rp->getParameterID().toRawUTF8(), rp->getCurrentValueAsText().toRawUTF8());
        return 0;
    }

    if (mode == "--diversity")
    {
        // Fingerprint each preset of a folder (spectrum + rhythm) and count near-identical pairs
        const juce::String folderName = argc > 3 ? juce::String (argv[3]) : Presets::categories()[0];
        const int c = Presets::categories().indexOf (folderName);
        if (c < 0) { std::printf ("unknown folder\n"); return 1; }
        const int start = Presets::categoryStart (c), count = std::min (Presets::categorySize (c), argc > 4 ? juce::String (argv[4]).getIntValue() : 120);
        juce::dsp::FFT fft (12);
        std::vector<std::vector<float>> prints;
        for (int i = 0; i < count; ++i)
        {
            juce::AudioBuffer<float> out;
            play (start + i, &out, 3.0);
            std::vector<float> v;
            // 32 log-spaced spectral bands averaged over the render
            std::vector<float> bands (32, 0.f);
            std::vector<float> data (8192);
            const int n = out.getNumSamples();
            for (int pos = 0; pos + 4096 <= n; pos += 4096)
            {
                std::fill (data.begin(), data.end(), 0.f);
                for (int s = 0; s < 4096; ++s)
                    data[(size_t) s] = out.getSample (0, pos + s) * (0.5f - 0.5f * std::cos (6.2831853f * (float) s / 4095.f));
                fft.performFrequencyOnlyForwardTransform (data.data(), true);
                for (int b = 0; b < 32; ++b)
                {
                    const float f0 = 30.f * std::pow (600.f, (float) b / 32.f), f1 = 30.f * std::pow (600.f, (float) (b + 1) / 32.f);
                    const int i0 = juce::jlimit (1, 2047, (int) (f0 / sr * 4096)), i1 = juce::jlimit (i0 + 1, 2048, (int) (f1 / sr * 4096));
                    for (int k = i0; k < i1; ++k) bands[(size_t) b] += data[(size_t) k];
                }
            }
            for (auto& b : bands) v.push_back (std::log1p (b));
            // 48 frames of loudness envelope (rhythm)
            const int frame = n / 48;
            for (int fr = 0; fr < 48; ++fr) v.push_back (out.getRMSLevel (0, fr * frame, frame) * 20.f);
            float norm = 0.f;
            for (auto x : v) norm += x * x;
            norm = std::sqrt (std::max (norm, 1.0e-9f));
            for (auto& x : v) x /= norm;
            prints.push_back (v);
        }
        int near = 0, pairs = 0;
        double sum = 0.0;
        for (size_t a = 0; a < prints.size(); ++a)
            for (size_t b = a + 1; b < prints.size(); ++b)
            {
                float dot = 0.f;
                for (size_t k = 0; k < prints[a].size(); ++k) dot += prints[a][k] * prints[b][k];
                ++pairs; sum += dot;
                if (dot > 0.985f) ++near;
            }
        std::printf ("%s: %d presets, average similarity %.3f, near-identical pairs %d of %d (%.1f%%)\n",
                     folderName.toRawUTF8(), count, sum / pairs, near, pairs, 100.0 * near / pairs);
        return 0;
    }

    if (mode == "--stress")
    {
        // Worst case: 16 voices, 2 x 16-voice unison wavetables, two filters, every FX on
        for (int os = 0; os < 4; ++os)
        {
            AcidanceProcessor proc;
            proc.setPlayConfigDetails (0, 2, sr, block);
            proc.setParam ("oversample", (float) os);
            proc.setParam ("osc1_wave", 7.f); proc.setParam ("osc1_uni", 16.f);
            proc.setParam ("osc2_on", 1.f); proc.setParam ("osc2_wave", 6.f); proc.setParam ("osc2_uni", 16.f); proc.setParam ("osc2_level", 0.6f);
            proc.setParam ("flt1_type", 1.f); proc.setParam ("flt2_type", 7.f); proc.setParam ("flt_routing", 1.f);
            proc.setParam ("env1_s", 1.f); proc.setParam ("mode", 0.f); proc.setParam ("seq_on", 0.f);
            proc.setParam ("mod1_src", (float) S_Lfo1); proc.setParam ("mod1_dst", (float) D_Cut1); proc.setParam ("mod1_amt", 0.3f);
            proc.setParam ("lfo1_rate", 300.f);
            for (auto id : { "dist_on", "mfx_on", "dly_on", "rev_on", "comp_on", "eq_on" }) proc.setParam (id, 1.f);
            proc.prepareToPlay (sr, block);
            juce::AudioBuffer<float> buf (2, block);
            juce::MidiBuffer midi;
            for (int n = 0; n < 16; ++n) midi.addEvent (juce::MidiMessage::noteOn (1, 40 + n * 3, (juce::uint8) 100), 0);
            const int blocks = (int) (sr * 3.0) / block;
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            for (int b = 0; b < blocks; ++b)
            {
                proc.processBlock (buf, midi);
                midi.clear();
            }
            const double secs = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
            std::printf ("oversampling %dx: %.1f%% of one CPU core (16 voices x 32 unison oscillators, all FX)\n", 1 << os, 100.0 * secs / 3.0);
        }
        return 0;
    }

    if (mode == "--demo")
    {
        // RenderTest <dir> --demo <preset number> ...: 6 s WAV of each preset, played like --calibrate
        for (int a = 3; a < argc; ++a)
        {
            const int i = juce::jlimit (1, Presets::count(), juce::String (argv[a]).getIntValue()) - 1;
            juce::AudioBuffer<float> out;
            auto r = play (i, &out, 6.0);
            std::printf ("%4d %-13s %-24s peak=%.3f rms=%.3f\n", i + 1, Presets::category (i).toRawUTF8(), Presets::name (i).toRawUTF8(), r.peak, r.rms);
            writeWav (outDir.getChildFile ("demo_" + juce::String (i + 1).paddedLeft ('0', 4) + ".wav"), out);
        }
        return 0;
    }

    if (mode == "--snap")
    {
        // RenderTest <dir> --snap <preset number> [skin]: every tab of one preset, one skin (default Hyper)
        AcidanceProcessor proc;
        const int index = juce::jlimit (1, Presets::count(), juce::String (argc > 3 ? argv[3] : "1").getIntValue()) - 1;
        proc.setCurrentProgram (index);
        proc.fader.flush(); // no message loop here: apply the preset now
        proc.currentStep = 4;
        proc.uiScale = 1.f;
        proc.skin = argc > 4 ? juce::String (argv[4]).getIntValue() : 5;
        std::unique_ptr<AcidanceEditor> ed (dynamic_cast<AcidanceEditor*> (proc.createEditor()));
        std::function<void (juce::Component*, juce::Array<juce::TextButton*>&)> find;
        find = [&find] (juce::Component* c, juce::Array<juce::TextButton*>& out)
        {
            for (auto* ch : c->getChildren())
            {
                if (auto* b = dynamic_cast<juce::TextButton*> (ch))
                    if (b->getRadioGroupId() == 1001) out.add (b);
                find (ch, out);
            }
        };
        juce::Array<juce::TextButton*> tabs;
        find (ed.get(), tabs);
        for (auto* t : tabs)
        {
            t->setToggleState (true, juce::dontSendNotification);
            t->onClick();
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            auto f = outDir.getChildFile ("snap_" + juce::String (index + 1) + "_"
                                          + t->getButtonText().toLowerCase().removeCharacters (" /") + ".png");
            f.deleteFile();
            juce::FileOutputStream fos (f);
            juce::PNGImageFormat().writeImageToStream (img, fos);
        }
        std::printf ("%s / %s\n", proc.currentPresetFolder.toRawUTF8(), proc.currentPresetName.toRawUTF8());
        return 0;
    }

    // Library metadata as JSON
    outDir.getChildFile ("factory_presets.json").replaceWithText (juce::JSON::toString (Presets::toJson()));

    // Demo renders: first preset of each folder
    for (int c = 0; c < Presets::categories().size(); ++c)
    {
        const int i = Presets::categoryStart (c);
        juce::AudioBuffer<float> out;
        auto r = play (i, &out, 6.0);
        std::printf ("%-13s %-24s peak=%.3f rms=%.3f\n", Presets::category (i).toRawUTF8(), Presets::name (i).toRawUTF8(), r.peak, r.rms);
        writeWav (outDir.getChildFile ("demo_" + Presets::category (i).toLowerCase().replace (" ", "_") + ".wav"), out);
    }

    // PANIC check: a long reverb + delay tail must be silent right after the kill switch
    {
        AcidanceProcessor proc;
        proc.setCurrentProgram (Presets::categoryStart (7));   // a pad with a long tail
        proc.fader.flush(); // no message loop here: apply the preset now
        proc.setPlayConfigDetails (0, 2, sr, block);
        proc.prepareToPlay (sr, block);
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        for (int b = 0; b < 200; ++b) { proc.processBlock (buf, midi); midi.clear(); }
        const float before = buf.getMagnitude (0, block);
        proc.panicRequest = true;
        proc.processBlock (buf, midi);
        bool sentCC = false;
        for (const auto m : midi) if (m.getMessage().isAllNotesOff()) sentCC = true;
        midi.clear();
        proc.processBlock (buf, midi);
        std::printf ("PANIC: level before=%.4f after=%.6f, CC123 sent=%s\n", before, buf.getMagnitude (0, block), sentCC ? "yes" : "no");
    }

    // Snapshot every tab in every skin, plus the preset browser
    AcidanceProcessor proc;
    proc.setCurrentProgram (Presets::categoryStart (0) + 3);
    proc.fader.flush(); // no message loop here: apply the preset now
    proc.currentStep = 4;
    proc.uiScale = 1.f;
    std::function<void (juce::Component*, juce::Array<juce::TextButton*>&)> findTabs;
    findTabs = [&findTabs] (juce::Component* c, juce::Array<juce::TextButton*>& out)
    {
        for (auto* ch : c->getChildren())
        {
            if (auto* b = dynamic_cast<juce::TextButton*> (ch))
                if (b->getRadioGroupId() == 1001) out.add (b);
            findTabs (ch, out);
        }
    };
    auto snap = [&outDir] (juce::Component& c, const juce::String& name)
    {
        auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
        auto f = outDir.getChildFile (name + ".png");
        f.deleteFile();
        juce::FileOutputStream fos (f);
        juce::PNGImageFormat().writeImageToStream (img, fos);
    };
    const char* skinNames[] = { "neon", "vintage", "chrome", "robot", "hyper", "bully" };
    for (int skin : { 5, 4, 3, 2, 1, 0 })
    {
        proc.skin = skin;
        std::unique_ptr<AcidanceEditor> ed (dynamic_cast<AcidanceEditor*> (proc.createEditor()));
        juce::Array<juce::TextButton*> tabs;
        findTabs (ed.get(), tabs);
        for (int i = 0; i < tabs.size(); ++i)
        {
            tabs[i]->setToggleState (true, juce::dontSendNotification);
            tabs[i]->onClick();
            snap (*ed, juce::String ("ui_") + skinNames[skin] + "_" + tabs[i]->getButtonText().toLowerCase());
        }
        ed->showPresetBrowser();
        snap (*ed, juce::String ("ui_") + skinNames[skin] + "_browser");
    }
    return 0;
}
