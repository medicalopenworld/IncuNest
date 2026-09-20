#!/usr/bin/env python3
"""Bateria de pruebas de banco contra el hardware real.

Complementa a las suites Unity de host (tools/host_tests): alli se prueba la
logica pura en el PC, aqui se prueba el firmware ENTERO corriendo en las placas
—tareas, enlace serie, maquina de alarmas, control y actuadores— usando los
endpoints /debug/* de cada una.

    python bench_tests.py --mb 192.168.137.50 --hmi 192.168.137.41
    python bench_tests.py --mb ... --hmi ... --only alarmas
    python bench_tests.py --mb ... --hmi ... --list

REGLA QUE CUMPLE TODA PRUEBA DE AQUI: deja el equipo como lo encontro. El modo
depuracion se apaga en el `finally`, y apagarlo retira de golpe cualquier medida
simulada y cualquier alarma forzada (ver modules/debug/debug_mode.h). Una
bateria que se interrumpe a medias no puede dejar una incubadora con la
temperatura del aire falseada.

La prueba de coredump NO entra en la tanda normal: reinicia la placa. Va aparte
con --crash.
"""

import argparse
import base64
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

# Las de Credentials_public.h. Las reales se pasan por las variables de entorno
# INCUNEST_WEB_USER / INCUNEST_WEB_PASS y NO por --password: un argumento de
# linea de ordenes lo ve cualquiera con `ps`, se queda en el historial del
# shell y acaba en los logs de CI. La contrasena del servidor web de estas
# placas tambien abre /update, que flashea firmware arbitrario.
DEFAULT_USER = os.environ.get("INCUNEST_WEB_USER", "incunest")
DEFAULT_PASS = os.environ.get("INCUNEST_WEB_PASS", "changeme")

# alarm_ids.h
ALARM_AIR_THERMAL_CUTOUT = 1
ALARM_SKIN_THERMAL_CUTOUT = 2
ALARM_AIR_SENSOR_FAULT = 3
ALARM_SKIN_SENSOR_FAULT_SKIN_MODE = 4
ALARM_FAN_FAILURE = 5
ALARM_AIR_OUTLET_BLOCKED = 6
ALARM_MAINS_INTERRUPTION = 7
ALARM_AIR_TEMP_DEVIATION_HIGH = 8
ALARM_AIR_TEMP_DEVIATION_LOW = 9
ALARM_HEATER_FAULT = 12
ALARM_SUPPLY_UNDERVOLTAGE = 13
ALARM_HMI_LINK_LOST = 14

# alarm_machine.h
ST_INACTIVE, ST_PENDING, ST_ACTIVE, ST_SILENCED, ST_ACKED = range(5)
SIGNALLING = (ST_ACTIVE, ST_SILENCED, ST_ACKED)


class BenchError(AssertionError):
    pass


class Board:
    def __init__(self, name, host, user, password, timeout=8.0):
        self.name = name
        self.host = host
        self.timeout = timeout
        token = base64.b64encode(f"{user}:{password}".encode()).decode()
        self.auth = f"Basic {token}"

    def _request(self, method, path, params=None):
        url = f"http://{self.host}{path}"
        data = None
        if params:
            body = urllib.parse.urlencode(params).encode()
            if method == "GET":
                url += "?" + body.decode()
            else:
                data = body
        req = urllib.request.Request(url, data=data, method=method)
        req.add_header("Authorization", self.auth)
        if data is not None:
            req.add_header("Content-Type", "application/x-www-form-urlencoded")
        with urllib.request.urlopen(req, timeout=self.timeout) as r:
            return r.status, r.read().decode("utf-8", "replace")

    def get_json(self, path):
        status, body = self._request("GET", path)
        if status != 200:
            raise BenchError(f"{self.name} GET {path} -> HTTP {status}")
        try:
            return json.loads(body)
        except json.JSONDecodeError as exc:
            raise BenchError(
                f"{self.name} GET {path} devolvio un JSON invalido ({exc}). "
                f"Primeros 200 B: {body[:200]!r}"
            ) from exc

    def post(self, path, expect=200, **params):
        try:
            status, body = self._request("POST", path, params)
        except urllib.error.HTTPError as exc:
            status, body = exc.code, exc.read().decode("utf-8", "replace")
        if expect is not None and status != expect:
            raise BenchError(
                f"{self.name} POST {path} {params} -> HTTP {status} "
                f"(esperado {expect}): {body[:200]}"
            )
        return status, body

    # --- atajos ---
    def state(self, with_tasks=False):
        # La tabla de tareas va bajo peticion: construirla cuesta ancho de banda
        # de PSRAM y en el display eso compite con el panel (flicker). Solo la
        # pide quien la necesita.
        return self.get_json("/debug/state" + ("?tasks=1" if with_tasks else ""))

    def debug(self, on):
        self.post("/debug/mode", on=1 if on else 0)

    def reachable(self):
        try:
            self.get_json("/debug/state")
            return True
        except Exception:
            return False


