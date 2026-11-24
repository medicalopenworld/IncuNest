# PANIC repetido de la motherBoard V18: el DNS de lwIP se usa desde dos hilos a la vez

Fecha: 2026-11-17. Placa: motherBoard V18 del banco. Firmware: 18.57.0 y 18.58.0.
Estado al escribirla: **diagnóstico, sin corregir.** No se había tocado código ni la placa.
Estado actual (2026-11-23): **corregido** en el PR #37, fusionado en dev como motherBoard 18.64.0
(las consultas DNS pasan al hilo de lwIP y el provisioning espera entre intentos). Ver la entrada 27 de
`Firmware/docs/known_issues.md`.

## 1. Resumen

- La placa se reinicia sola (`PANIC`) mientras intenta, sin éxito, abrir la
  conexión de provisioning de ThingsBoard por WiFi.
- El fallo no está en el código de la pulsioximetría ni en el de ThingsBoard.
  Está en cómo el core de Arduino (2.0.14) resuelve nombres: `WiFi.hostByName()`
  llama a `dns_gethostbyname()` de lwIP **desde la tarea de la aplicación**, sin
  pasar por el hilo TCP/IP y sin cerrojo. lwIP no admite eso: sus tablas de DNS
  solo las puede tocar su propio hilo (`tiT`).
- El firmware lo agrava: `WIFITBProvision()` se repite en cada vuelta de la
  tarea OTA, sin espera, así que hace una consulta DNS cada ~1,3 s (unas 2 800
  por hora). El camino de una unidad ya provisionada hace una cada 30 s.
- El volcado encaja con esto al detalle (apartado 3): el temporizador de DNS
  manda una consulta cuya entrada lleva la marca de «sin pcb» y, al usarla de
  índice, lee la variable de al lado, que vale 1. De ahí la dirección 0x15.
- **Sin confirmar:** la intercalación exacta entre los dos hilos (apartado 5).
- **Puede pasar en una unidad real** (apartado 6): sobre todo en una unidad sin
  provisionar conectada a una WiFi sin salida a internet o con el DNS caído.
- Arreglo propuesto, sin aplicar (apartado 7): espaciar los reintentos de
  provisioning (mitigación, tres líneas) y hacer que toda consulta DNS se
  ejecute dentro del hilo TCP/IP (arreglo de la causa).

## 2. Hechos medidos en el banco

Firmware compilado sin `include/Credentials.h`, es decir, con
`Credentials_public.h`, donde `THINGSBOARD_SERVER` es el nombre de relleno
`"myURL"`, que no resuelve.

| Firmware | Tiempo en marcha hasta el PANIC |
|---|---|
| 18.57.0 | ~39 385 s |
| 18.58.0 | ~1 117 s |
| 18.58.0 | ~3 054 s |
| 18.58.0 | ~1 995 s |
| 18.58.0 | ~6 230 s |

- Los cinco informes (`GET /debug/crash`) dicen `Reason : PANIC (4)`.
- En los cinco, las últimas líneas del registro son
  `[WiFi] no se pudo abrir la conexion de provisioning`, cada ~1,3 s.
- Volcado del último (`GET /debug/coredump`): tarea `tiT`, excepción 28
  (LoadProhibited), dirección leída 0x15, PC 0x4208261b, rastro
  `0x4208261b 0x4207b175 0x4207b257 0x4207b8a2 0x4207b8ef 0x42081d91 0x4207a735`.

Las cuatro caídas de la 18.58.0 suman 12 396 s: una cada ~52 minutos de media,
es decir, una por cada ~2 400 consultas DNS fallidas.

## 3. Lo que dice el volcado, traducido con cuidado

El ELF disponible es de una compilación 18.59.0, no el de la 18.58.0 que cayó.
Traducidas tal cual, las direcciones daban una cadena que no puede ser (dos
marcos seguidos dentro de `dns_send`, luego `dns_recv`, luego
`tcp_zero_window_probe`). lwIP viene precompilado (`liblwip.a`), así que su
código es idéntico en las dos compilaciones y solo cambia dónde queda colocado.

