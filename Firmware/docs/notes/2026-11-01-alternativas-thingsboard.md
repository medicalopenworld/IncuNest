# Alternativas a ThingsBoard: análisis comparativo de plataformas IoT

**Fecha de corte:** 1 de noviembre de 2026\
**Idioma:** español\
**Objetivo:** identificar alternativas a ThingsBoard, clasificarlas por
tipología y popularidad, y evaluar especialmente su madurez,
licenciamiento, independencia tecnológica y riesgo de cambio de
licencia.

------------------------------------------------------------------------

## 1. Resumen ejecutivo

ThingsBoard es una plataforma IoT integral: combina conectividad,
gestión de dispositivos, almacenamiento de telemetría, reglas, alarmas,
dashboards y administración multiusuario. Sustituirla no consiste
necesariamente en encontrar otro producto con el mismo nombre de
categoría. Hay dos caminos distintos:

1.  **Sustitución integral:** elegir otra plataforma que ya reúna la
    mayor parte de esas funciones.
2.  **Arquitectura modular:** construir una solución con piezas
    especializadas (broker MQTT, motor de flujos, base de datos,
    visualización, gestión de dispositivos y backend propio).

La segunda opción suele requerir más trabajo inicial, pero reduce la
dependencia de un único proveedor y permite reemplazar componentes de
forma independiente.

### Conclusiones principales

-   **OpenRemote** es una de las alternativas integrales más
    interesantes si se busca una plataforma autoalojada, con gestión de
    activos, reglas, automatización, conectividad y dashboards. Su
    licencia AGPL-3.0-or-later exige estudiar las obligaciones al
    modificar y ofrecer el software como servicio.
-   **Magistrala** (sucesor de Mainflux) merece una evaluación seria:
    plataforma IoT modular, multitenant, con mensajería y gestión de
    dispositivos, bajo Apache 2.0 en su backend. Conviene revisar por
    separado el alcance de la interfaz gráfica y de las funciones que el
    proveedor clasifica como Enterprise.
-   **EdgeX Foundry** es más apropiado como marco de integración y
    computación en el borde industrial que como reemplazo directo de
    ThingsBoard. Tiene gobernanza de fundación y licencia Apache 2.0 en
    sus servicios principales.
-   **FIWARE** no es un producto único, sino un ecosistema de
    componentes interoperables para gestión de contexto, gemelos
    digitales, datos y smart cities/industria. Su elección implica
    diseñar una arquitectura.
-   **EMQX + Node-RED + InfluxDB + Grafana** es una arquitectura modular
    flexible, pero hay que prestar atención: EMQX desde 5.9.0 usa BSL
    1.1, y Grafana usa AGPL-3.0 como licencia predeterminada de su
    núcleo. No debe describirse sin matices como una pila íntegramente
    Apache/MIT.
-   **Thinger.io** ofrece una experiencia de plataforma integrada, pero
    su servidor no es Open Source: lo son sus clientes y algunas
    herramientas. No es una alternativa Open Source integral si el
    requisito es poder auditar, modificar y autoalojar todo el servidor.
-   **La licencia de ThingsBoard cambió con la versión 4.4**, publicada
    el 29 de septiembre de 2026: las versiones 4.4 y posteriores pasan a
    BUSL 1.1, que es *source-available* y no una licencia Open Source
    aprobada por OSI. Las versiones anteriores mantienen Apache 2.0.
    Existe un programa de concesión para despliegues Community
    preexistentes; sus condiciones y plazo deben revisarse antes de
    actualizar.

**Recomendación inicial:** empezar una prueba comparativa de OpenRemote
y Magistrala como plataformas integrales, y diseñar en paralelo una
pequeña prueba modular con MQTT + Node-RED + base de datos + Grafana. Si
la prioridad es independencia a largo plazo, comparar no solo funciones
sino también licencias por componente, gobernanza, portabilidad de datos
y posibilidad real de sustituir cada pieza.

------------------------------------------------------------------------

## 2. Qué significa «alternativa a ThingsBoard»

Antes de comparar, conviene separar las funciones que ThingsBoard reúne:

  -----------------------------------------------------------------------
  Función                             Qué resuelve
  ----------------------------------- -----------------------------------
  Conectividad                        Entrada de datos por MQTT, HTTP,
                                      CoAP y otros protocolos

  Identidad y aprovisionamiento       Alta, autenticación, credenciales y
                                      ciclo de vida de dispositivos

  Modelo de activos                   Organización de dispositivos,
                                      ubicaciones, instalaciones y
                                      relaciones

  Telemetría e histórico              Recepción, persistencia, consulta y
                                      agregación de medidas

  Reglas y automatización             Transformaciones, condiciones,
                                      eventos y acciones

  Alarmas                             Detección, gestión y reconocimiento
                                      de incidencias

  Dashboards                          Visualización en tiempo real e
                                      histórica

  Usuarios y permisos                 Multiusuario, roles, tenants y
                                      control de acceso

  Escalabilidad                       Despliegue distribuido, alta
                                      disponibilidad y operación a gran
                                      escala

  Edge                                Procesamiento local, conectividad
                                      intermitente y sincronización
  -----------------------------------------------------------------------

Una plataforma puede ser excelente en conectividad y no incluir
dashboards. Otra puede ser muy buena en visualización, pero no gestionar
identidades de dispositivos. Por tanto, una comparación de «estrellas de
GitHub» o de nombres comerciales no mide equivalencia funcional.

### Criterios usados en este informe

-   **Cobertura funcional:** cuánto resuelve sin desarrollar componentes
    adicionales.
-   **Madurez y actividad:** releases, documentación, comunidad y
    mantenimiento.
-   **Licencia:** licencia del código realmente utilizado, no solo la
    etiqueta comercial.
