Analiza el proyecto y la documentación en firmware/docs para entender la arquitectura de la interfaz HMI, especialmente la pantalla de settings.

Contexto:

El sistema incluye funcionalidad de reproducción de audio (audio playback) integrada en la interfaz, principalmente en la pantalla de settings.

Objetivo (MUY IMPORTANTE):

NO eliminar funcionalidad.
NO modificar lógica interna.
NO comentar código.

Únicamente quiero que los BOTONES y elementos de la interfaz relacionados con audio NO sean visibles para el usuario.

Tareas:

1. Localiza los elementos de audio en la UI:
   - Botones de "play audio"
   - Labels de audio
   - Contenedores/paneles (containers) donde están estos elementos

2. Modificación estricta (SOLO UI):

   IMPORTANTE:
   - NO comentes código en main.cpp ni en ningún otro archivo
   - NO elimines líneas de código
   - NO refactorices

   ÚNICAMENTE haz lo siguiente:

   En UITask y en ElementCreation (o donde se creen los elementos UI):
      - Mantén la creación de los elementos
      - Justo después de crearlos, configúralos como ocultos (hidden / visible = false)

   Aplicar a:
   - Botones de audio
   - Labels de audio
   - Containers/paneles que los contienen

   Resultado esperado:
   - Los elementos siguen existiendo en el código
   - NO se renderizan en pantalla
   - NO son interactivos

3. Restricciones críticas:

   - NO tocar lógica de audio
   - NO eliminar funciones
   - NO modificar inicialización
   - NO comentar código existente
   - NO tocar archivos fuera de la UI salvo lo estrictamente necesario

4. Verificación:

   - Compilar el proyecto
   - Confirmar que no hay errores
   - Verificar que:
     - la pantalla de settings funciona correctamente
     - los botones de audio, labels y containers NO son visibles
     - no hay huecos visuales graves ni errores de layout

5. Devuelve:

   - Archivos modificados (especialmente UITask y ElementCreation)
   - Qué elementos han sido ocultados
   - Confirmación de que SOLO se ha cambiado la visibilidad

Resumen clave:

Este cambio es exclusivamente visual.
NO eliminar, NO comentar, NO refactorizar.
Solo hacer hidden los elementos de audio en el momento de su creación.