# Spec: Slide to Unlock — Lock Screen UX Redesign

**Date:** 2026-06-19  
**Scope:** `Firmware/Display_HMI` — lock screen interaction  
**Status:** Approved

---

## Problema

La pantalla de bloqueo actual usa un flujo de dos pasos:

1. Tap en cualquier parte → aparece popup con candado
2. Mantener pulsado el popup durante 1500 ms → desbloqueo

Esto genera dos problemas:

- **El usuario retira el dedo antes**: el hold de 1500 ms no es intuitivo; muchos usuarios asumen que el tap es suficiente y levantan el dedo al ver el popup.
- **El widget del pulsioxímetro no dispara el popup**: `LockScreenAnyTouch_cb` filtra por una whitelist de widgets (`ui_ScreenLock`, `ui_LockPPGChart`, `ui_LockHRCont`, `ui_LockPICont`). Cualquier toque cuyo target no esté en esa lista es ignorado.

Contexto adicional: el personal no usa guantes. El riesgo de desbloqueo accidental es principalmente el rozamiento casual de alguien que pasa cerca.

---

## Solución: Slide to Unlock

Reemplazar el popup hold-to-unlock por un **slider horizontal** que aparece al primer toque. El usuario debe arrastrar un thumb de izquierda a derecha para desbloquear. Si no completa el gesto, el thumb vuelve solo a la posición inicial.

### Flujo de interacción

1. Usuario toca **cualquier parte** de la pantalla de bloqueo → aparece el slider superpuesto en la parte inferior.
2. Usuario arrastra el **thumb** de izquierda a derecha.
3. Si el thumb alcanza ≥80% del recorrido → se carga `ui_ScreenMain`.
4. Si suelta antes → el thumb vuelve animado a la posición inicial (200 ms ease-out). Este snap-back enseña implícitamente que el gesto debe completarse.
5. Sin interacción durante **5 s** → el slider se oculta (mismo comportamiento que el popup actual).

---

## Diseño visual

```
┌─────────────────────────────────────────────────────────────┐
│  [🔓]  ←──────  DESLIZA PARA DESBLOQUEAR  ──────→          │
│  thumb                  label (fijo)                        │
└─────────────────────────────────────────────────────────────┘
```

| Elemento | Valor |
|---|---|
| Track ancho | 600 px |
| Track alto | 70 px |
| Track posición | centrado horizontalmente, `y = +175` desde centro de pantalla |
| Track fondo | negro semitransparente, borde blanco, border-radius 35 px |
| Thumb | círculo 66 px, color blanco, ícono `ui_img_candado_png` centrado |
| Thumb posición inicial | extremo izquierdo del track (x = 2 px de margen) |
| Fill | zona a la izquierda del thumb, blanco con opacidad 40% |
| Label | "DESLIZA PARA DESBLOQUEAR" / "SLIDE TO UNLOCK" / "GLISSEZ POUR DEVERROUILLER", Montserrat 18, blanco, centrado en track |

---

## Cambios técnicos

### Nuevos objetos UI (`ElementsCreation.cpp`)

Reemplazar `ui_UnlockCont` y sus hijos por:

```
ui_SlideUnlockCont    lv_obj, 600×70, centrado, y=+175
  ui_SlideTrack       lv_obj, 600×70 (fondo, no clickable)
  ui_SlideLabel       lv_label, "DESLIZA…", centrado en track
  ui_SlideThumb       lv_obj, 66×66, círculo, con candado img
  ui_SlideFill        lv_obj, h=66, ancho dinámico, detrás del thumb
```

### Lógica de arrastre (`UITask.cpp`)

Nueva función `SlideUnlock_event_cb` registrada sobre `ui_SlideThumb`:

| Evento | Acción |
|---|---|
| `LV_EVENT_PRESSED` | Guardar `drag_offset = indev_x - thumb_x` |
| `LV_EVENT_PRESSING` | `new_x = indev_x - drag_offset`, clamped a `[0, TRACK_W - THUMB_W]`; actualizar posición del thumb y ancho del fill |
| `LV_EVENT_RELEASED` / `LV_EVENT_PRESS_LOST` | Si `new_x >= 0.8 × (TRACK_W - THUMB_W)` → unlock; si no → animar thumb a x=0 con `lv_anim_t` (200 ms, ease-out) y fill a ancho 0 |

Constante nueva: `SLIDE_UNLOCK_THRESHOLD = 0.8f` (porcentaje del recorrido para desbloquear).

### Fix trigger universal (`UITask.cpp`)

En `LockScreenAnyTouch_cb`, eliminar el filtro de whitelist:

```cpp
// ANTES:
if (origin != ui_ScreenLock &&
    origin != ui_LockPPGChart &&
    origin != ui_LockHRCont &&
    origin != ui_LockPICont)
  return;

// DESPUÉS:
if (origin == ui_SlideThumb || origin == ui_SlideUnlockCont)
  return; // no re-triggerar si el toque ya está en el slider
```

### Eliminaciones

- `ui_UnlockCont`, `ui_Panel11`, `ui_Label4`, `ui_LockButton2`, `ui_Spinner1` — eliminados
- `start_lock_progress()`, `stop_lock_progress()`, `lock_progress_timer_cb()` — eliminadas
- `UnlockCont_event_cb()`, `add_unlock_press_cb_recursive()` — eliminadas
- `lockProgressArc`, `lockProgressTimer`, `lockProgressStart` — eliminadas
- `LOCK_PROGRESS_DURATION_MS` — eliminada
- `lockStopDebounceTimer`, `lock_stop_debounce_cb()` — eliminadas

### Sin cambios

- `UNLOCK_TIMEOUT_MS` (5000 ms) — se mantiene, mismo comportamiento
- `unlockTimeoutTimer` y `unlock_timeout_cb` — se mantienen
- `show_unlock_only()` → renombrar a `show_slide_unlock()`, misma lógica
- Soporte multilenguaje en `apply_language()` — añadir entrada para el nuevo label

---

## Casos de borde

| Caso | Comportamiento |
|---|---|
| Toque corto (tap rápido) | Slider aparece, thumb no se mueve, timeout 5s lo oculta |
| Dedo que se sale del thumb durante drag | `LV_EVENT_PRESS_LOST` → snap-back |
| Segundo toque mientras slider visible | `LockScreenAnyTouch_cb` solo resetea el timeout, no re-muestra |
| Alarma activa en lock screen | Sin cambio — alarma tiene su propio overlay |

---

## Archivos afectados

| Archivo | Cambio |
|---|---|
| `src/ui/ElementsCreation.cpp` | Sustituir `ui_UnlockCont` y hijos por `ui_SlideUnlockCont` |
| `include/ui/ElementsCreation.h` | Actualizar declaraciones de objetos |
| `src/tasks/UITask.cpp` | Nueva lógica de drag, fix whitelist, eliminar funciones de arco |
| `include/main.h` | Eliminar constantes y variables de lock progress |
