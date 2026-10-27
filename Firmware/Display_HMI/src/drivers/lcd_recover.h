#pragma once

// Recuperacion del panel RGB tras una operacion de flash ("LCD screen drift").
//
// El framebuffer vive en PSRAM y el DMA del panel lo lee a traves de los
// bounce buffers. Cualquier escritura o borrado de flash suspende la cache de
// PSRAM unos ms; si eso cae mientras el driver rellena un bounce buffer, el
// DMA se queda sin datos, pierde la sincronia con VSYNC y la imagen queda
// DESPLAZADA de forma permanente (visto en banco 2026-10-26: primer volcado de
// la tendencia a LittleFS -> GLITCH DETECTED en LCD_DIAG y pantalla movida que
// no se recupera). esp_lcd_rgb_panel_restart() rearma la transmision en el
// siguiente VSYNC y devuelve la imagen a su sitio.
//
// LCD_RequestRestart() solo marca la peticion (vale desde cualquier tarea, no
// toca el panel). UI_Task la atiende fuera de LVGL_Lock en su siguiente
// pasada. La llaman:
//   - modules/storage/hmi_fs.cpp tras cada escritura, cierre, renombrado,
//     borrado de rango y formateo;
//   - lcd_diagnostics_log() cuando detecta un ciclo de bounce buffer perdido
//     por cualquier otra causa (NVS, WiFi), como red de seguridad.
void LCD_RequestRestart(void);
