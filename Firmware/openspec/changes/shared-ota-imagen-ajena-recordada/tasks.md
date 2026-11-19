**Trabajo aplazado a propósito.** El bucle sólo se dispara con ThingsBoard mal
configurado (un binario de otra placa en el slot `fw_*` de un equipo), que hoy
no es el caso. Se anota con el análisis hecho para no repetirlo.

El riesgo del cambio está en dónde vive: la ruta de actualización. Un error aquí
deja equipos sin poder actualizarse en remoto, que es peor que el desgaste que
se intenta evitar. De ahí el orden: la invariante de "no bloquear lo legítimo"
se prueba ANTES de que exista el bloqueo.

Lanza `idf.py` desde PowerShell, nunca desde MSYS/Git Bash — un run bajo MSYS da
falso verde. Los tests de host van con
`.\Firmware\tools\host_tests\run_host_tests.ps1`.

## 1. Reproducir el bucle antes de arreglarlo

- [ ] 1.1 Montar el escenario en banco: asignar al Display HMI en ThingsBoard un
      binario de motherBoard y dejarlo correr. Confirmar con el log y con
      `/debug/state` que descarga, escribe y rechaza una vez por minuto.
- [ ] 1.2 Medir de verdad el coste por vuelta: bytes escritos en la partición
      OTA y bytes descargados. La cifra de ~2,5 MB de la propuesta es el tamaño
      del binario, no una medida.
- [ ] 1.3 Dejar el escenario documentado en `Firmware/docs/known_issues.md` con
      la cita del mecanismo (`fw_guarded_updater.h:33-49`).

## 2. Decisión de diseño: qué identifica "la misma imagen"

- [ ] 2.1 Comprobar qué metadatos entrega el SDK de ThingsBoard ANTES de la
      descarga (título, versión, checksum) y cuáles son fiables. Si el checksum
      llegara por adelantado sería mejor discriminador que la versión.
- [ ] 2.2 Escribir el ADR con la elección y el porqué. Si sólo hay
      título+versión, dejar escrito que dos binarios distintos con la misma
      versión son indistinguibles para este mecanismo, y que eso es aceptable
      porque reetiquetar es lo que hace el servidor al corregir el error.

## 3. La invariante primero (TDD en host)

- [ ] 3.1 Tests que fallan: un rechazo recordado NO impide instalar una imagen
      con título o versión distintos; una instalación correcta borra el
      recuerdo; el recuerdo se puede limpiar a mano.
- [ ] 3.2 Extraer la decisión a lógica pura y comprobable en host, como se hizo
      con `fan_guard`. `FwGuardedUpdater` no entra en los tests de host porque
      arrastra `IUpdater` y esp_ota; lo que se prueba es el predicado
      "¿descargar esta imagen?" con el estado recordado como entrada.
- [ ] 3.3 Verdes.

## 4. El recuerdo

- [ ] 4.1 Tests que fallan: tras un rechazo, el predicado dice que no se
      descargue el mismo título/versión; y sigue diciéndolo tras simular un
      reinicio (estado recargado de NVS).
- [ ] 4.2 Implementar la persistencia en NVS. Una clave por placa, en el
      namespace de configuración que ya existe. Mirar el tamaño de la partición
      antes de añadir (20 KB en el HMI) y no guardar más de lo necesario.
- [ ] 4.3 Enganchar el predicado en el punto que decide llamar a
      `Start_Firmware_Update()`, en las dos placas.
- [ ] 4.4 Verdes.

## 5. El aviso

- [ ] 5.1 Publicar el rechazo hacia ThingsBoard con título, versión y placa
      esperada. Una vez por rechazo nuevo, no en cada comprobación.
- [ ] 5.2 Dejarlo visible en el equipo para el servicio técnico.

## 6. Verificación en banco

- [ ] 6.1 Repetir 1.1 con el arreglo: confirmar que hay UNA descarga y UN
      rechazo, y que a partir de ahí no se escribe más flash.
- [ ] 6.2 Corregir el slot en ThingsBoard con el binario correcto y comprobar
      que la actualización entra sin tener que tocar el equipo.
- [ ] 6.3 `tools/bench_tests/board_guard_test.py`: las cinco en verde,
      especialmente la de la OTA legítima.
- [ ] 6.4 `tools/bench_tests/bench_tests.py`: la batería completa, para
      descartar regresiones colaterales.