Buscando el desplazamiento con el que **las seis** direcciones de retorno caen
justo después de una instrucción de llamada, sale un único valor: **0x1d0**
(464 bytes). Con él la cadena es coherente de arriba abajo, y cada marco llama
de verdad al anterior:

| Dirección del volcado | En el ELF 18.59.0 | Función (fichero:línea de lwIP) |
|---|---|---|
| 0x4208261b (PC) | 0x420827eb | `udp_sendto` (udp.c:540) |
| 0x4207b175 | 0x4207b345 | `dns_send` (dns.c:919) |
| 0x4207b257 | 0x4207b427 | `dns_check_entry` (dns.c:1175) |
| 0x4207b8a2 | 0x4207ba72 | `dns_tmr` → `dns_check_entries` (dns.c:453, 1208) |
| 0x4207b8ef | 0x4207babf | `dns_timeout_cb` (dns.c:438) |
| 0x42081d91 | 0x42081f61 | `sys_check_timeouts` (timeouts.c:411) |
| 0x4207a735 | 0x4207a905 | `tcpip_thread` (tcpip.c:148) |

Es decir: **el temporizador de DNS de lwIP** (una vez por segundo mientras haya
consultas vivas) repasa la tabla, decide enviar la consulta de una entrada y
revienta al enviarla. La función no es `udp_sendto_if_src`, como parecía, sino
`udp_sendto`.

La instrucción que falla es `l8ui a10, a2, 20`: lee el byte 20 del `pcb`
(`pcb->local_ip.type`). La dirección leída es 0x15 = 21, así que **el puntero
`pcb` vale 1**. No es nulo: `udp_sendto` comprueba el nulo dos instrucciones
antes y habría devuelto un error sin más.

¿De dónde sale un puntero que vale 1? `dns_send` lo saca de
`dns_pcbs[entry->pcb_idx]`. En este binario:

| Símbolo | Dirección | Tamaño |
|---|---|---|
| `dns_table` | 0x3fca7de8 | 0x4a0 (4 entradas) |
| `dns_seqno` | 0x3fca8288 | 1 |
| `dns_last_pcb_idx` | 0x3fca8289 | 1 |
| `dns_pcbs` | 0x3fca828c | 0x10 (4 punteros) |
| `is_tmr_start` | 0x3fca829c | 1 |

`dns_pcbs` tiene cuatro huecos (índices 0 a 3) y **justo detrás** está
`is_tmr_start`, la bandera de «el temporizador de DNS está en marcha», que vale
1 precisamente cuando el temporizador corre. El índice 4
(`DNS_MAX_SOURCE_PORTS`) es la marca de «esta entrada no tiene pcb»: la escribe
`dns_call_found()` al terminar una consulta, después de liberar su pcb
(comprobado en el binario: `udp_remove`, y luego escribe 4 en el campo
`pcb_idx`). Leer `dns_pcbs[4]` es leer `is_tmr_start` más tres bytes de relleno
a cero: **0x00000001**.

Así que el volcado dice, sin margen: *el temporizador de DNS envió una entrada
que estaba marcada como activa pero cuyo `pcb_idx` era la marca de «sin pcb»*.

Lo que es seguro y lo que no de este apartado:

- Seguro: tarea `tiT`, excepción, dirección 0x15 (vienen del volcado).
- Muy probable: la cadena de la tabla. El desplazamiento 0x1d0 es el único en
  ±8 KB que cuadra las seis direcciones a la vez, pero sigue siendo un ELF que
  no es el exacto.
- La vecindad `dns_pcbs` / `is_tmr_start` está leída en el ELF 18.59.0. Las dos
  variables son estáticas del mismo fichero objeto precompilado, así que su
  orden no cambia entre compilaciones del firmware.