def wait_for(fn, timeout_s, period_s=0.25, what="la condicion"):
    """Espera activa con plazo. Devuelve el ultimo valor, o levanta."""
    deadline = time.time() + timeout_s
    last = None
    while time.time() < deadline:
        last = fn()
        if last:
            return last
        time.sleep(period_s)
    raise BenchError(f"plazo agotado ({timeout_s:.0f} s) esperando {what}")


def alarm_state(state_json, alarm_id):
    for a in state_json.get("alarms", {}).get("list", []):
        if a["id"] == alarm_id:
            return a["state"]
    return ST_INACTIVE


def alarm_signalling(state_json, alarm_id):
    return alarm_state(state_json, alarm_id) in SIGNALLING


def clear_latched(mb):
    """Reconoce las alarmas enclavadas que hayan quedado.

    Los cortes termicos son latching por norma (201.15.4.2.1 aa)/bb)): retirar
    la condicion NO apaga el aviso. Una prueba que simule uno y se vaya deja la
    placa avisando y desordena todo lo que venga detras — que es exactamente lo
    que paso la primera vez que corrio esta bateria. Se reconoce por el mismo
    camino que usaria el operador desde el display.
    """
    mb.post("/debug/inject", line="HMI,ALM_RESET")
    deadline = time.time() + 10
    while time.time() < deadline:
        if not mb.state()["alarms"]["list"]:
            return True
        time.sleep(0.5)
    return False


# ===========================================================================
# Pruebas
#
# Cada una es fn(mb, hmi) y levanta BenchError si falla. El nombre corto del
# diccionario de abajo es lo que acepta --only.
# ===========================================================================


def t_estado_alcanzable(mb, hmi):
    """Las dos placas contestan /debug/state y el JSON esta completo."""
    for b in (mb, hmi):
        if b is None:
            continue
        st = b.state(with_tasks=True)
        # El volcado se trunca si no cabe; que parsee ya demuestra que no.
        for key in ("debug", "uptime_ms", "heap", "tasks"):
            if key not in st:
                raise BenchError(f"{b.name}: falta '{key}' en /debug/state")
        # Una tabla vacia con `tasks_note` no es un fallo: es una placa cuyo
        # core no trae configUSE_TRACE_FACILITY y por tanto no tiene
        # uxTaskGetSystemState(). Le pasa a la motherBoard con el core Arduino
        # 2.0.14. Vacia y SIN nota si es un fallo: el volcado se trunco o la
        # tabla no se construyo.
        if not st["tasks"] and "tasks_note" not in st:
            raise BenchError(f"{b.name}: la tabla de tareas viene vacia")


# Tareas que crea ESP-IDF, no nosotros. Sus pilas las dimensiona IDF y llevan
# anos funcionando; medirlas con el mismo rasero que las nuestras solo produce
# ruido —ipc0/ipc1/sys_evt viven de serie por debajo de 512 B libres— y una
# prueba que salta siempre es una prueba que nadie lee.
IDF_OWNED_TASKS = {
    "ipc0", "ipc1", "sys_evt", "esp_timer", "wifi", "tiT", "tcpip",
    "Tmr Svc", "IDLE", "IDLE0", "IDLE1", "mdns", "btController", "mqtt_task",
    "httpd", "sock_monitor", "wifi_apsta",
}


def t_margen_de_pila(mb, hmi):
    """Ninguna tarea esta cerca de desbordar la pila.

    `stack_min` viene de uxTaskGetStackHighWaterMark(), que EN ESP-IDF DEVUELVE
    BYTES, no palabras como en FreeRTOS de serie. La primera version de esta
    prueba lo leyo como palabras y con un suelo de 512 saltaba en cada pasada
    por ipc0/ipc1/sys_evt, que son de IDF y siempre andan por ahi.

    Asi que hay dos suelos: uno exigente para las tareas que creamos nosotros
    —donde el tamano lo elegimos y un desbordamiento es culpa nuestra— y uno
    minimo para las de IDF, que solo avisa si de verdad estan al borde.
    """
    OURS_FLOOR_BYTES = 512
    IDF_FLOOR_BYTES = 128
    problems = []
    for b in (mb, hmi):
        if b is None:
            continue
        for t in b.state(with_tasks=True)["tasks"]:
            idf = t["n"] in IDF_OWNED_TASKS
            floor = IDF_FLOOR_BYTES if idf else OURS_FLOOR_BYTES
            if t["stack_min"] < floor:
                problems.append(
                    f"{b.name}/{t['n']}: {t['stack_min']} B libres "
                    f"(suelo {floor}{', tarea de IDF' if idf else ''})"
                )
    if problems:
        raise BenchError("tareas con poco margen de pila: " + ", ".join(problems))


