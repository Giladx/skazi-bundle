#pragma once

// Audio to MIDI: monophonic pitch tracking (melody / bass), drum hit detection and chord recognition.
#include "Pattern.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>

namespace midigod
{
struct ConvertResult
{
    std::vector<Note> notes;   // beats at the tempo given, raw (not quantized)
    double lengthBeats = 16.0; // rounded up to whole bars
    bool drums = false;
    double seconds = 0.0;
    bool truncated = false;    // longer than kMaxSeconds: only the start was used
    juce::String error;
};

constexpr double kMaxSeconds = 240.0;

// sensitivity 0..1: higher picks up quieter and less clean notes
ConvertResult convertAudio (const juce::AudioBuffer<float>& audio, double sampleRate, int mode, float sensitivity, double bpm,
                            const std::atomic<bool>* cancel = nullptr);
ConvertResult convertFile (const juce::File& file, int mode, float sensitivity, double bpm, const std::atomic<bool>* cancel = nullptr);

bool isAudioFile (const juce::String& path);
} // namespace midigod
