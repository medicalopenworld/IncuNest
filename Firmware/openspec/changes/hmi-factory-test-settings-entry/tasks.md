## 1. Display_HMI

Commit: `feat(hmi): entrada al test de hardware desde Settings`.

- [x] 1.1 Fila "Test de hardware" en `ui_ScreenSettings`, mismo estilo que las
      existentes, habilitada solo sin control ni fototerapia activos, con
      subtexto del motivo cuando está en gris.
- [x] 1.2 Apertura por hand-off con el aviso "equipo vacío"; "No" vuelve a
      Settings (`s_openedFromSettings`), "Salir" a la pantalla principal.
- [x] 1.3 Refresco del estado de la fila con cada `CTRL,STATE` mientras
      Settings está en pantalla; idioma en `UI_ApplyLanguage()`.
- [x] 1.4 `pio run -e main` en verde.

## 2. Documentación

- [x] 2.1 `docs/hmi.md` §7: las dos entradas y la condición de habilitación.

## 3. Verificación manual (banco)

- [ ] 3.1 **Manual** — con control activo la fila está en gris y no abre nada;
      al apagar el control se habilita sin salir de Settings.
- [ ] 3.2 **Manual** — "No" en el aviso devuelve a Settings; una batería
      completa y "Salir" cargan la pantalla principal.
