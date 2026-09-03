#include "alarm_text.h"

#include "alarm_policy.h"

// Ingles es el idioma de reserva: cualquier idioma sin traduccion propia
// (hoy, PORTUGUESE) cae aqui en vez de quedarse sin texto.
#define ES(s) if (lang == SPANISH) return (s)
#define FR(s) if (lang == FRENCH) return (s)

const char *alarm_title_text(AlarmId id, Language lang) {
  switch (id) {
    // "FALLO" y no "CORTE": si el corte termico salta es que el termostato ha
    // fallado (201.15.4.2.1 aa lo describe asi), no que el equipo este
    // protegiendose de una situacion normal. El titulo tiene que decirle al
    // operador que esto es una averia, no un ciclo de trabajo.
    case ALARM_AIR_THERMAL_CUTOUT:
      ES("FALLO TERMICO AIRE");
      FR("PANNE THERMIQUE AIR");
      return "AIR THERMAL FAULT";
    case ALARM_SKIN_THERMAL_CUTOUT:
      ES("FALLO TERMICO PIEL");
      FR("PANNE THERMIQUE PEAU");
      return "SKIN THERMAL FAULT";
    case ALARM_AIR_SENSOR_FAULT:
      ES("FALLO SENSOR AIRE");
      FR("PANNE CAPTEUR AIR");
      return "AIR SENSOR FAULT";
    case ALARM_SKIN_SENSOR_FAULT_SKIN_MODE:
      ES("FALLO SONDA PIEL");
      FR("PANNE SONDE PEAU");
      return "SKIN PROBE FAULT";
    case ALARM_FAN_FAILURE:
      ES("FALLO VENTILADOR");
      FR("PANNE VENTILATEUR");
      return "FAN FAILURE";
    case ALARM_AIR_OUTLET_BLOCKED:
      ES("SALIDA DE AIRE OBSTRUIDA");
      FR("SORTIE D AIR OBSTRUEE");
      return "AIR OUTLET BLOCKED";
    case ALARM_MAINS_INTERRUPTION:
      ES("CORTE DE RED");
      FR("COUPURE SECTEUR");
      return "MAINS INTERRUPTION";
    case ALARM_AIR_TEMP_DEVIATION_HIGH:
      ES("TEMP AIRE ALTA");
      FR("TEMP AIR ELEVEE");
      return "AIR TEMP HIGH";
    case ALARM_AIR_TEMP_DEVIATION_LOW:
      ES("TEMP AIRE BAJA");
      FR("TEMP AIR BASSE");
      return "AIR TEMP LOW";
    case ALARM_SKIN_TEMP_DEVIATION_HIGH:
      ES("TEMP PIEL ALTA");
      FR("TEMP PEAU ELEVEE");
      return "SKIN TEMP HIGH";
    case ALARM_SKIN_TEMP_DEVIATION_LOW:
      ES("TEMP PIEL BAJA");
      FR("TEMP PEAU BASSE");
      return "SKIN TEMP LOW";
    case ALARM_HEATER_FAULT:
      ES("FALLO CALENTADOR");
      FR("PANNE CHAUFFAGE");
      return "HEATER FAULT";
    case ALARM_SUPPLY_UNDERVOLTAGE:
      ES("TENSION BAJA");
      FR("TENSION BASSE");
      return "SUPPLY UNDERVOLTAGE";
    case ALARM_HMI_LINK_LOST:
      ES("SIN ENLACE PANTALLA");
      FR("LIAISON ECRAN PERDUE");
      return "DISPLAY LINK LOST";
    case ALARM_SKIN_SENSOR_FAULT_AIR_MODE:
      ES("SONDA PIEL NO VALIDA");
      FR("SONDE PEAU INVALIDE");
      return "SKIN PROBE UNUSABLE";
    case ALARM_HUMIDITY_DEVIATION:
      ES("DESVIACION HUMEDAD");
      FR("ECART HUMIDITE");
      return "HUMIDITY DEVIATION";
    case ALARM_HEATER_SENSOR_FAULT:
      ES("FALLO SENSOR CALENTADOR");
      FR("PANNE CAPTEUR CHAUFFAGE");
      return "HEATER SENSOR FAULT";
    case ALARM_SENSORBOARD_LINK_LOST:
      ES("SIN ENLACE SENSORBOARD");
      FR("LIAISON SENSORBOARD PERDUE");
      return "SENSORBOARD LINK LOST";
    case ALARM_SENSORBOARD_DOOR_FAULT:
      ES("SENSOR PUERTA SOSPECHOSO");
      FR("CAPTEUR PORTE SUSPECT");
      return "DOOR SENSOR SUSPECT";
    default:
      return "ALARM";
  }
}

