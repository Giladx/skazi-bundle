// SHAKSUKA Setup: a single .exe that carries the VST3 + standalone app inside it,
// installs them for every DAW, adds shortcuts and an Add/Remove Programs entry,
// and doubles as the uninstaller ("/uninstall").
#include <juce_gui_extra/juce_gui_extra.h>
#include "BinaryData.h"
#include "../Source/MetalLookAndFeel.h"

using namespace juce;

namespace
{
    const String kVersion = "1.0.0";
    const String kUninstallKey = "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SHAKSUKA\\";

    File vst3Bundle()
    {
        return File (SystemStats::getEnvironmentVariable ("CommonProgramFiles", "C:\\Program Files\\Common Files"))
            .getChildFile ("VST3").getChildFile ("SHAKSUKA.vst3");
    }
    File appDir()       { return File::getSpecialLocation (File::globalApplicationsDirectory).getChildFile ("SHAKSUKA"); }
    File appExe()       { return appDir().getChildFile ("SHAKSUKA.exe"); }
    File uninstaller()  { return appDir().getChildFile ("Uninstall SHAKSUKA.exe"); }
    File startMenuLink()
    {
        return File (SystemStats::getEnvironmentVariable ("ProgramData", "C:\\ProgramData"))
            .getChildFile ("Microsoft\\Windows\\Start Menu\\Programs\\SHAKSUKA.lnk");
    }
    File desktopLink()  { return File::getSpecialLocation (File::userDesktopDirectory).getChildFile ("SHAKSUKA.lnk"); }

    bool isInstalled() { return vst3Bundle().exists() || appDir().exists(); }

    bool writeFile (const File& f, const void* data, int size)
    {
        f.getParentDirectory().createDirectory();
        return f.replaceWithData (data, (size_t) size);
    }
}

class SetupComponent : public Component
{
public:
    explicit SetupComponent (bool startUninstall)
    {
        setLookAndFeel (&lnf);
#if SHAK_HAS_LOGO
        logo = ImageCache::getFromMemory (BinaryData::skazi_logo_png, BinaryData::skazi_logo_pngSize);
#endif

        for (auto* t : { &optVst3, &optApp, &optDesktop })
        {
            t->setColour (ToggleButton::tickColourId, skin::red);
            t->setToggleState (true, dontSendNotification);
            addAndMakeVisible (t);
        }
        optVst3.setEnabled (false); // the plugin is the point of the installer

        installBtn.setToggleState (true, dontSendNotification); // drawn lit red
        installBtn.onClick = [this] { install(); };
        uninstallBtn.onClick = [this] { uninstall(); };
        closeBtn.onClick = [] { JUCEApplication::quit(); };
        for (auto* b : { &installBtn, &uninstallBtn, &closeBtn })
            addAndMakeVisible (b);

        refreshState();
        setSize (620, 420);

        if (startUninstall)
            MessageManager::callAsync ([this] { uninstall(); });
    }

    ~SetupComponent() override { setLookAndFeel (nullptr); }