def t_modo_debug_arranca_apagado(mb, hmi):
    """Regla 1: el modo no se persiste, asi que tras un arranque esta apagado.

    Solo es concluyente si la placa lleva poco encendida; si no, se salta en
    vez de mentir.
    """
    checked = 0
    for b in (mb, hmi):
        if b is None:
            continue
        st = b.state()
        if st["uptime_ms"] > 5 * 60 * 1000:
            continue  # lleva mucho encendida: alguien pudo encenderlo a mano
        if st["debug"]:
            raise BenchError(f"{b.name}: el modo depuracion esta ENCENDIDO "
                             "tras un arranque reciente")
        checked += 1
    if checked == 0:
        raise SkipTest("las dos placas llevan encendidas mas de 5 min")


def t_simular_requiere_modo(mb, hmi):
    """Con el modo apagado, todo lo que simula se rechaza con 409."""
    mb.debug(False)
    mb.post("/debug/sensor", expect=409, ch="air_temp", value=39.0)
    mb.post("/debug/alarm", expect=409, id=ALARM_FAN_FAILURE, present=1)
    mb.post("/debug/crash", expect=409, kind="abort")
    if hmi is not None:
        hmi.debug(False)
        hmi.post("/debug/inject", expect=409, line="CTRL,TEL,36.5,36.2,55.0,1,353")
    # ...pero LEER siempre se puede.
    mb.state()


def t_apagar_retira_las_simulaciones(mb, hmi):
    """Regla 2: apagar el modo limpia overrides y alarmas forzadas de golpe."""
    mb.debug(True)
    mb.post("/debug/sensor", ch="air_temp", value=42.0)
    mb.post("/debug/alarm", id=ALARM_HEATER_FAULT, present=1)

    st = mb.state()
    if "air_temp" not in st["overrides"]:
        raise BenchError("la simulacion de air_temp no quedo registrada")
    if st["alarms"]["forced_mask"] == 0:
        raise BenchError("la alarma forzada no quedo registrada")

    mb.debug(False)
    st = mb.state()
    if st["overrides"]:
        raise BenchError(f"apagar el modo dejo simulaciones vivas: {st['overrides']}")
    if st["alarms"]["forced_mask"] != 0:
        raise BenchError("apagar el modo dejo alarmas forzadas vivas")
    if st["debug"]:
        raise BenchError("el modo sigue encendido")


def t_medida_simulada_llega_al_control(mb, hmi):
    """Una temperatura de aire simulada aparece en el estado de la placa.

    Es la prueba de que el punto de inyeccion esta donde dice estar: despues de
    los sensores reales y antes de que nadie lea la medida.
    """
    mb.debug(True)
    try:
        target = 33.75
        mb.post("/debug/sensor", ch="air_temp", value=target)
        st = wait_for(
            lambda: (lambda s: s if abs(s["meas"]["air"] - target) < 0.05 else None)(mb.state()),
            timeout_s=5,
            what="que la temperatura de aire simulada llegue a in3",
        )
        if abs(st["meas"]["air"] - target) >= 0.05:
            raise BenchError(f"air={st['meas']['air']} != {target}")

        # Y al retirarla, la medida real vuelve sola.
        mb.post("/debug/sensor", ch="air_temp", clear=1)
        wait_for(
            lambda: abs(mb.state()["meas"]["air"] - target) > 0.05,
            timeout_s=5,
            what="que vuelva la medida real del sensor de aire",
        )
    finally:
        mb.debug(False)