## 4. Por qué pasa

### 4.1 El core de Arduino llama a lwIP fuera de su hilo

`WiFiGenericClass::hostByName()` (arduino-esp32 2.0.14, `WiFiGeneric.cpp`,
líneas 1566-1587) hace esto desde la tarea que lo llama:

```cpp
err_t err = dns_gethostbyname(aHostname, &addr, &wifi_dns_found_callback, &aResult);
```

`dns_gethostbyname()` es de la API «raw» de lwIP: solo se puede llamar desde el
hilo TCP/IP o con su cerrojo cogido. El SDK de este core está compilado sin
cerrojo (`CONFIG_LWIP_TCPIP_CORE_LOCKING` sin definir) y con el hilo TCP/IP
fijado al núcleo 0 (`CONFIG_LWIP_TCPIP_TASK_AFFINITY_CPU0`), mientras que
nuestras tareas corren en el núcleo 1 (`CORE_ID_FREERTOS`). Los dos hilos
ejecutan código de DNS **a la vez, de verdad, en núcleos distintos**, sobre las
mismas variables: `dns_table`, `dns_pcbs`, la lista de pcbs UDP y la lista de
temporizadores.

Los bits `WIFI_DNS_IDLE_BIT` / `WIFI_DNS_DONE_BIT` del core solo evitan que dos
tareas de Arduino resuelvan a la vez. No protegen frente al hilo `tiT`.

La versión 2.0.16 del core hace lo mismo. La 3.x ya resuelve con
`lwip_getaddrinfo()`, que sí ejecuta la consulta dentro del hilo TCP/IP
(visto en una copia local de la 3.1.0).

### 4.2 La secuencia que explica el volcado

Dentro de `dns_gethostbyname()`, `dns_enqueue()` prepara la entrada en este
orden (fuente de lwIP, y comprobado en el binario del firmware):

1. busca una entrada libre de la tabla; reutiliza la de la consulta anterior,
   que quedó con `pcb_idx = 4` («sin pcb»);
2. **escribe `state = DNS_STATE_NEW`**;
3. copia el nombre;
4. reserva un pcb UDP con puerto al azar y **escribe `pcb_idx`**;
5. llama a `dns_check_entry()`, que pasa la entrada a `ASKING` y envía;
6. si el temporizador no corría, lo arranca (`sys_timeout`).

Entre el paso 2 y el 4 la entrada está marcada como nueva pero todavía lleva el
`pcb_idx = 4` viejo. Si en ese hueco el temporizador de DNS (hilo `tiT`, núcleo
0) repasa la tabla, ve una entrada nueva, la pasa a `ASKING` y la envía él:
`dns_send` → `dns_pcbs[4]` → 1 → `udp_sendto(pcb = 1)` → LoadProhibited en 0x15.
Es exactamente la pila del volcado. Y como el temporizador está corriendo,
`is_tmr_start` vale 1 seguro.

Con un solo hilo esto no puede ocurrir: los pasos 2 a 5 van seguidos y nadie
mira la tabla entre medias. No he encontrado ningún camino de un solo hilo que
deje una entrada activa con `pcb_idx = 4`.

### 4.3 Por qué el firmware lo provoca tan a menudo

`OTA_WIFI_Task` (`motherBoard/src/main.cpp`, líneas 716-723; prioridad 4,
núcleo 1, periodo de 50 ms) llama a `WifiOTAHandler()` →  `WIFI_TB_OTA()`
(`motherBoard/src/tasks/Wifi_OTA.cpp`):

```cpp
// Wifi_OTA.cpp:1965-1971 — unidad SIN provisionar: sin espera entre intentos
if (!Wifi_TB.provisioned) {
  if (in3.serialNumber == 0) {
    ...
  } else if (!Wifi_TB.provision_request_sent) {
    WIFITBProvision();
  }
} else {
  // Wifi_OTA.cpp:1973-1977 — unidad provisionada: un intento cada 30 s
  if (!tb_wifi.connected()) {
    if (millis() - Wifi_TB.lastReconnectAttempt < THINGSBOARD_RECONNECT_DELAY) {
      return;
    }
```

