#pragma once

// "Update available" banner for every SKAZI plugin window.
//
//   return skazi::withUpdateNotice (new MyEditor (*this));   // in createEditor()
//
// Once a day (per machine, shared by all SKAZI plugins) a background thread asks GitHub for the
// latest SKAZI Bundle release. If it was published after this plugin was built, a small banner
// appears in the window's bottom-right corner; clicking it opens the download page, x hides it
// for this release. No internet, a timeout or a GitHub hiccup: nothing shows, nothing waits.
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>

namespace skazi
{
class UpdateChecker : private juce::Thread
{
public:
    UpdateChecker() : juce::Thread ("SKAZI update check") { startThread (juce::Thread::Priority::low); }
    ~UpdateChecker() override { stopThread (8000); }

    bool available() const { return newer.load(); }
    juce::String releaseName() const { const juce::ScopedLock sl (lock); return name; }
    void dismiss()
    {
        auto f = cacheFile();
        juce::PropertiesFile props (f, {});
        props.setValue ("dismissed", releaseName());
        props.saveIfNeeded();
        newer = false;
    }

    static juce::URL pageUrl() { return juce::URL ("https://slazoasher.github.io/skazi-bundle/"); }

private:
    static juce::File cacheFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("SKAZI").getChildFile ("update-check.settings");
    }

    void run() override
    {
        const auto file = cacheFile();
        file.getParentDirectory().createDirectory();
        juce::PropertiesFile::Options opts;
        juce::PropertiesFile props (file, opts);

        const auto now = juce::Time::getCurrentTime();
        const auto lastCheck = juce::Time (props.getValue ("checked").getLargeIntValue());
        juce::String published = props.getValue ("published"), tag = props.getValue ("tag");

        if (now - lastCheck > juce::RelativeTime::hours (24) || published.isEmpty())
        {
            auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                               .withConnectionTimeoutMs (6000)
                               .withExtraHeaders ("User-Agent: SKAZI-Bundle\r\nAccept: application/vnd.github+json");
            if (auto stream = juce::URL ("https://api.github.com/repos/slazoasher/skazi-bundle/releases/latest").createInputStream (options))
            {
                const auto json = juce::JSON::parse (stream->readEntireStreamAsString());
                if (json.hasProperty ("published_at"))
                {
                    published = json["published_at"].toString();
                    tag = json["tag_name"].toString();
                    props.setValue ("published", published);
                    props.setValue ("tag", tag);
                    props.setValue ("checked", juce::String (now.toMilliseconds()));
                    props.saveIfNeeded();
                }
            }
        }
        if (threadShouldExit() || published.isEmpty())
            return;

        // CI builds every plugin minutes before it publishes the release, so a release more than
        // several hours newer than this build is one this user doesn't have yet
        const auto releaseTime = juce::Time::fromISO8601 (published);
        const auto builtTime = juce::Time::getCompilationDate();
        if (releaseTime - builtTime > juce::RelativeTime::hours (6) && props.getValue ("dismissed") != tag)
        {
            { const juce::ScopedLock sl (lock); name = tag; }
            newer = true;
        }
    }

    std::atomic<bool> newer { false };
    juce::CriticalSection lock;
    juce::String name;
};

class UpdateBanner : public juce::Component, private juce::ComponentListener, private juce::Timer
{
public:
    explicit UpdateBanner (juce::Component& editorToJoin) : editor (editorToJoin)
    {
        editor.addChildComponent (this);
        editor.addComponentListener (this);
        setAlwaysOnTop (true);
        startTimer (1000);
    }

    ~UpdateBanner() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colour (0xf0181a1f));
        g.fillRoundedRectangle (r, 11.0f);
        g.setColour (juce::Colour (0xffa6ff2e));
        g.drawRoundedRectangle (r, 11.0f, 1.2f);
        g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
        g.drawText (juce::String::fromUTF8 ("\xe2\x96\xb2 NEW SKAZI BUNDLE UPDATE - CLICK TO DOWNLOAD"), getLocalBounds().withTrimmedRight (22),
                    juce::Justification::centred);
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.drawText ("x", getWidth() - 22, 0, 16, getHeight(), juce::Justification::centred);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.x >= getWidth() - 24) checker->dismiss();
        else UpdateChecker::pageUrl().launchInDefaultBrowser();
        setVisible (false);
    }

    void mouseEnter (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

private:
    void timerCallback() override
    {
        if (! checker->available()) return;
        stopTimer();
        place();
        setVisible (true);
        toFront (false);
    }

    void place()
    {
        const int w = juce::jmin (380, editor.getWidth() - 16), h = 24;
        setBounds (editor.getWidth() - w - 8, editor.getHeight() - h - 8, w, h);
    }

    void componentMovedOrResized (juce::Component&, bool, bool wasResized) override { if (wasResized) place(); }
    void componentBeingDeleted (juce::Component& c) override
    {
        c.removeComponentListener (this);
        delete this; // the banner lives exactly as long as the window it sits in
    }

    juce::Component& editor;
    juce::SharedResourcePointer<UpdateChecker> checker;
};

template <typename EditorType>
EditorType* withUpdateNotice (EditorType* editor)
{
    if (editor != nullptr)
        new UpdateBanner (*editor); // owns itself, deleted with the editor
    return editor;
}
} // namespace skazi
