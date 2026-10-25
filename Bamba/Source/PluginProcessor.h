#pragma once

#include "../../Common/SkaziProcessor.h"
#include "BambaEngine.h"
#include "RiserLibrary.h"

class BambaProcessor : public skazi::SkaziProcessor
{
public:
    BambaProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;

    bool acceptsMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 6.0; }

    void setStateInformation (const void* data, int sizeInBytes) override;

    const std::vector<skazi::Preset>& presetList() const override;
    skazi::Preset randomPreset (juce::Random& rng) const override;
    void presetLoaded (const skazi::Preset&) override;

    bamba::Settings settings() const { return bamba::Settings::fromValues ([this] (const char* id) { return v (id); }); }

    // Splice sample layer (message thread). The file path is saved with the state.
    bool loadSample (const juce::File&);
    void clearSample();
    bamba::SamplePtr getSample() const { return std::atomic_load (&sampleData); }
    juce::File getSampleFile() const { return juce::File (apvts.state.getProperty ("sampleFile").toString()); }

    // The browser's filters, saved with the session: kind -1 = all kinds, length RiserLibrary::Length
    int getBrowseKind() const { return (int) apvts.state.getProperty ("browseKind", -1); }
    int getBrowseLength() const { return (int) apvts.state.getProperty ("browseLength", 0); }
    void setBrowseFilter (int kind, int length);
    /** The library's samples that pass the filters, shortest first. */
    std::vector<RiserLibrary::Entry> browseList() const;
    /** < and >: the next sample in the filtered list; loads it and plays it. */
    void stepSample (int delta);

    /** Plays the whole effect once (C3) on the next audio block, for the audition buttons. */
    void audition() { auditionRequest = true; }

    bamba::Engine engine;
    juce::SharedResourcePointer<RiserLibrary> library;

private:
    void setSample (bamba::SamplePtr);

    bamba::SamplePtr sampleData;               // swapped atomically; the audio thread copies it per block
    std::vector<bamba::SamplePtr> retired;     // keeps replaced samples alive so the audio thread never frees one
    std::atomic<bool> auditionRequest { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BambaProcessor)
};
