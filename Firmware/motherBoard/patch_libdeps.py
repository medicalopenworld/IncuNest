"""Parches a dependencias vendorizadas, aplicados antes de compilar.

PlatformIO regenera .pio/libdeps/ en cada instalacion limpia (clone nuevo,
worktree nuevo, CI), asi que un arreglo hecho a mano ahi desaparece sin avisar.
Esto lo vuelve a poner en cada build y es idempotente.

--------------------------------------------------------------------------
PARCHE 1 - OTA_Handler::Handle_Request_Timeout() desreferencia un puntero nulo
--------------------------------------------------------------------------
El temporizador que vigila si un trozo de firmware tarda demasiado NO se
cancela al terminar la OTA. Cuando la descarga acaba, el SDK limpia
m_fw_callback y ese temporizador --todavia armado-- salta y hace:

    uint64_t const & timeout = m_fw_callback->Get_Timeout();   // OTA_Handler.h:321

sobre un puntero ya nulo. Resultado: LoadProhibited con EXCVADDR 0x38 (el
desplazamiento del campo dentro del objeto) desde la tarea de esp_timer.

Visto en produccion el 2026-09-21: las siete unidades de la tanda (352, 354-359)
reiniciaron con PANIC justo al completar la OTA a 18.30. Reproducido en banco y
decodificado con el .elf archivado:

    OTA_Handler::Handle_Request_Timeout()      OTA_Handler.h:321
      <- std::function<void()>::operator()()
      <- Callback_Watchdog::oneshot_timer_callback()   Callback_Watchdog.cpp:93
      <- timer_process_alarm (esp_timer)

No se puede arreglar subiendo de version: platformio.ini documenta que el SDK
v0.15.0 y master no compilan (llaman a IMQTT_Client::get_buffer_size(), que ya
no existe). Asi que se parchea la v0.13.0, que es la que compila.

La guarda es la minima posible: si no hay callback, no hay nada que reintentar.

--------------------------------------------------------------------------
PARCHES 2 a 4 - la guarda anterior NO bastaba: el puntero no siempre es nulo
--------------------------------------------------------------------------
El 2026-09-22 la unidad de banco volvio a reiniciar con PANIC al terminar una
OTA, con el parche 1 YA compilado dentro (header parcheado a las 23:10:04,
objetos de ThingsBoard compilados a las 23:10:51, placa arrancada a las
23:12:50). O sea: el fallo tiene un segundo camino.

La raiz comun es que el temporizador de trozo sobrevive al final de la OTA.
Lo que cambia es COMO esta el callback cuando ese temporizador salta:

  a) Ruta de desconexion. ThingsBoard.h:453 llama a Stop_Firmware_Update(),
     que si pone m_fw_callback = nullptr. Aqui el parche 1 salva la papeleta.
     Es lo que se vio en produccion con la 18.30.

  b) Ruta de exito. Finish_Firmware_Update() -> m_finish_callback() ->
     Firmware_OTA_Unsubscribe(), que hace:

         m_fw_callback = OTA_Update_Callback();

     pero eso reconstruye el objeto de ThingsBoardSized, NO el puntero de
     OTA_Handler, que sigue apuntando ahi y por tanto NO es nulo. Y como
     OTA_Update_Callback() y Callback() son ambos '= default' sin ningun
     inicializador de miembro, lo que queda apuntado tiene TODOS los campos
     indeterminados. Al saltar el temporizador se acaba en:

         if (!m_callback) { Logger::println(m_message); }   // Callback.h

     imprimiendo desde un char* basura -> LoadProhibited.

Arreglar solo (b) con otra guarda seria ir otra vez detras del sintoma. Lo
que corresponde es apagar el temporizador y anular el puntero en TODAS las
salidas del proceso, que son tres: el final feliz y las dos terminaciones por
fallo. Asi el parche 1 pasa a ser la red de seguridad para el caso en que el
aviso ya estuviera encolado en la tarea de esp_timer cuando se hizo detach()
--esa carrera no la cierra detach(), porque esp_timer_stop() no desencola un
aviso ya vencido.
"""
Import("env")

import os