    void paint (Graphics& g) override
    {
        g.drawImageAt (faceplate, 0, 0);
        skin::drawLogoBadge (g, { 30.0f, 24.0f, 150.0f, 46.0f }, logo);

        const auto tf = skin::titleFont (34.0f);
        const float shakW = GlyphArrangement::getStringWidth (tf, "SHAK");
        skin::engrave (g, "SHAK", { 196, 20, (int) shakW + 4, 38 }, tf, Colour (0xff26282c), Justification::left);
        skin::engrave (g, "SUKA", { 196 + (int) shakW, 20, 120, 38 }, tf, skin::red, Justification::left);
        skin::engrave (g, "SETUP", { 200 + (int) shakW + 90, 20, 120, 38 }, tf, Colour (0xff4a4d52), Justification::left);
        skin::engrave (g, String::fromUTF8 ("VERSION ") + kVersion + String::fromUTF8 ("  \xc2\xb7  BY ASHER SWISSA"),
                       { 197, 56, 300, 14 }, skin::font (10.5f), Colour (0xff3b3e43), Justification::left);

        auto panel = Rectangle<int> (30, 92, 560, 236);
        skin::drawInsetPanel (g, panel.toFloat());
        g.setColour (skin::white);
        g.setFont (skin::font (13.0f));
        g.drawText ("INSTALL", panel.getX() + 16, panel.getY() + 12, 200, 20, Justification::left);

        auto row = [&] (const Component& t, const String& title, const String& detail)
        {
            g.setColour (skin::white);
            g.setFont (skin::font (13.0f));
            g.drawText (title, t.getRight() + 8, t.getY() - 2, 400, 18, Justification::left);
            g.setColour (skin::textDim);
            g.setFont (skin::font (11.0f, false));
            g.drawText (detail, t.getRight() + 8, t.getY() + 15, 480, 16, Justification::left);
        };
        row (optVst3, "VST3 plugin (every DAW)", vst3Bundle().getFullPathName());
        row (optApp, "Standalone app + Start Menu shortcut", appDir().getFullPathName());
        row (optDesktop, "Desktop shortcut", "SHAKSUKA on your desktop");

        g.setFont (skin::font (12.0f));
        g.setColour (statusColour);
        g.drawFittedText (status, panel.getX() + 16, panel.getBottom() - 46, panel.getWidth() - 32, 36, Justification::centredLeft, 2);

        Random r (3);
        for (auto pt : { Point<float> (15, 15), Point<float> ((float) getWidth() - 15, 15),
                         Point<float> (15, (float) getHeight() - 15), Point<float> ((float) getWidth() - 15, (float) getHeight() - 15) })
            skin::drawScrew (g, pt, 7.0f, r.nextFloat() * MathConstants<float>::pi);
    }

    void resized() override
    {
        faceplate = skin::renderBrushedMetal (getWidth(), getHeight());
        optVst3.setBounds (46, 140, 36, 20);
        optApp.setBounds (46, 182, 36, 20);
        optDesktop.setBounds (46, 224, 36, 20);
        uninstallBtn.setBounds (30, 352, 110, 34);
        closeBtn.setBounds (370, 352, 90, 34);
        installBtn.setBounds (470, 352, 120, 34);
    }

private:
    void setStatus (const String& s, Colour c)
    {
        status = s;
        statusColour = c;
        repaint();
    }

    void refreshState()
    {
        uninstallBtn.setEnabled (isInstalled());
        installBtn.setButtonText (isInstalled() ? "REINSTALL" : "INSTALL");
        if (status.isEmpty())
            setStatus (isInstalled() ? "SHAKSUKA is already installed. Reinstall to update it, or uninstall to remove it."
                                     : "Ready to install. Close your DAW first.", skin::textDim);
    }

public:
    bool install()
    {
        const auto bundle = vst3Bundle();
        bool ok = writeFile (bundle.getChildFile ("Contents\\x86_64-win\\SHAKSUKA.vst3"),
                             BinaryData::SHAKSUKA_vst3, BinaryData::SHAKSUKA_vst3Size)
               && writeFile (bundle.getChildFile ("Contents\\Resources\\moduleinfo.json"),
                             BinaryData::moduleinfo_json, BinaryData::moduleinfo_jsonSize);

        if (ok && optApp.getToggleState())
        {
            ok = writeFile (appExe(), BinaryData::SHAKSUKA_exe, BinaryData::SHAKSUKA_exeSize);
            startMenuLink().getParentDirectory().createDirectory();
            appExe().createShortcut ("SHAKSUKA by Asher Swissa", startMenuLink());
        }
        if (ok && optDesktop.getToggleState() && appExe().existsAsFile())
            appExe().createShortcut ("SHAKSUKA by Asher Swissa", desktopLink());

        // keep a copy of this exe as the uninstaller and register with Add/Remove Programs
        const auto self = File::getSpecialLocation (File::currentExecutableFile);
        if (ok && self != uninstaller())
        {
            appDir().createDirectory();
            ok = self.copyFileTo (uninstaller());
        }
        if (ok)
        {
            WindowsRegistry::setValue (kUninstallKey + "DisplayName", "SHAKSUKA");
            WindowsRegistry::setValue (kUninstallKey + "DisplayVersion", kVersion);
            WindowsRegistry::setValue (kUninstallKey + "Publisher", "SKAZI");
            WindowsRegistry::setValue (kUninstallKey + "InstallLocation", appDir().getFullPathName());
            WindowsRegistry::setValue (kUninstallKey + "DisplayIcon", uninstaller().getFullPathName());
            WindowsRegistry::setValue (kUninstallKey + "UninstallString", "\"" + uninstaller().getFullPathName() + "\" /uninstall");
            WindowsRegistry::setValue (kUninstallKey + "NoModify", (uint32) 1);
            WindowsRegistry::setValue (kUninstallKey + "NoRepair", (uint32) 1);
            WindowsRegistry::setValue (kUninstallKey + "EstimatedSize",
                                       (uint32) ((BinaryData::SHAKSUKA_vst3Size + BinaryData::SHAKSUKA_exeSize) / 1024 * 2));
            setStatus (String::fromUTF8 ("Installed \xe2\x9c\x93  Open your DAW and rescan plugins: SHAKSUKA is under SKAZI."),
                       Colour (0xff22e3a5));
        }
        else
        {
            setStatus ("Could not write the files. Close your DAW (it may be using SHAKSUKA) and try again.", skin::red2);
        }
        refreshState();
        return ok;
    }

