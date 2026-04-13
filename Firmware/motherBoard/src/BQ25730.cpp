/*
  BQ25730RSNR – Driver de inicialización y monitorización
  Hardware: IncuNest V16, batería Plomo-Ácido AGM 12V 7Ah (NX FR)
  Bus: I2C primario (SDA=47, SCL=48), dirección 0x6B
  Referencia: datasheet PDF V2 (mapa de registros alternativo)

  Mapa de registros usado (PDF V2):
    0x00  ChargeOption0
    0x02  ChargeCurrent
    0x04  MaxChargeVoltage
    0x0A  VINDPM (InputVoltage)     ← antes en 0x34
    0x0C  VSYS_MIN                  ← antes en 0x36
    0x0E  IIN_HOST                  ← antes en 0x38
    0x20  ChargerStatus
    0x26-0x2C  Registros ADC
    0x30  ChargeOption1             ← antes en 0x06
    0x32  ChargeOption2             ← antes en 0x08
    0x34  ChargeOption3             ← antes en 0x0A
    0x3A  ADCOption                 ← antes en 0x10 (no existía)

  Curva de carga implementada:
    Etapa 1 – ABSORCIÓN: 14.4V / 960 mA hasta ICHG < 320 mA
    Etapa 2 – FLOTACIÓN: 13.65V / CV permanente (mantenimiento)
*/

#include "main.h" // Incluye BQ25730.h, logI/logE, Wire, etc.

// ─── Estado interno del módulo ────────────────────────────────────────────────
bool chargerPresent = false;
static TwoWire *_i2c = nullptr;
static bool     _initialized = false;

// ─── Helpers I2C de bajo nivel ────────────────────────────────────────────────

static bool write_reg16(uint8_t reg, uint16_t value) {
    _i2c->beginTransmission(BQ25730_ADDR);
    _i2c->write(reg);
    _i2c->write((uint8_t)(value & 0xFF));        // Byte bajo (dirección base)
    _i2c->write((uint8_t)((value >> 8) & 0xFF)); // Byte alto (dirección base+1)
    return (_i2c->endTransmission() == 0);
}

static bool read_reg16(uint8_t reg, uint16_t *out) {
    _i2c->beginTransmission(BQ25730_ADDR);
    _i2c->write(reg);
    if (_i2c->endTransmission(false) != 0) return false;
    if (_i2c->requestFrom((uint8_t)BQ25730_ADDR, (uint8_t)2) != 2) return false;
    uint8_t lo = _i2c->read();
    uint8_t hi = _i2c->read();
    *out = (uint16_t)lo | ((uint16_t)hi << 8);
    return true;
}

static bool read_reg8(uint8_t reg, uint8_t *out) {
    _i2c->beginTransmission(BQ25730_ADDR);
    _i2c->write(reg);
    if (_i2c->endTransmission(false) != 0) return false;
    if (_i2c->requestFrom((uint8_t)BQ25730_ADDR, (uint8_t)1) != 1) return false;
    *out = _i2c->read();
    return true;
}

// ─── Funciones de cálculo de valores de registro ─────────────────────────────
//
// ChargeCurrent   (0x02) bits[12:6]:  valor = mA / 64      → reg = valor << 6
// MaxChargeVoltage(0x04) bits[14:4]:  valor = mV / 16      → reg = valor << 4
// VINDPM          (0x0A) bits[13:6]:  valor = (mV-3200)/64 → reg = valor << 6
// VSYS_MIN        (0x0C) bits[15:8]:  valor = (mV-1000)/100→ reg = valor << 8
// IIN_HOST        (0x0E) bits[14:8]:  valor = mA/100 - 1   → reg = valor << 8

static uint16_t calc_charge_current_reg(uint16_t ma) {
    // 64 mA/bit, campo [12:6].
    // Corrección shunt: I_set = I_real × (R_actual / R_nominal) = I_real × 3/5
    uint16_t ma_adj = (uint16_t)((uint32_t)ma * BQ25730_RSNS_ACTUAL_MOHM
                                             / BQ25730_RSNS_NOMINAL_MOHM);
    uint16_t v = (ma_adj / 64U) & 0x7FU;
    return (uint16_t)(v << 6);
}

