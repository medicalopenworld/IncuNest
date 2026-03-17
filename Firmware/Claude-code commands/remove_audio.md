Analiza todo el proyecto y la documentación existente en firmware/docs para entender la arquitectura, especialmente la parte de la interfaz HMI y la pantalla de settings.

Objetivo:

Eliminar completamente cualquier funcionalidad relacionada con audio en la interfaz.

Tareas:

1. Localiza todo el código relacionado con audio, incluyendo:
   - Botones de "play audio"
   - Controles o settings de audio
   - Variables, flags o estados relacionados con audio
   - Funciones que reproduzcan o gestionen audio

2. En la pantalla de settings:
   - Haz invisibles o elimina los botones de "play audio"
   - Elimina cualquier elemento visual relacionado con audio

3. A nivel de lógica:
   - Desactiva las funciones de reproducción de audio
   - Asegúrate de que no quedan referencias activas que puedan provocar errores
   - Mantén la estabilidad del sistema (no romper dependencias)

4. Revisa el resto del proyecto:
   - Elimina referencias innecesarias a audio si ya no se utilizan
   - Si alguna parte depende del audio, sustitúyela por un comportamiento neutro

5. Mantén el estilo del proyecto:
   - Respeta la arquitectura existente
   - No introduzcas cambios innecesarios fuera del scope

6. Devuelve:
   - Lista de archivos modificados
   - Explicación breve de los cambios
   - Confirmación de que la UI sigue funcionando correctamente

Importante:

No solo ocultes visualmente los botones: asegúrate de que toda la funcionalidad de audio queda desactivada (no accesible por el usuario). Compila el programa y verificalo.