`WIFITBProvision()` (`Wifi_OTA.cpp:1209-1217`) llama a
`tb_wifi.connect(THINGSBOARD_SERVER, "provision", THINGSBOARD_PORT)`, que baja
por el SDK de ThingsBoard y PubSubClient hasta `WiFiClient::connect(host, port)`
y de ahí a `hostByName()`. Si el nombre no resuelve, devuelve falso, se escribe
la línea de aviso y **la vuelta siguiente, 50 ms después, se repite**. El
nombre fallido no se guarda en la caché de lwIP, así que cada intento es una
consulta nueva, con su entrada nueva en la tabla y su pcb nuevo.

Cada intento es una oportunidad de caer en el hueco del apartado 4.2. El camino
provisionado tiene 120 por hora; este, unas 2 800.

Efectos secundarios del mismo bucle, que no son el PANIC pero se ven:

- La tarea OTA pasa casi todo el tiempo bloqueada dentro de `hostByName()`. Esa
  misma tarea atiende el servidor web (`/update`, `/debug/*`), la consola de
  depuración y la orden `WIFI_EN`, que solo avanzan una vez por vuelta.
- El registro circular del informe de caídas se llena con la misma línea, y los
  cinco informes no conservan nada de lo anterior.

### 4.4 Las otras hipótesis del encargo

- **Cierre del cliente desde otra tarea.** `tb_wifi.disconnect()` se llama desde
  `WIFIProvisionResponse()`, que corre dentro de `tb_wifi.loop()`, en la misma
  tarea OTA. Y cerrar un socket va por la API de sockets, que sí es segura entre
  hilos y no toca `dns_pcbs`. No explica un `pcb` que vale 1.
- **WiFi que cae a mitad de una consulta.** `WIFITBProvision()` solo se llama
  con `WIFIIsConnected()` cierto, y sin interfaz `udp_sendto` devuelve un error
  de ruta, no revienta. Tampoco da un `pcb` que vale 1.
- **`GPRS.cpp`.** `TBProvision()` (`GPRS.cpp:922-929`) tiene el mismo patrón de
  reintento sin espera, pero va por `TinyGsmClient`: el nombre lo resuelve el
  módem, no lwIP. No puede dar este PANIC. Sí comparte el defecto de insistir
  sin pausa.

Otras tareas que también llaman a `hostByName()` y tienen el mismo riesgo,
aunque con mucha menos frecuencia: `DriveUpload.cpp:351` y
`ftest_sim_activation.cpp:147`. `ensureWifiTimeZoneSynced()`
(`Wifi_OTA.cpp:1879`) lo hace desde la propia tarea OTA.

## 5. Qué queda sin confirmar

1. **La intercalación exacta.** El volcado prueba el estado final (entrada
   activa con `pcb_idx = 4`, enviada por el temporizador). Que se llegue a él
   por el hueco del apartado 4.2 es la explicación que encaja y la única que he
   encontrado, pero no está observada.
2. **La frecuencia no me cuadra del todo.** El hueco dura decenas de
   microsegundos por consulta y el temporizador pasa una vez por segundo: una
   cuenta simple da una caída cada varias horas (lo que encaja con las ~11 h de
   la 18.57.0), no cada ~52 minutos. O el hueco real es más ancho (la tarea OTA
   tiene prioridad 4 y la desalojan la tarea del AFE a 500 Hz y las demás del
   núcleo 1, y mientras está desalojada el hueco sigue abierto), o hay otra
   intercalación entre los dos hilos que no he identificado. El arreglo de la
   causa es el mismo en los dos casos.
