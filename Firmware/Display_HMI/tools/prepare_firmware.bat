@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul 2>&1
title IncuNest - Preparar firmware para distribucion

echo.
echo  ============================================
echo    IncuNest - Preparar firmware distribuible
echo    ^(Ejecutar en el PC con PlatformIO^)
echo  ============================================
echo.

set BUILD_DIR=%~dp0.pio\build\main
set OUT_DIR=%~dp0firmware

:: Verificar que existe el build de PlatformIO
if not exist "%BUILD_DIR%\firmware.bin" (
    echo  ERROR: No se encuentra el build de PlatformIO.
    echo.
    echo  Compila primero el proyecto con:
    echo    pio run -e main
    echo.
    echo  Si no tienes PlatformIO, instálalo desde:
    echo    https://platformio.org/install/cli
    goto :error
)

:: Crear carpeta de salida
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

echo  Copiando binarios desde .pio\build\main\ ...
echo.

:: Copiar binarios obligatorios
copy "%BUILD_DIR%\bootloader.bin" "%OUT_DIR%\bootloader.bin" >nul
echo  OK  bootloader.bin

copy "%BUILD_DIR%\partitions.bin" "%OUT_DIR%\partitions.bin" >nul
echo  OK  partitions.bin

copy "%BUILD_DIR%\firmware.bin" "%OUT_DIR%\firmware.bin" >nul
echo  OK  firmware.bin

:: OTA data (opcional pero recomendado para reset limpio)
if exist "%BUILD_DIR%\ota_data_initial.bin" (
    copy "%BUILD_DIR%\ota_data_initial.bin" "%OUT_DIR%\otadata.bin" >nul
    echo  OK  otadata.bin
)

:: SPIFFS (imagenes + audio en data\)
if exist "%BUILD_DIR%\spiffs.bin" (
    copy "%BUILD_DIR%\spiffs.bin" "%OUT_DIR%\spiffs.bin" >nul
    echo  OK  spiffs.bin
) else (
    echo.
    echo  AVISO: No se encontro spiffs.bin.
    echo  Si el proyecto usa la carpeta data\, genera el SPIFFS con:
    echo    pio run -e main -t buildfs
    echo  y vuelve a ejecutar este script.
)

echo.
echo  ============================================
echo    Listo! Archivos en: firmware\
echo  ============================================
echo.
echo  Copia al PC de destino:
echo    - flash_display.bat
echo    - firmware\  ^(toda la carpeta^)
echo.
echo  El PC de destino NO necesita PlatformIO ni Python.
echo  Solo necesita los drivers USB del chip CH340 o CP210x.
goto :end

:error
echo.
pause
exit /b 1

:end
pause
exit /b 0
