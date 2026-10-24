// Checks the preset system: every factory kick loads cleanly and sounds sane,
// user presets round-trip through a file, the "modified" mark tracks edits,
// and the host state brings the preset name back.
//   KickassPresetTest [folder]   also writes every factory kick there as a WAV to audition

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "KickLibrary.h"

static int failures = 0;

static void check (bool ok, const juce::String& what)
{
    if (! ok)
    {
        ++failures;
        std::printf ("FAIL  %s\n", what.toRawUTF8());
    }
}

static bool near (float a, float b) { return std::abs (a - b) <= 0.001f * juce::jmax (1.0f, std::abs (b)); }

static bool sameParams (const kick::Params& a, const kick::Params& b)
{
    return near (a.startHz, b.startHz) && near (a.punchHz, b.punchHz) && near (a.endHz, b.endHz)
        && near (a.snapMs, b.snapMs) && near (a.pitchMs, b.pitchMs) && near (a.holdMs, b.holdMs)
        && near (a.ampMs, b.ampMs) && near (a.click, b.click) && near (a.offsetMs, b.offsetMs)
        && near (a.drive, b.drive) && near (a.levelDb, b.levelDb) && near (a.clickTune, b.clickTune)
        && near (a.clickHpfHz, b.clickHpfHz) && near (a.clickLpfHz, b.clickLpfHz)
        && near (a.subLevel, b.subLevel) && near (a.sampleLenMs, b.sampleLenMs)
        && near (a.eqLowDb, b.eqLowDb) && near (a.eqMidDb, b.eqMidDb) && near (a.eqMidHz, b.eqMidHz)
        && near (a.eqHighDb, b.eqHighDb);
}

