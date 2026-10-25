#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>
#include <functional>

/** The user's own riser / uplift / sweep / impact samples (their Splice downloads by
    default), found by scanning a folder in the background, the same way KICKASS finds
    kicks. Nothing is copied or bundled: BAMBA plays the files where they are. One
    library is shared by every BAMBA instance; get it with
    juce::SharedResourcePointer<RiserLibrary>. */
class RiserLibrary : private juce::Thread
{
public:
    enum Kind { Rise, Down, Sweep, Impact, Reverse, NumKinds };
    static const char* kindName (int kind);

    // Length filter: SHORT is under this many seconds, LONG is this or more
    static constexpr float shortSeconds = 4.0f;
    enum Length { AnyLength, Short, Long };

    struct Entry
    {
        juce::File file;
        juce::String pack; // the Splice pack (or first sub-folder) it came from
        int kind = Rise;
        float seconds = 0;
        bool matches (int kindFilter, int lengthFilter) const; // kindFilter -1 = any
    };

    RiserLibrary();
    ~RiserLibrary() override;

    juce::File getFolder() const;
    /** Remembers the folder for next time and rescans it. */
    void setFolder (const juce::File&);
    void rescan();

    bool isScanning() const { return isThreadRunning(); }
    /** Bumped after every scan, so the editor knows to refresh. */
    int getVersion() const { return version.load(); }

    /** Shortest first, so < and > walk from quick hits to long builds. */
    std::vector<Entry> getEntries() const;

    /** The kind of FX a file's name (or its folder) says it is, or -1 when it isn't one. */
    static int classify (const juce::File&);
    /** Scans a folder tree; a missing or empty folder just finds nothing. */
    static std::vector<Entry> scan (const juce::File& root, const std::function<bool()>& shouldStop = {});

private:
    void run() override;
    static juce::File defaultFolder();
    std::unique_ptr<juce::PropertiesFile> openSettings() const;

    mutable juce::CriticalSection lock;
    juce::File folder;
    std::vector<Entry> entries;
    std::atomic<int> version { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiserLibrary)
};