-   **Gobernanza:** quién decide el roadmap y quién puede relicenciar.
-   **Despliegue:** facilidad de autoalojamiento, Docker/Kubernetes,
    edge y ARM.
-   **Portabilidad:** uso de protocolos y formatos abiertos, exportación
    de datos y APIs.
-   **Coste operativo:** infraestructura, mantenimiento, actualizaciones
    y soporte.

------------------------------------------------------------------------

## 3. Clasificación por tipología

### 3.1 Plataformas IoT integrales

Estas son las candidatas más cercanas a ThingsBoard.

  ----------------------------------------------------------------------------
  Proyecto          Enfoque                Qué aporta        Principales
                                                             reservas
  ----------------- ---------------------- ----------------- -----------------
  **OpenRemote**    Plataforma IoT/gestión Dispositivos,     AGPL; hay que
                    de activos             activos,          valorar la curva
                                           automatización,   de aprendizaje y
                                           reglas, roles,    el modelo de
                                           conectividad,     datos
                                           edge y dashboards 

  **Magistrala**    Framework/plataforma   Mensajería,       Revisar qué
  (antes Mainflux)  IoT                    identidad,        funcionalidades y
                                           dispositivos,     elementos de UI
                                           multi-tenancy,    pertenecen a cada
                                           APIs, reglas y    edición
                                           servicios         
                                           asociados         

  **Kaa IoT**       Plataforma IoT         Gestión de        Verificar qué
                    comercial con          dispositivos,     partes son
                    componentes de         datos y           realmente
                    desarrollo             aplicaciones IoT  autoalojables y
                                                             abiertas en la
                                                             edición concreta

  **Thinger.io**    Plataforma IoT         Conexión de       El servidor no es
                    gestionada y clientes  dispositivos,     Open Source;
                    abiertos               buckets,          clientes y
                                           dashboards,       herramientas sí
                                           alarmas, API y    
                                           herramientas      

  **Losant /        SaaS IoT               Puesta en marcha  Dependencia del
  Ubidots /                                rápida,           proveedor, costes
  Datacake /                               dashboards y      recurrentes y
  TagoIO**                                 conectores        menor control del
                                                             backend
  ----------------------------------------------------------------------------

**OpenRemote** y **Magistrala** son las dos opciones que priorizaría
para una evaluación Open Source integral. Kaa puede ser interesante si
el modelo comercial y el alcance de la edición elegida encajan.
Thinger.io es útil si se acepta un servidor propietario y se valora la
sencillez de conexión de dispositivos.

### 3.2 Plataformas industriales y de edge computing

  -----------------------------------------------------------------------
  Proyecto                Papel principal         Cuándo tiene sentido
  ----------------------- ----------------------- -----------------------
  **EdgeX Foundry**       Framework de            Pasarelas industriales,
                          microservicios para     heterogeneidad de
                          integración de          protocolos y
                          dispositivos y edge     procesamiento local

  **FIWARE**              Ecosistema de           Smart cities, energía,
                          componentes para        industria, gemelos
                          gestión de contexto y   digitales e
                          datos                   interoperabilidad

  **Eclipse Kapua**       Plataforma de gestión   Administración de
                          de                      flotas y gateways en
                          gateways/dispositivos y ecosistema Eclipse
                          conectividad            

  **Eclipse Hono**        Ingesta escalable y     Backend que necesita
                          APIs uniformes de       desacoplar protocolos
                          mensajería IoT          de dispositivos y
                                                  aplicaciones

  **ChirpStack**          Servidor de red LoRaWAN Redes LoRaWAN propias;
                                                  no sustituye por sí
                                                  solo a ThingsBoard
  -----------------------------------------------------------------------

No son reemplazos uno-a-uno. EdgeX, Hono y Kapua resuelven capas
concretas de la arquitectura. FIWARE es un conjunto de componentes, no
una instalación monolítica que se despliega y ya ofrece todo.

### 3.3 Componentes para construir una plataforma modular

  -----------------------------------------------------------------------
  Componente              Función                 Ejemplos
  ----------------------- ----------------------- -----------------------
  Broker de mensajería    Recibir y distribuir    Mosquitto, EMQX, HiveMQ
                          mensajes                

  Integración y flujos    Transformar, enrutar y  Node-RED
                          automatizar             

  Base de datos temporal  Almacenar telemetría    InfluxDB, TimescaleDB

  Visualización           Dashboards y            Grafana
                          exploración             

  Gemelos digitales       Modelo de estado de     Eclipse Ditto
                          entidades               

  Ingesta IoT             Adaptadores y APIs de   Eclipse Hono
                          dispositivos            

  LoRaWAN                 Red y gestión LoRaWAN   ChirpStack

  Analítica/streaming     Procesamiento de        Kafka, Flink,
                          eventos                 componentes propios
  -----------------------------------------------------------------------

Una pila modular puede igualar o superar a ThingsBoard, pero el
integrador pasa a ser responsable de autenticación, permisos, modelo de
dispositivos, alarmas, retención, actualizaciones, observabilidad y
coherencia entre componentes.

### 3.4 Servicios comerciales y SaaS

AWS IoT Core, Azure IoT Hub, Cumulocity, PTC ThingWorx, Losant, Ubidots,
Datacake, TagoIO, Blynk, ThingSpeak, Golioth y Particle pueden ser
alternativas operativas, pero no deben confundirse con alternativas Open
Source. Son relevantes cuando se prioriza reducir el trabajo de
operación, disponer de soporte contractual o acelerar la salida al
mercado.

------------------------------------------------------------------------

## 4. Análisis de las alternativas más relevantes

### 4.1 OpenRemote

**Web:** https://openremote.io/\
**Código:** https://github.com/openremote/core\
**Licencia principal observada:** AGPL-3.0-or-later.

