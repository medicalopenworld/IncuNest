#pragma once
#include <Arduino.h>
#include <Wire.h>

// ─── Direcciones I2C
// ────────────────────────────────────────────────────────── ADDR pin HIGH →
// 0x6B  |  ADDR pin LOW → 0x6A
#define BQ25730_ADDR 0x6B

// ─── Mapa de registros (todos 16-bit, little-endian: byte bajo en addr base)
// ── Fuente: datasheet PDF V2 (layout alternativo al SLUSD80B)
//
// Registros de configuración básica (R/W) – misma posición en ambos datasheets
#define BQ25730_REG_CHARGE_OPTION0 0x00 // Opciones generales del cargador
#define BQ25730_REG_CHARGE_CURRENT 0x02 // Corriente de carga
#define BQ25730_REG_MAX_CHG_VOLT 0x04   // Tensión máxima de carga

// Registros de configuración eléctrica (R/W) – según PDF V2
// (en SLUSD80B estos estaban en 0x30/0x34/0x36/0x38)
#define BQ25730_REG_INPUT_VOLTAGE 0x0A // VINDPM: tensión mínima de entrada
#define BQ25730_REG_VSYS_MIN 0x0C      // Tensión mínima de sistema
#define BQ25730_REG_IIN_HOST 0x0E      // Límite de corriente de entrada (host)

// Registros ChargeOption / PROCHOT – según PDF V2
// (en SLUSD80B estos estaban en 0x06/0x08/0x0A/0x0C/0x0E)
#define BQ25730_REG_CHARGE_OPTION1 0x30
#define BQ25730_REG_CHARGE_OPTION2 0x32
#define BQ25730_REG_CHARGE_OPTION3 0x34
#define BQ25730_REG_PROCHOT_OPT0 0x36
#define BQ25730_REG_PROCHOT_OPT1 0x38

// Registro ADCOption – según PDF V2 (en SLUSD80B era 0x10, que devuelve 0xFFFF)
#define BQ25730_REG_ADC_OPTION 0x3A

// Registros de estado (R)
#define BQ25730_REG_CHARGER_STATUS 0x20 // Estado del cargador y faults
#define BQ25730_REG_PROCHOT_STATUS 0x22
#define BQ25730_REG_IIN_DPM 0x24 // Corriente de entrada en DPM activo
#define BQ25730_REG_ADC_VBUS_PSYS                                              \
  0x26                            // ADC: PSYS (byte bajo) / VBUS (byte alto)
#define BQ25730_REG_ADC_IBAT 0x28 // ADC: IDCHG (byte bajo) / ICHG (byte alto)
#define BQ25730_REG_ADC_IBUS_IDCHG                                             \
  0x2A // ADC: CMPIN (byte bajo) / IBUS (byte alto)
#define BQ25730_REG_ADC_VSYS_VBAT                                              \
  0x2C // ADC: VBAT (byte bajo) / VSYS (byte alto)

// Registros de identificación (R, 8-bit)
#define BQ25730_REG_MFR_ID 0x2E // Fabricante
#define BQ25730_REG_DEV_ID 0x2F // Dispositivo

#define BQ25730_MFR_ID_EXPECTED 0x40 // Valor real observado en PCB V16A
#define BQ25730_DEV_ID_EXPECTED 0xD5 // Valor real en PCB V16A

// ─── ChargeOption0 (0x00) – bits según PDF V2
// ───────────────────────────────── Byte alto (0x01): [7]=EN_LWPWR
// [6:5]=WDTMR_ADJ [4]=IIN_DPM_AUTO_DISABLE
//                   [3]=OTG_ON_CHRGOK [2]=EN_OOA [1]=PWM_FREQ
//                   [0]=LOW_PTM_RIPPLE
// Byte bajo (0x00): [7]=EN_CMP_LATCH [6]=VSYS_UVP_ENZ [5]=EN_LEARN
//                   [4]=IADPT_GAIN [3]=IBAT_GAIN [2]=EN_LDO [1]=EN_IIN_DPM
//                   [0]=CHRG_INHIBIT
// POR = 0xE70E
#define BQ25730_OPT0_EN_LWPWR ((uint16_t)(1 << 15)) // Low Power Mode (ADC off)
#define BQ25730_OPT0_WDTMR_ADJ1 ((uint16_t)(1 << 14)) // Watchdog [1]
#define BQ25730_OPT0_WDTMR_ADJ0 ((uint16_t)(1 << 13)) // Watchdog [0]
#define BQ25730_OPT0_EN_OOA ((uint16_t)(1 << 10))     // Out-of-audio (>25 kHz)
#define BQ25730_OPT0_PWM_FREQ ((uint16_t)(1 << 9))    // Frec: 0=800kHz 1=400kHz
#define BQ25730_OPT0_LOW_PTM_RIPPLE ((uint16_t)(1 << 8)) // Ripple reduction PTM
#define BQ25730_OPT0_IBAT_GAIN ((uint16_t)(1 << 3))      // IBAT amp: 0=8x 1=16x
#define BQ25730_OPT0_EN_LDO ((uint16_t)(1 << 2))         // Modo LDO
#define BQ25730_OPT0_EN_IDPM                                                   \
  ((uint16_t)(1 << 1)) // EN_IIN_DPM (regulación entrada)
