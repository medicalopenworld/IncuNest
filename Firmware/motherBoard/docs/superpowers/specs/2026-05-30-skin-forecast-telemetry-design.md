# Skin Forecast Telemetry — Design Spec

**Date:** 2026-05-30
**Status:** Approved
**Scope:** `motherBoard` firmware — ThingsBoard telemetry only. No sensor logic changes.

---

## 1. Goal

Publish `skinTemperatureForecast` and `skinContactQuality` (added in the skin-temp-forecast
feature) to ThingsBoard via both the WiFi and GPRS telemetry paths.

---

## 2. New ThingsBoard Keys

Two new fields added to the recurring telemetry payload:

| Key | Type | Source | Condition |
|---|---|---|---|
| `"Skin_temp_forecast"` | float, 2 decimals | `in3.skinTemperatureForecast` | Only when `!isnan()` |
| `"Skin_contact_quality"` | int | `in3.skinContactQuality` (0=unknown, 1=poor, 2=good) | Always |

---

## 3. Key Macros (main.h)

Add alongside existing `*_KEY` defines (around line 345):

```cpp
#define SKIN_TEMP_FORECAST_KEY   "Skin_temp_forecast"
#define SKIN_CONTACT_QUALITY_KEY "Skin_contact_quality"
```

---

## 4. Serialization — WiFi (`src/Wifi_OTA.cpp`)

In `addTelemetriesToWIFIJSON()`, immediately after the existing `SKIN_TEMPERATURE_KEY` line:

```cpp
if (!isnan(in3.skinTemperatureForecast))
  addVariableToTelemetryWIFIJSON[SKIN_TEMP_FORECAST_KEY] =
      roundSignificantDigits(in3.skinTemperatureForecast, TELEMETRIES_DECIMALS);
addVariableToTelemetryWIFIJSON[SKIN_CONTACT_QUALITY_KEY] = in3.skinContactQuality;
```

---

## 5. Serialization — GPRS (`src/GPRS.cpp`)

In `addTelemetriesToGPRSJSON()`, immediately after the existing `SKIN_TEMPERATURE_KEY` line:

```cpp
if (!isnan(in3.skinTemperatureForecast))
  addVariableToTelemetryGPRSJSON[SKIN_TEMP_FORECAST_KEY] =
      roundSignificantDigits(in3.skinTemperatureForecast, TELEMETRIES_DECIMALS);
addVariableToTelemetryGPRSJSON[SKIN_CONTACT_QUALITY_KEY] = in3.skinContactQuality;
```

---

## 6. What Is NOT Changed

- `in3.skinTemperatureForecast` and `in3.skinContactQuality` — read-only here
- Sensor logic, PID, alarms, display — untouched
- THINGSBOARD_FIELDS_AMOUNT (`= 64`) — current count is well below limit; adding 2 fields
  is safe without increasing this constant
- No new struct fields — `Skin_good_contact` was explicitly excluded

---

## 7. Files Modified

| File | Change |
|---|---|
| `include/main.h` | Add 2 key macros |
| `src/Wifi_OTA.cpp` | Add 2 fields in `addTelemetriesToWIFIJSON()` |
| `src/GPRS.cpp` | Add 2 fields in `addTelemetriesToGPRSJSON()` |