OpenRemote se presenta como una plataforma IoT Open Source completa. Su
repositorio describe gestión y aprovisionamiento de dispositivos, tipos
de activos personalizables, reglas `when-then`, reglas de flujo y
JavaScript/Groovy, analítica, conectividad mediante agentes de protocolo
y APIs, multi-tenancy, usuarios y roles, gateway edge y constructor de
dashboards.

**Fortalezas** - Es una de las opciones más cercanas a ThingsBoard en
amplitud funcional. - Modelo de activos y relaciones útil para
instalaciones reales, no solo para visualizar sensores. - Incluye
automatización y herramientas de administración. - Autoalojable y con
repositorio público. - Proyecto activo: la organización publica releases
y mantiene documentación y comunidad.

**Debilidades/riesgos** - AGPL requiere atención si se modifica el
software y se ofrece a usuarios mediante una red. - El modelo de activos
y las reglas deben probarse con el caso de uso concreto. - Una
plataforma completa reduce trabajo de integración, pero también crea
dependencia de su modelo interno y sus APIs.

**Valoración:** candidato prioritario para una prueba de concepto
integral. Conviene probar importación/exportación de activos, permisos,
alarmas, retención histórica y despliegue ARM/edge antes de
comprometerse.

### 4.2 Magistrala (antes Mainflux)

**Web:** https://www.absmach.eu/products/magistrala\
**Código:** https://github.com/absmach/magistrala\
**Documentación:** https://magistrala.absmach.eu/docs/\
**Licencia del backend:** Apache-2.0.

Mainflux fue renombrado y migrado a Magistrala. No conviene seguir
evaluando el antiguo repositorio `mainflux/mainflux` como si fuera el
repositorio principal: el desarrollo continúa en `absmach/magistrala`.

Magistrala se describe como una plataforma/framework IoT para
mensajería, identidad, dispositivos, multi-tenancy, APIs y servicios de
procesamiento. La documentación menciona persistencia de mensajes,
reglas, alarmas, triggers, event sourcing, herramientas de desarrollo y
despliegue Docker.

**Fortalezas** - Apache 2.0 en el backend: licencia permisiva y adecuada
para uso comercial, modificación y redistribución bajo sus
condiciones. - Arquitectura basada en servicios y mensajería. -
Orientación explícita a IoT, edge y multi-tenancy. - Actividad visible
en GitHub y documentación propia. - Alternativa atractiva para equipos
que aceptan construir su propia interfaz o integrar herramientas de
visualización.

**Debilidades/riesgos** - No asumir que todo el stack distribuido por el
proveedor está cubierto por la misma licencia. Revisar backend, UI,
imágenes, paquetes y funciones Enterprise de forma separada. - Puede
requerir más integración que ThingsBoard para lograr una experiencia de
administración y visualización equivalente. - Al ser un framework, la
flexibilidad puede traducirse en más trabajo de arquitectura y
operación.

**Valoración:** candidato prioritario si se busca una base backend
abierta y permisiva. Antes de elegirlo, realizar una auditoría de
edición y artefactos: código de la UI, licencia de imágenes Docker,
reglas, alarmas, dashboards y condiciones de uso del servicio
gestionado.

### 4.3 EdgeX Foundry

**Web:** https://www.edgexfoundry.org/\
**Código:** https://github.com/edgexfoundry/edgex-go\
**Licencia del repositorio principal:** Apache-2.0.

EdgeX es un marco de microservicios neutral respecto a fabricante para
computación IoT en el edge. Facilita la integración de dispositivos,
servicios y protocolos mediante componentes desacoplados. Está alojado
en el entorno de Linux Foundation.

**Fortalezas** - Apache 2.0 en sus servicios principales. - Gobernanza
de proyecto bajo Linux Foundation. - Diseño modular y orientado a
edge. - Adecuado para integrar equipos y protocolos heterogéneos. - No
obliga a usar un único proveedor de hardware.

**Debilidades/riesgos** - No es un sustituto directo de ThingsBoard: no
debe esperarse la misma experiencia de dashboards, gestión de usuarios y
analítica de aplicación sin añadir otros componentes. - El despliegue
requiere comprender sus servicios, versiones compatibles y
composición. - La modularidad añade carga de integración y operación.

**Valoración:** muy interesante para gateways y sistemas industriales
donde la integración local sea prioritaria. Lo combinaría con un backend
de datos y visualización separado.

### 4.4 FIWARE

**Web:** https://www.fiware.org/\
**Catálogo de componentes:** https://www.fiware.org/catalogue/\
**Licencias:** dependen de cada Generic Enabler/componente.

FIWARE es un ecosistema de componentes abiertos. Su núcleo conceptual es
la gestión de contexto: representar entidades (sensores, activos,
edificios, máquinas, vehículos, etc.) y sus atributos, relaciones y
cambios. Orion Context Broker y Orion-LD son ejemplos de brokers de
contexto.

**Fortalezas** - Interoperabilidad y modelos de datos. - Amplio
ecosistema y presencia en smart cities, energía e industria. -
Arquitectura componible y posibilidad de evitar una plataforma
monolítica. - Útil para integrar información procedente de múltiples
sistemas.

**Debilidades/riesgos** - No existe una única licencia FIWARE: hay que
revisar cada componente. - Requiere seleccionar, desplegar e integrar
piezas. - Orion Context Broker y Orion-LD se publican bajo AGPL-3.0; no
asumir que todo FIWARE usa Apache. - El modelo NGSI/NGSI-LD requiere
aprendizaje.

**Valoración:** opción sólida cuando el problema es la interoperabilidad
de datos y activos de múltiples fuentes. No la elegiría solo para
sustituir rápidamente los dashboards y reglas de ThingsBoard.

### 4.5 EMQX + Node-RED + InfluxDB + Grafana