static uint16_t calc_charge_voltage_reg(uint16_t mv) {
    // 16 mV/bit, campo [14:4].
    // Para 14400 mV: 14400/16 = 900 → 0x3840
    // Para 13648 mV: 13648/16 = 853 → 0x3550
    uint16_t v = (mv / 16U) & 0x7FFU;
    return (uint16_t)(v << 4);
}

static uint16_t calc_vindpm_reg(uint16_t mv) {
    // 64 mV/bit, offset 3200 mV, campo [13:6].
    // Para 19520 mV: (19520-3200)/64 = 255 → 0x3FC0
    if (mv < 3200U) mv = 3200U;
    uint16_t v = ((mv - 3200U) / 64U) & 0xFFU;
    return (uint16_t)(v << 6);
}

static uint16_t calc_vsys_min_reg(uint16_t mv) {
    // 100 mV/bit + 1000 mV offset, campo [15:8] (byte alto).
    // Para 11000 mV: (11000-1000)/100 = 100 → 0x6400
    if (mv < 1000U) mv = 1000U;
    uint16_t v = ((mv - 1000U) / 100U) & 0xFFU;
    return (uint16_t)(v << 8);
}

static uint16_t calc_iin_host_reg(uint16_t ma) {
    // 100 mA/bit + 100 mA offset, campo [14:8] (byte alto).
    // I_chip = (code + 1) × 100 mA → code = I/100 - 1
    // Corrección shunt: I_set = I_real × (R_actual / R_nominal)
    // Para 6400 mA real: 6400×3/5 = 3840 mA → code = 3840/100-1 = 37 → 0x2500
    uint16_t ma_adj = (uint16_t)((uint32_t)ma * BQ25730_RSNS_ACTUAL_MOHM
                                             / BQ25730_RSNS_NOMINAL_MOHM);
    if (ma_adj < 100U) ma_adj = 100U;
    uint16_t code = ((ma_adj / 100U) - 1U) & 0x7FU;
    return (uint16_t)(code << 8);
}

