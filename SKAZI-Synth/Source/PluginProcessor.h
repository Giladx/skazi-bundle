#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Common/PresetFader.h"
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
    void randomizeSound(); // RANDOM button: every section rolled again, loudness-checked
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
    skazi::PresetFader fader; // fades around preset changes
    BassBank seq;
    std::atomic<float> lfoCustom[kNumLfos][kLfoSteps];

    std::atomic<int> currentStep { -1 };
    std::atomic<bool> hostPlaying { false };

    // On-screen PITCH and MOD wheels: the editor writes, processBlock turns them into MIDI.
    // ui* follow every wheel move (screen or keyboard) so the wheels on screen move too.
    std::atomic<float> screenPitch { 0.f }, screenMod { 0.f }, uiBend { 0.f }, uiMod { 0.f };
    std::atomic<bool> pitchMoved { false }, modMoved { false };
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
    bool seqCanCatchUp() const;
    void arpKeyDown (int note, float vel, bool playing);
    void arpKeyUp (int note);
    void arpFire();
    void arpReleaseSounding();
    void arpStop();
    void arpRebuild();

    OscP oscP[2];
    FltP fltP[2];
    EnvP envP[kNumEnvs];
    LfoP lfoP[kNumLfos];
    ModP modP[kNumModSlots];
    std::atomic<float>* pSubWave, *pSubOct, *pSubLevel, *pNoiseType, *pNoiseLevel, *pNoisePitch, *pRouting;
    std::atomic<float>* pMode, *pGlide, *pBend, *pVelSens, *pAccent;
    std::atomic<float>* pSeqOn, *pSeqPattern, *pSeqTranspose, *pSeqGate, *pBpm, *pRun, *pShuffle;
    std::atomic<float>* pDistOn, *pDistType, *pDistDrive, *pDistMix, *pDistTone;
    std::atomic<float>* pFxfOn, *pFxfType, *pFxfCut, *pFxfRes, *pFxfDrive, *pFxfMix;
    std::atomic<float>* pWidOn, *pWidAmt, *pWidMono;
    std::atomic<float>* pArpOn, *pArpMode, *pArpDiv, *pArpOct, *pArpGate, *pArpSwing;
    std::atomic<float>* pMfxOn, *pMfxType, *pMfxRate, *pMfxDepth, *pMfxFb, *pMfxMix;
    std::atomic<float>* pDlyOn, *pDlySync, *pDlyDiv, *pDlyMs, *pDlyPP, *pDlyFb, *pDlyMix;
    std::atomic<float>* pRevOn, *pRevSize, *pRevDecay, *pRevDamp, *pRevMix;
    std::atomic<float>* pCompOn, *pCompMode, *pCompThr, *pCompRatio, *pCompAtt, *pCompRel, *pCompMakeup, *pCompDepth;
    std::atomic<float>* pEqOn, *pEqLf, *pEqLg, *pEqMf, *pEqMq, *pEqMg, *pEqHf, *pEqHg;
    std::atomic<float>* pVolume, *pPan;
    std::atomic<float>* pOversample, *pWarmth, *pDrift;

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
    double stepFrac = 0.0, stepOffset = 0.0;   // where the clock is inside the current 16th
    bool clockPlaying = false;
    int seqKeyOffset = 0;
    std::array<bool, 128> seqKeys {}; // keys held while the sequencer is armed
    int seqKeyCount = 0;

    // Arpeggiator: held keys are played one step at a time on the song grid (or a free clock)
    int arpHeld[32] {}, arpOrder[32] {};
    int arpHeldCount = 0;
    float arpVel = 1.f;
    int arpSeq[256] {};
    int arpSeqLen = 0, arpIndex = 0;
    bool arpDirty = true, arpWasActive = false;
    int64_t arpStep = std::numeric_limits<int64_t>::min();
    bool arpStarted = false, arpGateDone = false;
    double arpFrac = 0.0, arpOffset = 0.0, arpFreePos = 0.0;
    int arpSounding[32] {};
    int arpSoundingCount = 0;
    uint32_t arpRng = 0x2468aceu;
    int lastArpModeOct = -1;

    Fx::Distortion dist;
    Fx::FxFilter fxFilter;
    Fx::Width width;
    Fx::ModFx modFx;
    Fx::Delay delay;
    Fx::Reverb reverb;
    Fx::Compressor comp;
    Fx::Eq eq;

    int currentProgram = 0;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplers[3];   // 2x, 4x, 8x
    int oversampleBlock = 0, activeOversample = -1;
    // Every oversampling factor is padded to the same total delay, reported to the host,
    // so switching 1x..8x never moves the sound and the DAW compensates it exactly
    int latencyTotal = 0, latencyPad[4] {};
    std::vector<float> padBuf[2];
    int padPos = 0;
    double sr = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcidanceProcessor)
};