**EMQX:** https://www.emqx.com/\
**Node-RED:** https://nodered.org/\
**InfluxDB:** https://www.influxdata.com/\
**Grafana:** https://grafana.com/

Es una arquitectura de componentes especializados:

-   EMQX recibe y distribuye telemetría MQTT.
-   Node-RED implementa flujos, transformaciones, integraciones y
    automatizaciones.
-   InfluxDB almacena series temporales.
-   Grafana proporciona dashboards, consultas y alertas.

**Fortalezas** - Componentes conocidos y con ecosistemas amplios. - Cada
pieza puede reemplazarse por otra compatible. - Buen encaje con MQTT y
telemetría. - Permite adaptar la arquitectura a necesidades concretas.

**Debilidades/riesgos** - No existe de forma automática una consola
unificada de gestión de dispositivos, usuarios, activos y permisos. - La
seguridad y el modelo de identidad deben diseñarse con cuidado. -
Node-RED no equivale a un motor de reglas multi-tenant de plataforma
IoT. - La licencia actual de EMQX requiere especial atención: desde
5.9.0, EMQX usa BSL 1.1, no Apache 2.0. Su concesión gratuita permite
producción de un único nodo y contempla una excepción para
organizaciones sin ánimo de lucro/educativas bajo las condiciones
publicadas. - Grafana usa AGPL-3.0 como licencia predeterminada del
proyecto, aunque algunos directorios y componentes tienen licencias
distintas. - InfluxDB 3 Core declara componentes Open Source bajo MIT o
Apache 2.0; hay que distinguir Core, Enterprise y versiones 1.x/2.x/3.x.

**Valoración:** excelente como arquitectura modular, siempre que se
acepte la labor de integración y se revise la licencia exacta de cada
versión. Si el objetivo es reducir riesgo de relicenciamiento, se podría
valorar Mosquitto en lugar de EMQX y otras alternativas de base de
datos/visualización, pero habría que comprobar que cubren las
necesidades de escalabilidad y operación.

### 4.6 Thinger.io

**Web:** https://thinger.io/\
**Documentación:** https://docs.thinger.io/

Thinger.io proporciona conexión de dispositivos, Data Buckets,
dashboards, alarmas, endpoints, API y herramientas de administración.
Sus clientes para Arduino, Linux, ESP-IDF y otras plataformas se
publican como Open Source bajo MIT.

**Punto crítico:** la documentación oficial indica que el servidor no es
Open Source. Por tanto, no es equivalente a OpenRemote o Magistrala si
se exige disponer del código del backend y poder mantener un fork
independiente.

**Valoración:** interesante por sencillez y experiencia de uso, pero no
lo consideraría una opción plenamente Open Source de extremo a extremo.

### 4.7 Eclipse Hono, Kapua, Ditto y ChirpStack

-   **Eclipse Hono:** APIs uniformes para conectar gran cantidad de
    dispositivos; ingesta de telemetría y comandos. Licencia EPL-2.0 en
    el repositorio del proyecto. Es una capa de ingesta, no una
    plataforma de dashboards.
-   **Eclipse Kapua:** servicios para gestión de dispositivos/gateways y
    conectividad. El repositorio declara EPL-2.0. Proyecto más
    especializado que ThingsBoard.
-   **Eclipse Ditto:** framework de gemelos digitales. EPL-2.0. Adecuado
    para modelar y sincronizar el estado digital de dispositivos, pero
    necesita otros componentes para formar una solución completa.
-   **ChirpStack:** servidor de red LoRaWAN, licencia MIT. Es una pieza
    excelente si la conectividad es LoRaWAN, pero no sustituye la
    gestión general de telemetría, reglas y dashboards.

------------------------------------------------------------------------

## 5. Popularidad y señales de adopción

No existe una métrica única y fiable de «popularidad» para plataformas
IoT. Se deben separar al menos cuatro indicadores:

1.  **Uso declarado en encuestas:** mide lo que los participantes dicen
    utilizar.
2.  **Estrellas de GitHub:** reflejan interés o visibilidad, no
    instalaciones ni producción.
3.  **Actividad de desarrollo:** commits, releases, pull requests y
    respuesta a incidencias.
4.  **Adopción comercial:** clientes, despliegues, partners y presencia
    en proyectos reales.

### 5.1 Encuesta Eclipse 2023: uso declarado de middleware

En la encuesta Eclipse IoT & Edge Developer Survey 2023, la pregunta
«Which of the following IoT middleware do you use?» permitió seleccionar
hasta tres opciones. Los resultados publicados fueron:

  Middleware              Porcentaje de participantes
  --------------------- -----------------------------
  AWS IoT                                        36 %
  Microsoft Azure IoT                            25 %
  EMQ                                            14 %
  Cumulocity                                      9 %
  OpenRemote                                      8 %
  ThingsBoard                                     7 %
  ThingWorx                                       6 %
  Ericsson IoT                                    6 %

La encuesta reunió 1.037 participantes globales entre abril y julio de
2023. No es una cuota de mercado: es una muestra de desarrolladores,
committers, arquitectos y responsables que respondieron a una encuesta.
Tampoco debe extrapolarse automáticamente a 2026.

### 5.2 GitHub: una fotografía, no un ranking de calidad

