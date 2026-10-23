#pragma once
#include <juce_core/juce_core.h>
#include <vector>

class AcidanceProcessor;

// Factory library: 1000 generated presets in root folders, each with search tags.
// User presets are XML files (*.skazi) in Documents/SKAZI/Presets.
namespace Presets
{
    int count();
    const juce::StringArray& categories();
    int categoryStart (int category);
    int categorySize (int category);
    juce::String name (int index);
    juce::String category (int index);
    const juce::StringArray& tags (int index);
    const juce::StringArray& allTags();          // most used first
    void apply (int index, AcidanceProcessor& p);
    juce::String randomSound (AcidanceProcessor& p); // RND: random style + a fresh random effects chain; returns the style
    juce::var toJson();                          // whole factory library as JSON metadata

    struct UserPreset { juce::File file; juce::String name; juce::StringArray tags; };
    juce::File userFolder();
    std::vector<UserPreset> userPresets();
    bool saveUser (AcidanceProcessor& p, const juce::String& name, const juce::StringArray& tags);
    bool loadUser (AcidanceProcessor& p, const juce::File& file);
}