def t_corte_termico_del_aire(mb, hmi):
    """Subir la temperatura de aire simulada dispara el corte termico y corta
    el calefactor. Es la clausula 201.15.4.2.1 aa)."""
    mb.debug(True)
    try:
        mb.post("/debug/sensor", ch="air_temp", value=45.0)
        st = wait_for(
            lambda: (lambda s: s if alarm_signalling(s, ALARM_AIR_THERMAL_CUTOUT) else None)(mb.state()),
            timeout_s=30,
            what="ALARM_AIR_THERMAL_CUTOUT con el aire a 45 C",
        )
        if not st["heater"]["must_cut"]:
            raise BenchError(
                "el corte termico esta senalizando pero heater_must_cut es false"
            )

        # Y al enfriarse el equipo SE RECUPERA SOLO: se libera el calefactor y
        # el aviso se va con la condicion, sin que nadie pulse nada.
        #
        # El corte termico dejo de ser latching el 2026-09-11 (decision del
        # responsable del producto). El episodio no se pierde por eso: queda en
        # el registro persistido de alarmas (6.12.2). La unica alarma que exige
        # intervencion humana es el fallo de corriente del calefactor, porque
        # revisar su cableado pide el equipo apagado.
        mb.post("/debug/sensor", ch="air_temp", clear=1)
        wait_for(
            lambda: not mb.state()["heater"]["must_cut"],
            timeout_s=20,
            what="que el corte de calefactor se libere al bajar la temperatura",
        )
        wait_for(
            lambda: not alarm_signalling(mb.state(), ALARM_AIR_THERMAL_CUTOUT),
            timeout_s=20,
            what="que el corte termico se retire SOLO al enfriarse",
        )
    finally:
        mb.debug(False)


def t_frontera_del_corte_termico(mb, hmi):
    """El corte del aire esta en 40 C, no en 38.

    La prueba de arriba sube el aire a 45 C, que dispara con 38 y con 40: no
    distingue donde esta el umbral. Esta si, y por eso existe — el umbral se
    movio el 2026-09-14 (consigna a 39, corte a 40) y sin una prueba de
    frontera cualquiera puede devolverlo a 38 sin que nada se queje.

    39.0 C es el punto que discrimina: por encima del tope de consigna, por
    debajo del corte. Con el umbral viejo aqui YA habria saltado.

    OJO SI ESTA FALLA EN 39.0: lo mas probable no es que el firmware este mal,
    sino que ESTA unidad lleve un air_tmax menor guardado en NVS, que manda
    sobre el compilado. La placa lo avisa al arrancar ("corte termico de aire
    a X C, por debajo del tope de consigna"). Se arregla con
    POST /config air_tmax=40, no tocando esta prueba.
    """
    mb.debug(True)
    try:
        mb.post("/debug/sensor", ch="air_temp", value=39.0)
        time.sleep(3)
        st = mb.state()
        if alarm_signalling(st, ALARM_AIR_THERMAL_CUTOUT):
            raise BenchError(
                "el corte ha saltado con el aire a 39.0 C: el umbral sigue por "
                "debajo de 40 (mira air_tmax en NVS, no solo el compilado)"
            )
        if st["heater"]["must_cut"]:
            raise BenchError("calefactor cortado a 39.0 C sin corte termico declarado")

        # Y por encima del umbral si salta.
        mb.post("/debug/sensor", ch="air_temp", value=40.5)
        st = wait_for(
            lambda: (lambda s: s if alarm_signalling(s, ALARM_AIR_THERMAL_CUTOUT) else None)(mb.state()),
            timeout_s=30,
            what="ALARM_AIR_THERMAL_CUTOUT con el aire a 40.5 C",
        )
        if not st["heater"]["must_cut"]:
            raise BenchError("corte declarado pero heater_must_cut es false")

        # El limite que la placa publica tiene que ser el que se esta usando:
        # es lo que el display le ensena al operador.
        for a in st["alarms"]["list"]:
            if a["id"] == ALARM_AIR_THERMAL_CUTOUT and "limit" in a:
                if abs(float(a["limit"]) - 40.0) > 0.05:
                    raise BenchError(
                        f"la placa publica limite {a['limit']} y deberia ser 40.0"
                    )
                break
    finally:
        mb.post("/debug/sensor", ch="air_temp", clear=1)
        mb.debug(False)