3. **Por qué cada intento tarda ~1,3 s.** El bucle solo espera 50 ms, así que
   el resto es la consulta DNS. No sé qué responde el router del banco a
   `"myURL"` ni cuánto tarda. Importa para el apartado 6: con un DNS que
   responda el error al instante, los intentos serían mucho más seguidos.
4. **Los otros cuatro informes.** Solo se ha descodificado el volcado del
   último. Que los otros cuatro sean el mismo fallo se apoya en que tienen el
   mismo motivo y las mismas últimas líneas, no en su pila.
5. **El ELF no es el exacto** (apartado 3).

La comprobación más directa, sin firmware nuevo: abrir el volcado que ya está
descargado con el ELF exacto de la 18.58.0 y mirar (a) la pila de la tarea
`OTA`: si la hipótesis es buena, estará dentro de `dns_gethostbyname` (copiando
el nombre o reservando el pcb); (b) `dns_table`: una entrada con `state` 1 o 2
y `pcb_idx` 4; (c) `is_tmr_start` a 1. Ojo con el issue #18 de
`known_issues.md`: `/debug/coredump` puede servir el volcado de una caída
anterior.

## 6. ¿Puede pasar en una unidad real?

Sí. El defecto está en el core, no en las credenciales de relleno; el nombre
que no resuelve solo multiplica las oportunidades.

**Unidad sin provisionar** (recién salida del flasher, con número de serie,
hasta que completa el primer provisioning). Con credenciales reales y la WiFi
conectada, basta con que el nombre del servidor no resuelva: router sin salida
a internet, DNS caído, portal cautivo, red de hospital que filtra. Entra en el
mismo bucle que el banco. Dos variantes:

- El DNS responde el error enseguida: los intentos se suceden tan rápido como
  tarde la respuesta más 50 ms. Puede ser bastante más frecuente que en el
  banco (más de un intento por segundo) y la placa caería antes. Sin medir.
- El DNS no responde: cada intento se queda bloqueado hasta ~15 s. Menos
  intentos por hora y menos caídas, pero la tarea OTA queda casi parada.

Con internet bien, el nombre resuelve, el provisioning sale a la primera y el
bucle se acaba: por eso no se ha visto en unidades normales.

**Unidad provisionada.** Solo consulta el DNS cuando ha perdido la conexión con
ThingsBoard, y entonces una vez cada 30 s (120 por hora). Si el riesgo por
consulta fuera el del banco (una caída por cada ~2 400), una unidad con internet
caído de forma sostenida se reiniciaría del orden de una vez al día. Es una
extrapolación, no una medida. Con la conexión a ThingsBoard en pie no hay
consultas desde la tarea OTA, salvo la de la zona horaria una vez al día.

**Gravedad.** Cada PANIC reinicia la placa que gobierna el calefactor. No he
revisado aquí qué hace el calefactor durante el arranque ni cuánto tarda en
recuperarse el control; eso habría que mirarlo aparte. En cualquier caso, una
unidad sin provisionar en una red sin DNS puede reiniciarse varias veces por
hora, y eso es un escenario plausible en una primera instalación.

## 7. Arreglo propuesto (sin aplicar)

Son dos cambios independientes. El primero es la mitigación mínima; el segundo
quita la causa. Propongo hacer los dos, en ese orden.

### 7.1 Espaciar los reintentos de provisioning (mitigación)

En `WIFI_TB_OTA()`, rama sin provisionar, no llamar a `WIFITBProvision()` más
de una vez cada cierto tiempo, con espera creciente. Esbozo:

```cpp
// Wifi_OTA.cpp, WIFI_TB_OTA(), unprovisioned branch (sketch, not compiled)
} else if (!Wifi_TB.provision_request_sent &&
           millis() - Wifi_TB.lastProvisionAttempt >= Wifi_TB.provisionBackoffMs) {
  Wifi_TB.lastProvisionAttempt = millis();
  WIFITBProvision();
  // 5 s, 10 s, 20 s ... capped at 5 min; back to 5 s once the request is out.
  Wifi_TB.provisionBackoffMs = Wifi_TB.provision_request_sent
      ? 5000UL
      : min(Wifi_TB.provisionBackoffMs * 2UL, 300000UL);
}
```