const char *alarm_action_text(AlarmId id, Language lang) {
  switch (id) {
    // Tres cosas, en este orden: que el bebe esta sin calefaccion, que la
    // causa es una averia del equipo y no una condicion pasajera, y que el
    // aviso seguira puesto aunque la temperatura baje. Lo ultimo importa
    // porque la alarma es latching (201.15.4.2.1 aa/bb) y sin decirlo el
    // operador cree que la pantalla se ha quedado colgada.
    case ALARM_AIR_THERMAL_CUTOUT:
    case ALARM_SKIN_THERMAL_CUTOUT:
      ES("CALEFACTOR CORTADO - REVISAR AL BEBE - AVERIA: AVISO FIJO HASTA REINICIAR");
      FR("CHAUFFAGE COUPE - VERIFIER LE BEBE - PANNE: ALARME FIXE JUSQU AU REDEMARRAGE");
      return "HEATER CUT - CHECK THE BABY - FAULT: ALARM STAYS UNTIL RESTART";
    case ALARM_AIR_SENSOR_FAULT:
      ES("CALEFACTOR CORTADO - SIN MEDIDA DE AIRE - REVISAR AL BEBE");
      FR("CHAUFFAGE COUPE - PAS DE MESURE D AIR - VERIFIER LE BEBE");
      return "HEATER CUT - NO AIR READING - CHECK THE BABY";
    case ALARM_SKIN_SENSOR_FAULT_SKIN_MODE:
      ES("CALEFACTOR CORTADO - REVISAR SONDA O PASAR A MODO AIRE");
      FR("CHAUFFAGE COUPE - VERIFIER LA SONDE OU PASSER EN MODE AIR");
      return "HEATER CUT - CHECK PROBE OR SWITCH TO AIR MODE";
    case ALARM_FAN_FAILURE:
      ES("CALEFACTOR CORTADO - SIN CIRCULACION DE AIRE - REVISAR AL BEBE Y EL EQUIPO");
      FR("CHAUFFAGE COUPE - PAS DE CIRCULATION D AIR - VERIFIER LE BEBE ET L APPAREIL");
      return "HEATER CUT - NO AIR CIRCULATION - CHECK THE BABY AND SERVICE THE UNIT";
    case ALARM_AIR_OUTLET_BLOCKED:
      ES("CALEFACTOR CORTADO - DESPEJAR LA SALIDA DE AIRE");
      FR("CHAUFFAGE COUPE - DEGAGER LA SORTIE D AIR");
      return "HEATER CUT - CLEAR THE AIR OUTLET";
    case ALARM_MAINS_INTERRUPTION:
      ES("REVISAR LA CONEXION A LA RED");
      FR("VERIFIER LE RACCORDEMENT SECTEUR");
      return "CHECK THE MAINS CONNECTION";
    case ALARM_AIR_TEMP_DEVIATION_HIGH:
      ES("CALEFACTOR CORTADO - AIRE MAS DE 3 C SOBRE LA CONSIGNA");
      FR("CHAUFFAGE COUPE - AIR A PLUS DE 3 C AU DESSUS DE LA CONSIGNE");
      return "HEATER CUT - AIR OVER 3 C ABOVE SETPOINT";
    case ALARM_AIR_TEMP_DEVIATION_LOW:
      ES("AIRE MAS DE 3 C BAJO LA CONSIGNA");
      FR("AIR A PLUS DE 3 C SOUS LA CONSIGNE");
      return "AIR OVER 3 C BELOW SETPOINT";
    case ALARM_SKIN_TEMP_DEVIATION_HIGH:
      ES("CALEFACTOR CORTADO - PIEL MAS DE 1 C SOBRE LA CONSIGNA");
      FR("CHAUFFAGE COUPE - PEAU A PLUS DE 1 C AU DESSUS DE LA CONSIGNE");
      return "HEATER CUT - SKIN OVER 1 C ABOVE SETPOINT";
    case ALARM_SKIN_TEMP_DEVIATION_LOW:
      ES("PIEL MAS DE 1 C BAJO LA CONSIGNA");
      FR("PEAU A PLUS DE 1 C SOUS LA CONSIGNE");
      return "SKIN OVER 1 C BELOW SETPOINT";
    case ALARM_HEATER_FAULT:
      ES("EL EQUIPO NO CALIENTA - REVISAR AL BEBE Y EL EQUIPO");
      FR("L APPAREIL NE CHAUFFE PAS - VERIFIER LE BEBE ET L APPAREIL");
      return "UNIT NOT HEATING - CHECK THE BABY AND SERVICE THE UNIT";
    case ALARM_SUPPLY_UNDERVOLTAGE:
      ES("REVISAR FUENTE Y CABLEADO");
      FR("VERIFIER L ALIMENTATION ET LE CABLAGE");
      return "CHECK SUPPLY AND WIRING";
    case ALARM_HMI_LINK_LOST:
      ES("DATOS NO FIABLES - REVISAR AL BEBE");
      FR("DONNEES NON FIABLES - VERIFIER LE BEBE");
      return "DATA UNRELIABLE - CHECK THE BABY";
    case ALARM_SKIN_SENSOR_FAULT_AIR_MODE:
      ES("SIN TEMP DE PIEL - MODO AIRE ACTIVO");
      FR("PAS DE TEMP PEAU - MODE AIR ACTIF");
      return "NO SKIN TEMP - AIR MODE ACTIVE";
    case ALARM_HUMIDITY_DEVIATION:
      ES("REVISAR DEPOSITO DE AGUA");
      FR("VERIFIER LE RESERVOIR D EAU");
      return "CHECK THE WATER TANK";
    // Dice explicitamente que el calentador puede estar bien: es la diferencia
    // util frente a ALARM_HEATER_FAULT. Lo que hay que revisar es el conector
    // del sensor de corriente, no el del calefactor.
    case ALARM_HEATER_SENSOR_FAULT:
      ES("CALEFACTOR CORTADO - SIN MEDIDA DE CONSUMO - REVISAR SENSOR DE CORRIENTE");
      FR("CHAUFFAGE COUPE - PAS DE MESURE DE COURANT - VERIFIER LE CAPTEUR");
      return "HEATER CUT - NO CURRENT READING - SERVICE THE CURRENT SENSOR";
    // En un equipo con SensorBoard, sus SHT40 SON el sensor de aire de la
    // incubadora: perder el enlace deja al PID sin variable de control y
    // dispara ALARM_AIR_SENSOR_FAULT, que corta el calefactor. El texto tiene
    // que decir eso.
    //
    // Decia justo lo contrario ("no afecta al control termico"), heredado de
    // cuando esto se diseno como telemetria auxiliar. Se vio en banco
    // (2026-09-02) leyendo la linea CTRL,ALM real: el operador habria leido
    // que no afecta al control mientras el calefactor estaba cortado.
    case ALARM_SENSORBOARD_LINK_LOST:
      ES("SIN TEMPERATURA DE AIRE - REVISAR CONEXION DEL SENSORBOARD");
      FR("PAS DE TEMPERATURE D AIR - VERIFIER LA CONNEXION SENSORBOARD");
      return "NO AIR TEMPERATURE - CHECK THE SENSORBOARD CONNECTION";
    case ALARM_SENSORBOARD_DOOR_FAULT:
      ES("SENSOR HALL POSIBLE AVERIA - NO USAR COMO ENTRADA DE CONTROL");
      FR("CAPTEUR HALL PANNE POSSIBLE - NE PAS UTILISER COMME ENTREE");
      return "HALL SENSOR POSSIBLE FAULT - DO NOT USE AS A CONTROL INPUT";
    default:
      return "alarm";
  }
}

const char *alarm_priority_mark(AlarmId id) {
  // La prioridad sale de alarm_priority(), no de una lista propia: duplicarla
  // aqui seria un segundo sitio que decir lo mismo, y en esta misma rama ya se
  // desincronizo una copia manual del conjunto de alarmas criticas.
  switch (alarm_priority(id)) {
    case ALARM_PRIORITY_HIGH:
      return "!!!";
    case ALARM_PRIORITY_MEDIUM:
      return "!!";
    default:
      return "!";
  }
}

#undef ES
#undef FR