def t_fallo_calefactor_se_enclava(mb, hmi):
    """El fallo de corriente del calefactor SI se enclava, y solo lo quita el reset.

    Es la unica alarma latching que queda. En la placa la declara el autotest de
    arranque cuando la corriente del calefactor se sale de rango —la firma de un
    calefactor mal cableado— y su condicion no se retira nunca mientras el equipo
    sigue encendido, asi que la instruccion correcta al operador es apagar,
    revisar y volver a encender.

    Aqui se fuerza y se libera la condicion a mano para poder observar el
    enclavamiento, cosa que en la placa real no ocurre.
    """
    mb.debug(True)
    try:
        mb.post("/debug/alarm", id=ALARM_HEATER_FAULT, present=1)
        wait_for(
            lambda: alarm_signalling(mb.state(), ALARM_HEATER_FAULT),
            timeout_s=40,
            what="que el fallo de calefactor llegue a senalizar",
        )

        # Se retira la CONDICION: una alarma normal se iria; esta no.
        mb.post("/debug/alarm", id=ALARM_HEATER_FAULT, present=0)
        time.sleep(3)
        st = mb.state()
        if not alarm_signalling(st, ALARM_HEATER_FAULT):
            raise BenchError(
                "el fallo de calefactor se ha ido solo: deberia enclavarse"
            )
        if not any(a["id"] == ALARM_HEATER_FAULT and a["latched"]
                   for a in st["alarms"]["list"]):
            raise BenchError("la placa no lo reporta como enclavado")

        # Y el reset manual SI lo quita, ahora que su condicion ya no esta.
        if not clear_latched(mb):
            raise BenchError("HMI,ALM_RESET no ha retirado el fallo de calefactor")
    finally:
        mb.post("/debug/alarm", id=ALARM_HEATER_FAULT, present=0)
        mb.debug(False)


def t_alarma_de_ventilador_se_retira(mb, hmi):
    """EL FALLO DE BANCO DEL 2026-09-11.

    Simula el ventilador parado hasta que salta ALARM_FAN_FAILURE, y despues lo
    devuelve a regimen. Antes del arreglo la alarma NO se retiraba nunca:
    declararla cortaba la alimentacion del ventilador, con lo que las rpm eran 0
    por construccion. Ver ongoingFanCriticalAlarm() en security.cpp.
    """
    mb.debug(True)
    actuation_was = mb.state()["ctl"]["actuation"]
    try:
        if not mb.state()["fan"]["feedback"]:
            raise SkipTest("esta unidad no tiene tacometro en el ventilador")

        # El ventilador solo gira con la actuacion encendida, asi que se
        # enciende con la MISMA trama que mandaria el display. Se restaura en
        # el finally: la prueba no puede dejar el equipo actuando.
        if not mb.state()["fan"]["commanded"]:
            mb.post("/debug/inject",
                    line="HMI,1,0,1,36.50,37.00,50,0,0,1,0")
            wait_for(
                lambda: mb.state()["fan"]["commanded"],
                timeout_s=10,
                what="que la actuacion inyectada ordene el ventilador",
            )

        mb.post("/debug/sensor", ch="fan_rpm", value=0.0)
        wait_for(
            lambda: alarm_signalling(mb.state(), ALARM_FAN_FAILURE),
            timeout_s=40,
            what="ALARM_FAN_FAILURE con el ventilador simulado parado",
        )

        # Ahora vuelve a girar por encima del umbral + histeresis (3000+300).
        mb.post("/debug/sensor", ch="fan_rpm", value=4000.0)
        wait_for(
            lambda: not alarm_signalling(mb.state(), ALARM_FAN_FAILURE),
            timeout_s=40,
            what="que ALARM_FAN_FAILURE se RETIRE al recuperarse el ventilador",
        )
    finally:
        if actuation_was == 0:
            # Devolver el equipo a reposo importa mas que el resultado: una
            # bateria no puede dejar una incubadora calentando.
            try:
                mb.post("/debug/inject",
                        line="HMI,0,0,1,36.50,37.00,50,0,0,1,0")
            except Exception:  # noqa: BLE001
                pass
        mb.debug(False)


def t_alarma_forzada_respeta_la_maquina(mb, hmi):
    """Forzar una condicion no salta el retardo de anuncio ni el resto de la
    maquina: pasa por PENDING antes de senalizar, como una real."""
    mb.debug(True)
    try:
        mb.post("/debug/alarm", id=ALARM_AIR_OUTLET_BLOCKED, present=1)
        wait_for(
            lambda: alarm_state(mb.state(), ALARM_AIR_OUTLET_BLOCKED) != ST_INACTIVE,
            timeout_s=10,
            what="que la alarma forzada deje de estar en reposo",
        )
        wait_for(
            lambda: alarm_signalling(mb.state(), ALARM_AIR_OUTLET_BLOCKED),
            timeout_s=40,
            what="que la alarma forzada llegue a senalizar",
        )
        mb.post("/debug/alarm", id=ALARM_AIR_OUTLET_BLOCKED, present=0)
        wait_for(
            lambda: not alarm_signalling(mb.state(), ALARM_AIR_OUTLET_BLOCKED),
            timeout_s=40,
            what="que la alarma forzada se retire al liberarla",
        )
    finally:
        mb.debug(False)