Algunos indicadores visibles en repositorios consultados alrededor del 1
de noviembre de 2026:

  ------------------------------------------------------------------------------
  Proyecto/repositorio               Estrellas aproximadas Lectura
                                                observadas 
  --------------------------- ---------------------------- ---------------------
  ThingsBoard                                       22.500 Muy alta visibilidad
                                                           y comunidad

  Magistrala                                         2.600 Tracción
  (`absmach/magistrala`)                                   significativa para
                                                           una plataforma IoT
                                                           especializada

  OpenRemote                                         1.900 Comunidad visible y
  (`openremote/core`)                                      proyecto activo

  EdgeX Foundry (`edgex-go`)                         1.500 Relevante en edge
                                                           industrial; un
                                                           repositorio no
                                                           representa todo el
                                                           proyecto

  ChirpStack                                         1.100 Comunidad importante
  (`chirpstack/chirpstack`)                                en LoRaWAN

  Eclipse Hono                                         500 Proyecto
                                                           especializado del
                                                           ecosistema Eclipse

  Eclipse Kapua                                        246 Proyecto
                                                           especializado;
                                                           estrellas no capturan
                                                           despliegues
                                                           industriales
  ------------------------------------------------------------------------------

**Advertencias importantes:** - Los números cambian constantemente y son
aproximados. - No todos los proyectos agrupan el código en un único
repositorio. - Los repositorios de ThingsBoard y Magistrala no tienen
exactamente el mismo alcance que `edgex-go` o `chirpstack`. - Las
estrellas no indican usuarios activos, calidad, seguridad, estabilidad
ni adopción industrial. - Un proyecto empresarial puede tener poca
actividad pública porque gran parte del trabajo ocurre en repositorios
privados.

### 5.3 Lectura cualitativa de popularidad

-   **Muy alta visibilidad:** ThingsBoard. Tiene una comunidad GitHub
    muy superior a la mayoría de las alternativas examinadas.
-   **Alta visibilidad dentro de IoT Open Source:** Magistrala y
    OpenRemote.
-   **Alta relevancia por especialización:** EdgeX Foundry, FIWARE,
    ChirpStack y los proyectos Eclipse.
-   **Popularidad transversal, no comparable como plataforma IoT:**
    Node-RED, Grafana, InfluxDB y EMQX. Su adopción es amplia, pero cada
    uno resuelve una capa distinta.

No presentaría un ranking numérico único como si midiera cuota de
mercado. Es más honesto mostrar por separado adopción declarada,
actividad y comunidad.

------------------------------------------------------------------------

## 6. Licencias y riesgo de que un proyecto deje de ser Open Source

### 6.1 Open Source no significa «siempre Open Source»

Un titular puede cambiar la licencia de versiones futuras si tiene los
derechos necesarios sobre el código. Eso no suele borrar los derechos
que ya se concedieron sobre versiones anteriores: una versión publicada
legítimamente bajo MIT, Apache 2.0, GPL o AGPL continúa disponible bajo
los términos de esa licencia.

Hay que distinguir:

-   **Código publicado:** qué derechos se concedieron sobre esa versión.
-   **Versiones futuras:** el titular puede decidir publicarlas bajo
    otra licencia, si controla los derechos necesarios.
-   **Continuidad:** que el proyecto siga desarrollándose, corrigiéndose
    y publicando versiones.
-   **Independencia:** que exista capacidad real de mantener un fork y
    sustituir servicios externos.

### 6.2 Licencias permisivas

**MIT, BSD y Apache 2.0** permiten reutilizar, modificar y redistribuir
con pocas obligaciones, respetando sus condiciones.

Apache 2.0 destaca por incluir una concesión expresa de patentes de los
contribuidores, con condiciones y cláusula de terminación en caso de
litigio por patentes. MIT es más breve y no contiene una concesión de
patentes equivalente en esos términos.

### 6.3 Copyleft: GPL, AGPL y EPL

-   **GPL:** al distribuir una obra derivada, se aplican obligaciones de
    copyleft.
-   **AGPL:** añade obligaciones relacionadas con ofrecer a usuarios
    interacción remota con una versión modificada del programa.
-   **EPL 2.0:** copyleft débil a nivel de determinados
    módulos/archivos, con reglas propias sobre contribuciones y
    distribución.

AGPL no significa que cualquier aplicación que se comunique por HTTP o
MQTT deba hacerse Open Source. El análisis depende de si hay una obra
derivada, cómo se integra el software y cómo se ofrece. Para un producto
comercial, conviene pedir revisión jurídica específica.

### 6.4 Source-available no equivale a Open Source

Una licencia puede publicar el código y permitir modificarlo, pero
imponer límites de producción, número de nodos, usuarios, clientes,
redistribución o prestación de servicios. Si impone restricciones de
campo de uso, normalmente no cumple la definición de Open Source de OSI.

La **Business Source License (BUSL/BSL 1.1)** es una licencia
source-available con restricciones temporales o de uso, y una fecha de
cambio a otra licencia. Hasta esa fecha, no debe describirse como Open
Source en sentido OSI.

### 6.5 Situación específica de ThingsBoard 4.4

La información oficial consultada indica:

-   ThingsBoard 4.4 se publicó el **29 de septiembre de 2026**.
-   Desde 4.4, la plataforma unificada pasa a **BUSL 1.1**.
-   Las versiones anteriores a 4.4 permanecen bajo **Apache 2.0**.
-   La concesión gratuita para producción de 4.4+ permite:
    -   uso no comercial/educativo/sin ánimo de lucro hasta 1.000
        dispositivos, sin el límite de un servidor indicado para el caso
        comercial; o
    -   uso comercial hasta 100 dispositivos y un único servidor de
        producción.
-   La licencia exige conservar la marca/atribución y usar una clave
    válida para acogerse a la concesión gratuita.
-   Cada versión se convierte a Apache 2.0 cuatro años después de su
    publicación, conforme a la licencia publicada.

Además, ThingsBoard ha publicado un **Community Grant Program** para
instalaciones Community Edition que ya estaban en producción antes del
29 de septiembre de 2026. El programa permite registrar el despliegue y
conservar capacidad equivalente a la existente en esa fecha, con un
margen de dispositivos definido por el programa. La inscripción tiene
fecha límite publicada del 20 de julio de 2027. No debe confundirse esta
concesión para instalaciones preexistentes con una licencia Apache 2.0
general para nuevas instalaciones.

