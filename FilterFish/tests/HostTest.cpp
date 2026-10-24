// Stress-test host: loads the built VST3 exactly like a DAW would and hammers it.
// Usage: FilterFishTest "<path to The FilterFish.vst3>"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>

using namespace juce;

namespace
{
int failures = 0;

void fail (const String& what)
{
    ++failures;
    std::cout << "  FAIL: " << what << std::endl;
}

// Feeds a mix of noise, a sweep and silence, returns the peak output (or -1 for NaN/inf).
float runBlocks (AudioPluginInstance& p, int numBlocks, int blockSize, Random& rng)
{
    AudioBuffer<float> buffer (2, blockSize);
    MidiBuffer midi;
    float peak = 0.0f;
    static double phase = 0.0;
    for (int b = 0; b < numBlocks; ++b)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            phase += 0.01 + 0.002 * (b % 50);
            const float s = (b % 7 == 3) ? 0.0f : 0.4f * (float) std::sin (phase) + 0.2f * (rng.nextFloat() * 2.0f - 1.0f);
            buffer.setSample (0, i, s);
            buffer.setSample (1, i, -s * 0.8f);
        }
        p.processBlock (buffer, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < blockSize; ++i)
            {
                const float v = buffer.getSample (ch, i);
                if (! std::isfinite (v)) return -1.0f;
                peak = jmax (peak, std::abs (v));
            }
    }
    return peak;
}

// a DAW keeps processing audio and running its message loop: do both until a preset fade-switch is done
void settle (AudioPluginInstance& p, int blockSize, Random& rng)
{
    const int blocks = jmax (2, 1024 / blockSize);
    for (int i = 0; i < 6; ++i)
    {
        runBlocks (p, blocks, blockSize, rng);
        MessageManager::getInstance()->runDispatchLoopUntil (1);
    }
}

void testEditor (AudioPluginInstance& p)
{
    std::unique_ptr<AudioProcessorEditor> editor (p.createEditorIfNeeded());
    if (editor == nullptr) { fail ("no editor"); return; }
    for (auto size : { Point<int> (1280, 800), Point<int> (1100, 688), Point<int> (1920, 1200) })
    {
        editor->setSize (size.x, size.y);
        auto snapshot = editor->createComponentSnapshot (editor->getLocalBounds()); // runs every paint() routine
        if (! snapshot.isValid()) fail ("editor snapshot failed");
    }
    // Pump the message loop so timers / repaints run while the editor exists
    MessageManager::getInstance()->runDispatchLoopUntil (300);
    p.editorBeingDeleted (editor.get());
}
} // namespace