Hacen falta dos campos nuevos en `WIFIstruct` (`include/tasks/Wifi_OTA.h`). La
variante más corta, sin campos nuevos, es reutilizar `lastReconnectAttempt` y
`THINGSBOARD_RECONNECT_DELAY` (30 s fijos), igual que la rama provisionada.

Qué consigue: baja las consultas de ~2 800 por hora a 120 o menos, deja libre
la tarea OTA y deja de inundar el registro. **No elimina el fallo**: cada
consulta sigue teniendo su hueco.

`TBProvision()` de `GPRS.cpp` tiene el mismo patrón y admitiría la misma espera,
aunque allí no hay riesgo de este PANIC.

### 7.2 Que toda consulta DNS se ejecute en el hilo TCP/IP (causa)

Opción recomendada: envolver `dns_gethostbyname` en el enlazado
(`-Wl,--wrap=dns_gethostbyname` en `build_flags`) y que el envoltorio traslade
la llamada al hilo TCP/IP con `tcpip_api_call()`. Cubre de una vez todos los
que llaman (`WiFiClient`, `WiFiClientSecure`, `hostByName` directo) sin parchear
el core. Esbozo:

```cpp
// src/system/lwip_dns_guard.cpp (sketch, not compiled)
#include "lwip/dns.h"
#include "lwip/priv/tcpip_priv.h"
#include "arch/sys_arch.h"

extern "C" {
err_t __real_dns_gethostbyname(const char *name, ip_addr_t *addr,
                               dns_found_callback found, void *arg);

struct DnsCall {
  struct tcpip_api_call_data call;  // must be the first member
  const char *name;
  ip_addr_t *addr;
  dns_found_callback found;
  void *arg;
};

static err_t dns_call_in_tcpip(struct tcpip_api_call_data *data) {
  DnsCall *c = reinterpret_cast<DnsCall *>(data);
  return __real_dns_gethostbyname(c->name, c->addr, c->found, c->arg);
}

err_t __wrap_dns_gethostbyname(const char *name, ip_addr_t *addr,
                               dns_found_callback found, void *arg) {
  // Already in the TCP/IP thread (SNTP resolves from there): call straight
  // through, tcpip_api_call() from that thread would wait on itself.
  if (sys_thread_tcpip(LWIP_CORE_LOCK_QUERY_HOLDER)) {
    return __real_dns_gethostbyname(name, addr, found, arg);
  }
  DnsCall c = {{}, name, addr, found, arg};
  return tcpip_api_call(dns_call_in_tcpip, &c.call);
}
}
```

Comprobado: `tcpip_api_call` y `sys_thread_tcpip` están en el ELF actual. Sin
comprobar: que compile tal cual y que `--wrap` alcance también a las llamadas
que salen de dentro de `liblwip.a` (SNTP). Si no las alcanza no pasa nada: esas
ya corren en el hilo correcto.

Alternativa sin tocar el enlazado: resolver nosotros con `getaddrinfo()` (que sí
es seguro) y pasar a `tb_wifi.connect()` la IP en texto; `hostByName()` no
consulta el DNS si recibe una IP. Solo cubre los sitios que cambiemos a mano.

El porte a ESP-IDF nativo (rama de Pablo) no usa `hostByName()` de Arduino, así
que en principio no hereda este fallo. No lo he comprobado.

## 8. Cómo reproducirlo y cómo comprobar el arreglo