PATCHES = [
    {
        "nombre": "OTA_Handler: guarda de puntero nulo en Handle_Request_Timeout",
        "fichero": os.path.join("$PROJECT_LIBDEPS_DIR", "$PIOENV", "ThingsBoard",
                                "src", "OTA_Handler.h"),
        "buscar": "    void Handle_Request_Timeout()  {\n"
                  "        uint64_t const & timeout = m_fw_callback->Get_Timeout();",
        "poner": "    void Handle_Request_Timeout()  {\n"
                 "        // PARCHE IncuNest (ver patch_libdeps.py): el temporizador de\n"
                 "        // trozo puede saltar DESPUES de que la OTA haya terminado y\n"
                 "        // limpiado el callback. Sin esta guarda es un LoadProhibited.\n"
                 "        if (m_fw_callback == nullptr) { return; }\n"
                 "        uint64_t const & timeout = m_fw_callback->Get_Timeout();",
        "marca": "PARCHE IncuNest",
    },
    {
        "nombre": "OTA_Handler: apagar el temporizador al terminar bien la OTA",
        "fichero": os.path.join("$PROJECT_LIBDEPS_DIR", "$PIOENV", "ThingsBoard",
                                "src", "OTA_Handler.h"),
        "buscar": "        (void)m_send_fw_state_callback(FW_STATE_UPDATING, nullptr);\n"
                  "        m_fw_callback->Call_Callback<Logger>(true);\n"
                  "        (void)m_finish_callback();\n"
                  "    }",
        "poner": "        (void)m_send_fw_state_callback(FW_STATE_UPDATING, nullptr);\n"
                 "        m_fw_callback->Call_Callback<Logger>(true);\n"
                 "        (void)m_finish_callback();\n"
                 "        // PARCHE IncuNest 2 (ver patch_libdeps.py): m_finish_callback()\n"
                 "        // reconstruye el callback de ThingsBoardSized, pero NO toca este\n"
                 "        // puntero, que se queda apuntando a un objeto con todos los campos\n"
                 "        // indeterminados. Hay que apagar el temporizador y anularlo aqui.\n"
                 "        m_watchdog.detach();\n"
                 "        m_fw_callback = nullptr;\n"
                 "    }",
        "marca": "PARCHE IncuNest 2",
    },
    {
        "nombre": "OTA_Handler: apagar el temporizador al agotar los reintentos",
        "fichero": os.path.join("$PROJECT_LIBDEPS_DIR", "$PIOENV", "ThingsBoard",
                                "src", "OTA_Handler.h"),
        "buscar": "        if (m_retries <= 0) {\n"
                  "            (void)m_send_fw_state_callback(FW_STATE_FAILED, error_message);\n"
                  "            m_fw_callback->Call_Callback<Logger>(false);\n"
                  "            (void)m_finish_callback();\n"
                  "            return;\n"
                  "        }",
        "poner": "        if (m_retries <= 0) {\n"
                 "            (void)m_send_fw_state_callback(FW_STATE_FAILED, error_message);\n"
                 "            m_fw_callback->Call_Callback<Logger>(false);\n"
                 "            (void)m_finish_callback();\n"
                 "            // PARCHE IncuNest 3 (ver patch_libdeps.py): misma razon que el 2.\n"
                 "            m_watchdog.detach();\n"
                 "            m_fw_callback = nullptr;\n"
                 "            return;\n"
                 "        }",
        "marca": "PARCHE IncuNest 3",
    },
    {
        "nombre": "OTA_Handler: apagar el temporizador al abortar sin reintento",
        "fichero": os.path.join("$PROJECT_LIBDEPS_DIR", "$PIOENV", "ThingsBoard",
                                "src", "OTA_Handler.h"),
        "buscar": "            case OTA_Failure_Response::RETRY_NOTHING:\n"
                  "                (void)m_send_fw_state_callback(FW_STATE_FAILED, error_message);\n"
                  "                m_fw_callback->Call_Callback<Logger>(false);\n"
                  "                (void)m_finish_callback();\n"
                  "                break;",
        "poner": "            case OTA_Failure_Response::RETRY_NOTHING:\n"
                 "                (void)m_send_fw_state_callback(FW_STATE_FAILED, error_message);\n"
                 "                m_fw_callback->Call_Callback<Logger>(false);\n"
                 "                (void)m_finish_callback();\n"
                 "                // PARCHE IncuNest 4 (ver patch_libdeps.py): misma razon que el 2.\n"
                 "                m_watchdog.detach();\n"
                 "                m_fw_callback = nullptr;\n"
                 "                break;",
        "marca": "PARCHE IncuNest 4",
    },
]


def aplicar(source, target, env):
    for p in PATCHES:
        ruta = env.subst(p["fichero"])
        if not os.path.isfile(ruta):
            print(f"[patch_libdeps] NO ENCONTRADO, no se parchea: {ruta}")
            continue
        with open(ruta, "r", encoding="utf-8", errors="ignore") as f:
            texto = f.read()
        if p["marca"] in texto:
            continue  # ya parcheado
        if p["buscar"] not in texto:
            # Que no falle el build en silencio: si el upstream cambia, hay que
            # enterarse, porque el fallo que esto evita es un panic en campo.
            print(f"[patch_libdeps] *** AVISO: no casa el patron de '{p['nombre']}'.")
            print(f"[patch_libdeps] *** Revisa {ruta}: el parche NO se ha aplicado.")
            continue
        with open(ruta, "w", encoding="utf-8") as f:
            f.write(texto.replace(p["buscar"], p["poner"], 1))
        print(f"[patch_libdeps] aplicado: {p['nombre']}")


aplicar(None, None, env)
