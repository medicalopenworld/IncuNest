@echo off
cd /d "%~dp0"
echo Installing dependencies...
pip install -r requirements.txt pyinstaller

echo Building IncuNest_Flasher.exe...
pyinstaller flasher.spec --clean --noconfirm

echo.
echo Build complete.
echo Executable: dist\IncuNest_Flasher.exe
echo.
echo Distribution package:
echo   1. Copy dist\IncuNest_Flasher.exe
echo   2. Copy firmware\ folder (with populated bin files)
echo   3. Zip both together.
pause