static int presetIndex (const char* name)
{
    const auto& bank = kick::factoryPresets();
    for (int i = 0; i < (int) bank.size(); ++i)
        if (juce::String (bank[(size_t) i].name) == name)
            return i;
    return 0;
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    KickassProcessor proc;
    const auto& bank = kick::factoryPresets();
    const juce::File wavDir = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File();
    if (wavDir != juce::File())
        wavDir.createDirectory();

    // Factory kicks
    for (int i = 0; i < (int) bank.size(); ++i)
    {
        const auto& pre = bank[(size_t) i];
        const juce::String label = juce::String (pre.category) + " / " + pre.name;
        proc.setCurrentProgram (i);

        check (proc.getCurrentProgram() == i, label + ": program index");
        check (sameParams (proc.currentParams(), pre.params), label + ": values out of parameter range");
        check (! proc.isPresetModified(), label + ": marked modified right after loading");
        check (proc.getPresetDisplayName() == label, label + ": display name is " + proc.getPresetDisplayName());

        kick::Engine e;
        e.setParams (pre.params);
        e.trigger();
        juce::AudioBuffer<float> audio (1, 44100 * 2);
        float peak = 0.0f;
        int lastSound = 0;
        for (int n = 0; n < audio.getNumSamples(); ++n)
        {
            const float s = e.processSample (1.0f / 44100.0f);
            audio.setSample (0, n, s);
            peak = juce::jmax (peak, std::abs (s));
            if (std::abs (s) > 0.001f)
                lastSound = n;
        }
        check (peak > 0.2f && peak <= 1.0f, label + ": peak " + juce::String (peak, 3));

        // A kick, not a zap: by 30 ms the body must be near its root note (60 ms for the tight styles).
        const juce::String cat (pre.category);
        const float root = pre.params.endHz;
        const float at30 = kick::pitchAt (pre.params, 30.0f), at60 = kick::pitchAt (pre.params, 60.0f);
        check (pre.params.drive == 0.0f, label + ": drive is not off");
        const bool tight = cat == "Psytrance" || cat == "Progressive" || cat == "Goa Forest"
                        || cat == "House" || cat == "Tech House";
        if (tight || cat == "Techno" || cat == "Init")
            check (at30 < 3.0f * root, label + ": still at " + juce::String (at30, 0) + " Hz after 30 ms (laser)");
        if (tight)
            check (at60 < 1.6f * root, label + ": still at " + juce::String (at60, 0) + " Hz after 60 ms");
        if (cat == "Psytrance" || cat == "Goa Forest")
            check (lastSound < 44100 * 0.3, label + ": tail runs past 300 ms, into the bassline");

        std::printf ("  %-28s peak %.2f  pitch@30ms %4.0f Hz  @60ms %4.0f Hz  length %4d ms\n", label.toRawUTF8(),
                     peak, at30, at60, lastSound * 1000 / 44100);

        if (wavDir != juce::File())
        {
            const auto file = wavDir.getChildFile (juce::File::createLegalFileName (
                juce::String (i + 1).paddedLeft ('0', 2) + " " + pre.category + " - " + pre.name + ".wav"));
            file.deleteFile();
            audio.setSize (1, juce::jmax (4410, lastSound + 2205), true);
            juce::WavAudioFormat wav;
            if (auto out = std::unique_ptr<juce::AudioFormatWriter> (
                    wav.createWriterFor (new juce::FileOutputStream (file), 44100.0, 1, 24, {}, 0)))
                out->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
        }
    }

    // Edits show as modified
    proc.loadFactoryPreset (3);
    proc.apvts.getParameter ("decay")->setValueNotifyingHost (0.9f);
    check (proc.isPresetModified(), "edit not marked modified");
    check (proc.getPresetDisplayName().endsWith (" *"), "modified name has no *");

    // User preset round trip
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("kickass_preset_test");
    dir.deleteRecursively();
    const auto file = dir.getChildFile ("My Test Kick.xml");
    const auto saved = proc.currentParams();
    check (proc.saveUserPreset (file), "save failed");
    check (! proc.isPresetModified(), "modified right after saving");
    check (proc.getPresetDisplayName() == "User / My Test Kick", "saved name is " + proc.getPresetDisplayName());

    proc.loadFactoryPreset (0);
    check (! sameParams (proc.currentParams(), saved), "factory load did not change the sound");
    check (proc.loadUserPreset (file), "load failed");
    check (sameParams (proc.currentParams(), saved), "user preset did not restore its values");
    check (proc.getPresetIndex() == -1 && proc.getUserPresetFile() == file, "user preset not tracked");
    check (! proc.isPresetModified(), "modified right after loading user preset");

    // Host state keeps the label and starts clean
    proc.loadFactoryPreset (5);
    proc.apvts.getParameter ("click")->setValueNotifyingHost (0.1f);
    juce::MemoryBlock state;
    proc.getStateInformation (state);
    KickassProcessor reopened;
    reopened.setStateInformation (state.getData(), (int) state.getSize());
    check (sameParams (reopened.currentParams(), proc.currentParams()), "state did not restore values");
    check (reopened.getPresetIndex() == 5, "state lost the preset index");
    check (! reopened.isPresetModified(), "reopened session marked modified");

    // Stepping wraps around the factory bank (when no user presets exist yet)
    if (KickassProcessor::getUserPresets().isEmpty())
    {
        proc.loadFactoryPreset (0);
        proc.stepPreset (-1);
        check (proc.getPresetIndex() == (int) bank.size() - 1, "< from the first kick did not wrap to the last");
        proc.stepPreset (1);
        check (proc.getPresetIndex() == 0, "> from the last kick did not wrap to the first");
    }
    proc.loadFactoryPreset (2);
    proc.stepPreset (1);
    check (proc.getPresetIndex() == 3, "> did not go to the next kick");

    dir.deleteRecursively();

    // MIDI: a note-on plays the kick; key track follows the note.
    auto playNote = [] (KickassProcessor& p, int note, bool track)
    {
        p.apvts.getParameter ("keytrack")->setValueNotifyingHost (track ? 1.0f : 0.0f);
        p.setPlayConfigDetails (0, 2, 44100.0, 512);
        p.prepareToPlay (44100.0, 512);
        juce::AudioBuffer<float> out (2, 512);
        std::vector<float> mono;
        for (int block = 0; block < 40; ++block)
        {
            juce::MidiBuffer midi;
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 127), 0);
            p.processBlock (out, midi);
            mono.insert (mono.end(), out.getReadPointer (0), out.getReadPointer (0) + 512);
        }
        return mono;
    };
    auto crossings = [] (const std::vector<float>& x, size_t from, size_t to)
    {
        int n = 0;
        for (size_t i = from + 1; i < to; ++i)
            n += (x[i - 1] < 0.0f) != (x[i] < 0.0f);
        return n;
    };

    KickassProcessor midiProc;
    midiProc.loadFactoryPreset (presetIndex ("Long 808")); // a long, steady tail to count cycles on
    const auto atRoot = playNote (midiProc, 36, true);
    const auto octaveUp = playNote (midiProc, 48, true);
    const auto untracked = playNote (midiProc, 48, false);
    float peak = 0.0f;
    for (auto s : atRoot) peak = juce::jmax (peak, std::abs (s));
    check (peak > 0.2f, "MIDI note-on made no sound");
    const int c36 = crossings (atRoot, 8820, 17640), c48 = crossings (octaveUp, 8820, 17640),
              cOff = crossings (untracked, 8820, 17640);
    std::printf ("  tail zero crossings: C2 %d, C3 tracked %d, C3 untracked %d\n", c36, c48, cOff);
    check (std::abs (c48 - 2 * c36) <= 2, "key track: an octave up is not double the pitch");
    check (std::abs (cOff - c36) <= 1, "key track off: the note changed the pitch");

    // Click tune and filters
    auto clickOnly = [] (kick::Params p)
    {
        p.ampMs = 20.0f; p.endHz = p.startHz = 40.0f; p.drive = 0.0f; p.levelDb = 0.0f; p.click = 1.0f;
        auto s = std::make_shared<kick::ClickSample>();
        for (int i = 0; i < 4410; ++i) // 100 ms of 200 Hz + 6 kHz
            s->data.push_back (0.5f * std::sin (6.2831853f * 200.0f * i / 44100.0f)
                               + 0.5f * std::sin (6.2831853f * 6000.0f * i / 44100.0f));
        kick::Engine e;
        e.setParams (p);
        e.setClickSample (s);
        e.trigger();
        std::vector<float> click;
        for (int i = 0; i < 8820; ++i)
            click.push_back (e.processFrame (1.0f / 44100.0f).click);
        return click;
    };
    auto energy = [] (const std::vector<float>& x, size_t from, size_t to)
    {
        double sum = 0.0;
        for (size_t i = from; i < to; ++i) sum += x[i] * x[i];
        return sum;
    };
    kick::Params flat;
    const auto dry = clickOnly (flat);
    auto hp = flat; hp.clickHpfHz = 2000.0f;
    auto lp = flat; lp.clickLpfHz = 1000.0f;
    auto up = flat; up.clickTune = 12.0f;
    const double eDry = energy (dry, 200, 4000);
    check (energy (clickOnly (hp), 200, 4000) < 0.65 * eDry, "click HPF did not remove the 200 Hz part");
    check (energy (clickOnly (lp), 200, 4000) < 0.65 * eDry, "click LPF did not remove the 6 kHz part");
    const auto tuned = clickOnly (up);
    check (energy (tuned, 2400, 4400) < 0.01 * energy (dry, 2400, 4400), "click tune +12 did not halve its length");
    // Count cycles of the 200 Hz part alone (the LPF removes the 6 kHz part, which would swamp the count).
    auto lpUp = lp; lpUp.clickTune = 12.0f;
    const int cLow = crossings (clickOnly (lp), 200, 2000), cLowUp = crossings (clickOnly (lpUp), 200, 2000);
    std::printf ("  click 200 Hz part crossings: untuned %d, +12 st %d\n", cLow, cLowUp);
    check (std::abs (cLowUp - 2 * cLow) <= 2, "click tune +12 did not double its pitch");

    // DRIVE 0 is bit-exact clean; the EQ bands each do their job.
    {
        auto render = [] (kick::Params p)
        {
            p.click = 0.0f; p.levelDb = 0.0f;
            kick::Engine e;
            e.setParams (p);
            e.trigger();
            std::vector<float> out;
            for (int i = 0; i < 44100 / 2; ++i)
                out.push_back (e.processSample (1.0f / 44100.0f));
            return out;
        };
        kick::Params clean;
        clean.drive = 0.0f;
        {
            kick::Engine e;
            auto p = clean; p.click = 0.0f; p.levelDb = 0.0f;
            e.setParams (p);
            e.trigger();
            float worst = 0.0f;
            for (int i = 0; i < 22050; ++i)
            {
                const auto f = e.processFrame (1.0f / 44100.0f);
                worst = juce::jmax (worst, std::abs (f.out - (f.sub + f.click)));
            }
            check (worst == 0.0f, "DRIVE 0 changes the sound (" + juce::String (worst) + ")");
        }
        auto driven = clean; driven.drive = 0.5f;
        const auto a = render (clean), b = render (driven);
        double diff = 0;
        for (size_t i = 0; i < a.size(); ++i) diff += std::abs (a[i] - b[i]);
        check (diff > 1.0, "DRIVE 0.5 does nothing");

        // Energy of the tail (200-400 ms, pure root note) for LOW; of the snap (first 3 ms) for HIGH.
        auto energy = [] (const std::vector<float>& x, size_t from, size_t to)
        {
            double sum = 0;
            for (size_t i = from; i < to; ++i) sum += x[i] * x[i];
            return sum;
        };
        auto lowUp = clean; lowUp.eqLowDb = 6.0f; lowUp.holdMs = 400.0f;
        auto flatLong = clean; flatLong.holdMs = 400.0f;
        const double ratioLow = energy (render (lowUp), 8820, 17640) / energy (render (flatLong), 8820, 17640);
        // HIGH is a shelf at 5 kHz: +6 dB should about double a 10 kHz tone (x4 energy).
        kick::Biquad shelf;
        shelf.setupShelf (true, kick::eqHighHz, 6.0f, 44100.0f);
        double eIn = 0, eOut = 0;
        for (int i = 0; i < 4410; ++i)
        {
            const float x = std::sin (6.2831853f * 10000.0f * (float) i / 44100.0f);
            const float y = shelf.process (x);
            if (i > 441) { eIn += x * x; eOut += y * y; }
        }
        const double ratioHigh = eOut / eIn;
        auto midCut = clean; midCut.eqMidDb = -9.0f; midCut.eqMidHz = 180.0f;
        const double ratioMid = energy (render (midCut), 0, 1323) / energy (a, 0, 1323);
        std::printf ("  EQ: LOW +6 dB -> x%.2f tail energy, HIGH +6 dB -> x%.2f energy at 10 kHz, MID -9 dB @180 Hz -> x%.2f body energy\n",
                     ratioLow, ratioHigh, ratioMid);
        check (ratioLow > 3.0, "EQ LOW does not boost the root note");
        check (ratioHigh > 3.0, "EQ HIGH does not boost 10 kHz");
        check (ratioMid < 0.7, "EQ MID does not cut the punch");
    }

    // Sample LENGTH trims the layer: full level early, silent after LENGTH.
    {
        kick::Params p;
        p.sampleLenMs = 100.0f;
        check (kick::sampleGainAt (p, 50.0f) == 1.0f && kick::sampleGainAt (p, 100.0f) < 1.0e-6f
                   && kick::sampleGainAt (p, 85.0f) > 0.0f && kick::sampleGainAt (p, 85.0f) < 1.0f,
               "sample LENGTH fade is wrong");
        p.sampleLenMs = 2000.0f;
        check (kick::sampleGainAt (p, 1900.0f) == 1.0f, "sample LENGTH at max should not fade");
    }

    // Kick library: scans the user's folder (Splice by default) and every kick it lists loads.
    {
        juce::SharedResourcePointer<KickLibrary> library;
        for (int i = 0; i < 300 && library->isScanning(); ++i)
            juce::Thread::sleep (100);
        const auto entries = library->getEntries();
        std::printf ("  kick library: %d kicks in %s\n", (int) entries.size(),
                     library->getFolder().getFullPathName().toRawUTF8());
        if (library->getFolder().getChildFile ("packs").isDirectory())
            check (! entries.empty(), "kick library found no kicks in the Splice folder");

        KickassProcessor libProc;
        int loaded = 0;
        for (const auto& e : entries)
        {
            if (libProc.loadClickSample (e.file))
                ++loaded;
            else
                std::printf ("  could not load %s\n", e.file.getFullPathName().toRawUTF8());

            auto s = libProc.getClickSample();
            float peak = 0.0f;
            if (s != nullptr)
                for (auto v : s->data) peak = juce::jmax (peak, std::abs (v));
            check (s == nullptr || std::abs (peak - 1.0f) < 0.001f || peak < 1.0e-4f,
                   e.file.getFileName() + ": not peak-normalised");
        }
        std::printf ("  loaded %d / %d library kicks\n", loaded, (int) entries.size());
        check (loaded == (int) entries.size(), "some library kicks failed to load");
    }

    std::printf (failures == 0 ? "ALL PASSED (%d factory kicks)\n" : "%d FAILURE(S)\n",
                 failures == 0 ? (int) bank.size() : failures);
    return failures == 0 ? 0 : 1;
}