// ─── init_BQ25730() ───────────────────────────────────────────────────────────
bool init_BQ25730(TwoWire *i2c) {
    _i2c          = i2c;
    _initialized  = false;
    chargerPresent = false;

    // ── 1. Verificar identidad del chip ───────────────────────────────────────
    uint8_t mfr_id = 0, dev_id = 0;
    if (!read_reg8(BQ25730_REG_MFR_ID, &mfr_id) ||
        !read_reg8(BQ25730_REG_DEV_ID, &dev_id)) {
        logE("[BQ25730] I2C sin respuesta en 0x" + String(BQ25730_ADDR, HEX));
        return false;
    }
    if (mfr_id != BQ25730_MFR_ID_EXPECTED || dev_id != BQ25730_DEV_ID_EXPECTED) {
        logE("[BQ25730] ID inesperado: mfr=0x" + String(mfr_id, HEX) +
             " dev=0x" + String(dev_id, HEX) +
             " (esperado 0x" + String(BQ25730_MFR_ID_EXPECTED, HEX) +
             "/0x" + String(BQ25730_DEV_ID_EXPECTED, HEX) + ") – continuando");
    } else {
        logI("[BQ25730] Chip detectado (mfr=0x" + String(mfr_id, HEX) +
             " dev=0x" + String(dev_id, HEX) + ")");
    }

    // ── 2. ChargeOption0 (0x00) ───────────────────────────────────────────────
    //  Derivado del POR (0xE70E) limpiando EN_LWPWR y WDTMR_ADJ:
    //    POR: EN_LWPWR=1, WDTMR=11(175s), EN_OOA=1, PWM_FREQ=1, LOW_PTM_RIPPLE=1
    //         IBAT_GAIN=1(16x), EN_LDO=1, EN_IIN_DPM=1, CHRG_INHIBIT=0
    //  Cambios: EN_LWPWR=0 (habilita ADC), WDTMR=00 (watchdog off, sin host)
    //  Resultado: 0x070E
    //    bit10(EN_OOA)=1, bit9(PWM_FREQ)=1→400kHz, bit8(LOW_PTM_RIPPLE)=1
    //    bit3(IBAT_GAIN)=1, bit2(EN_LDO)=1, bit1(EN_IIN_DPM)=1
    const uint16_t charge_option0 = BQ25730_OPT0_EN_OOA         |  // bit 10
                                    BQ25730_OPT0_PWM_FREQ        |  // bit  9
                                    BQ25730_OPT0_LOW_PTM_RIPPLE  |  // bit  8
                                    BQ25730_OPT0_IBAT_GAIN       |  // bit  3
                                    BQ25730_OPT0_EN_LDO          |  // bit  2
                                    BQ25730_OPT0_EN_IDPM;           // bit  1
    if (!write_reg16(BQ25730_REG_CHARGE_OPTION0, charge_option0)) {
        logE("[BQ25730] Error escribiendo ChargeOption0");
        return false;
    }
    logI("[BQ25730] ChargeOption0 = 0x" + String(charge_option0, HEX));

    // ── 3. ChargeOption1 (0x30) ───────────────────────────────────────────────
    //  • EN_IBAT = 1   → Medición de corriente de batería por ADC.
    //  • RSNS_RAC = 1  → Shunt de entrada = 5 mΩ.
    //  • RSNS_RSR = 1  → Shunt de batería = 5 mΩ.
    //  Valor: 0x8C00
    const uint16_t charge_option1 = BQ25730_OPT1_EN_IBAT   |  // bit 15
                                    BQ25730_OPT1_RSNS_RAC   |  // bit 11
                                    BQ25730_OPT1_RSNS_RSR;     // bit 10
    if (!write_reg16(BQ25730_REG_CHARGE_OPTION1, charge_option1)) {
        logE("[BQ25730] Error escribiendo ChargeOption1");
        return false;
    }
    logI("[BQ25730] ChargeOption1 = 0x" + String(charge_option1, HEX));

    // ── 4. ChargeOption2 y ChargeOption3 (0x32 / 0x34) ───────────────────────
    if (!write_reg16(BQ25730_REG_CHARGE_OPTION2, 0x0000) ||
        !write_reg16(BQ25730_REG_CHARGE_OPTION3, 0x0000)) {
        logE("[BQ25730] Error escribiendo ChargeOption2/3");
        return false;
    }

    // ── 5. ADCOption (0x3A): modo continuo, canales VBUS/IIN/ICHG/VSYS/VBAT ──
    //  ADC_CONV=1 (continuo, 1s), ADC_FULLSCALE=1 (3.06V para PSYS/CMPIN)
    //  Canales: VBUS, IIN, IDCHG, ICHG, VSYS, VBAT
    //  Byte alto: 0xA0 (ADC_CONV=1, ADC_FULLSCALE=1)
    //  Byte bajo: 0x5F (VBUS|IIN|IDCHG|ICHG|VSYS|VBAT)
    //  Valor: 0xA05F
    const uint16_t adc_option = BQ25730_ADC_CONV      |  // bit 15: continuo
                                BQ25730_ADC_FULLSCALE  |  // bit 13: rango 3.06V
                                BQ25730_ADC_EN_VBUS    |  // bit  6
                                BQ25730_ADC_EN_IIN     |  // bit  4
                                BQ25730_ADC_EN_IDCHG   |  // bit  3
                                BQ25730_ADC_EN_ICHG    |  // bit  2
                                BQ25730_ADC_EN_VSYS    |  // bit  1
                                BQ25730_ADC_EN_VBAT;      // bit  0
    if (!write_reg16(BQ25730_REG_ADC_OPTION, adc_option)) {
        logE("[BQ25730] Error escribiendo ADCOption");
        return false;
    }
    logI("[BQ25730] ADCOption = 0x" + String(adc_option, HEX));

    // ── 6. IIN_HOST (0x0E): límite de corriente de entrada ───────────────────
    //  6400 mA real → adj 3840 mA → code = 3840/100-1 = 37 → reg 0x2500
    const uint16_t iin_host = calc_iin_host_reg(BQ25730_IIN_LIMIT_MA);
    if (!write_reg16(BQ25730_REG_IIN_HOST, iin_host)) {
        logE("[BQ25730] Error escribiendo IIN_HOST");
        return false;
    }
    logI("[BQ25730] IIN_HOST = " + String(BQ25730_IIN_LIMIT_MA) + " mA (reg=0x" +
         String(iin_host, HEX) + ")");

    // ── 7. VINDPM (0x0A): umbral mínimo de entrada ────────────────────────────
    //  Para 19520 mV: (19520-3200)/64 = 255 → 0x3FC0
    const uint16_t vindpm = calc_vindpm_reg(BQ25730_VINDPM_MV);
    if (!write_reg16(BQ25730_REG_INPUT_VOLTAGE, vindpm)) {
        logE("[BQ25730] Error escribiendo VINDPM");
        return false;
    }
    logI("[BQ25730] VINDPM = " + String(BQ25730_VINDPM_MV) + " mV (reg=0x" +
         String(vindpm, HEX) + ")");

    // ── 8. VSYS_MIN (0x0C): tensión mínima de sistema ────────────────────────
    //  11000 mV: (11000-1000)/100 = 100 → reg 0x6400
    const uint16_t vsys_min = calc_vsys_min_reg(BQ25730_VSYS_MIN_MV);
    if (!write_reg16(BQ25730_REG_VSYS_MIN, vsys_min)) {
        logE("[BQ25730] Error escribiendo VSYS_MIN");
        return false;
    }
    logI("[BQ25730] VSYS_MIN = " + String(BQ25730_VSYS_MIN_MV) + " mV (reg=0x" +
         String(vsys_min, HEX) + ")");

    // ── 9. ChargeCurrent (0x02) ───────────────────────────────────────────────
    //  960 mA real → adj 576 mA → 576/64=9 → reg 0x0240
    const uint16_t chg_current = calc_charge_current_reg(BQ25730_ICHG_MA);
    if (!write_reg16(BQ25730_REG_CHARGE_CURRENT, chg_current)) {
        logE("[BQ25730] Error escribiendo ChargeCurrent");
        return false;
    }
    logI("[BQ25730] ChargeCurrent = " + String(BQ25730_ICHG_MA) + " mA (reg=0x" +
         String(chg_current, HEX) + ")");

    // ── 10. MaxChargeVoltage (0x04): tensión de absorción ─────────────────────
    //  14400 mV: 14400/16=900 → reg 0x3840
    const uint16_t chg_voltage = calc_charge_voltage_reg(BQ25730_VCHARGE_ABSORPTION_MV);
    if (!write_reg16(BQ25730_REG_MAX_CHG_VOLT, chg_voltage)) {
        logE("[BQ25730] Error escribiendo MaxChargeVoltage");
        return false;
    }
    logI("[BQ25730] MaxChargeVoltage = " + String(BQ25730_VCHARGE_ABSORPTION_MV) +
         " mV (absorción) (reg=0x" + String(chg_voltage, HEX) + ")");

    _initialized   = true;
    chargerPresent = true;
    logI("[BQ25730] Init OK (PDF V2) – Vabs=14.4V Ichg=960mA IIN=6400mA VSYS=11V VINDPM=19.5V");
    return true;
}

