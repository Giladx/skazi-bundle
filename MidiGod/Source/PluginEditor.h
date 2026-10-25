#pragma once

#include "PluginProcessor.h"
#include "../../Common/SkaziUI.h"

// The loop as a piano roll, with the play position
class PianoRoll : public juce::Component
{
public:
    explicit PianoRoll (MidiGodProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
private:
    MidiGodProcessor& proc;
};

// GOD MODE: a new variation
class GodButton : public juce::Button
{
public:
    GodButton() : juce::Button ("GOD MODE") {}
    void paintButton (juce::Graphics& g, bool over, bool down) override;
    float flash = 0.0f;
};

// Grab it and drop it on a DAW track: the loop arrives as a MIDI clip
class DragTile : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit DragTile (MidiGodProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseDrag (const juce::MouseEvent& e) override;
private:
    MidiGodProcessor& proc;
    bool dragging = false;
};

// Where audio files are dropped (or clicked to browse for one)
class DropZone : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit DropZone (MidiGodProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
    void mouseUp (const juce::MouseEvent& e) override { if (onClick && getLocalBounds().contains (e.getPosition())) onClick(); }
    std::function<void()> onClick;
    bool hover = false;
    float phase = 0.0f;
private:
    MidiGodProcessor& proc;
};

class MidiGodEditor : public skazi::EditorBase, public juce::FileDragAndDropTarget
{
public:
    explicit MidiGodEditor (MidiGodProcessor&);

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dropZone.hover = true; }
    void fileDragExit (const juce::StringArray&) override { dropZone.hover = false; }
    void filesDropped (const juce::StringArray& files, int, int) override;

protected:
    void layout() override;
    void tick() override;

private:
    void browseAudio();
    void saveMidi();

    MidiGodProcessor& mp;
    PianoRoll roll;
    GodButton godBtn;
    DragTile dragTile;
    DropZone dropZone;
    juce::TextButton saveBtn { "SAVE .MID" }, genBtn { "GENERATOR" };
    std::unique_ptr<juce::FileChooser> chooser;

    skazi::ChoiceRow *genreRow, *roleRow, *keyRow, *barsRow, *gridRow, *modeRow, *quantRow;
    juce::ComboBox* scaleBox;
    skazi::Knob *density, *swing, *gate, *dyn, *move, *human, *octave, *seed, *sens, *vol;
    skazi::ParamToggle *snap, *midiOut, *freeRun;
};
