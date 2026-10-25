#pragma once

// The MIDI GOD pattern generator: genre + role + key + groove -> a loop of notes.
// Notes are in beats (quarter notes); MIDI 60 = C3 (Ableton / Cubase / FL naming).
#include "../../Common/SkaziCore.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace midigod
{
enum Genre { FullOn, ProgPsy, PsyTech, TechHouse, DeepHouse, BassHouse, MelodicTechno, HardTechno, Trance, Edm, NumGenres };
enum Role { Bass, Acid, Lead, Pluck, Arp, Chords, Pad, Counter, Drums, NumRoles };
enum Scale { Minor, Major, Phrygian, HarmMinor, Dorian, Freygish, Hungarian, Pentatonic, NumScales };
enum ConvertMode { Melody, BassLine, DrumHits, ChordTrack, NumModes };

struct GenreInfo { const char* label; const char* title; double bpm; float swing; };
const GenreInfo& genreInfo (int genre);
const char* roleTitle (int role);
const char* keyName (int key);
const char* scaleTitle (int scale);
const char* modeTitle (int mode);
const std::vector<int>& scaleSteps (int scale);

// GM drum notes the DRUMS role and the drum converter use
enum DrumNote { KickNote = 36, RimNote = 37, SnareNote = 38, ClapNote = 39, HatNote = 42, OpenHatNote = 46, RideNote = 51, ShakerNote = 70 };

struct Note
{
    int pitch = 60;
    double start = 0.0, length = 0.25; // beats
    int velocity = 100;
};

struct Pattern
{
    std::vector<Note> notes;
    double lengthBeats = 16.0;
    bool drums = false;
    juce::String title;    // shown over the piano roll
    juce::String fileStem; // name of the exported .mid
};

std::vector<skazi::ParamDef> paramDefs();

struct Settings
{
    int genre = FullOn, role = Bass, key = 6, scale = Minor, bars = 4, octave = 0, seed = 1;
    bool triplet = false;
    float density = 0.6f, swing = 0.5f, gate = 0.7f, dynamics = 0.6f, humanize = 0.0f, movement = 0.5f;

    template <typename Get>
    static Settings fromValues (Get v)
    {
        static const int barCounts[] = { 1, 2, 4, 8 };
        Settings s;
        s.genre = (int) v ("genre");
        s.role = (int) v ("role");
        s.key = (int) v ("key");
        s.scale = (int) v ("scale");
        s.bars = barCounts[juce::jlimit (0, 3, (int) v ("bars"))];
        s.triplet = (int) v ("grid") == 1;
        s.octave = (int) v ("octave");
        s.seed = (int) v ("seed");
        s.density = v ("density") / 100.0f;
        s.swing = v ("swing") / 100.0f;
        s.gate = v ("gate") / 100.0f;
        s.dynamics = v ("dyn") / 100.0f;
        s.humanize = v ("human") / 100.0f;
        s.movement = v ("move") / 100.0f;
        return s;
    }
};

Pattern generate (const Settings&);

// Finishing applied to notes that came from audio: grid, key and octave
struct Finish { double quantize = 0.0; bool snap = false; int key = 0, scale = Minor, octave = 0; bool drums = false; };
double quantizeBeats (int choice); // a2mQuant choice -> grid in beats (0 = off)
void finishNotes (std::vector<Note>&, double lengthBeats, const Finish&);

// No two notes of the same pitch overlap, and nothing runs past the loop end
void tidy (std::vector<Note>&, double lengthBeats);

juce::MidiFile toMidiFile (const Pattern&, double bpm);
} // namespace midigod