// ─── charge_status() ──────────────────────────────────────────────────────────
bool charge_status(BQ25730_Status *status) {
    if (!_initialized || status == nullptr) return false;

    uint16_t raw_status    = 0;
    uint16_t raw_vbus_psys = 0;
    uint16_t raw_ibat      = 0;
    uint16_t raw_ibus      = 0;
    uint16_t raw_vsys_vbat = 0;

    // REG 0x20: ChargerStatus
    if (!read_reg16(BQ25730_REG_CHARGER_STATUS, &raw_status)) {
        logE("[BQ25730] Error leyendo ChargerStatus");
        return false;
    }

    // REG 0x26: byte bajo = PSYS, byte alto = VBUS (96 mV/bit, sin offset)
    if (!read_reg16(BQ25730_REG_ADC_VBUS_PSYS, &raw_vbus_psys)) {
        logE("[BQ25730] Error leyendo ADC_VBUS");
        return false;
    }

    // REG 0x28: byte bajo = IDCHG, byte alto = ICHG
    if (!read_reg16(BQ25730_REG_ADC_IBAT, &raw_ibat)) {
        logE("[BQ25730] Error leyendo ADC_IBAT");
        return false;
    }

    // REG 0x2A: byte bajo = CMPIN, byte alto = IBUS
    if (!read_reg16(BQ25730_REG_ADC_IBUS_IDCHG, &raw_ibus)) {
        logE("[BQ25730] Error leyendo ADC_IBUS");
        return false;
    }

    // REG 0x2C: byte bajo = VBAT, byte alto = VSYS
    if (!read_reg16(BQ25730_REG_ADC_VSYS_VBAT, &raw_vsys_vbat)) {
        logE("[BQ25730] Error leyendo ADC_VSYS_VBAT");
        return false;
    }

    // ── Parseo del registro de estado ────────────────────────────────────────
    status->raw_status    = raw_status;
    status->ac_present    = (raw_status & BQ25730_STAT_AC_STAT)   != 0;
    status->fast_charging = (raw_status & BQ25730_STAT_IN_FCHRG)  != 0;
    status->pre_charging  = (raw_status & BQ25730_STAT_IN_PCHRG)  != 0;
    status->fault         = (raw_status & BQ25730_STAT_ANY_FAULT) != 0;

    if (status->fault) {
        status->state = BQ25730_STATE_FAULT;
    } else if (!status->ac_present) {
        status->state = BQ25730_STATE_NO_POWER;
    } else if (status->fast_charging || status->pre_charging) {
        status->state = BQ25730_STATE_ABSORPTION;
    } else {
        status->state = BQ25730_STATE_FLOAT;
    }

    // ── Conversión ADC ────────────────────────────────────────────────────────

    // VBUS: byte alto de 0x26, 96 mV/bit, sin offset
    uint8_t adc_vbus = (uint8_t)(raw_vbus_psys >> 8);
    status->vbus_mv  = (uint16_t)(adc_vbus * BQ25730_ADC_VBUS_STEP_MV);

    // VBAT: byte bajo de 0x2C, 64 mV/bit + 2880 mV
    uint8_t adc_vbat = (uint8_t)(raw_vsys_vbat & 0xFF);
    status->vbat_mv  = (uint16_t)(adc_vbat * BQ25730_ADC_VBAT_STEP_MV +
                                  BQ25730_ADC_VBAT_OFFSET_MV);

    // VSYS: byte alto de 0x2C, 64 mV/bit + 2880 mV
    uint8_t adc_vsys = (uint8_t)(raw_vsys_vbat >> 8);
    status->vsys_mv  = (uint16_t)(adc_vsys * BQ25730_ADC_VSYS_STEP_MV +
                                  BQ25730_ADC_VSYS_OFFSET_MV);

    // ICHG: byte alto de 0x28, 128 mA/bit nominal (RSNS_RSR=1).
    // Corrección shunt real 3mΩ: × (R_nominal / R_actual) = × 5/3
    uint8_t adc_ichg = (uint8_t)(raw_ibat >> 8);
    status->ichg_ma  = (int16_t)((uint32_t)adc_ichg * BQ25730_ADC_ICHG_STEP_MA
                                 * BQ25730_RSNS_NOMINAL_MOHM / BQ25730_RSNS_ACTUAL_MOHM);

    // IBUS: byte alto de 0x2A, 100 mA/bit nominal (RSNS_RAC=1).
    uint8_t adc_ibus = (uint8_t)(raw_ibus >> 8);
    status->ibus_ma  = (int16_t)((uint32_t)adc_ibus * BQ25730_ADC_IBUS_STEP_MA
                                 * BQ25730_RSNS_NOMINAL_MOHM / BQ25730_RSNS_ACTUAL_MOHM);

    return true;
}

// ─── set_charge_voltage() ─────────────────────────────────────────────────────
bool set_charge_voltage(uint16_t voltage_mv) {
    if (!_initialized) {
        logE("[BQ25730] set_charge_voltage: chip no inicializado");
        return false;
    }
    uint16_t reg_val = calc_charge_voltage_reg(voltage_mv);
    if (!write_reg16(BQ25730_REG_MAX_CHG_VOLT, reg_val)) {
        logE("[BQ25730] Error escribiendo tensión: " + String(voltage_mv) + " mV");
        return false;
    }
    logI("[BQ25730] Tensión de carga cambiada a " + String(voltage_mv) +
         " mV (reg=0x" + String(reg_val, HEX) + ")");
    return true;
}
