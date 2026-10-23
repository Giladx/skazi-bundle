#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    // Transparent below 0.8, then rounds off smoothly towards 1.0
    inline float softLimit (float x)
    {
        const float a = std::abs (x);
        if (a <= 0.8f) return x;
        return std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), x);
    }

    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;
    using API = juce::AudioParameterInt;
    using APC = juce::AudioParameterChoice;
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

    juce::ParameterID pid (const juce::String& s) { return { s, 2 }; }

    void addF (Layout& l, const juce::String& id, const juce::String& name, float lo, float hi, float def, float centre = 0.f)
    {
        juce::NormalisableRange<float> r (lo, hi);
        if (centre > 0.f) r.setSkewForCentre (centre);
        l.add (std::make_unique<APF> (pid (id), name, r, def));
    }
    void addC (Layout& l, const juce::String& id, const juce::String& name, const juce::StringArray& items, int def)
    {
        l.add (std::make_unique<APC> (pid (id), name, items, def));
    }
    void addB (Layout& l, const juce::String& id, const juce::String& name, bool def)
    {
        l.add (std::make_unique<APB> (pid (id), name, def));
    }
    void addI (Layout& l, const juce::String& id, const juce::String& name, int lo, int hi, int def)
    {
        l.add (std::make_unique<API> (pid (id), name, lo, hi, def));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout AcidanceProcessor::createLayout()
{
    Layout l;

    for (int o = 0; o < 2; ++o)
    {
        const juce::String p = "osc" + juce::String (o + 1) + "_";
        const juce::String n = "Osc " + juce::String (o + 1) + " ";
        addB (l, p + "on", n + "On", o == 0);
        addC (l, p + "wave", n + "Wave", Lists::oscWaves, 1);
        addF (l, p + "wtpos", n + "WT Position", 0.f, 1.f, 0.f);
        addI (l, p + "oct", n + "Octave", -3, 3, 0);
        addI (l, p + "semi", n + "Semi", -12, 12, 0);
        addF (l, p + "fine", n + "Fine", -100.f, 100.f, 0.f);
        addF (l, p + "level", n + "Level", 0.f, 1.f, 0.8f);
        addI (l, p + "uni", n + "Unison", 1, kMaxUnison, 1);
        addF (l, p + "detune", n + "Detune", 0.f, 1.f, 0.25f);
        addF (l, p + "blend", n + "Blend", 0.f, 1.f, 0.5f);
        addF (l, p + "phase", n + "Phase", 0.f, 1.f, 0.f);
        addB (l, p + "rphase", n + "Random Phase", true);
        addB (l, p + "lock", n + "Phase Lock", false);
        addC (l, p + "warp", n + "Warp Mode", Lists::warpModes, 0);
        addF (l, p + "warpamt", n + "Warp Amount", 0.f, 1.f, 0.f);
    }
    addC (l, "sub_wave", "Sub Wave", Lists::subWaves, 0);
    addC (l, "sub_oct", "Sub Octave", Lists::subOcts, 0);
    addF (l, "sub_level", "Sub Level", 0.f, 1.f, 0.f);
    addC (l, "noise_type", "Noise Type", Lists::noiseTypes, 0);
    addF (l, "noise_level", "Noise Level", 0.f, 1.f, 0.f);
    addF (l, "noise_pitch", "Noise Pitch", 0.f, 1.f, 1.f);

    for (int f = 0; f < 2; ++f)
    {
        const juce::String p = "flt" + juce::String (f + 1) + "_";
        const juce::String n = "Filter " + juce::String (f + 1) + " ";
        addC (l, p + "type", n + "Type", Lists::filterTypes, f == 0 ? 3 : 0);
        addF (l, p + "cutoff", n + "Cutoff", 20.f, 20000.f, f == 0 ? 2500.f : 800.f, 1000.f);
        addF (l, p + "reso", n + "Resonance", 0.f, 1.f, 0.2f);
        addF (l, p + "drive", n + "Drive", 0.f, 1.f, 0.f);
        addF (l, p + "key", n + "Key Track", 0.f, 1.f, 0.3f);
        addF (l, p + "env", n + "Env Amount", -1.f, 1.f, f == 0 ? 0.3f : 0.f);
    }
    addC (l, "flt_routing", "Filter Routing", Lists::routings, 0);

    const float envDef[4][4] = { { 0.003f, 0.6f, 0.8f, 0.25f }, { 0.001f, 0.4f, 0.2f, 0.3f },
                                 { 0.01f, 0.5f, 0.f, 0.3f }, { 0.5f, 1.f, 1.f, 0.5f } };
    for (int e = 0; e < kNumEnvs; ++e)
    {
        const juce::String p = "env" + juce::String (e + 1) + "_";
        const juce::String n = "Env " + juce::String (e + 1) + " ";
        addF (l, p + "a", n + "Attack", 0.001f, 8.f, envDef[e][0], 0.3f);
        addF (l, p + "d", n + "Decay", 0.005f, 8.f, envDef[e][1], 0.5f);
        addF (l, p + "s", n + "Sustain", 0.f, 1.f, envDef[e][2]);
        addF (l, p + "r", n + "Release", 0.005f, 10.f, envDef[e][3], 0.6f);
    }

    for (int i = 0; i < kNumLfos; ++i)
    {
        const juce::String p = "lfo" + juce::String (i + 1) + "_";
        const juce::String n = "LFO " + juce::String (i + 1) + " ";
        addC (l, p + "shape", n + "Shape", Lists::lfoShapes, 0);
        addF (l, p + "rate", n + "Rate", 0.02f, 500.f, 2.f, 2.f);   // up to audio rate
        addB (l, p + "sync", n + "Sync", false);
        addC (l, p + "div", n + "Division", Lists::divisions, 8);
        addC (l, p + "trig", n + "Trigger", Lists::lfoTrig, 0);
        addF (l, p + "phase", n + "Phase", 0.f, 1.f, 0.f);
        addF (l, p + "fade", n + "Fade In", 0.f, 5.f, 0.f, 0.5f);
    }

    for (int m = 0; m < kNumModSlots; ++m)
    {
        const juce::String p = "mod" + juce::String (m + 1) + "_";
        const juce::String n = "Mod " + juce::String (m + 1) + " ";
        addC (l, p + "src", n + "Source", Lists::modSources, 0);
        addC (l, p + "dst", n + "Destination", Lists::modDests, 0);
        addF (l, p + "amt", n + "Amount", -1.f, 1.f, 0.f);
    }

    addC (l, "mode", "Voice Mode", Lists::voiceModes, 0);
    addF (l, "glide", "Glide", 0.f, 2.f, 0.f, 0.2f);
    addI (l, "bend", "Pitch Bend Range", 0, 24, 2);
    addF (l, "vel_sens", "Velocity", 0.f, 1.f, 0.5f);
    addF (l, "warmth", "Analog Warmth", 0.f, 1.f, 0.3f);
    addC (l, "oversample", "Oversampling", juce::StringArray { "1x", "2x", "4x", "8x" }, 1);
    addF (l, "accent", "Accent", 0.f, 1.f, 0.5f);

    addB (l, "seq_on", "Sequencer On", false);
    addI (l, "seq_pattern", "Seq Pattern", 1, kNumPatterns, 1);
    addI (l, "seq_transpose", "Seq Transpose", -24, 24, 0);
    addF (l, "seq_gate", "Seq Gate", 0.1f, 1.f, 0.5f);
    l.add (std::make_unique<APF> (pid ("bpm"), "Tempo", juce::NormalisableRange<float> (60.f, 200.f, 0.1f), 132.f));
    addB (l, "run", "Run", false);
    addF (l, "shuffle", "Shuffle", 0.f, 1.f, 0.f);

    addB (l, "dist_on", "Dist On", false);
    addC (l, "dist_type", "Dist Type", Lists::distTypes, 0);
    addF (l, "dist_drive", "Dist Drive", 0.f, 1.f, 0.3f);
    addF (l, "dist_mix", "Dist Mix", 0.f, 1.f, 1.f);

    addB (l, "mfx_on", "Mod FX On", false);
    addC (l, "mfx_type", "Mod FX Type", Lists::modFxTypes, 0);
    addF (l, "mfx_rate", "Mod FX Rate", 0.02f, 8.f, 0.6f, 0.8f);
    addF (l, "mfx_depth", "Mod FX Depth", 0.f, 1.f, 0.5f);
    addF (l, "mfx_fb", "Mod FX Feedback", 0.f, 1.f, 0.2f);
    addF (l, "mfx_mix", "Mod FX Mix", 0.f, 1.f, 0.5f);

    addB (l, "dly_on", "Delay On", false);
    addB (l, "dly_sync", "Delay Sync", true);
    addC (l, "dly_div", "Delay Division", Lists::divisions, 6);
    addF (l, "dly_ms", "Delay Time", 1.f, 2000.f, 350.f, 300.f);
    addB (l, "dly_pp", "Delay Ping-Pong", true);
    addF (l, "dly_fb", "Delay Feedback", 0.f, 0.95f, 0.4f);
    addF (l, "dly_mix", "Delay Mix", 0.f, 1.f, 0.3f);

    addB (l, "rev_on", "Reverb On", false);
    addF (l, "rev_size", "Reverb Size", 0.f, 1.f, 0.6f);
    addF (l, "rev_decay", "Reverb Decay", 0.2f, 20.f, 2.5f, 2.5f);
    addF (l, "rev_damp", "Reverb High Cut", 1000.f, 20000.f, 7000.f, 5000.f);
    addF (l, "rev_mix", "Reverb Mix", 0.f, 1.f, 0.3f);

    addB (l, "comp_on", "Comp On", false);
    addC (l, "comp_mode", "Comp Mode", Lists::compModes, 0);
    addF (l, "comp_thr", "Comp Threshold", -60.f, 0.f, -18.f);
    addF (l, "comp_ratio", "Comp Ratio", 1.f, 20.f, 4.f, 4.f);
    addF (l, "comp_att", "Comp Attack", 0.1f, 100.f, 5.f, 10.f);
    addF (l, "comp_rel", "Comp Release", 10.f, 1000.f, 120.f, 150.f);
    addF (l, "comp_makeup", "Comp Makeup", 0.f, 24.f, 0.f);
    addF (l, "comp_depth", "OTT Depth", 0.f, 1.f, 0.5f);

    addB (l, "eq_on", "EQ On", false);
    addF (l, "eq_lf", "EQ Low Freq", 20.f, 1000.f, 120.f, 150.f);
    addF (l, "eq_lg", "EQ Low Gain", -18.f, 18.f, 0.f);
    addF (l, "eq_mf", "EQ Mid Freq", 100.f, 10000.f, 1000.f, 1000.f);
    addF (l, "eq_mq", "EQ Mid Q", 0.3f, 8.f, 1.f, 1.5f);
    addF (l, "eq_mg", "EQ Mid Gain", -18.f, 18.f, 0.f);
    addF (l, "eq_hf", "EQ High Freq", 1000.f, 20000.f, 8000.f, 6000.f);
    addF (l, "eq_hg", "EQ High Gain", -18.f, 18.f, 0.f);

    addF (l, "volume", "Master Volume", 0.f, 2.f, 0.8f);
    addF (l, "pan", "Master Pan", -1.f, 1.f, 0.f);
    return l;
}

AcidanceProcessor::AcidanceProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ACIDANCE", createLayout())
{
    auto get = [this] (const juce::String& id)
    {
        auto* p = apvts.getRawParameterValue (id);
        jassert (p != nullptr);
        return p;
    };

    for (int o = 0; o < 2; ++o)
    {
        const juce::String p = "osc" + juce::String (o + 1) + "_";
        oscP[o] = { get (p + "on"), get (p + "wave"), get (p + "wtpos"), get (p + "oct"), get (p + "semi"), get (p + "fine"),
                    get (p + "level"), get (p + "uni"), get (p + "detune"), get (p + "blend"), get (p + "phase"),
                    get (p + "rphase"), get (p + "warp"), get (p + "warpamt"), get (p + "lock") };
    }
    for (int f = 0; f < 2; ++f)
    {
        const juce::String p = "flt" + juce::String (f + 1) + "_";
        fltP[f] = { get (p + "type"), get (p + "cutoff"), get (p + "reso"), get (p + "drive"), get (p + "key"), get (p + "env") };
    }
    for (int e = 0; e < kNumEnvs; ++e)
    {
        const juce::String p = "env" + juce::String (e + 1) + "_";
        envP[e] = { get (p + "a"), get (p + "d"), get (p + "s"), get (p + "r") };
    }
    for (int i = 0; i < kNumLfos; ++i)
    {
        const juce::String p = "lfo" + juce::String (i + 1) + "_";
        lfoP[i] = { get (p + "shape"), get (p + "rate"), get (p + "sync"), get (p + "div"), get (p + "trig"), get (p + "phase"), get (p + "fade") };
        for (int s = 0; s < kLfoSteps; ++s) lfoCustom[i][s] = (s % 4 == 0) ? 1.f : -0.5f;
    }
    for (int m = 0; m < kNumModSlots; ++m)
    {
        const juce::String p = "mod" + juce::String (m + 1) + "_";
        modP[m] = { get (p + "src"), get (p + "dst"), get (p + "amt") };
    }
    pSubWave = get ("sub_wave"); pSubOct = get ("sub_oct"); pSubLevel = get ("sub_level");
    pNoiseType = get ("noise_type"); pNoiseLevel = get ("noise_level"); pNoisePitch = get ("noise_pitch");
    pRouting = get ("flt_routing");
    pMode = get ("mode"); pGlide = get ("glide"); pBend = get ("bend"); pVelSens = get ("vel_sens"); pAccent = get ("accent");
    pSeqOn = get ("seq_on"); pSeqPattern = get ("seq_pattern"); pSeqTranspose = get ("seq_transpose");
    pSeqGate = get ("seq_gate"); pBpm = get ("bpm"); pRun = get ("run"); pShuffle = get ("shuffle");
    pDistOn = get ("dist_on"); pDistType = get ("dist_type"); pDistDrive = get ("dist_drive"); pDistMix = get ("dist_mix");
    pMfxOn = get ("mfx_on"); pMfxType = get ("mfx_type"); pMfxRate = get ("mfx_rate"); pMfxDepth = get ("mfx_depth");
    pMfxFb = get ("mfx_fb"); pMfxMix = get ("mfx_mix");
    pDlyOn = get ("dly_on"); pDlySync = get ("dly_sync"); pDlyDiv = get ("dly_div"); pDlyMs = get ("dly_ms");
    pDlyPP = get ("dly_pp"); pDlyFb = get ("dly_fb"); pDlyMix = get ("dly_mix");
    pRevOn = get ("rev_on"); pRevSize = get ("rev_size"); pRevDecay = get ("rev_decay"); pRevDamp = get ("rev_damp"); pRevMix = get ("rev_mix");
    pCompOn = get ("comp_on"); pCompMode = get ("comp_mode"); pCompThr = get ("comp_thr"); pCompRatio = get ("comp_ratio");
    pCompAtt = get ("comp_att"); pCompRel = get ("comp_rel"); pCompMakeup = get ("comp_makeup"); pCompDepth = get ("comp_depth");
    pEqOn = get ("eq_on"); pEqLf = get ("eq_lf"); pEqLg = get ("eq_lg"); pEqMf = get ("eq_mf"); pEqMq = get ("eq_mq");
    pEqMg = get ("eq_mg"); pEqHf = get ("eq_hf"); pEqHg = get ("eq_hg");
    pVolume = get ("volume"); pPan = get ("pan");
    pOversample = get ("oversample"); pWarmth = get ("warmth");

    for (int v = 0; v <= kNumVoices; ++v) voices[v].seed ((uint32_t) (v + 1) * 7919u);
    for (int p = 0; p < kNumPatterns; ++p) seq.randomise (p, (uint32_t) (1234 + p * 77));

    Wavetables::get();   // build tables up front, not on the audio thread
    setCurrentProgram (0);
}

int AcidanceProcessor::getNumPrograms() { return Presets::count(); }

const juce::String AcidanceProcessor::getProgramName (int index)
{
    return "[" + Presets::category (index) + "] " + Presets::name (index);
}

void AcidanceProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    else
        jassertfalse;
}