**Implicación práctica:** si una instalación existente de ThingsBoard
Community Edition funciona bien y está publicada bajo Apache 2.0, no hay
que migrar precipitadamente. Conviene estudiar el grant, mantener una
copia de la versión Apache 2.0 y diseñar una alternativa para el futuro.

### 6.6 Otros cambios de licencia relevantes

-   **EMQX:** desde la versión 5.9.0 se distribuye bajo BSL 1.1. Las
    versiones anteriores publicadas bajo Apache 2.0 conservan esa
    licencia. La concesión de uso adicional permite producción de un
    solo nodo y contiene una excepción para ciertas organizaciones
    educativas y sin ánimo de lucro. La operación en clúster o el uso
    embebido/hosted puede requerir licencia comercial.
-   **Grafana:** el proyecto principal usa AGPL-3.0 como licencia
    predeterminada, pero existen directorios y componentes con otras
    licencias. Hay que revisar el componente concreto.
-   **InfluxDB:** el proyecto declara que sus componentes Open Source se
    ofrecen bajo MIT o Apache 2.0, pero existen productos/ediciones
    comerciales y diferencias entre líneas de versión.
-   **Thinger.io:** clientes MIT, servidor no Open Source.
-   **OpenRemote:** AGPL-3.0-or-later.
-   **Magistrala:** Apache-2.0 para el backend del repositorio
    principal; comprobar la UI y componentes distribuidos por separado.
-   **EdgeX Foundry:** Apache-2.0 en el repositorio `edgex-go`;
    verificar cada servicio y dependencia.
-   **FIWARE:** licencias por componente; Orion y Orion-LD usan
    AGPL-3.0.
-   **Eclipse Hono, Kapua y Ditto:** EPL-2.0 en los repositorios
    consultados.
-   **ChirpStack:** MIT.

### 6.7 Cómo medir el riesgo de relicenciamiento

La licencia por sí sola no basta. Revisar también:

1.  Quién posee el copyright del código.
2.  Si los contribuidores ceden derechos o firman un CLA.
3.  Si una empresa controla las marcas, repositorios, infraestructura y
    releases.
4.  Si existe una fundación neutral y una gobernanza pública.
5.  Si el proyecto puede compilarse y desplegarse sin servicios
    propietarios.
6.  Si existen forks mantenidos y documentación suficiente para mantener
    uno propio.

Una fundación reduce ciertos riesgos de dependencia de una empresa, pero
no garantiza que cada componente vaya a mantener eternamente la misma
licencia.

------------------------------------------------------------------------

## 7. Arquitecturas de referencia

### A. Plataforma integral

**OpenRemote** o **Magistrala** como núcleo, con componentes externos
solo cuando sea necesario.

Ventajas: - Menor número de sistemas que integrar. - Modelo común de
dispositivos/activos. - Administración y permisos más coherentes.

Riesgos: - Dependencia del roadmap y modelo interno de la plataforma. -
Si se necesita una función muy específica, puede requerir extensiones o
un backend paralelo.

**Adecuada cuando:** se quiere sustituir ThingsBoard con el menor
desarrollo propio posible.

### B. Pila modular sencilla

``` text
Dispositivos
    |
 MQTT
    |
Broker (Mosquitto / EMQX)
    |
Node-RED (flujos, reglas e integraciones)
    |
Base de datos temporal (InfluxDB / TimescaleDB)
    |
Grafana (dashboards y alertas)
```

Ventajas: - Cada pieza tiene una función clara. - Fácil de ampliar y
sustituir por partes. - Buen ecosistema y abundante documentación.

Riesgos: - Gestión de dispositivos, usuarios, permisos y multi-tenancy
no queda resuelta automáticamente. - La lógica de negocio puede acabar
dispersa entre Node-RED, SQL y aplicaciones. - Debe diseñarse una
estrategia de seguridad extremo a extremo.

**Adecuada cuando:** se prioriza flexibilidad y se dispone de capacidad
de integración.

### C. Edge industrial

``` text
Dispositivos / PLC / sensores
    |
EdgeX Foundry
    |
Broker / bus de eventos
    |
Backend de aplicación
    |
Base de datos + Grafana
```

Ventajas: - Integración de protocolos y equipos heterogéneos. -
Procesamiento local y separación entre edge y cloud. - Adecuada para
instalaciones con conectividad irregular.

Riesgos: - Mayor complejidad operativa. - No aporta por sí sola toda la
experiencia de ThingsBoard.

**Adecuada cuando:** la integración de dispositivos industriales en el
borde es el principal problema.

### D. Arquitectura orientada a gemelos digitales

``` text
Dispositivos
    |
MQTT / HTTP / Hono
    |
Backend de ingestión
    |
Eclipse Ditto (gemelos digitales)
    |
Servicios de aplicación / reglas
    |
Base de datos y visualización
```

Ventajas: - Separa el estado digital del dispositivo de los protocolos
físicos. - Facilita integrar múltiples fuentes y aplicaciones.

Riesgos: - Requiere diseñar el backend y la visualización. - No es una
sustitución rápida de ThingsBoard.

**Adecuada cuando:** el modelo de activos y su estado digital es más
importante que disponer de una consola IoT monolítica.

------------------------------------------------------------------------

## 8. Comparativa resumida y shortlist

