@echo off
:: The FilterFish by Asher Swissa - Windows installer
:: Copies the VST3 to the standard VST3 folder and the standalone app to Program Files.

net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Asking for administrator permission...
    powershell -NoProfile -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

set "HERE=%~dp0"
set "VST3DIR=%CommonProgramFiles%\VST3"
set "APPDIR=%ProgramFiles%\Asher Swissa\The FilterFish"

echo.
echo  ==========================================
echo    THE FILTERFISH  by Asher Swissa
echo  ==========================================
echo.

if not exist "%VST3DIR%" mkdir "%VST3DIR%"
if exist "%VST3DIR%\The FilterFish.vst3" rmdir /s /q "%VST3DIR%\The FilterFish.vst3"
xcopy "%HERE%The FilterFish.vst3" "%VST3DIR%\The FilterFish.vst3\" /e /i /y /q >nul
echo  [OK] VST3 installed to %VST3DIR%

if not exist "%APPDIR%" mkdir "%APPDIR%"
copy /y "%HERE%The FilterFish.exe" "%APPDIR%\" >nul
echo  [OK] Standalone app installed to %APPDIR%

powershell -NoProfile -Command "$s=(New-Object -ComObject WScript.Shell).CreateShortcut([Environment]::GetFolderPath('CommonStartMenu')+'\Programs\The FilterFish.lnk'); $s.TargetPath='%APPDIR%\The FilterFish.exe'; $s.Save()"
echo  [OK] Start Menu shortcut created

echo.
echo  Done! Open your DAW and rescan plugins.
echo.
pause