void AcidanceProcessor::resetAllParams()
{
    static const juce::StringArray keep { "bpm", "run" };
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! keep.contains (rp->getParameterID()))
                rp->setValueNotifyingHost (rp->getDefaultValue());
}

void AcidanceProcessor::setCurrentProgram (int index)
{
    index = juce::jlimit (0, Presets::count() - 1, index);
    currentProgram = index;
    Presets::apply (index, *this);
    currentPresetName = Presets::name (index);
    currentPresetFolder = Presets::category (index);
    ++programChanges;
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

void AcidanceProcessor::randomizeSound()
{
    const auto style = Presets::randomSound (*this);
    currentPresetName = "Random " + style + " #" + juce::String (juce::Random::getSystemRandom().nextInt ({ 1000, 10000 }));
    currentPresetFolder = "Random";
    ++programChanges;
}

bool AcidanceProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void AcidanceProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    // Voices allocate for the highest oversampled rate once, then just switch rates
    for (auto& v : voices) v.prepare (sampleRate * 8.0);
    activeOversample = -1;
    oversampleBlock = std::max (samplesPerBlock, 512);
    for (int k = 0; k < 3; ++k)
    {
        oversamplers[k] = std::make_unique<juce::dsp::Oversampling<float>> (
            2, (size_t) (k + 1), juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        oversamplers[k]->initProcessing ((size_t) oversampleBlock);
    }
    numHeld = 0;
    modFx.prepare (sampleRate);
    delay.prepare (sampleRate);
    reverb.prepare (sampleRate);
    comp.prepare (sampleRate, samplesPerBlock);
    eq.prepare (sampleRate);
    clockStep = std::numeric_limits<int64_t>::min();
    wasRunning = false;
}

int AcidanceProcessor::findVoice()
{
    for (int v = 0; v < kNumVoices; ++v)
        if (! voices[v].isActive()) return v;
    int best = 0;
    uint32_t oldest = 0;
    for (int v = 0; v < kNumVoices; ++v)
        if (voices[v].isReleased() && voices[v].getAge() >= oldest) { oldest = voices[v].getAge(); best = v; }
    if (oldest > 0) return best;
    for (int v = 0; v < kNumVoices; ++v)
        if (voices[v].getAge() >= oldest) { oldest = voices[v].getAge(); best = v; }
    return best;
}

void AcidanceProcessor::resetLfoPhasesIfIdle()
{
    bool anyHeld = false;
    for (auto& v : voices) anyHeld |= (v.isActive() && ! v.isReleased());
    if (! anyHeld)
        for (int i = 0; i < kNumLfos; ++i)
            if (lfoP[i].sync->load() < 0.5f) globalLfo[i].phase = 0.f;
}

void AcidanceProcessor::noteOn (int note, float vel, bool accent)
{
    resetLfoPhasesIfIdle();
    lastVelocity = vel;
    lastKey = (note - 60.f) / 24.f;

    const int mode = (int) pMode->load();
    const bool glideOn = pGlide->load() > 0.0005f;

    if (mode == 0)
    {
        lastVoice = findVoice();
        voices[lastVoice].start (note, vel, accent, true, glideOn, vp);
        return;
    }

    const bool legato = numHeld > 0;
    if (numHeld < 16) heldNotes[numHeld++] = note;
    lastVoice = 0;
    if (mode == 1) voices[0].start (note, vel, accent, true, glideOn, vp);
    else           voices[0].start (note, vel, accent, ! legato, legato, vp);
}

void AcidanceProcessor::noteOff (int note)
{
    if ((int) pMode->load() == 0)
    {
        for (int v = 0; v < kNumVoices; ++v)
            if (voices[v].getNote() == note && ! voices[v].isReleased()) voices[v].release();
        return;
    }

    int idx = -1;
    for (int i = 0; i < numHeld; ++i) if (heldNotes[i] == note) idx = i;
    if (idx < 0)
    {
        // a note that never made the held list (more than 16 keys down): still let go of it
        if (numHeld == 0 && voices[0].getNote() == note && ! voices[0].isReleased()) voices[0].release();
        return;
    }
    const bool wasTop = idx == numHeld - 1;
    for (int i = idx; i < numHeld - 1; ++i) heldNotes[i] = heldNotes[i + 1];
    --numHeld;

    if (numHeld == 0) voices[0].release();
    else if (wasTop) voices[0].start (heldNotes[numHeld - 1], lastVelocity, false, false, true, vp);
}

void AcidanceProcessor::allNotesOff()
{
    for (auto& v : voices) v.release();
    numHeld = 0;
    lastSlide = false;
}

void AcidanceProcessor::resetControllers()
{
    gm.modWheel = 0.f;
    gm.aftertouch = 0.f;
    gm.bend = 0.f;
    uiBend.store (0.f);
    uiMod.store (0.f);
}

// Kill switch: silences every voice instantly and flushes all DSP state
void AcidanceProcessor::panic()
{
    for (auto& v : voices) v.hardReset();
    numHeld = 0;
    lastSlide = false;
    seqKeyOffset = 0;
    seqKeys.fill (false);
    seqKeyCount = 0;
    resetControllers();
    dist.reset();
    modFx.reset();
    delay.reset();
    reverb.reset();
    comp.reset();
    eq.reset();
}

bool AcidanceProcessor::seqGateOpen() const
{
    // The pattern never plays by itself: only while MIDI notes are held (a DAW clip or a keyboard).
    // RUN just provides the clock when no song is playing.
    return seqKeyCount > 0;
}

void AcidanceProcessor::seqStepStart (int step)
{
    currentStep.store (step);
    if (pSeqOn->load() < 0.5f) return;
    if (! seqGateOpen())
    {
        auto& sv = voices[kNumVoices];
        if (! sv.isReleased()) sv.release();
        lastSlide = false;
        return;
    }

    auto& sv = voices[kNumVoices];
    const int pat = juce::jlimit (0, kNumPatterns - 1, (int) pSeqPattern->load() - 1);
    auto& s = seq.steps[pat][step];
    if (s.gate.load())
    {
        const int note = s.midiNote() + (int) pSeqTranspose->load() + seqKeyOffset;
        const bool slide = lastSlide && sv.isActive() && ! sv.isReleased();
        if (! slide) resetLfoPhasesIfIdle();
        sv.start (note, 1.f, s.accent.load() != 0, ! slide, slide, vp);
        lastVoice = kNumVoices;
        lastSlide = s.slide.load() != 0;
    }
    else
    {
        if (! sv.isReleased()) sv.release();
        lastSlide = false;
    }
}

void AcidanceProcessor::seqStepEnd()
{
    auto& sv = voices[kNumVoices];
    if (! lastSlide && ! sv.isReleased()) sv.release();
}

void AcidanceProcessor::handleMidi (const juce::MidiMessage& m, bool seqArmed)
{
    if (m.isPitchWheel())
    {
        const float norm = juce::jlimit (-1.f, 1.f, (float) (m.getPitchWheelValue() - 8192) / 8192.f);
        gm.bend = norm * pBend->load();
        uiBend.store (norm);
        return;
    }
    if (m.isController())
    {
        const int cc = m.getControllerNumber();
        if (cc == 120) { panic(); return; }
        if (cc == 121) { resetControllers(); return; }
        if (cc == 123) { allNotesOff(); return; }
    }
    if (m.isController() && m.getControllerNumber() == 1)
    {
        gm.modWheel = m.getControllerValue() / 127.f;
        uiMod.store (gm.modWheel);
        return;
    }
    if (m.isChannelPressure()) { gm.aftertouch = m.getChannelPressureValue() / 127.f; return; }
    if (m.isAftertouch()) { gm.aftertouch = m.getAfterTouchValue() / 127.f; return; }

    if (seqArmed)
    {
        // While the sequencer is on, holding keys plays the pattern and transposes it (C3 = no transpose);
        // letting go of the last key stops it
        if (m.isNoteOn())
        {
            seqKeyOffset = m.getNoteNumber() - 60;
            if (! seqKeys[(size_t) m.getNoteNumber()]) { seqKeys[(size_t) m.getNoteNumber()] = true; ++seqKeyCount; }
        }
        else if (m.isNoteOff() && seqKeys[(size_t) m.getNoteNumber()])
        {
            seqKeys[(size_t) m.getNoteNumber()] = false;
            if (--seqKeyCount == 0 && ! seqGateOpen())
            {
                auto& sv = voices[kNumVoices];
                if (! sv.isReleased()) sv.release();
                lastSlide = false;
            }
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            seqKeys.fill (false);
            seqKeyCount = 0;
            allNotesOff();
        }
        return;
    }

    if (m.isNoteOn()) noteOn (m.getNoteNumber(), m.getFloatVelocity(), m.getVelocity() >= 100);
    else if (m.isNoteOff()) noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff()) allNotesOff();
}

void AcidanceProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    const bool panicNow = panicRequest.exchange (false);
    if (panicNow) panic();
    if (pitchMoved.exchange (false))
        midi.addEvent (juce::MidiMessage::pitchWheel (1, juce::jlimit (0, 16383, (int) std::round ((screenPitch.load() + 1.f) * 8191.5f))), 0);
    if (modMoved.exchange (false))
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, juce::jlimit (0, 127, (int) std::round (screenMod.load() * 127.f))), 0);

    // --- Transport -------------------------------------------------------
    bool playing = false, hostRunning = false;
    double bpm = pBpm->load(), pos16 = 0.0, ppq = 0.0;
    if (auto* ph = getPlayHead())
        if (auto info = ph->getPosition())
        {
            if (auto b = info->getBpm()) bpm = *b;
            if (info->getIsPlaying())
            {
                hostRunning = true;
                if (auto q = info->getPpqPosition()) { ppq = *q; pos16 = ppq * 4.0; }
            }
        }
    hostPlaying.store (hostRunning);

    if (hostRunning) playing = true;
    else if (pRun->load() > 0.5f)
    {
        if (! wasRunning) internalPos = -0.0001;
        playing = true;
        pos16 = internalPos;
    }

    const bool seqOn = pSeqOn->load() > 0.5f;
    if (! playing && wasRunning)
    {
        voices[kNumVoices].release();
        lastSlide = false;
        clockStep = std::numeric_limits<int64_t>::min();
        currentStep.store (-1);
    }
    wasRunning = playing;
    const bool seqRunning = playing && seqOn; // armed: keys go to the pattern, which plays only while one is held
    if (! seqRunning && seqKeyCount > 0) { seqKeys.fill (false); seqKeyCount = 0; }

    const double inc16 = bpm / 60.0 * 4.0 / sr;
    const double shuffleOff = pShuffle->load() * 0.33;
    const double gateLen = pSeqGate->load();

    // --- Voice parameters for this block ---------------------------------
    for (int o = 0; o < 2; ++o)
    {
        auto& os = vp.osc[o];
        const auto& p = oscP[o];
        os.on = p.on->load() > 0.5f;
        os.wave = (int) p.wave->load();
        os.wtPos = p.wtpos->load();
        os.semis = p.oct->load() * 12.f + p.semi->load() + p.fine->load() * 0.01f;
        os.level = p.level->load();
        os.uni = (int) p.uni->load();
        os.detune = p.detune->load();
        os.blend = p.blend->load();
        os.phase = p.phase->load();
        os.randPhase = p.rphase->load() > 0.5f;
        os.phaseLock = p.lock->load() > 0.5f;
        os.warp = (int) p.warp->load();
        os.warpAmt = p.warpamt->load();
    }
    vp.subWave = (int) pSubWave->load(); vp.subOct = (int) pSubOct->load(); vp.subLevel = pSubLevel->load();
    vp.noiseType = (int) pNoiseType->load(); vp.noiseLevel = pNoiseLevel->load(); vp.noisePitch = pNoisePitch->load();
    for (int f = 0; f < 2; ++f)
        vp.flt[f] = { (int) fltP[f].type->load(), fltP[f].cutoff->load(), fltP[f].reso->load(),
                      fltP[f].drive->load(), fltP[f].key->load(), fltP[f].env->load() };
    vp.routing = (int) pRouting->load();
    for (int e = 0; e < kNumEnvs; ++e)
        vp.env[e] = { envP[e].a->load(), envP[e].d->load(), envP[e].s->load(), envP[e].r->load() };
    float lfoInc[kNumLfos];
    for (int i = 0; i < kNumLfos; ++i)
    {
        auto& ls = vp.lfo[i];
        ls.shape = (int) lfoP[i].shape->load();
        const bool synced = lfoP[i].sync->load() > 0.5f;
        const double beats = Lists::divisionBeats[juce::jlimit (0, 13, (int) lfoP[i].div->load())];
        ls.rateHz = synced ? (float) (bpm / 60.0 / beats) : lfoP[i].rate->load();
        ls.retrig = (int) lfoP[i].trig->load() == 1;
        ls.phase = lfoP[i].phase->load();
        ls.fade = lfoP[i].fade->load();
        for (int s = 0; s < kLfoSteps; ++s) ls.custom[s] = lfoCustom[i][s].load();
        lfoInc[i] = ls.rateHz / (float) sr;
        if (synced && hostRunning && ! ls.retrig)
            globalLfo[i].phase = (float) std::fmod (ppq / beats, 1.0);
    }
    vp.numActiveMods = 0;
    vp.lfoUsed = 0;
    for (int m = 0; m < kNumModSlots; ++m)
    {
        const ModSlot slot { (int) modP[m].src->load(), (int) modP[m].dst->load(), modP[m].amt->load() };
        if (slot.src <= 0 || slot.dst <= 0 || slot.amt == 0.f) continue;
        vp.mods[vp.numActiveMods++] = slot;
        if (slot.src >= S_Lfo1 && slot.src < S_Lfo1 + kNumLfos) vp.lfoUsed |= 1u << (slot.src - S_Lfo1);
    }
    vp.accent = pAccent->load(); vp.velSens = pVelSens->load();
    vp.warmth = pWarmth->load();
    const float glide = pGlide->load();
    const float seqGlide = glide > 0.001f ? glide : 0.06f;

    // --- Global modulation (FX targets and global LFO rates), once per block
    float gmod[D_Count] {};
    {
        const auto& lv = voices[lastVoice];
        float src[S_Count] {};
        for (int i = 0; i < kNumLfos; ++i) src[S_Lfo1 + i] = gm.lfo[i];
        src[S_Env2] = lv.envOut[1]; src[S_Env3] = lv.envOut[2]; src[S_Env4] = lv.envOut[3];
        src[S_ModWheel] = gm.modWheel; src[S_Velocity] = lastVelocity;
        src[S_Aftertouch] = gm.aftertouch; src[S_Keytrack] = lastKey;
        for (int k = 0; k < vp.numActiveMods; ++k)
            gmod[vp.mods[k].dst] += src[vp.mods[k].src] * vp.mods[k].amt;
    }

    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getWritePointer (1);
    auto midiIt = midi.cbegin();

    // --- Oversampling: voices run at F x the host rate, then filter back down
    int factorIndex = juce::jlimit (0, 3, (int) pOversample->load());
    if (numSamples > oversampleBlock || oversamplers[0] == nullptr) factorIndex = 0;
    const int F = 1 << factorIndex;
    if (F != activeOversample)
    {
        activeOversample = F;
        for (auto& v : voices) v.setSampleRate (sr * F);
        for (auto& o : oversamplers) if (o != nullptr) o->reset();
    }
    juce::dsp::AudioBlock<float> hostBlock (buffer);
    juce::dsp::AudioBlock<float> osBlock;
    float* vL = outL;
    float* vR = outR;
    if (F > 1)
    {
        osBlock = oversamplers[factorIndex - 1]->processSamplesUp (hostBlock.getSubBlock (0, (size_t) numSamples));
        vL = osBlock.getChannelPointer (0);
        vR = osBlock.getChannelPointer (1);
    }
    const float invF = 1.f / (float) F;

    for (int i = 0; i < numSamples; ++i)
    {
        while (midiIt != midi.cend() && (*midiIt).samplePosition <= i)
        {
            handleMidi ((*midiIt).getMessage(), seqRunning);
            ++midiIt;
        }

        if (playing)
        {
            const double pos = pos16 + i * inc16;
            if (pos >= 0.0)
            {
                const int64_t s = (int64_t) std::floor (pos);
                const double frac = pos - (double) s;
                const double off = (s & 1) ? shuffleOff : 0.0;
                if (s != clockStep) { clockStep = s; stepStarted = false; stepEndDone = false; }
                if (! stepStarted && frac >= off) { stepStarted = true; seqStepStart ((int) (s % kNumSteps)); }
                if (stepStarted && ! stepEndDone && frac >= off + gateLen * 0.98)
                {
                    stepEndDone = true;
                    if (seqOn) seqStepEnd();
                }
            }
        }

        for (int j = 0; j < F; ++j)
        {
            // Global LFOs tick at the oversampled rate so audio-rate modulation stays smooth
            for (int l = 0; l < kNumLfos; ++l)
            {
                if ((vp.lfoUsed & (1u << l)) == 0) continue;
                globalLfo[l].advance (lfoInc[l] * invF * std::exp2 (gmod[D_Lfo1Rate + l] * 3.f));
                float ph = globalLfo[l].phase + vp.lfo[l].phase;
                ph -= std::floor (ph);
                gm.lfo[l] = lfoShapeValue (vp.lfo[l].shape, ph, globalLfo[l].sh, vp.lfo[l].custom);
            }

            float L = 0.f, R = 0.f;
            for (int v = 0; v < kNumVoices; ++v)
                if (voices[v].isActive()) voices[v].process (vp, gm, glide, L, R);
            if (voices[kNumVoices].isActive()) voices[kNumVoices].process (vp, gm, seqGlide, L, R);
            vL[i * F + j] = L * 0.5f;
            vR[i * F + j] = R * 0.5f;
        }
    }

    if (F > 1)
    {
        auto outBlock = hostBlock.getSubBlock (0, (size_t) numSamples);
        oversamplers[factorIndex - 1]->processSamplesDown (outBlock);
    }

    while (midiIt != midi.cend())
    {
        handleMidi ((*midiIt).getMessage(), seqRunning);
        ++midiIt;
    }
    midi.clear();
    if (panicNow)
        for (int ch = 1; ch <= 16; ++ch)
        {
            midi.addEvent (juce::MidiMessage::allNotesOff (ch), 0);
            midi.addEvent (juce::MidiMessage::controllerEvent (ch, 121, 0), 0);
        }

    // --- FX rack -----------------------------------------------------------
    auto clamp01 = [] (float x) { return juce::jlimit (0.f, 1.f, x); };
    if (pDistOn->load() > 0.5f)
        dist.process (outL, outR, numSamples, (int) pDistType->load(),
                      clamp01 (pDistDrive->load() + gmod[D_Dist]), pDistMix->load());
    if (pMfxOn->load() > 0.5f)
        modFx.process (outL, outR, numSamples, (int) pMfxType->load(), pMfxRate->load(), pMfxDepth->load(),
                       pMfxFb->load(), clamp01 (pMfxMix->load() + gmod[D_ModFx]));
    if (pDlyOn->load() > 0.5f)
    {
        const float t = pDlySync->load() > 0.5f
                          ? (float) (Lists::divisionBeats[juce::jlimit (0, 13, (int) pDlyDiv->load())] * 60.0 / bpm)
                          : pDlyMs->load() * 0.001f;
        delay.process (outL, outR, numSamples, t, pDlyPP->load() > 0.5f, pDlyFb->load(),
                       clamp01 (pDlyMix->load() + gmod[D_Delay]));
    }
    if (pRevOn->load() > 0.5f)
        reverb.process (outL, outR, numSamples, pRevSize->load(), pRevDecay->load(), pRevDamp->load(),
                        clamp01 (pRevMix->load() + gmod[D_Reverb]));
    if (pCompOn->load() > 0.5f)
        comp.process (outL, outR, numSamples, (int) pCompMode->load(), pCompThr->load(), pCompRatio->load(),
                      pCompAtt->load(), pCompRel->load(), pCompMakeup->load(), pCompDepth->load());
    if (pEqOn->load() > 0.5f)
        eq.process (outL, outR, numSamples, pEqLf->load(), pEqLg->load(), pEqMf->load(), pEqMq->load(),
                    pEqMg->load(), pEqHf->load(), pEqHg->load());

    const float vol = pVolume->load(), pan = pPan->load();
    const float gl = vol * std::min (1.f, 1.f - pan), gr = vol * std::min (1.f, 1.f + pan);
    for (int i = 0; i < numSamples; ++i)
    {
        outL[i] = softLimit (outL[i] * gl);
        outR[i] = softLimit (outR[i] * gr);
    }

    int w = scopeWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < numSamples; ++i)
    {
        scope[(size_t) w].store ((outL[i] + outR[i]) * 0.5f, std::memory_order_relaxed);
        w = (w + 1) % kScopeSize;
    }
    scopeWrite.store (w, std::memory_order_release);

    if (playing && ! hostRunning)
        internalPos += numSamples * inc16;
}

