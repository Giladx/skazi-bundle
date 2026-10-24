// Renders the KICKASS editor to a PNG without a host, for checking the skin.
//   KickassSkinShot <out.png> [click.wav|-] [offsetMs] [attack|full] [factory preset index] [fullkick|top]

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "WaveformDisplay.h"
#include "KickLibrary.h"

static WaveformDisplay* findDisplay (juce::Component& c)
{
    if (auto* d = dynamic_cast<WaveformDisplay*> (&c))
        return d;
    for (auto* child : c.getChildren())
        if (auto* d = findDisplay (*child))
            return d;
    return nullptr;
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const juce::File out = cwd.getChildFile (argc > 1 ? argv[1] : "kickass.png");

    KickassProcessor processor;
    if (argc > 5)
        processor.loadFactoryPreset (juce::String (argv[5]).getIntValue());
    if (argc > 2 && juce::String (argv[2]) != "-" && ! processor.loadClickSample (cwd.getChildFile (argv[2])))
    {
        std::fprintf (stderr, "could not load %s\n", argv[2]);
        return 1;
    }
    if (argc > 6)
    {
        if (juce::String (argv[6]) == "fullkick") processor.useSampleAsFullKick();
        if (juce::String (argv[6]) == "top")      processor.useSampleAsTopLayer();
    }
    if (argc > 3)
        if (auto* p = processor.apvts.getParameter ("offset"))
            p->setValueNotifyingHost (p->convertTo0to1 (juce::String (argv[3]).getFloatValue()));

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    // Let the kick library finish scanning and the panel's timer pick everything up.
    juce::SharedResourcePointer<KickLibrary> library;
    for (int i = 0; i < 100 && library->isScanning(); ++i)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
    if (argc > 4 && juce::String (argv[4]) == "attack")
        if (auto* d = findDisplay (*editor))
            d->setZoomed (true);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    out.deleteFile();
    juce::FileOutputStream stream (out);
    if (! stream.openedOk() || ! juce::PNGImageFormat().writeImageToStream (image, stream))
    {
        std::fprintf (stderr, "could not write %s\n", out.getFullPathName().toRawUTF8());
        return 1;
    }

    std::printf ("wrote %s (%dx%d)\n", out.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());
    editor.reset();
    return 0;
}
