# Builds "SHAKSUKA Setup.exe": compiles the plugin, stages it in Installer\payload,
# then compiles the installer around it. Output: Builds\Windows\SHAKSUKA Setup.exe
#
#   powershell -ExecutionPolicy Bypass -File Installer\build_installer.ps1 [-BuildDir C:\shak\build] [-Juce C:\path\to\JUCE]
#
# Keep BuildDir short: Visual Studio fails on paths over 260 characters.
param(
    [string]$BuildDir = "$env:LOCALAPPDATA\Temp\shak\build",
    [string]$Juce = ""
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$payload = Join-Path $PSScriptRoot "payload"

$cfg = @("-S", $root, "-B", $BuildDir, "-G", "Visual Studio 17 2022", "-A", "x64")
if ($Juce) { $cfg += "-DFETCHCONTENT_SOURCE_DIR_JUCE=$Juce" }

function Run($exe, $argList) {
    & $exe @argList
    if ($LASTEXITCODE -ne 0) { throw "$exe failed ($LASTEXITCODE)" }
}

Write-Host "== Building plugin"
Run cmake ($cfg + "-DSHAK_INSTALLER=OFF")
Run cmake @("--build", $BuildDir, "--config", "Release", "--target", "Shaksuka_VST3", "Shaksuka_Standalone", "--parallel")

Write-Host "== Staging payload"
$art = Join-Path $BuildDir "Shaksuka_artefacts\Release"
New-Item -ItemType Directory -Force $payload | Out-Null
Copy-Item -Force "$art\VST3\SHAKSUKA.vst3\Contents\x86_64-win\SHAKSUKA.vst3" "$payload\SHAKSUKA.vst3"
Copy-Item -Force "$art\VST3\SHAKSUKA.vst3\Contents\Resources\moduleinfo.json" "$payload\moduleinfo.json"
Copy-Item -Force "$art\Standalone\SHAKSUKA.exe" "$payload\SHAKSUKA.exe"

Write-Host "== Building installer"
Run cmake ($cfg + "-DSHAK_INSTALLER=ON")
Run cmake @("--build", $BuildDir, "--config", "Release", "--target", "ShaksukaSetup", "--parallel")

$out = Join-Path $root "Builds\Windows"
New-Item -ItemType Directory -Force $out | Out-Null
Copy-Item -Force (Join-Path $BuildDir "ShaksukaSetup_artefacts\Release\SHAKSUKA Setup.exe") (Join-Path $out "SHAKSUKA Setup.exe")
Write-Host "== Done: $out\SHAKSUKA Setup.exe"