std::unique_ptr<juce::XmlElement> AcidanceProcessor::createStateXml()
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    auto* extra = xml->createNewChildElement ("ACIDANCE_DATA");
    extra->setAttribute ("seq", seq.serialise());
    extra->setAttribute ("program", currentProgram);
    extra->setAttribute ("presetName", currentPresetName);
    extra->setAttribute ("presetFolder", currentPresetFolder);
    extra->setAttribute ("skin", skin.load());
    extra->setAttribute ("uiScale", (double) uiScale.load());
    juce::String shapes;
    for (auto& l : lfoCustom)
        for (auto& s : l) shapes << juce::String (s.load(), 3) << ",";
    extra->setAttribute ("lfoShapes", shapes);
    return xml;
}

void AcidanceProcessor::applyStateXml (const juce::XmlElement& source)
{
    juce::XmlElement xml (source);
    if (auto* extra = xml.getChildByName ("ACIDANCE_DATA"))
    {
        seq.deserialise (extra->getStringAttribute ("seq"));
        currentProgram = juce::jlimit (0, Presets::count() - 1, extra->getIntAttribute ("program"));
        currentPresetName = extra->getStringAttribute ("presetName", Presets::name (currentProgram));
        currentPresetFolder = extra->getStringAttribute ("presetFolder", Presets::category (currentProgram));
        auto vals = juce::StringArray::fromTokens (extra->getStringAttribute ("lfoShapes"), ",", "");
        for (int i = 0; i < std::min (vals.size(), kNumLfos * kLfoSteps); ++i)
            lfoCustom[i / kLfoSteps][i % kLfoSteps] = juce::jlimit (-1.f, 1.f, vals[i].getFloatValue());
        xml.removeChildElement (extra, true);
    }
    if (xml.hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (xml));
    ++programChanges;
}

void AcidanceProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    copyXmlToBinary (*createStateXml(), destData);
}

void AcidanceProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr) return;
    if (auto* extra = xml->getChildByName ("ACIDANCE_DATA"))
    {
        skin = juce::jlimit (0, 4, extra->getIntAttribute ("skin", 4));   // the skin only comes from the session, not presets
        uiScale = (float) juce::jlimit (0.5, 1.5, extra->getDoubleAttribute ("uiScale", 0.9));
    }
    applyStateXml (*xml);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AcidanceProcessor();
}

juce::AudioProcessorEditor* AcidanceProcessor::createEditor()
{
    return new AcidanceEditor (*this);
}
