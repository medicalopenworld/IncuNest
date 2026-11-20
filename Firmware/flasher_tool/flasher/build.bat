@echo off
cd /d "%~dp0"
echo Installing dependencies...
pip install -r requirements.txt pyinstaller
if errorlevel 1 ( echo ERROR: pip install failed & exit /b 1 )

echo Building IncuNest_Flasher.exe...
pyinstaller flasher.spec --clean --noconfirm
if errorlevel 1 ( echo ERROR: PyInstaller build failed & exit /b 1 )

echo.
echo Build complete.
echo Executable: dist\IncuNest_Flasher.exe
echo.
if exist "..\data\firmware\motherboard\NO_SOBRESCRIBIR.txt" goto :pinned
if exist "..\data\firmware\display_hmi\NO_SOBRESCRIBIR.txt" goto :pinned
if exist "..\data\firmware\sensorboard\NO_SOBRESCRIBIR.txt" goto :pinned

echo Comprobando que los binarios no llevan credenciales dentro...
python check_no_secrets.py
if errorlevel 2 (
  echo.
  echo  AVISO: no se ha podido comprobar. Revisalo a mano antes de publicar.
  echo.
) else if errorlevel 1 goto :secret_found

echo Distribution package:
echo   1. Copy dist\IncuNest_Flasher.exe
echo   2. Copy firmware\ folder (with populated bin files)
echo   3. Zip both together.
pause
exit /b 0

:secret_found
echo ***************************************************************
echo  NO EMPAQUETAR: un binario de data\firmware\ lleva dentro una
echo  credencial de toda la organizacion (ver el detalle de arriba).
echo.
echo  El .exe de dist\ si es valido: no empaqueta data\firmware\.
echo  Lo que no se puede hacer es el zip con esos binarios.
echo ***************************************************************
pause
exit /b 1

:pinned
echo ***************************************************************
echo  NO EMPAQUETAR: hay un NO_SOBRESCRIBIR.txt en data\firmware\.
echo.
echo  Esa carpeta lleva binarios puestos a mano. Si salen de un
echo  entorno _factory, su firmware.bin contiene ONOMONDO_API_KEY,
echo  que controla TODAS las SIM de la organizacion, y el paquete
echo  del flasher se publica como asset de GitHub Releases en un
echo  repositorio publico.
echo.
echo  El .exe de dist\ si esta construido y es valido: no empaqueta
echo  data\firmware\ (ver flasher.spec). Lo que no se puede hacer es
echo  el zip del paso 2 con la carpeta en este estado.
echo.
echo  Para publicar: borra el NO_SOBRESCRIBIR.txt, pulsa "Actualizar
echo  binarios locales" para dejar el build de distribucion y vuelve
echo  a lanzar este script.
echo ***************************************************************
pause
exit /b 1