1. **Tal como ocurrió.** Firmware sin `Credentials.h`, unidad con número de
   serie y sin provisionar, WiFi conectada. Esperar. En el banco ha caído entre
   19 y 104 minutos con la 18.58.0. Firma a comprobar en el volcado: tarea
   `tiT`, LoadProhibited, dirección 0x00000015.
2. **Acelerado (pone a prueba la hipótesis).** Una orden temporal de la consola
   de depuración que llame a `WiFi.hostByName("myURL", ip)` en bucle desde la
   tarea OTA, con una pausa corta (10 ms), sin pasar por ThingsBoard. Si la
   hipótesis es buena, la misma firma debe aparecer mucho antes, y el tiempo
   hasta la caída debe bajar más o menos en proporción al número de consultas
   por segundo. Si no se acelera, la hipótesis está incompleta.
3. **Con el arreglo 7.2.** La misma prueba acelerada, durante al menos diez
   veces el tiempo medio hasta la caída medido en el punto 2, sin ningún PANIC.
   Y que siga funcionando lo normal: provisioning con credenciales reales, hora
   por SNTP, zona horaria por IP, servidor web de OTA.
4. **Con el arreglo 7.1 solo.** Comprobar en el registro que los intentos se
   espacian y que el servidor web responde con fluidez sin provisionar. No se
   puede demostrar la ausencia del PANIC con este cambio solo: lo hace más raro.

Todo esto necesita cargar firmware en la placa del banco; no se ha hecho.

## 9. Entrada propuesta para `known_issues.md`

`known_issues.md` tiene ahora cambios sin commit de otra sesión, así que la
entrada queda aquí para pegarla. El número (27) hay que confirmarlo al pegar.

```markdown
## 27. Repeated `PANIC` in the TCP/IP thread while ThingsBoard provisioning cannot resolve its host (OPEN)

Found on the V18 bench board on 2026-11-17 (18.57.0 and 18.58.0, built without
`Credentials.h`, so `THINGSBOARD_SERVER` is the placeholder `"myURL"`). Five
crash reports, all `PANIC (4)`, all ending in
`[WiFi] no se pudo abrir la conexion de provisioning` every ~1.3 s. Uptime
before the crash: ~39 385 s once on 18.57.0, then ~1 117, ~3 054, ~1 995 and
~6 230 s on 18.58.0.

The core dump of the last one: task `tiT` (lwIP's TCP/IP thread),
`LoadProhibited` reading address 0x15, in
`tcpip_thread → sys_check_timeouts → dns_timeout_cb → dns_tmr →
dns_check_entry → dns_send → udp_sendto`. The `pcb` passed to `udp_sendto` is
the value 1, which is what `dns_pcbs[DNS_MAX_SOURCE_PORTS]` reads: index 4 is
lwIP's "this entry has no pcb" marker and the variable right after the array is
`is_tmr_start`. The DNS timer sent an entry that was marked active while its
`pcb_idx` still held that marker.

**Cause (most likely, interleaving not observed).** arduino-esp32 2.0.14's
`WiFiGenericClass::hostByName()` calls lwIP's raw `dns_gethostbyname()` from
the calling task, with no TCP/IP core lock (`CONFIG_LWIP_TCPIP_CORE_LOCKING` is
not set) while the TCP/IP thread runs on the other core. `dns_enqueue()` sets
the entry to `DNS_STATE_NEW` before it assigns `pcb_idx`; a DNS timer tick in
that gap sends the entry with the stale index. A name that does not resolve is
never cached, so every attempt is a fresh enqueue, and the unprovisioned branch
of `WIFI_TB_OTA()` calls `WIFITBProvision()` on every pass of the OTA task with
no delay (~2 800 lookups per hour; the provisioned branch does 120).

**Impact.** Not limited to the placeholder credentials. An unprovisioned unit
on a WiFi network whose DNS fails (no uplink, captive portal, filtered network)
runs the same loop and can reboot several times per hour; this board drives the
heater. A provisioned unit only looks the host up once every 30 s while its
ThingsBoard link is down, so it is exposed far less. While it loops, the OTA
task is blocked inside `hostByName()` almost all the time, which also starves
the web server and the debug console.

**Fix, not applied yet.** (1) Space out the provisioning retries with a growing
delay, as the provisioned branch already does with
`THINGSBOARD_RECONNECT_DELAY`: fewer lookups, not a cure. (2) Run every DNS
lookup inside the TCP/IP thread: wrap `dns_gethostbyname` at link time
(`-Wl,--wrap`) and forward it with `tcpip_api_call()`, or resolve with
`getaddrinfo()` and hand the IP to the client. The 3.x core resolves with
`lwip_getaddrinfo()` and does not have this problem.

Details, the address translation and the bench test plan:
`docs/notes/2026-11-17-panic-dns-provisioning.md`.
```

