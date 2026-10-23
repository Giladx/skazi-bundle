// One-file installer for The FilterFish by Asher Swissa.
// The plugin files ride inside this exe as an embedded zip ("payload.zip").
// Run with /uninstall to remove everything (that's what Windows "Apps & features" calls).
using System;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Windows.Forms;
using Microsoft.Win32;

static class Setup
{
    const string Product = "The FilterFish";
    const string Maker = "Asher Swissa";
    const string Version = "1.0.0";
    const string RegKey = @"Software\Microsoft\Windows\CurrentVersion\Uninstall\TheFilterFish";

    static readonly string Vst3Root = Path.Combine (Environment.GetFolderPath (Environment.SpecialFolder.CommonProgramFiles), "VST3");
    static readonly string Vst3 = Path.Combine (Vst3Root, Product + ".vst3");
    static readonly string AppDir = Path.Combine (Environment.GetFolderPath (Environment.SpecialFolder.ProgramFiles), Maker, Product);
    static readonly string Shortcut = Path.Combine (Environment.GetFolderPath (Environment.SpecialFolder.CommonPrograms), Product + ".lnk");

    [STAThread]
    static int Main (string[] args)
    {
        Application.EnableVisualStyles();
        try
        {
            if (args.Length > 0 && args[0].Equals ("/uninstall", StringComparison.OrdinalIgnoreCase))
                return Uninstall();
            return Install();
        }
        catch (Exception e)
        {
            MessageBox.Show ("Something went wrong:\n\n" + e.Message, Product + " Setup", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return 1;
        }
    }

    static int Install()
    {
        var answer = MessageBox.Show ("Install THE FILTERFISH by Asher Swissa?\n\n"
                                      + "Plugin (VST3):  " + Vst3Root + "\n"
                                      + "Standalone app:  " + AppDir,
                                      Product + " Setup", MessageBoxButtons.OKCancel, MessageBoxIcon.Information);
        if (answer != DialogResult.OK)
            return 0;

        string tmp = Path.Combine (Path.GetTempPath(), "FilterFishSetup_" + Guid.NewGuid().ToString ("N"));
        using (var payload = Assembly.GetExecutingAssembly().GetManifestResourceStream ("payload.zip"))
        using (var zip = new ZipArchive (payload))
            zip.ExtractToDirectory (tmp);

        if (Directory.Exists (Vst3))
            Directory.Delete (Vst3, true);
        CopyDir (Path.Combine (tmp, Product + ".vst3"), Vst3);

        Directory.CreateDirectory (AppDir);
        string appExe = Path.Combine (AppDir, Product + ".exe");
        File.Copy (Path.Combine (tmp, Product + ".exe"), appExe, true);
        string uninstaller = Path.Combine (AppDir, "Uninstall.exe");
        File.Copy (Assembly.GetExecutingAssembly().Location, uninstaller, true);
        Directory.Delete (tmp, true);

        CreateShortcut (Shortcut, appExe);

        using (var key = Registry.LocalMachine.CreateSubKey (RegKey))
        {
            key.SetValue ("DisplayName", Product);
            key.SetValue ("Publisher", Maker);
            key.SetValue ("DisplayVersion", Version);
            key.SetValue ("DisplayIcon", appExe);
            key.SetValue ("InstallLocation", AppDir);
            key.SetValue ("UninstallString", "\"" + uninstaller + "\" /uninstall");
            key.SetValue ("NoModify", 1, RegistryValueKind.DWord);
            key.SetValue ("NoRepair", 1, RegistryValueKind.DWord);
        }

        MessageBox.Show ("THE FILTERFISH is installed!\n\nOpen your DAW and rescan plugins.\nYou'll find it under \"Asher Swissa\".",
                         Product + " Setup", MessageBoxButtons.OK, MessageBoxIcon.Information);
        return 0;
    }

    static int Uninstall()
    {
        if (MessageBox.Show ("Remove The FilterFish by Asher Swissa from this computer?", Product,
                             MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes)
            return 0;

        if (Directory.Exists (Vst3)) Directory.Delete (Vst3, true);
        if (File.Exists (Shortcut)) File.Delete (Shortcut);
        Registry.LocalMachine.DeleteSubKeyTree (RegKey, false);

        // This exe lives in AppDir, so remove the folder a moment after we exit.
        Process.Start (new ProcessStartInfo ("cmd.exe", "/c timeout /t 2 >nul & rmdir /s /q \"" + AppDir + "\"")
                       { WindowStyle = ProcessWindowStyle.Hidden, CreateNoWindow = true });

        MessageBox.Show ("The FilterFish was removed.", Product, MessageBoxButtons.OK, MessageBoxIcon.Information);
        return 0;
    }

    static void CopyDir (string from, string to)
    {
        Directory.CreateDirectory (to);
        foreach (var file in Directory.GetFiles (from))
            File.Copy (file, Path.Combine (to, Path.GetFileName (file)), true);
        foreach (var dir in Directory.GetDirectories (from))
            CopyDir (dir, Path.Combine (to, Path.GetFileName (dir)));
    }

    static void CreateShortcut (string path, string target)
    {
        var shellType = Type.GetTypeFromProgID ("WScript.Shell");
        object shell = Activator.CreateInstance (shellType);
        object link = shellType.InvokeMember ("CreateShortcut", BindingFlags.InvokeMethod, null, shell, new object[] { path });
        link.GetType().InvokeMember ("TargetPath", BindingFlags.SetProperty, null, link, new object[] { target });
        link.GetType().InvokeMember ("Save", BindingFlags.InvokeMethod, null, link, null);
    }
}
