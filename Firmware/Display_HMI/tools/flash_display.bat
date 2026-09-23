@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul 2>&1
title IncuNest - Programar Display HMI

echo.
echo  ============================================
echo    IncuNest - Programador Display HMI
echo    ESP32-S3  /  16MB Flash  /  QIO 80MHz
echo  ============================================
echo.

:: ── Configuracion ────────────────────────────────────────────────────────────
set ESPTOOL_VERSION=v4.7.0
set ESPTOOL_ZIP_NAME=esptool-%ESPTOOL_VERSION%-win64.zip
set ESPTOOL_URL=https://github.com/espressif/esptool/releases/download/%ESPTOOL_VERSION%/%ESPTOOL_ZIP_NAME%
set ESPTOOL_DIR=%~dp0tools
set ESPTOOL_EXE=%ESPTOOL_DIR%\esptool.exe

set BAUD=460800
set CHIP=esp32s3
set FLASH_MODE=qio
set FLASH_FREQ=80m
set FLASH_SIZE=16MB

:: Direcciones de memoria (segun IncuNest_display_v1_audio.csv)
set ADDR_BOOTLOADER=0x0
set ADDR_PARTITIONS=0x8000
set ADDR_OTADATA=0xe000
set ADDR_APP0=0x10000
set ADDR_SPIFFS=0x610000

:: Carpeta con los binarios
set FW_DIR=%~dp0firmware

:: ── Paso 1: Verificar firmware ────────────────────────────────────────────────
echo [1/4] Verificando archivos de firmware...

if not exist "%FW_DIR%" (
    echo.
    echo  ERROR: No se encuentra la carpeta "firmware\".
    echo.
    echo  Ejecuta prepare_firmware.bat en el PC de desarrollo primero,
    echo  y luego copia la carpeta completa "flash_display\" a este PC.
    goto :error
)

set MISSING=0
if not exist "%FW_DIR%\bootloader.bin"  ( echo  FALTA: firmware\bootloader.bin  && set MISSING=1 )
if not exist "%FW_DIR%\partitions.bin"  ( echo  FALTA: firmware\partitions.bin  && set MISSING=1 )
if not exist "%FW_DIR%\firmware.bin"    ( echo  FALTA: firmware\firmware.bin    && set MISSING=1 )
if !MISSING! == 1 goto :error

echo  OK - bootloader.bin, partitions.bin, firmware.bin encontrados.
if exist "%FW_DIR%\spiffs.bin"    echo  OK - spiffs.bin encontrado ^(se incluira en la programacion^).
if exist "%FW_DIR%\otadata.bin"   echo  OK - otadata.bin encontrado.
echo.

:: ── Paso 2: Obtener esptool ───────────────────────────────────────────────────
echo [2/4] Verificando esptool...

if exist "%ESPTOOL_EXE%" (
    echo  esptool ya disponible en tools\
) else (
    echo  Descargando esptool %ESPTOOL_VERSION% para Windows...
    echo  ^(requiere conexion a Internet solo la primera vez^)
    echo.

    if not exist "%ESPTOOL_DIR%" mkdir "%ESPTOOL_DIR%"

    powershell -NoProfile -Command ^
        "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12;" ^
        "Invoke-WebRequest -Uri '%ESPTOOL_URL%' -OutFile '%ESPTOOL_DIR%\esptool.zip' -UseBasicParsing"

    if errorlevel 1 (
        echo.
        echo  ERROR: No se pudo descargar esptool.
        echo  Verifica la conexion a Internet e intentalo de nuevo.
        goto :error
    )

    powershell -NoProfile -Command ^
        "Expand-Archive -Path '%ESPTOOL_DIR%\esptool.zip' -DestinationPath '%ESPTOOL_DIR%\extracted' -Force"

    :: Buscar esptool.exe en cualquier subcarpeta del zip
    for /r "%ESPTOOL_DIR%\extracted" %%f in (esptool.exe) do (
        copy "%%f" "%ESPTOOL_EXE%" >nul 2>&1
    )

    del "%ESPTOOL_DIR%\esptool.zip" >nul 2>&1
    rmdir /s /q "%ESPTOOL_DIR%\extracted" >nul 2>&1

    if not exist "%ESPTOOL_EXE%" (
        echo.
        echo  ERROR: No se encontro esptool.exe dentro del zip descargado.
        goto :error
    )
    echo  esptool descargado y listo.
)
echo.

:: ── Paso 3: Seleccionar puerto COM ───────────────────────────────────────────
echo [3/4] Deteccion de puerto COM...
echo.
echo  Asegurate de que el Display HMI esta conectado por USB.
echo.

:: Mostrar puertos COM disponibles con sus descripciones
echo  Puertos COM detectados:
powershell -NoProfile -Command ^
    "Get-WmiObject Win32_PnPEntity | Where-Object { $_.Name -match 'COM\d+' } | ForEach-Object { '    ' + $_.Name }" 2>nul

echo.
set /p COMNUM=  Introduce el numero del puerto COM (solo el numero, ej: 3 para COM3):
if "!COMNUM!"=="" (
    echo  ERROR: No introdujiste ningun numero.
    goto :error
)
set PORT=COM!COMNUM!
echo.
echo  Puerto seleccionado: !PORT!
echo.

:: ── Paso 4: Programar ────────────────────────────────────────────────────────
echo [4/4] Programando firmware en !PORT!...
echo.
echo  !! NO desconectes el USB durante la programacion !!
echo.

:: Construir la lista de binarios a escribir
set FLASH_ARGS=--chip %CHIP% --port !PORT! --baud %BAUD% --before default_reset --after hard_reset write_flash -z --flash_mode %FLASH_MODE% --flash_freq %FLASH_FREQ% --flash_size %FLASH_SIZE%
set FLASH_ARGS=%FLASH_ARGS% %ADDR_BOOTLOADER% "%FW_DIR%\bootloader.bin"
set FLASH_ARGS=%FLASH_ARGS% %ADDR_PARTITIONS% "%FW_DIR%\partitions.bin"

if exist "%FW_DIR%\otadata.bin" (
    set FLASH_ARGS=!FLASH_ARGS! %ADDR_OTADATA% "%FW_DIR%\otadata.bin"
)

set FLASH_ARGS=%FLASH_ARGS% %ADDR_APP0% "%FW_DIR%\firmware.bin"

if exist "%FW_DIR%\spiffs.bin" (
    echo  Incluyendo datos SPIFFS ^(imagenes y audio^)...
    set FLASH_ARGS=!FLASH_ARGS! %ADDR_SPIFFS% "%FW_DIR%\spiffs.bin"
)

echo.
"%ESPTOOL_EXE%" %FLASH_ARGS%

if errorlevel 1 (
    echo.
    echo  ============================================
    echo    ERROR: La programacion ha fallado.
    echo  ============================================
    echo.
    echo  Posibles causas:
    echo    - Puerto COM incorrecto
    echo    - Cable USB defectuoso o sin datos
    echo    - Drivers CH340 / CP210x no instalados
    echo    - El dispositivo no entra en modo bootloader
    echo.
    echo  Consejo: mantén pulsado BOOT al conectar el USB
    echo  para forzar el modo de programacion.
    goto :error
)

echo.
echo  ============================================
echo    Programacion completada con exito!
echo  ============================================
echo.
echo  El Display HMI se reiniciara automaticamente.
goto :end

:error
echo.
pause
exit /b 1

:end
echo.
pause
exit /b 0
