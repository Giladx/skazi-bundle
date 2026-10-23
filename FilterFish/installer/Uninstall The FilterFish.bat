@echo off
:: The FilterFish by Asher Swissa - uninstaller

net session >nul 2>&1
if %errorlevel% neq 0 (
    powershell -NoProfile -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

rmdir /s /q "%CommonProgramFiles%\VST3\The FilterFish.vst3" 2>nul
rmdir /s /q "%ProgramFiles%\Asher Swissa\The FilterFish" 2>nul
del "%ProgramData%\Microsoft\Windows\Start Menu\Programs\The FilterFish.lnk" 2>nul

echo The FilterFish has been removed.
pause