    bool uninstall()
    {
        bool ok = ! vst3Bundle().exists() || vst3Bundle().deleteRecursively();
        startMenuLink().deleteFile();
        desktopLink().deleteFile();
        WindowsRegistry::deleteKey (kUninstallKey.dropLastCharacters (1));

        const auto self = File::getSpecialLocation (File::currentExecutableFile);
        if (appDir().exists())
        {
            if (self.isAChildOf (appDir()))
            {
                // running from the install folder: delete everything else now, the folder once we exit
                for (auto& f : appDir().findChildFiles (File::findFilesAndDirectories, false))
                    if (f != self)
                        ok = f.deleteRecursively() && ok;
                selfDeleteOnExit = true;
            }
            else
            {
                ok = appDir().deleteRecursively() && ok;
            }
        }

        if (ok)
            setStatus (String::fromUTF8 ("SHAKSUKA was removed \xe2\x9c\x93"), Colour (0xff22e3a5));
        else
            setStatus ("Some files are in use. Close your DAW and uninstall again.", skin::red2);
        refreshState();
        return ok;
    }

    bool selfDeleteOnExit = false;

private:
    skin::MetalLookAndFeel lnf;
    Image faceplate, logo;
    ToggleButton optVst3, optApp, optDesktop;
    TextButton installBtn { "INSTALL" }, uninstallBtn { "UNINSTALL" }, closeBtn { "CLOSE" };
    String status;
    Colour statusColour = skin::textDim;
};

class SetupApp : public JUCEApplication
{
public:
    const String getApplicationName() override    { return "SHAKSUKA Setup"; }
    const String getApplicationVersion() override { return kVersion; }
    bool moreThanOneInstanceAllowed() override    { return false; }

    // "/silent" installs with the default options and no window ("/uninstall /silent" removes);
    // the exit code is 0 on success
    void initialise (const String& commandLine) override
    {
        const bool uninstallMode = commandLine.containsIgnoreCase ("/uninstall");
        if (commandLine.containsIgnoreCase ("/silent"))
        {
            silent = std::make_unique<SetupComponent> (false);
            content = silent.get();
            const bool ok = uninstallMode ? silent->uninstall() : silent->install();
            setApplicationReturnValue (ok ? 0 : 1);
            quit();
            return;
        }
        content = new SetupComponent (uninstallMode);
        window = std::make_unique<Window> (content);
    }

    void shutdown() override
    {
        const bool selfDelete = content != nullptr && content->selfDeleteOnExit;
        window = nullptr;
        silent = nullptr;
        content = nullptr;
        if (selfDelete)
        {
            // a detached cmd waits for this process to exit, then removes the install folder
            const auto dir = appDir().getFullPathName();
            ChildProcess cmd;
            cmd.start ("cmd.exe /c cd /d %TEMP% & ping 127.0.0.1 -n 3 > nul & rmdir /s /q \"" + dir + "\"", 0);
        }
    }

private:
    struct Window : public DocumentWindow
    {
        explicit Window (Component* c)
            : DocumentWindow ("SHAKSUKA Setup", Colours::black, DocumentWindow::closeButton)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (c, true);
            setResizable (false, false);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { JUCEApplication::quit(); }
    };

    SetupComponent* content = nullptr; // owned by the window, or by `silent`
    std::unique_ptr<SetupComponent> silent;
    std::unique_ptr<Window> window;
};

START_JUCE_APPLICATION (SetupApp)