Escala cualitativa: **Alta / Media / Baja**. No pretende ser una
puntuación de laboratorio; indica el encaje típico según documentación y
arquitectura del proyecto.

  ------------------------------------------------------------------------------------------------
  Opción       Plataforma    Autoalojable   Apertura del         Licencia principal  Esfuerzo de
               integral                     servidor             observada           integración
  ------------ ------------- -------------- -------------------- ------------------- -------------
  OpenRemote   Alta          Sí             Sí                   AGPL-3.0-or-later   Medio

  Magistrala   Media-alta    Sí             Backend sí; revisar  Apache-2.0 backend  Medio-alto
                                            UI/ediciones                             

  EdgeX        Baja como     Sí             Sí                   Apache-2.0 en       Alto
  Foundry      plataforma                                        servicios           
               final; alta                                       principales         
               como edge                                                             
               framework                                                             

  FIWARE       Depende de    Sí             Sí, por componente   Mixta; incluye AGPL Alto
               componentes                                                           

  EMQX +       Alta tras     Sí             Mixta según          BSL, Apache/MIT,    Alto
  Node-RED +   integración                  componente/versión   AGPL, etc.          
  InfluxDB +                                                                         
  Grafana                                                                            

  Thinger.io   Alta como     Depende del    No para el servidor  Clientes MIT;       Bajo-medio
               servicio      servicio                            servidor            
                                                                 propietario         

  Hono +       Modular       Sí             Sí                   EPL-2.0             Alto
  Ditto +                                                                            
  backend                                                                            

  ChirpStack   No; LoRaWAN   Sí             Sí                   MIT                 Medio si
                                                                                     LoRaWAN
  ------------------------------------------------------------------------------------------------

### Shortlist recomendada

**1. OpenRemote --- mejor candidato integral Open Source**\
Empezaría aquí si se necesita una plataforma con consola, activos,
reglas y automatización. La condición principal es aceptar AGPL y
validar el encaje funcional.

**2. Magistrala --- mejor candidato de backend permisivo**\
Lo evaluaría si se valora Apache 2.0, una arquitectura backend propia y
la posibilidad de construir o integrar la interfaz necesaria. Revisar la
UI/Enterprise antes de dar por hecho que todo el despliegue es abierto.

**3. EMQX/Mosquitto + Node-RED + InfluxDB + Grafana --- mejor
arquitectura modular**\
Es la opción con mayor libertad de composición, pero también la que más
ingeniería exige. Revisar especialmente BSL de EMQX y AGPL de Grafana.

**4. EdgeX Foundry --- mejor para edge industrial**\
No lo pondría como reemplazo directo, sino como infraestructura de
campo/gateway dentro de una arquitectura más amplia.

**5. FIWARE --- mejor para interoperabilidad y contexto**\
Lo elegiría si se necesitan modelos de datos interoperables, integración
urbana/industrial o gemelos de contexto; no para una migración rápida de
dashboards.

**6. ChirpStack --- complemento específico**\
Si hay LoRaWAN, puede formar parte de cualquiera de las arquitecturas
anteriores.

------------------------------------------------------------------------

## 9. Plan de evaluación práctica

No conviene elegir únicamente por documentación o popularidad.
Recomiendo ejecutar la misma prueba en OpenRemote, Magistrala y una pila
modular.

### Prueba funcional mínima

1.  Conectar 10 dispositivos simulados mediante MQTT.
2.  Recibir telemetría a distintas frecuencias.
3.  Mantener atributos y metadatos por dispositivo.
4.  Crear grupos, ubicaciones y relaciones entre dispositivos.
5.  Crear dos usuarios con permisos distintos.
6.  Definir una regla de umbral y una alarma con reconocimiento.
7.  Mostrar valores actuales e históricos en un dashboard.
8.  Consultar histórico por intervalo y exportar datos.
9.  Desconectar dispositivos y detectar ausencia de mensajes.
10. Reiniciar servicios y comprobar recuperación sin pérdida indebida.
11. Realizar backup y restauración en otra máquina.
12. Probar despliegue en hardware ARM si se necesita edge/Raspberry Pi.

### Prueba de independencia

-   ¿Puedo exportar todos los datos en formatos documentados?
-   ¿Puedo acceder a la telemetría mediante MQTT/API sin pasar por la
    interfaz propietaria?
-   ¿Puedo sustituir la base de datos?
-   ¿Puedo ejecutar el sistema sin cuenta cloud ni licencia remota?
-   ¿Puedo compilar desde el código fuente?
-   ¿Puedo mantener un fork?
-   ¿La UI y los componentes de administración tienen licencia abierta?
-   ¿Puedo actualizar sin perder datos ni depender de una clave emitida
    por el proveedor?

### Criterios de decisión sugeridos

  Criterio                                   Peso sugerido
  ---------------------------------------- ---------------
  Cobertura funcional real                            25 %
  Licencia y control del código                       20 %
  Facilidad de operación y mantenimiento              15 %
  Portabilidad de datos y APIs                        15 %
  Seguridad y gestión de identidades                  10 %
  Comunidad, releases y soporte                       10 %
  Rendimiento y escalabilidad                          5 %

Los pesos deben adaptarse al proyecto. Si el riesgo de dependencia es
prioritario, aumentar el peso de licencia, gobernanza y portabilidad.

------------------------------------------------------------------------

## 10. Recomendaciones para reducir el lock-in

1.  **Usar protocolos abiertos en el borde:** MQTT, HTTP, CoAP, Modbus,
    OPC UA o LoRaWAN según el dispositivo.
2.  **Mantener un modelo de datos propio:** documentar nombres,
    unidades, timestamps, identificadores y relaciones, sin depender
    exclusivamente del modelo interno del proveedor.
3.  **Separar ingestión y visualización:** que el dashboard no sea el
    único modo de acceder a los datos.
4.  **Guardar telemetría en formatos exportables:** definir retención,
    backups y procedimientos de restauración.
5.  **Fijar versiones y conservar artefactos:** guardar releases,
    imágenes, dependencias, SBOM y documentación de despliegue.
6.  **Evitar funciones críticas solo disponibles en ediciones
    propietarias:** o encapsularlas tras interfaces reemplazables.
