#pragma once
// =============================================================================
//  TABLA DE POLÍTICA DE TRANSPORTE  —  GPRS vs WiFi
// =============================================================================
//  Punto único donde se define QUÉ se manda, POR DÓNDE y CADA CUÁNTO.
//  Para cambiar el comportamiento se toca AQUÍ, no en los .cpp.
//
//  Por qué existe: el montaje de telemetría está escrito dos veces a mano
//  (addConfigTelemetriesToGPRSJSON en GPRS.cpp y su gemela en Wifi_OTA.cpp).
//  Las dos copias habían divergido en 18 claves sin que nadie lo decidiera.
//  Los flags de grupo de abajo convierten esa divergencia en algo explícito
//  y reversible con un 0/1.
//
//  Comprobación de deriva:  python tools/check_transport_matrix.py
// =============================================================================

// -----------------------------------------------------------------------------
// 1. PERIODOS DE TRANSMISIÓN
// -----------------------------------------------------------------------------
// WiFi: cadencia fija, la red es barata.
#define TX_WIFI_PUBLISH_MS 5000UL         // telemetría completa
#define TX_WIFI_OTA_CHECK_MS 60000UL      // 1 min
#define TX_WIFI_RECONNECT_MS 30000UL      // 30 s

// GPRS: cadencia variable según lo que esté haciendo la incubadora, porque
// cada publish son datos de pago y una ráfaga AT bloqueante.
// Son valores POR DEFECTO: se sobrescriben en NVS vía /config.
#define TX_GPRS_PERIOD_ACTUATING_S 60     // controlando temperatura/humedad
#define TX_GPRS_PERIOD_PHOTOTHERAPY_S 180 // solo fototerapia
#define TX_GPRS_PERIOD_STANDBY_S 3600     // en reposo: 1 h
#define TX_GPRS_OTA_CHECK_MS 600000UL     // 10 min
#define TX_GPRS_RECONNECT_MS 10000UL      // 10 s

// Común a ambos transportes.
#define TX_THINGSBOARD_RECONNECT_MS 30000UL

// Zona horaria: una vez resuelta (por NITZ o por IP) se refresca cada 24 h en
// vez de darse por buena para siempre, para que un cambio de horario de
// verano/invierno se corrija solo en un equipo que lleve semanas sin
// reiniciar. Mientras no haya ninguna zona resuelta, cada ruta usa su propio
// intervalo corto de reintento (GPRS_TIME_SYNC_RETRY_INTERVAL / el de IP).
#define TX_TIMEZONE_REFRESH_MS 86400000UL // 24 h

// -----------------------------------------------------------------------------
// 2. GRUPOS DE TELEMETRÍA        1 = se publica por ese transporte, 0 = no
// -----------------------------------------------------------------------------
// ANTES DE PONER UN 0 A 1, MIRA EL PRESUPUESTO:
// GPRS_JSON y WIFI_JSON son StaticJsonDocument<JSON_OBJECT_SIZE(
// THINGSBOARD_FIELDS_AMOUNT)>, con THINGSBOARD_FIELDS_AMOUNT = 64 (main.h).
// Al llenarse, ArduinoJson descarta campos EN SILENCIO: no hay ninguna llamada
// a overflowed() en el código. Encender un grupo añade claves a una publicación
// que ya puede rondar el límite.  python tools/check_transport_matrix.py
// imprime el máximo posible de cada transporte.
// CORE: constantes vitales, alarmas, actuadores, datos de bebé. Lo clínico.
//       Nunca debería desactivarse; está aquí para que la tabla esté completa.
#define TX_GROUP_CORE_GPRS 1
#define TX_GROUP_CORE_WIFI 1

// CELLULAR: IMEI, APN, operador, CSQ. Solo tienen sentido físico por GPRS;
//           por WiFi serían campos vacíos o rancios. Divergencia CORRECTA.
#define TX_GROUP_CELLULAR_GPRS 1
#define TX_GROUP_CELLULAR_WIFI 0

// DIAG: boot count, heap libre, uptime, resets del HMI, kills del módem.
//       No tienen nada de específico del transporte. Está a 0 en WiFi porque
//       es lo que hace el firmware hoy, no porque se decidiera: ponlo a 1 si
//       quieres diagnóstico también en los equipos conectados por WiFi.
#define TX_GROUP_DIAG_GPRS 1
#define TX_GROUP_DIAG_WIFI 0

// CALIBRATION: referencias y ajuste fino de los sensores de temperatura.
//              Mismo caso que DIAG: divergencia heredada, no decidida.
#define TX_GROUP_CALIBRATION_GPRS 1
#define TX_GROUP_CALIBRATION_WIFI 0