def t_alarma_llega_al_display(mb, hmi):
    """Una alarma de la placa aparece en el display por el cable.

    Prueba el enlace entero: securityCheck -> CTRL,ALM -> parseo del HMI ->
    alarmList. Es la unica prueba que necesita las DOS placas.
    """
    if hmi is None:
        raise SkipTest("no se ha dado la IP del display")
    mb.debug(True)
    try:
        mb.post("/debug/alarm", id=ALARM_AIR_OUTLET_BLOCKED, present=1)
        wait_for(
            lambda: alarm_signalling(mb.state(), ALARM_AIR_OUTLET_BLOCKED),
            timeout_s=40,
            what="que la placa senalice la alarma",
        )
        wait_for(
            lambda: any(a["id"] == ALARM_AIR_OUTLET_BLOCKED
                        for a in hmi.state()["alarms"]),
            timeout_s=15,
            what="que la alarma aparezca en el display",
        )
        # El bitmask que el display dice tener tiene que ACABAR coincidiendo con
        # el de la placa: dos fuentes de verdad desincronizadas es un fallo que
        # este proyecto ya ha tenido.
        #
        # Se espera a que converja en vez de compararlo una sola vez. Los dos
        # datos viajan en tramas DISTINTAS —la lista en CTRL,ALM, el bitmask en
        # CTRL,STATE, que sale a 1 Hz— asi que discrepar durante menos de un
        # segundo es el comportamiento normal, no un fallo. Lo que seria un
        # fallo es que no converjan.
        def masks_match():
            return mb.state()["alarms"]["mask"] == hmi.state()["state"]["alarm_mask"]

        try:
            wait_for(masks_match, timeout_s=10,
                     what="que el bitmask del display alcance al de la placa")
        except BenchError:
            mb_mask = mb.state()["alarms"]["mask"]
            hmi_mask = hmi.state()["state"]["alarm_mask"]
            raise BenchError(
                f"el bitmask de alarmas no converge: placa=0x{mb_mask:x} "
                f"display=0x{hmi_mask:x}"
            ) from None
    finally:
        mb.post("/debug/alarm", id=ALARM_AIR_OUTLET_BLOCKED, present=0)
        mb.debug(False)


def t_telemetria_inyectada_en_el_display(mb, hmi):
    """Una linea CTRL,TEL inyectada llega a ctrl_tel_msg tras pasar por el
    parseador de verdad."""
    if hmi is None:
        raise SkipTest("no se ha dado la IP del display")
    hmi.debug(True)
    try:
        # Se reinyecta en cada intento, no una sola vez.
        #
        # La placa manda su CTRL,TEL de verdad a 1 Hz y pisa lo inyectado, asi
        # que una sola inyeccion abre una ventana de menos de un segundo y la
        # prueba se convierte en una carrera contra el muestreo por HTTP: fallaba
        # un intento de cada dos sin que hubiera nada roto. Refrescandolo en cada
        # pasada la ventana deja de importar, y lo que se ejercita sigue siendo
        # exactamente el mismo camino (cola -> Comm_Task -> parse_message).
        def inject_and_read():
            hmi.post("/debug/inject", line="CTRL,TEL,31.25,32.50,44.00,1,353")
            st = hmi.state()
            return st if abs(st["tel"]["air"] - 31.25) < 0.02 else None

        st = wait_for(
            inject_and_read,
            timeout_s=15,
            period_s=0.1,
            what="que el display parsee la telemetria inyectada",
        )
        if abs(st["tel"]["skin"] - 32.50) > 0.02:
            raise BenchError(f"skin={st['tel']['skin']} != 32.50")
        if abs(st["tel"]["hum"] - 44.00) > 0.02:
            raise BenchError(f"hum={st['tel']['hum']} != 44.00")
    finally:
        hmi.debug(False)


def t_linea_con_prefijo_malo_se_rechaza(mb, hmi):
    """El display no acepta inyectar algo que el camino real no habria mirado."""
    if hmi is None:
        raise SkipTest("no se ha dado la IP del display")
    hmi.debug(True)
    try:
        hmi.post("/debug/inject", expect=409, line="BASURA,1,2,3")
    finally:
        hmi.debug(False)


