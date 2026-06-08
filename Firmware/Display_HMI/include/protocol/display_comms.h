#ifndef DISPLAY_COMMS_H
#define DISPLAY_COMMS_H

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>
#include <Stream.h>

/* ======= Config ======= */
#define DC_PROTO_VERSION        1
#define DC_MAX_FRAME_SIZE       1024
#define DC_MAX_PAYLOAD_SIZE     (DC_MAX_FRAME_SIZE - 16)
#define DC_TX_QUEUE_LEN         8
#define DC_RX_QUEUE_LEN         8
#define DC_ACK_TIMEOUT_MS       50
#define DC_MAX_RETRIES          3
#define DC_HEARTBEAT_PERIOD_MS  1000
#define DC_RX_IDLE_TIMEOUT_MS   200

/* ======= Framing ======= */
/* Frame:
 * 0:1   Preamble 0xAA,0x55
 * 2     Version
 * 3     MsgType
 * 4:5   Seq (LE)
 * 6:7   PayloadLen (LE)
 * 8..   Payload (TLVs)
 * end-2:end-1 CRC16-CCITT (X25) over bytes [2 .. end-3]
 */
#define DC_PREAMBLE_A 0xAA
#define DC_PREAMBLE_B 0x55

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  DC_MSG_TELEMETRY      = 0x01, // Motherboard -> Display (TLVs)
  DC_MSG_COMMAND        = 0x02, // Display -> Motherboard (TLVs)
  DC_MSG_ACK            = 0x03,
  DC_MSG_NACK           = 0x04,
  DC_MSG_HEARTBEAT      = 0x05, // both directions
  DC_MSG_REQUEST_FULL   = 0x06  // Display -> Motherboard
} dc_msg_type_t;

/* TLV: [Type:u16][Len:u16][Value:Len] */
typedef enum {
  /* Motherboard -> Display */
  DC_T_TEMPERATURES           = 0x0101, // double[SENSOR_TEMP_QTY]
  DC_T_HUMIDITIES             = 0x0102, // double[SENSOR_HUM_QTY]
  DC_T_SYSTEM_CURRENT         = 0x0103, // double
  DC_T_SYSTEM_VOLTAGE         = 0x0104, // double
  DC_T_HEATER_CURRENT         = 0x0105, // double
  DC_T_FAN_CURRENT            = 0x0106, // double
  DC_T_HUMIDIFIER_CURRENT     = 0x0107, // double
  DC_T_HUMIDIFIER_VOLTAGE     = 0x0108, // double
  DC_T_PHOTOTHERAPY_CURRENT   = 0x0109, // double
  DC_T_USB_CURRENT            = 0x010A, // double
  DC_T_USB_VOLTAGE            = 0x010B, // double
  DC_T_BAT_CURRENT            = 0x010C, // double
  DC_T_BAT_VOLTAGE            = 0x010D, // double
  DC_T_HEATER_SAFE_MAXPWM     = 0x010E, // int32
  DC_T_SERIAL_NUMBER          = 0x010F, // int32
  DC_T_RESET_REASON           = 0x0110, // int32
  DC_T_RESTORE_STATE          = 0x0111, // bool
  DC_T_CONTROL_MODE           = 0x0112, // uint8/enum
  DC_T_CALIBRATION_ERROR      = 0x0113, // bool
  DC_T_STANDBY_TIME           = 0x0114, // float (s)
  DC_T_CONTROL_ACTIVE_TIME    = 0x0115, // float (s)
  DC_T_HEATER_ACTIVE_TIME     = 0x0116, // float (s)
  DC_T_FAN_ACTIVE_TIME        = 0x0117, // float (s)
  DC_T_PHOTO_ACTIVE_TIME      = 0x0118, // float (s)
  DC_T_HUMID_ACTIVE_TIME      = 0x0119, // float (s)
  DC_T_ALARMS_ENABLED         = 0x0120, // bool
  DC_T_ALARM_TO_REPORT        = 0x0121, // bool[NUM_ALARMS]
  DC_T_ALARM_MESSAGE          = 0x0122, // char[]
  DC_T_PREV_ALARM_REPORT      = 0x0123, // bool
  DC_T_FAN_RPM                = 0x0124, // float
  DC_T_HW_CRITICAL_ERROR      = 0x0130, // bool
  DC_T_HW_TEST_ERROR_CODE     = 0x0131, // uint32

  /* Display -> Motherboard */
  DC_T_ACTUATION              = 0x0201, // int32
  DC_T_DESIRED_TEMP           = 0x0202, // double
  DC_T_DESIRED_HUM            = 0x0203, // double
  DC_T_TEMP_CONTROL_EN        = 0x0204, // bool
  DC_T_HUM_CONTROL_EN         = 0x0205, // bool
  DC_T_PHOTOTHERAPY_EN        = 0x0206  // bool
} dc_tlv_type_t;

/* API de callbacks para tu app */
typedef struct {
  /* Llamado cuando llega un COMMAND o REQUEST_FULL */
  void (*on_command_tlv)(uint16_t t, const uint8_t* v, uint16_t l);
  void (*on_request_full_state)(void);
  /* Notificaciones de link */
  void (*on_link_up)(void);
  void (*on_link_down)(void);
  /* NUEVO: llamada por cada TLV recibido en DC_MSG_TELEMETRY */
  void (*on_telemetry_tlv)(uint16_t t, const uint8_t* v, uint16_t l);
} dc_callbacks_t;

/* Handle del driver */
typedef struct {
  Stream*         io;         // USB CDC (Serial), UART (Serial1/2), etc.
  TaskHandle_t    rxTask;
  TaskHandle_t    hbTask;
  QueueHandle_t   txQueue;
  SemaphoreHandle_t ioMtx;
  dc_callbacks_t  cbs;
  uint16_t        txSeq;
  uint16_t        rxLastSeq;
  uint32_t        lastRxMs;
  bool            linkUp;
} dc_handle_t;

/* ======= API ======= */

/* Inicializa con cualquier Stream (USB CDC o UART). No hace begin(). */
void dc_init_stream(dc_handle_t* dc, Stream* port, dc_callbacks_t* cbs);

/* Conveniencia: para UART clásico (HardwareSerial) con begin(baud). */
void dc_init(dc_handle_t* dc, HardwareSerial* port, uint32_t baud, dc_callbacks_t* cbs);

/* Lanza tasks RX + Heartbeat (debe llamarse tras init) */
void dc_start(dc_handle_t* dc);

/* Envío de mensajes (empaqueta múltiples TLVs en una trama) */
bool dc_begin_frame(uint8_t msgType);
bool dc_add_tlv_u8(uint16_t t, uint8_t v);
bool dc_add_tlv_u16(uint16_t t, uint16_t v);
bool dc_add_tlv_u32(uint16_t t, uint32_t v);
bool dc_add_tlv_i32(uint16_t t, int32_t v);
bool dc_add_tlv_float(uint16_t t, float v);
bool dc_add_tlv_double(uint16_t t, double v);
bool dc_add_tlv_blob(uint16_t t, const void* data, uint16_t len);
/* Cola la trama para envío con gestión de ACK/reintentos */
bool dc_end_frame(dc_handle_t* dc);

/* Helpers frecuentes */
bool dc_send_heartbeat(dc_handle_t* dc);
bool dc_send_request_full(dc_handle_t* dc);

#ifdef __cplusplus
}
#endif

#endif /* DISPLAY_COMMS_H */
