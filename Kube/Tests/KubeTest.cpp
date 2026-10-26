// Plays every KUBE preset offline and checks the result: not silent, no NaNs, no clipping, and the CPU cost.
//   KubeTest [render folder]   writes one WAV per preset when a folder is given
//   KubeTest --trim            (from the Kube folder) re-measures every factory preset and rewrites Source/PresetTrims.h,
//                              so they all play at the same loudness. Run it twice after changing a preset or the engine.
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/KubeEngine.h"
#include "../Source/Presets.h"
#include "../Source/PresetTrims.h"

namespace
{
constexpr double kSr = 48000.0, kBpm = 145.0;
constexpr int kBlock = 256;

struct Note { double beat, len; int note; };

// Psy bass: the three sixteenths after each kick. Everything else: a little goa riff with a chord at the end.
std::vector<Note> pattern (const juce::String& category)
{
    std::vector<Note> n;
    if (category == "BASS" || category == "ACID")
    {
        for (int beat = 0; beat < 8; ++beat)
            for (int s = 1; s < 4; ++s)
                n.push_back ({ beat + s * 0.25, category == "ACID" ? 0.3 : 0.2, s == 3 && beat % 2 ? 43 : 40 });
        return n;
    }
    const int riff[] = { 64, 67, 71, 64, 72, 71, 67, 66 };
    for (int i = 0; i < 8; ++i) n.push_back ({ i * 0.5, 0.4, riff[i] });
    for (int k : { 52, 59, 64, 67 }) n.push_back ({ 4.0, 3.0, k });
    return n;
}
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init;
    const bool trim = argc > 1 && juce::String (argv[1]) == "--trim";
    const juce::File outDir = argc > 1 && ! trim ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File();
    constexpr double kTarget = -17.0; // loudness while playing, dB
    juce::StringArray trims;
    if (outDir != juce::File()) outDir.createDirectory();

    const auto& list = kube::presets();
    int failures = 0, index = 0;
    double worstCpu = 0.0; juce::String worstName;
    juce::WavAudioFormat wav;

    const int dump = juce::SystemStats::getEnvironmentVariable ("KUBE_DUMP", "-1").getIntValue();
    for (auto& p : list)
    {
        auto values = p.values;
        if (index == dump) // KUBE_DUMP=<index> lists the preset's values
            for (auto& [id, x] : values) std::printf ("  %s = %g\n", id.toRawUTF8(), x);
        const auto s = kube::Settings::fromValues ([&] (const char* id) { return values.at (id); });
        kube::Engine engine;
        engine.prepare (kSr, kBlock);

        const auto notes = pattern (p.category);
        const int total = (int) (kSr * 60.0 / kBpm * 10.0); // 8 beats of notes + 2 of tail
        juce::AudioBuffer<float> out (2, total);
        double peak = 0, sumSq = 0; bool bad = false;

        const auto t0 = juce::Time::getHighResolutionTicks();
        for (int pos = 0; pos < total; pos += kBlock)
        {
            const int n = juce::jmin (kBlock, total - pos);
            juce::MidiBuffer midi;
            const double b0 = pos / kSr * kBpm / 60.0, b1 = (pos + n) / kSr * kBpm / 60.0;
            for (auto& nt : notes)
            {
                auto at = [&] (double b) { return (int) std::round ((b - b0) * 60.0 / kBpm * kSr); };
                if (nt.beat >= b0 && nt.beat < b1) midi.addEvent (juce::MidiMessage::noteOn (1, nt.note, 0.9f), at (nt.beat));
                if (nt.beat + nt.len >= b0 && nt.beat + nt.len < b1) midi.addEvent (juce::MidiMessage::noteOff (1, nt.note), at (nt.beat + nt.len));
            }
            juce::AudioBuffer<float> block (2, n);
            block.clear();
            skazi::HostTime host; host.bpm = kBpm; host.ppq = b0; host.playing = true;
            engine.process (block, midi, s, host);
            for (int c = 0; c < 2; ++c)
            {
                out.copyFrom (c, pos, block, c, 0, n);
                for (int i = 0; i < n; ++i)
                {
                    const float x = block.getSample (c, i);
                    if (! std::isfinite (x)) bad = true;
                    peak = juce::jmax (peak, (double) std::abs (x));
                    sumSq += x * x;
                }
            }
        }
        const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
        const double cpu = secs / (total / kSr) * 100.0;
        if (cpu > worstCpu) { worstCpu = cpu; worstName = p.name; }
        // loudness while playing: the power of the loudest 30% of 50 ms windows
        std::vector<double> windows;
        const int win = (int) (kSr * 0.05);
        for (int w0 = 0; w0 + win <= total; w0 += win)
        {
            double e = 0;
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < win; ++i) e += (double) out.getSample (c, w0 + i) * out.getSample (c, w0 + i);
            windows.push_back (e / (2.0 * win));
        }
        std::sort (windows.rbegin(), windows.rend());
        double energy = 0;
        const int top = juce::jmax (1, (int) windows.size() * 3 / 10);
        for (int i = 0; i < top; ++i) energy += windows[(size_t) i];
        const double loudDb = juce::Decibels::gainToDecibels (std::sqrt (energy / top), -120.0);
        juce::ignoreUnused (sumSq);