7.  **Revisar las licencias de cada versión:** no asumir que el nombre
    del proyecto implica una licencia estable.
8.  **Mantener un plan de salida:** probar periódicamente exportación,
    restauración y migración a otra implementación.
9.  **Revisar CLA y copyright:** entender si una empresa puede
    relicenciar contribuciones y quién controla los derechos.
10. **No confundir código público con independencia:** un servidor puede
    publicar parte del código y seguir dependiendo de servicios, claves
    o componentes cerrados.

------------------------------------------------------------------------

## 11. Fuentes y referencias

Las páginas siguientes son las fuentes principales consultadas. Para
decisiones de producción, revisar siempre el texto de licencia de la
versión/tag exacto que se vaya a desplegar; las páginas `main` o
`master` pueden cambiar.

### ThingsBoard y cambio de licencia

-   ThingsBoard, anuncio de relicenciamiento (29-09-2026):
    https://thingsboard.io/blog/one-thingsboard-source-available/
-   ThingsBoard, texto de licencia del repositorio:
    https://github.com/thingsboard/thingsboard/blob/master/LICENSE
-   ThingsBoard, programa Community Grant:
    https://thingsboard.io/community-grant-program/
-   ThingsBoard, releases:
    https://github.com/thingsboard/thingsboard/releases

### Plataformas y proyectos

-   OpenRemote, repositorio principal:
    https://github.com/openremote/core
-   OpenRemote, organización GitHub: https://github.com/openremote
-   Magistrala, repositorio principal:
    https://github.com/absmach/magistrala
-   Magistrala, documentación: https://magistrala.absmach.eu/docs/
-   Magistrala, planes y ediciones:
    https://www.absmach.eu/magistrala/pricing/
-   EdgeX Foundry, servicios Go y licencia:
    https://github.com/edgexfoundry/edgex-go
-   FIWARE Orion Context Broker, licencia:
    https://github.com/telefonicaid/fiware-orion
-   FIWARE Orion-LD, repositorio:
    https://github.com/FIWARE/context.Orion-LD
-   Eclipse Hono: https://github.com/eclipse-hono/hono
-   Eclipse Kapua: https://github.com/eclipse-kapua/kapua
-   Eclipse Ditto, aviso legal/licencia:
    https://github.com/eclipse-ditto/ditto/blob/master/legal/NOTICE.md
-   ChirpStack: https://github.com/chirpstack/chirpstack
-   Thinger.io, documentación sobre Open Source:
    https://docs.thinger.io/get-started

### Componentes modulares y licencias

-   EMQX, preguntas frecuentes de licencia:
    https://www.emqx.com/en/content/license-faq
-   Grafana, licenciamiento: https://grafana.com/licensing/
-   Grafana, detalle de licencias por directorio:
    https://github.com/grafana/grafana/blob/main/LICENSING.md
-   InfluxDB, repositorio y licencias:
    https://github.com/influxdata/influxdb
-   Node-RED: https://nodered.org/

### Popularidad y encuestas

-   Eclipse Foundation, informe IoT & Edge Developer Survey 2023 (PDF):
    https://5413615.fs1.hubspotusercontent-na1.net/hubfs/5413615/Eclipse%20IoT%20White%20Papers%20and%20Case%20Studies/2023%20IoT%20%26%20Edge%20Developer%20Survey%20Report.pdf
-   Eclipse Foundation, anuncio de resultados 2023:
    https://newsroom.eclipse.org/news/announcements/eclipse-foundation-releases-2023-iot-edge-developer-survey-results
-   Eclipse Foundation, índice de encuestas IoT:
    https://iot.eclipse.org/community/resources/iot-surveys/
-   ThingsBoard GitHub: https://github.com/thingsboard/thingsboard
-   Magistrala GitHub: https://github.com/absmach/magistrala
-   OpenRemote GitHub: https://github.com/openremote/core
-   EdgeX Foundry GitHub: https://github.com/edgexfoundry/edgex-go
-   ChirpStack GitHub: https://github.com/chirpstack/chirpstack

### Licencias Open Source

-   Open Source Initiative, definición de Open Source:
    https://opensource.org/osd
-   Apache License 2.0: https://www.apache.org/licenses/LICENSE-2.0
-   GNU GPL/AGPL: https://www.gnu.org/licenses/
-   Eclipse Public License 2.0: https://www.eclipse.org/legal/epl-2.0/

------------------------------------------------------------------------

## 12. Limitaciones y cuestiones pendientes

Este documento es un análisis tecnológico preliminar, no una auditoría
jurídica ni una prueba de rendimiento.

Antes de tomar una decisión final faltaría:

-   comprobar licencias de cada dependencia y artefacto Docker de las
    versiones concretas;
-   confirmar el alcance de las funciones Community/Enterprise y la
    licencia de la UI de Magistrala;
-   revisar la licencia y los términos de despliegue de cada componente
    FIWARE elegido;
-   probar rendimiento con el volumen real de dispositivos, mensajes y
    retención;
-   revisar políticas de seguridad, CVE, frecuencia de releases y
    soporte;
-   medir consumo de RAM/CPU y compatibilidad ARM en el hardware
    objetivo;
-   validar backup, restauración, actualización y migración;
-   revisar con asesoramiento jurídico cualquier uso comercial de
    AGPL/EPL/BUSL.

**Conclusión final:** si se busca una sustitución integral y
autoalojable, la primera comparación debería ser OpenRemote frente a
Magistrala. Si se busca máxima independencia tecnológica, una
arquitectura modular puede ser mejor, siempre que se acepte el coste de
integrar y mantener sus componentes. En cualquiera de los casos, la
verdadera protección frente a un cambio futuro de licencia no es elegir
un nombre concreto: es diseñar el sistema con protocolos abiertos, datos
portables, dependencias reemplazables y un plan probado de salida.