#define BQ25730_OPT0_CHRG_INHIBIT                                              \
  ((uint16_t)(1 << 0)) // Inhibir carga (0=cargar)

// ─── ChargeOption1 (0x30) – bits según PDF V2 ────────────────────────────────
#define BQ25730_OPT1_EN_IBAT ((uint16_t)(1 << 15)) // Habilitar medición IBAT
#define BQ25730_OPT1_RSNS_RAC                                                  \
  ((uint16_t)(1 << 11)) // Shunt entrada: 0=10mΩ 1=5mΩ
#define BQ25730_OPT1_RSNS_RSR                                                  \
  ((uint16_t)(1 << 10)) // Shunt batería:  0=10mΩ 1=5mΩ

// ─── ADCOption (0x3A) – bits según PDF V2
// ───────────────────────────────────── Byte alto (0x3B): [7]=ADC_CONV
// [6]=ADC_START [5]=ADC_FULLSCALE Byte bajo (0x3A): [7]=EN_ADC_CMPIN
// [6]=EN_ADC_VBUS [5]=EN_ADC_PSYS
//                   [4]=EN_ADC_IIN [3]=EN_ADC_IDCHG [2]=EN_ADC_ICHG
//                   [1]=EN_ADC_VSYS [0]=EN_ADC_VBAT
// POR = 0x2000 (ADC_FULLSCALE=1, todos los canales deshabilitados)
#define BQ25730_ADC_CONV ((uint16_t)(1 << 15))  // 0=one-shot 1=continuo (1s)
#define BQ25730_ADC_START ((uint16_t)(1 << 14)) // Trigger one-shot (auto-reset)
#define BQ25730_ADC_FULLSCALE ((uint16_t)(1 << 13)) // Rango PSYS/CMPIN: 1=3.06V
#define BQ25730_ADC_EN_VBUS ((uint16_t)(1 << 6))
#define BQ25730_ADC_EN_IIN ((uint16_t)(1 << 4))
#define BQ25730_ADC_EN_IDCHG ((uint16_t)(1 << 3))
#define BQ25730_ADC_EN_ICHG ((uint16_t)(1 << 2))
#define BQ25730_ADC_EN_VSYS ((uint16_t)(1 << 1))
#define BQ25730_ADC_EN_VBAT ((uint16_t)(1 << 0))

// ─── ChargerStatus (0x20) – máscaras de bits
// ──────────────────────────────────
#define BQ25730_STAT_AC_STAT ((uint16_t)(1 << 15))   // Adaptador CA conectado
#define BQ25730_STAT_ICO_DONE ((uint16_t)(1 << 14))  // Algoritmo ICO completado
#define BQ25730_STAT_IN_VINDPM ((uint16_t)(1 << 12)) // Regulando por VINDPM
#define BQ25730_STAT_IN_IINDPM ((uint16_t)(1 << 11)) // Regulando por IIN-DPM
#define BQ25730_STAT_IN_FCHRG ((uint16_t)(1 << 10))  // Carga rápida (CC) activa
#define BQ25730_STAT_IN_PCHRG ((uint16_t)(1 << 9))   // Pre-carga activa
#define BQ25730_STAT_IN_OTG ((uint16_t)(1 << 8))     // Modo OTG activo
#define BQ25730_STAT_F_ACOV                                                    \
  ((uint16_t)(1 << 7)) // Fault: sobretensión adaptador
#define BQ25730_STAT_F_BATOC                                                   \
  ((uint16_t)(1 << 6)) // Fault: sobrecorriente batería
#define BQ25730_STAT_F_ACOC                                                    \
  ((uint16_t)(1 << 5)) // Fault: sobrecorriente adaptador
#define BQ25730_STAT_F_SYS_OV                                                  \
  ((uint16_t)(1 << 4)) // Fault: sobretensión sistema
#define BQ25730_STAT_F_SYS_UV ((uint16_t)(1 << 3)) // Fault: subtensión sistema
#define BQ25730_STAT_F_LATCHOFF ((uint16_t)(1 << 2)) // Fault: forzado off