        const bool quiet = loudDb < kTarget - 6.0 && peak < 0.75, hot = loudDb > kTarget + 4.0, loud = peak > 0.97;
        if (bad || quiet || hot || loud) ++failures;
        std::printf ("%3d %-8s %-22s peak %5.2f  loud %6.1f dB  cpu %5.2f%%%s%s%s\n", index, p.category.toRawUTF8(), p.name.toRawUTF8(),
                     peak, loudDb, cpu, bad ? "  NAN" : "", quiet ? "  QUIET" : hot ? "  HOT" : "", loud ? "  CLIPS" : "");

        if (trim)
        {
            const float old = index < (int) std::size (kube::kPresetTrims) ? kube::kPresetTrims[index] : 0.0f;
            double delta = kTarget - loudDb;
            delta = juce::jmin (delta, juce::Decibels::gainToDecibels (0.8 / juce::jmax (1.0e-6, peak))); // keep peaks clear of the clipper
            trims.add (juce::String (juce::jlimit (-30.0, 24.0, old + delta), 1) + "f");
        }

        if (outDir != juce::File())
        {
            const auto f = outDir.getChildFile (juce::String (index).paddedLeft ('0', 3) + " " + p.category + " " + p.name + ".wav");
            f.deleteFile();
            if (auto stream = std::unique_ptr<juce::OutputStream> (f.createOutputStream()))
                if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), kSr, 2, 24, {}, 0)))
                {
                    stream.release();
                    w->writeFromAudioSampleBuffer (out, 0, total);
                }
        }
        ++index;
    }
    // Serum-style import: write an 8-frame table (saw morphing to square), load it and play it
    {
        const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("kube_test_table.wav");
        juce::AudioBuffer<float> table (1, 8 * kube::kTableSize);
        for (int f = 0; f < 8; ++f)
            for (int i = 0; i < kube::kTableSize; ++i)
            {
                const float x = i / (float) kube::kTableSize, saw = 1.0f - 2.0f * x, sq = x < 0.5f ? 1.0f : -1.0f;
                table.setSample (0, f * kube::kTableSize + i, 0.8f * (saw + (sq - saw) * f / 7.0f));
            }
        file.deleteFile();
        if (auto stream = std::unique_ptr<juce::OutputStream> (file.createOutputStream()))
            if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), 44100.0, 1, 32, {}, 0)))
            {
                stream.release();
                w->writeFromAudioSampleBuffer (table, 0, table.getNumSamples());
            }
        juce::String error;
        auto loaded = kube::loadWavetable (file, error);
        const bool framesOk = loaded != nullptr && loaded->numFrames() == 8;
        float peak = 0.0f;
        if (loaded != nullptr)
        {
            kube::Engine engine;
            engine.prepare (kSr, kBlock);
            engine.userTables[0].store (loaded.get());
            auto values = skazi::defaultValues (kube::paramDefs());
            values["aTable"] = (float) kube::kUserTable;
            values["aPos"] = 100.0f;
            const auto s = kube::Settings::fromValues ([&] (const char* id) { return values.at (id); });
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 57, 0.9f), 0);
            juce::AudioBuffer<float> block (2, kBlock);
            for (int b = 0; b < 40; ++b)
            {
                block.clear();
                engine.process (block, midi, s, {});
                midi.clear();
                peak = juce::jmax (peak, block.getMagnitude (0, kBlock));
            }
        }
        const bool ok = framesOk && peak > 0.05f && peak < 1.0f;
        if (! ok) ++failures;
        std::printf ("\nUSER wavetable: %s (%d frames, peak %.2f)%s\n", loaded != nullptr ? "loaded" : error.toRawUTF8(),
                     loaded != nullptr ? loaded->numFrames() : 0, peak, ok ? "" : "  FAILED");
        file.deleteFile();
    }

    if (trim)
    {
        juce::String body = "#pragma once\n\n"
                            "// Written by KubeTest --trim: dB added to each factory preset's Output so they all play equally loud.\n"
                            "namespace kube\n{\ninline constexpr float kPresetTrims[] = {\n";
        for (int i = 0; i < trims.size(); i += 12)
        {
            juce::StringArray row;
            for (int j = i; j < juce::jmin (i + 12, trims.size()); ++j) row.add (trims[j]);
            body << "    " << row.joinIntoString (", ") << ",\n";
        }
        body << "};\n} // namespace kube\n";
        juce::File::getCurrentWorkingDirectory().getChildFile ("Source/PresetTrims.h").replaceWithText (body);
        std::printf ("wrote Source/PresetTrims.h\n");
    }
    std::printf ("\n%d presets, %d failed, heaviest %s at %.2f%% of one core\n", (int) list.size(), failures, worstName.toRawUTF8(), worstCpu);
    return failures == 0 ? 0 : 1;
}
