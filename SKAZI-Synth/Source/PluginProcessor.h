#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include "SynthVoice.h"
#include "Effects.h"
#include "Patterns.h"

constexpr int kNumVoices = 16;   // poly voices; one extra voice is reserved for the sequencer

class AcidanceProcessor : public juce::AudioProcessor
{
public:
    AcidanceProcessor();
    ~AcidanceProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SKAZI"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }   // PANIC sends CC 123 + CC 121
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    void randomizeSound(); // RND button: random style + random effects
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setParam (const juce::String& id, float value);
    void resetAllParams();
    std::unique_ptr<juce::XmlElement> createStateXml();
    void applyStateXml (const juce::XmlElement& xml);

    // Name shown in the preset bar (factory or user preset), message thread only
    juce::String currentPresetName, currentPresetFolder;

    juce::AudioProcessorValueTreeState apvts;
    BassBank seq;
    std::atomic<float> lfoCustom[kNumLfos][kLfoSteps];

    std::atomic<int> currentStep { -1 };
    std::atomic<bool> hostPlaying { false };
    std::atomic<int> programChanges { 0 };
    std::atomic<int> skin { 4 };   // 0 = Neon, 1 = Vintage, 2 = Chrome, 3 = Robot, 4 = Hyper (editor-only, saved with the session)
    std::atomic<float> uiScale { 0.9f };   // editor window size, saved with the session

    // PANIC: the editor sets this, the audio thread performs the reset
    std::atomic<bool> panicRequest { false };

    // Post-FX mono signal for the oscilloscope / spectrum display
    static constexpr int kScopeSize = 8192;
    std::array<std::atomic<float>, kScopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };

private:
    struct OscP { std::atomic<float>* on, *wave, *wtpos, *oct, *semi, *fine, *level, *uni, *detune, *blend, *phase, *rphase, *warp, *warpamt, *lock; };
    struct FltP { std::atomic<float>* type, *cutoff, *reso, *drive, *key, *env; };
    struct EnvP { std::atomic<float>* a, *d, *s, *r; };
    struct LfoP { std::atomic<float>* shape, *rate, *sync, *div, *trig, *phase, *fade; };
    struct ModP { std::atomic<float>* src, *dst, *amt; };

    void noteOn (int note, float vel, bool accent);
    void noteOff (int note);
    void seqStepStart (int step);
    void seqStepEnd();
    void allNotesOff();
    void panic();
    void resetControllers();
    void handleMidi (const juce::MidiMessage& m, bool seqArmed);
    bool seqGateOpen() const; // the pattern only plays while a key is held (or RUN in the standalone app)
    int findVoice();
    void resetLfoPhasesIfIdle();

    OscP oscP[2];
    FltP fltP[2];
    EnvP envP[kNumEnvs];
    LfoP lfoP[kNumLfos];
    ModP modP[kNumModSlots];
    std::atomic<float>* pSubWave, *pSubOct, *pSubLevel, *pNoiseType, *pNoiseLevel, *pNoisePitch, *pRouting;
    std::atomic<float>* pMode, *pGlide, *pBend, *pVelSens, *pAccent;
    std::atomic<float>* pSeqOn, *pSeqPattern, *pSeqTranspose, *pSeqGate, *pBpm, *pRun, *pShuffle;
    std::atomic<float>* pDistOn, *pDistType, *pDistDrive, *pDistMix;
    std::atomic<float>* pMfxOn, *pMfxType, *pMfxRate, *pMfxDepth, *pMfxFb, *pMfxMix;
    std::atomic<float>* pDlyOn, *pDlySync, *pDlyDiv, *pDlyMs, *pDlyPP, *pDlyFb, *pDlyMix;
    std::atomic<float>* pRevOn, *pRevSize, *pRevDecay, *pRevDamp, *pRevMix;
    std::atomic<float>* pCompOn, *pCompMode, *pCompThr, *pCompRatio, *pCompAtt, *pCompRel, *pCompMakeup, *pCompDepth;
    std::atomic<float>* pEqOn, *pEqLf, *pEqLg, *pEqMf, *pEqMq, *pEqMg, *pEqHf, *pEqHg;
    std::atomic<float>* pVolume, *pPan;
    std::atomic<float>* pOversample, *pWarmth;

    SynthVoice voices[kNumVoices + 1];
    VoiceParams vp;
    GlobalMod gm;
    LfoState globalLfo[kNumLfos];
    int lastVoice = 0;
    float lastVelocity = 1.f, lastKey = 0.f;

    int heldNotes[16] {};
    int numHeld = 0;

    double internalPos = 0.0;
    bool wasRunning = false;
    int64_t clockStep = std::numeric_limits<int64_t>::min();
    bool stepStarted = false, stepEndDone = false, lastSlide = false;
    int seqKeyOffset = 0;
    std::array<bool, 128> seqKeys {}; // keys held while the sequencer is armed
    int seqKeyCount = 0;

    Fx::Distortion dist;
    Fx::ModFx modFx;
    Fx::Delay delay;
    Fx::Reverb reverb;
    Fx::Compressor comp;
    Fx::Eq eq;

    int currentProgram = 0;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplers[3];   // 2x, 4x, 8x
    int oversampleBlock = 0, activeOversample = -1;
    double sr = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcidanceProcessor)
};