#define BQ25730_STAT_ANY_FAULT                                                 \
  (BQ25730_STAT_F_ACOV | BQ25730_STAT_F_BATOC | BQ25730_STAT_F_ACOC |          \
   BQ25730_STAT_F_SYS_OV | BQ25730_STAT_F_SYS_UV | BQ25730_STAT_F_LATCHOFF)

// ─── Sense resistors – corrección hardware PCB V16A
// ─────────────────────────── El chip con RSNS=1 asume shunts de 5 mΩ
// internamente. El PCB V16A monta shunts de 3 mΩ por error de componente.
// TODO: cambiar a 3 mΩ → 5 mΩ en la siguiente revisión de PCB y eliminar
//       la corrección software (igualar ambas defines).
#define BQ25730_RSNS_NOMINAL_MOHM 5 // mΩ asumido por chip (RSNS=1)
#define BQ25730_RSNS_ACTUAL_MOHM 3 // mΩ real en PCB — cambiar en rev. siguiente

// ─── Resoluciones del ADC interno (según PDF V2)
// ──────────────────────────────
#define BQ25730_ADC_VBUS_STEP_MV 96  // 96 mV/bit, rango 0–24.48V (sin offset)
#define BQ25730_ADC_VBUS_OFFSET_MV 0 // Sin offset en PDF V2
#define BQ25730_ADC_VBAT_STEP_MV 64  // 64 mV/bit
#define BQ25730_ADC_VBAT_OFFSET_MV 2880 // Offset 2.88 V → rango 2.88–19.2 V
#define BQ25730_ADC_VSYS_STEP_MV 64     // 64 mV/bit
#define BQ25730_ADC_VSYS_OFFSET_MV 2880
#define BQ25730_ADC_ICHG_STEP_MA 128 // mA/bit nominal con RSNS_RSR=1 (5 mΩ)
#define BQ25730_ADC_IBUS_STEP_MA 100 // mA/bit nominal con RSNS_RAC=1 (5 mΩ)

// ─── Parámetros de carga para batería Plomo-Ácido AGM 12V 7Ah
// ─────────────────
#define BQ25730_VCHARGE_ABSORPTION_MV 14400 // 900 × 16 mV → registro 0x3840
#define BQ25730_VCHARGE_FLOAT_MV 13648      // 853 × 16 mV → registro 0x3550
#define BQ25730_ICHG_MA 960                 // Corriente de carga real [mA]
#define BQ25730_ICHG_TERM_MA 320            // Umbral fin absorción [mA]
#define BQ25730_IIN_LIMIT_MA 6400           // Límite entrada máximo [mA real]
// VSYS_MIN: 100 mV/bit + 1000 mV offset → 11000 mV
#define BQ25730_VSYS_MIN_MV 11000 // 100 × 100 mV + 1000 = 11000 mV
// VINDPM: 64 mV/bit + 3200 mV offset → máx. registro
#define BQ25730_VINDPM_MV 19520 // (255×64)+3200 → máx. registro

// ─── Estado de carga
// ──────────────────────────────────────────────────────────
typedef enum {
  BQ25730_STATE_NO_POWER,   // Sin adaptador CA
  BQ25730_STATE_ABSORPTION, // Carga rápida CC/CV a 14.4 V
  BQ25730_STATE_FLOAT,      // Flotación CV a 13.65 V
  BQ25730_STATE_FAULT       // Fallo activo
} BQ25730_ChargeState;

// ─── Estructura de estado devuelta por charge_status()
// ────────────────────────
typedef struct {
  uint16_t raw_status;       // Valor bruto del registro 0x20
  bool ac_present;           // Adaptador CA conectado
  bool fast_charging;        // Carga rápida (CC) activa
  bool pre_charging;         // Pre-carga activa
  bool fault;                // Cualquier fault activo
  BQ25730_ChargeState state; // Estado lógico de carga
  uint16_t vbat_mv;          // Tensión batería [mV]
  uint16_t vsys_mv;          // Tensión sistema [mV]
  uint16_t vbus_mv;          // Tensión entrada VBUS [mV]
  int16_t ichg_ma;           // Corriente de carga [mA]
  int16_t ibus_ma;           // Corriente de entrada [mA]
} BQ25730_Status;

// ─── Variable de presencia (definida en BQ25730.cpp) ─────────────────────────
extern bool chargerPresent;

// ─── Funciones públicas
// ───────────────────────────────────────────────────────
bool init_BQ25730(TwoWire *i2c);
bool charge_status(BQ25730_Status *status);
bool set_charge_voltage(uint16_t voltage_mv);