def t_link_lost_del_display(mb, hmi):
    """Enlace simulado como perdido: el display lo declara y lo retira.

    Cubre el camino del aviso (banner + audio) sin tocar el cable. El fallo que
    motivo esta prueba: el banner solo se des-ocultaba si CAMBIABA el texto, asi
    que la SEGUNDA vez que aparecia la misma condicion sonaba sin verse.
    Por eso se hace dos veces.
    """
    if hmi is None:
        raise SkipTest("no se ha dado la IP del display")
    hmi.debug(True)
    try:
        for vuelta in (1, 2):
            hmi.post("/debug/link", mute=1)
            wait_for(
                lambda: hmi.state()["link"]["lost"],
                timeout_s=10,
                what=f"LINK LOST simulado (vuelta {vuelta})",
            )
            hmi.post("/debug/link", mute=0)
            wait_for(
                lambda: not hmi.state()["link"]["lost"],
                timeout_s=15,
                what=f"que el enlace vuelva (vuelta {vuelta})",
            )
    finally:
        hmi.post("/debug/link", mute=0)
        hmi.debug(False)


def t_memoria_estable(mb, hmi):
    """La DRAM INTERNA no se desangra durante la tanda.

    Mide la interna, no el total. La primera version miraba el total y por eso
    NO VIO la fuga que ella misma provocaba: el volcado de /debug/state pedia
    4 KB con malloc() —que sirve de la interna primero— y los copiaba a un
    String, 8 KB de interna por peticion. En el display eso bajo la interna de
    23,8 KB a 5,7 KB y salieron `wifi:mem fail`, glitches en el panel y un HMI
    LINK LOST fantasma... mientras el total, con 7 MB de PSRAM detras, no se
    movia. El numero que manda es la interna, y dentro de ella el mayor bloque
    contiguo: es el que tiene que dar para los bounce buffers del panel y para
    los descriptores DMA de WiFi.
    """
    LEAK_FLOOR = 4 * 1024
    REQUESTS = 20
    for b in (mb, hmi):
        if b is None:
            continue
        before = b.state()["heap"]["int_free"]
        for _ in range(REQUESTS):
            b.state()
        after = b.state()["heap"]
        lost = before - after["int_free"]
        if lost > LEAK_FLOOR:
            largest = after["int_largest"]
            detalle = (f"; mayor bloque contiguo {largest} B" if largest >= 0
                       else "; mayor bloque no medido en esta placa")
            raise BenchError(
                f"{b.name}: la DRAM interna bajo {lost} B en {REQUESTS} "
                f"peticiones ({before} -> {after['int_free']})" + detalle
            )


def t_reserva_de_dram_interna(mb, hmi):
    """Queda DRAM interna suficiente para WiFi y para el panel.

    No es una fuga sino un suelo absoluto. El display es el caso justo: vive con
    ~23 KB de interna libre, WiFi necesita memoria DMA-capaz para sus frames de
    gestion y el panel RGB necesita bloques contiguos. Cuando la interna se
    hundio, el sintoma NO fue un fallo de memoria legible sino
    `wifi:m f assoc req` en el log y glitches en la pantalla.
    """
    FLOOR_FREE = 6 * 1024
    FLOOR_LARGEST = 4 * 1024
    problems = []
    for b in (mb, hmi):
        if b is None:
            continue
        h = b.state()["heap"]
        if h["int_free"] < FLOOR_FREE:
            problems.append(f"{b.name}: solo {h['int_free']} B de interna libre")
        # int_largest negativo = la placa no lo mide. El display lo dejo de
        # medir porque heap_caps_get_largest_free_block() recorre el pool con
        # las interrupciones deshabilitadas y disparaba el interrupt watchdog
        # (ver el comentario en su debug_mode.cpp). No es un cero: es "no hay
        # dato", y tratarlo como cero daria un fallo falso en cada pasada.
        if h["int_largest"] >= 0 and h["int_largest"] < FLOOR_LARGEST:
            problems.append(
                f"{b.name}: mayor bloque interno contiguo {h['int_largest']} B"
            )
    if problems:
        raise BenchError("; ".join(problems))


class SkipTest(Exception):
    pass


