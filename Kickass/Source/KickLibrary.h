#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

/** The user's own kick one-shots (their Splice downloads by default), found by
    scanning a folder in the background. Nothing is copied or bundled: the plugin
    plays the files where they are. One library is shared by every KICKASS instance;
    get it with juce::SharedResourcePointer<KickLibrary>. */
class KickLibrary : private juce::Thread
{
public:
    struct Entry
    {
        juce::File file;
        juce::String pack; // the Splice pack (or first sub-folder) it came from
    };

    KickLibrary();
    ~KickLibrary() override;

    juce::File getFolder() const;
    /** Remembers the folder for next time and rescans it. */
    void setFolder (const juce::File&);
    void rescan();

    bool isScanning() const { return isThreadRunning(); }
    /** Bumped after every scan, so the editor knows to refresh. */
    int getVersion() const { return version.load(); }

    std::vector<Entry> getEntries() const;
    int indexOf (const juce::File&) const;

    /** Kick-named one-shots only: no loops, no "no kick" loops, nothing long. */
    static bool looksLikeKickOneShot (const juce::File&);

private:
    void run() override;
    static juce::File defaultFolder();
    std::unique_ptr<juce::PropertiesFile> openSettings() const;

    mutable juce::CriticalSection lock;
    juce::File folder;
    std::vector<Entry> entries;
    std::atomic<int> version { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickLibrary)
};
