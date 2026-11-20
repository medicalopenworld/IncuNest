#pragma once
// Umbrales de servicio por actuador, en HORAS DE USO desde el ultimo servicio
// registrado (ver modules/service/service_calc.h).
//
// PROVISIONALES. Salen de una estimacion de vida util tipica de cada
// componente, no del plan de mantenimiento del fabricante: hay que sustituirlos
// por los valores de ese plan cuando se tenga. Estan compilados a proposito
// (sin configuracion remota todavia): cambiarlos exige una version de firmware,
// que es lo que se quiere mientras sean una estimacion.
//
// Un umbral de 0 desactiva el aviso de ese actuador.
//
// Los ids son los de `ServiceActuator` (shared/include/service_hours_proto.h).
// Una hora de uso del humidificador es una hora con el control de humedad
// activo (timeTrackHandler en system/updateData.cpp), no una hora de agua
// nebulizada de verdad: el piezo del nebulizador es el componente que manda.
#define SERVICE_THRESHOLD_HEATER_H      5000u   // resistencia calefactora
#define SERVICE_THRESHOLD_FAN_H         10000u  // ventilador
#define SERVICE_THRESHOLD_HUMIDIFIER_H  2000u   // piezo del nebulizador
#define SERVICE_THRESHOLD_PHOTO_H       10000u  // LED de fototerapia