TESTS = [
    ("alcanzable", t_estado_alcanzable),
    ("pila", t_margen_de_pila),
    ("modo-apagado-al-arrancar", t_modo_debug_arranca_apagado),
    ("simular-requiere-modo", t_simular_requiere_modo),
    ("apagar-retira-todo", t_apagar_retira_las_simulaciones),
    ("medida-simulada", t_medida_simulada_llega_al_control),
    ("corte-termico", t_corte_termico_del_aire),
    ("frontera-corte-termico", t_frontera_del_corte_termico),
    ("ventilador-se-retira", t_alarma_de_ventilador_se_retira),
    ("calefactor-enclava", t_fallo_calefactor_se_enclava),
    ("alarma-forzada", t_alarma_forzada_respeta_la_maquina),
    ("alarma-al-display", t_alarma_llega_al_display),
    ("telemetria-inyectada", t_telemetria_inyectada_en_el_display),
    ("prefijo-malo", t_linea_con_prefijo_malo_se_rechaza),
    ("link-lost", t_link_lost_del_display),
    ("memoria", t_memoria_estable),
    ("dram-interna", t_reserva_de_dram_interna),
]


def run_crash_test(board, kind):
    """Prueba de coredump. REINICIA LA PLACA, por eso va aparte."""
    print(f"\n=== coredump: provocando '{kind}' en {board.name} ===")
    before = board.state()["uptime_ms"]
    board.debug(True)
    board.post("/debug/crash", kind=kind, delay_ms=500)
    print("    peticion aceptada; esperando a que reinicie...")
    deadline = time.time() + 60
    while time.time() < deadline:
        time.sleep(2)
        try:
            now = board.state()["uptime_ms"]
        except Exception:
            continue
        if now < before:
            print(f"    la placa ha reiniciado (uptime {before} -> {now} ms)")
            print("    ahora leer el coredump:")
            print("      idf.py -p <PUERTO> coredump-info")
            return True
    print("    NO se ha visto el reinicio dentro del plazo")
    return False


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--mb", help="IP o host de la motherBoard")
    ap.add_argument("--hmi", help="IP o host del display")
    ap.add_argument("--user", default=DEFAULT_USER)
    ap.add_argument("--password", default=DEFAULT_PASS)
    ap.add_argument("--only", action="append", default=[],
                    help="ejecuta solo estas pruebas (repetible)")
    ap.add_argument("--list", action="store_true", help="lista las pruebas y sale")
    ap.add_argument("--crash", metavar="KIND",
                    help="prueba de coredump: abort|null|stack|wdt|assert. "
                         "REINICIA la placa; no se ejecuta con la tanda normal")
    ap.add_argument("--crash-board", default="mb", choices=("mb", "hmi"))
    args = ap.parse_args()

    if args.list:
        for name, fn in TESTS:
            head = (fn.__doc__ or "").strip().splitlines()[0]
            print(f"  {name:28s} {head}")
        return 0

    if not args.mb:
        ap.error("hace falta --mb")

    mb = Board("MB", args.mb, args.user, args.password)
    hmi = Board("HMI", args.hmi, args.user, args.password) if args.hmi else None

    if args.crash:
        board = mb if args.crash_board == "mb" else hmi
        if board is None:
            ap.error("--crash-board hmi necesita --hmi")
        return 0 if run_crash_test(board, args.crash) else 1

    selected = [(n, f) for n, f in TESTS if not args.only or n in args.only]
    unknown = set(args.only) - {n for n, _ in TESTS}
    if unknown:
        ap.error(f"pruebas desconocidas: {', '.join(sorted(unknown))}")

    print(f"MB  = {args.mb}")
    print(f"HMI = {args.hmi or '(no dado: se saltan las pruebas del display)'}")
    print()

    passed = failed = skipped = 0
    failures = []
    for name, fn in selected:
        head = (fn.__doc__ or "").strip().splitlines()[0]
        print(f"[ .. ] {name:28s} {head}")
        t0 = time.time()
        try:
            fn(mb, hmi)
        except SkipTest as exc:
            skipped += 1
            print(f"[SKIP] {name:28s} {exc}")
        except Exception as exc:  # noqa: BLE001 - el informe importa mas
            failed += 1
            failures.append((name, exc))
            print(f"[FALLA] {name:28s} {exc}")
        else:
            passed += 1
            print(f"[ OK ] {name:28s} ({time.time() - t0:.1f} s)")

    # Red de seguridad: pase lo que pase, las dos placas se quedan sin simular.
    for b in (mb, hmi):
        if b is None:
            continue
        try:
            b.debug(False)
        except Exception as exc:  # noqa: BLE001
            print(f"AVISO: no se ha podido apagar el modo depuracion en "
                  f"{b.name}: {exc}")

    print()
    print(f"{passed} OK, {failed} fallan, {skipped} saltadas")
    if failures:
        print("\nFallos:")
        for name, exc in failures:
            print(f"  - {name}: {exc}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