// SENSORBOARD: hasta 11 claves sb_* de la placa auxiliar por USB (enlace,
//              3 temperaturas, 3 humedades, luz, sonido, puerta).
//
// Apagado en GPRS a proposito, y no por el coste de datos: el presupuesto ya
// esta al limite POR ARRIBA en ese transporte. check_transport_matrix.py
// avisa de que el maximo posible (87 claves con CELLULAR+DIAG+CALIBRATION
// encendidos) supera THINGSBOARD_FIELDS_AMOUNT = 64, y ArduinoJson descarta
// campos EN SILENCIO al llenarse; ademas MAX_MESSAGE_SIZE son 1024 B de
// buffer MQTT. Anadir 11 claves auxiliares a esa publicacion puede tirar
// campos CLINICOS sin avisar, que es peor que no publicar el SensorBoard.
//
// Por WiFi los grupos pesados (CELLULAR, DIAG, CALIBRATION) estan apagados,
// asi que ahi caben. Para subir el GPRS a 1: medir primero con
// tools/check_transport_matrix.py y comprobar en banco el tamano real de una
// publicacion, o subir THINGSBOARD_FIELDS_AMOUNT y MAX_MESSAGE_SIZE.
//
// OJO con el conteo del script: las claves sb_* no se escriben literalmente
// en GPRS.cpp/Wifi_OTA.cpp sino dentro de sensorboard_add_telemetry(), asi
// que el script cuenta 0 en este grupo. Suma 11 a mano al leer su maximo.
#define TX_GROUP_SENSORBOARD_GPRS 0
#define TX_GROUP_SENSORBOARD_WIFI 1

// -----------------------------------------------------------------------------
// 3. FUNCIONALIDADES POR TRANSPORTE
// -----------------------------------------------------------------------------
// Documentan lo que hay implementado. Cambiar un 0 por un 1 aquí NO crea la
// funcionalidad: marca lo que falta por portar al otro transporte.
#define TX_FEATURE_OTA_GPRS 1             // GPRSCheckOTA()
#define TX_FEATURE_OTA_WIFI 1             // WIFICheckOTA()
#define TX_FEATURE_PROVISIONING_GPRS 1
#define TX_FEATURE_PROVISIONING_WIFI 1
#define TX_FEATURE_BABY_CLOUD_GPRS 1      // cola de eventos de paciente
#define TX_FEATURE_BABY_CLOUD_WIFI 1
#define TX_FEATURE_TIME_SYNC_GPRS 1       // NITZ / AT+CNTP
#define TX_FEATURE_TIME_SYNC_WIFI 1       // SNTP
#define TX_FEATURE_TRIANGULATION_GPRS 1   // posición por torre; no existe en WiFi
#define TX_FEATURE_TRIANGULATION_WIFI 0
// Snapshot PPG. Un snapshot son 400 muestras ≈ 23 KB de JSON.
// El RPC capturePPG (captura bajo demanda) está en los dos transportes.
#define TX_FEATURE_PPG_SNAPSHOT_GPRS 1
#define TX_FEATURE_PPG_SNAPSHOT_WIFI 1
// La captura AUTOMÁTICA cada 15 min es otra cosa: por GPRS son ~23 KB × 4/h
// ≈ 2,2 MB/día de datos de pago, y cada envío bloquea el módem varios
// segundos. Por eso va aparte y está a 0: por GPRS se captura solo si alguien
// lo pide con el RPC. Súbelo a 1 solo con una tarifa que lo aguante.
#define TX_FEATURE_PPG_AUTOCAPTURE_GPRS 0
#define TX_FEATURE_PPG_AUTOCAPTURE_WIFI 1

// -----------------------------------------------------------------------------
// 4. RPC DISPONIBLES POR TRANSPORTE
// -----------------------------------------------------------------------------
// Las listas rpc_callbacks[] (GPRS.cpp) y wifi_rpc_callbacks[] (Wifi_OTA.cpp)
// son independientes: un RPC solo responde por el transporte donde está
// registrado. Esta tabla refleja el registro real.
//   RPC          GPRS  WiFi
//   restart       sí    NO
//   getDiag       sí    NO
//   wipeBabies    sí    NO
//   setWifi       sí    sí
//   checkOta      sí    sí
//   capturePPG    sí    sí   (GPRS bajo TX_FEATURE_PPG_SNAPSHOT_GPRS)
// Un RPC probado en banco por WiFi puede no responder en campo por GPRS.