## 10. Visto de paso (no tocado)

- Dentro de `dns_enqueue()` el core también llama a `sys_timeout()` desde la
  tarea de la aplicación. La lista de temporizadores de lwIP tampoco está
  protegida: es otra vía de fallo del mismo defecto, con otra firma (un
  temporizador que se pierde o una lista corrupta). El arreglo 7.2 la cubre.
- `hostByName()` espera 15 s como mucho. Si se rinde antes de que lwIP dé por
  perdida la consulta, deja registrada la dirección de una variable local que
  ya no existe; si la respuesta llega tarde y es buena, lwIP escribe en ella.
  No tiene que ver con este PANIC. El arreglo 7.2 no lo cubre.
- `applyWifiCredentials()` se llama también desde la tarea GPRS
  (`GPRS.cpp:158`, RPC `setWifi`), es decir, toca la API WiFi de Arduino desde
  una tarea que no es la OTA. El comentario de `Wifi_OTA.cpp` sobre `WIFI_EN`
  advierte de esa clase de carreras. No lo he analizado.

## 11. Añadido el 2026-11-18: la intercalación, vista en un volcado

Lo que el apartado 5 dejaba sin confirmar (la intercalación exacta y el ELF
no exacto) queda resuelto con una sexta caída de la misma placa, esta vez con
la 18.60.0 y con su ELF exacto (la huella SHA-256 del volcado coincide con la
del ELF). Uptime antes de caer: ~4 403 s. Las dos pilas, tal como las da gdb:

- Hilo TCP/IP (`tiT`), el que falla:
  `tcpip_thread → sys_check_timeouts → dns_timeout_cb → dns_tmr →
  dns_check_entry(i=0) → dns_send → udp_sendto(pcb=0x1, dst_port=53)`.
- Tarea OTA, en el mismo instante:
  `OTA_WIFI_Task → WifiOTAHandler → WIFI_TB_OTA → WIFITBProvision →
  ThingsBoard::connect → PubSubClient::connect → WiFiClient::connect →
  WiFiGenericClass::hostByName("myURL") → dns_gethostbyname →
  dns_gethostbyname_addrtype → dns_enqueue → dns_alloc_pcb →
  dns_alloc_random_port → udp_bind`.

Es la secuencia del apartado 4.2: la tarea OTA está dentro de `dns_enqueue()`,
con la entrada 0 ya marcada como nueva y sin `pcb_idx` asignado todavía, y el
temporizador de DNS la envía con el índice viejo (4), que lee `is_tmr_start`.

Sobre la frecuencia (apartado 5): el hueco no son unas pocas instrucciones.
Abarca `udp_new()` y `udp_bind()`, y la tarea OTA (prioridad 4) puede ser
desalojada dentro de él por las tareas de más prioridad de su núcleo. Eso
encaja mejor con una caída por hora que la cuenta simple; es un razonamiento,
no una medida.

El arreglo de los apartados 7.1 y 7.2 está escrito en la rama
`fix/dns-provisioning-panic` (motherBoard 18.59.1) y cargado en la placa del
banco desde el 2026-11-18.
