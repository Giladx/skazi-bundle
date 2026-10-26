// Renders the KUBE editor to PNGs without a host, one per tab, after playing a few notes so the displays have something to show.
//   KubeShot <out folder> [factory preset index]
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "shots");
    outDir.createDirectory();

    KubeProcessor proc;
    const double sr = 48000.0;
    proc.setRateAndBufferSizeDetails (sr, 512);
    proc.prepareToPlay (sr, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    if (argc > 2) proc.loadPreset (juce::String (argv[2]).getIntValue());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

    auto* tabbed = dynamic_cast<juce::TabbedComponent*> (editor->findChildWithID ("tabs"));
    const int numTabs = tabbed != nullptr ? tabbed->getNumTabs() : 1;

    juce::AudioBuffer<float> buf (2, 512);
    for (int t = 0; t < numTabs; ++t)
    {
        if (tabbed != nullptr) tabbed->setCurrentTabIndex (t);
        for (int pos = 0; pos < (int) sr / 2; pos += 512)
        {
            juce::MidiBuffer midi;
            if (pos == 0) { midi.addEvent (juce::MidiMessage::noteOn (1, 45, 0.9f), 0); midi.addEvent (juce::MidiMessage::noteOn (1, 57, 0.9f), 0); }
            buf.clear();
            proc.processBlock (buf, midi);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);

        // real repaint cost: paint into a software image at 2x, as a HiDPI window would
        auto cost = [] (juce::Component& c, int times)
        {
            juce::Image im (juce::Image::ARGB, c.getWidth() * 2, c.getHeight() * 2, true, juce::SoftwareImageType());
            const auto start = juce::Time::getMillisecondCounterHiRes();
            for (int i = 0; i < times; ++i)
            {
                juce::Graphics g (im);
                g.addTransform (juce::AffineTransform::scale (2.0f));
                c.paintEntireComponent (g, true);
            }
            return (juce::Time::getMillisecondCounterHiRes() - start) / times;
        };
        std::printf ("tab %d: %.1f ms per full repaint at 2x\n", t + 1, cost (*editor, 10));
        if (tabbed != nullptr)
            for (auto* panel : tabbed->getCurrentContentComponent()->getChildren())
            {
                std::printf ("   %-30s %.2f ms\n", typeid (*panel).name(), cost (*panel, 10));
                for (auto* view : panel->getChildren())
                    std::printf ("      %-27s %.2f ms\n", typeid (*view).name(), cost (*view, 10));
            }
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        const auto out = outDir.getChildFile ("kube_tab" + juce::String (t + 1) + ".png");
        out.deleteFile();
        juce::FileOutputStream stream (out);
        if (! stream.openedOk() || ! juce::PNGImageFormat().writeImageToStream (image, stream))
        {
            std::fprintf (stderr, "could not write %s\n", out.getFullPathName().toRawUTF8());
            return 1;
        }
        std::printf ("wrote %s (%dx%d)\n", out.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());

        juce::MidiBuffer off;
        off.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        buf.clear();
        proc.processBlock (buf, off);
    }
    editor.reset();
    return 0;
}