int main (int argc, char* argv[])
{
    ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::cout << "usage: FilterFishTest <plugin.vst3>" << std::endl; return 2; }

    AudioPluginFormatManager formats;
    addDefaultFormatsToManager (formats);
    OwnedArray<PluginDescription> types;
    for (auto* f : formats.getFormats())
        f->findAllTypesForFile (types, String (argv[1]));
    if (types.isEmpty()) { std::cout << "plugin not found" << std::endl; return 2; }
    std::cout << "Testing " << types[0]->name << " by " << types[0]->manufacturerName << std::endl;

    Random rng (1234);

    for (int round = 0; round < 3; ++round)
    {
        std::cout << "Round " << round + 1 << ": load plugin" << std::endl;
        String error;
        auto plugin = formats.createPluginInstance (*types[0], 48000.0, 512, error);
        if (plugin == nullptr) { fail ("load: " + error); break; }

        const int numPrograms = plugin->getNumPrograms();
        if (plugin->getName() == "The FilterFish" && numPrograms != 801) fail ("expected 801 programs, got " + String (numPrograms));

        struct Config { double sr; int bs; };
        const Config configs[] = { { 44100.0, 512 }, { 48000.0, 64 }, { 96000.0, 1024 }, { 44100.0, 37 } };
        const int programStep = round == 0 ? 1 : 7; // round 1 checks every preset, later rounds sample them

        for (auto cfg : configs)
        {
            plugin->setPlayConfigDetails (2, 2, cfg.sr, cfg.bs);
            plugin->prepareToPlay (cfg.sr, cfg.bs);
            const int blocks = jmax (8, (int) (cfg.sr * 0.25 / cfg.bs)); // ~0.25 s of audio per preset

            int loud = 0;
            for (int prog = 0; prog < numPrograms; prog += programStep)
            {
                plugin->setCurrentProgram (prog);
                settle (*plugin, cfg.bs, rng); // let the preset fade-switch finish, like a DAW would
                const float peak = runBlocks (*plugin, blocks, cfg.bs, rng);
                if (peak < 0.0f) fail ("NaN/inf in preset " + String (prog) + " " + plugin->getProgramName (prog) + " @" + String (cfg.sr));
                else if (peak > 8.0f) { ++loud; if (loud <= 5) fail ("very loud (" + String (peak, 1) + ") preset " + String (prog) + " " + plugin->getProgramName (prog)); }
            }

            // Random fuzz: every parameter random, including extreme corners
            auto& params = plugin->getParameters();
            for (int it = 0; it < 150; ++it)
            {
                for (auto* prm : params)
                {
                    const float r = rng.nextFloat();
                    prm->setValueNotifyingHost (it % 10 == 0 ? (r < 0.5f ? 0.0f : 1.0f) : r);
                }
                const float peak = runBlocks (*plugin, 6, cfg.bs, rng);
                if (peak < 0.0f) fail ("NaN/inf during fuzz iteration " + String (it));
            }
            plugin->releaseResources();
            std::cout << "  ok: " << cfg.sr << " Hz / " << cfg.bs << " samples" << std::endl;
        }

        // State save / restore round trip
        plugin->setCurrentProgram (123);
        MessageManager::getInstance()->runDispatchLoopUntil (400); // audio stopped: the change lands via the message thread
        MemoryBlock state;
        plugin->getStateInformation (state);
        // check a continuous knob (switch-type parameters snap to their steps, which is correct)
        auto* p5 = plugin->getParameters()[5];
        for (auto* prm : plugin->getParameters())
            if (! prm->isDiscrete() && ! prm->isBoolean() && prm->getNumSteps() == AudioProcessor::getDefaultNumParameterSteps()) { p5 = prm; break; }
        const float before = p5->getValue();
        plugin->setCurrentProgram (456);
        MessageManager::getInstance()->runDispatchLoopUntil (400);
        p5->setValueNotifyingHost (before < 0.5f ? 0.9f : 0.1f); // make sure the restore has something to undo
        plugin->setStateInformation (state.getData(), (int) state.getSize());
        MessageManager::getInstance()->runDispatchLoopUntil (100);
        if (std::abs (p5->getValue() - before) > 1.0e-2f)
            fail ("state restore mismatch on '" + p5->getName (40) + "': saved " + String (before, 4) + ", restored " + String (p5->getValue(), 4));
        // a program change right before a project restore must not overwrite the restored sound
        plugin->setCurrentProgram (321);
        plugin->setStateInformation (state.getData(), (int) state.getSize());
        MessageManager::getInstance()->runDispatchLoopUntil (400);
        if (std::abs (p5->getValue() - before) > 1.0e-2f) fail ("program change overwrote the restored state on " + p5->getName (40));
        std::cout << "  ok: state save/restore" << std::endl;

        testEditor (*plugin);
        testEditor (*plugin);
        std::cout << "  ok: editor open/paint/close" << std::endl;

        plugin.reset(); // unload, like closing a project
        std::cout << "  ok: unload" << std::endl;
    }

    std::cout << (failures == 0 ? "ALL TESTS PASSED" : String (failures) + " FAILURE(S)") << std::endl;
    return failures == 0 ? 0 : 1;
}
