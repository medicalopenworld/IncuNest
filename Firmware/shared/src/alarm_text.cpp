#include "alarm_text.h"

#include "alarm_policy.h"

// Ingles es el idioma de reserva: un valor de idioma que no corresponda a
// ninguno de los soportados cae aqui en vez de quedarse sin texto. Hoy los
// cuatro idiomas del enum estan traducidos, y test_alarm_text lo comprueba
// alarma por alarma — el fallback es silencioso (cabe, es ASCII, no esta
// vacio), asi que sin ese test una traduccion olvidada no se nota hasta que
// alguien lee la alarma en el idioma equivocado junto a la incubadora.
#define ES(s) if (lang == SPANISH) return (s)
#define FR(s) if (lang == FRENCH) return (s)
#define PT(s) if (lang == PORTUGUESE) return (s)

const char *alarm_title_text(AlarmId id, Language lang) {
  switch (id) {
    // "FALLO" y no "CORTE": si el corte termico salta es que el termostato ha
    // fallado (201.15.4.2.1 aa lo describe asi), no que el equipo este
    // protegiendose de una situacion normal. El titulo tiene que decirle al
    // operador que esto es una averia, no un ciclo de trabajo.
    case ALARM_AIR_THERMAL_CUTOUT:
      ES("FALLO TERMICO AIRE");
      FR("PANNE THERMIQUE AIR");
      PT("FALHA TERMICA AR");
      return "AIR THERMAL FAULT";
    case ALARM_SKIN_THERMAL_CUTOUT:
      ES("FALLO TERMICO PIEL");
      FR("PANNE THERMIQUE PEAU");
      PT("FALHA TERMICA PELE");
      return "SKIN THERMAL FAULT";
    case ALARM_AIR_SENSOR_FAULT:
      ES("FALLO SENSOR AIRE");
      FR("PANNE CAPTEUR AIR");
      PT("FALHA SENSOR AR");
      return "AIR SENSOR FAULT";
    case ALARM_SKIN_SENSOR_FAULT_SKIN_MODE:
      ES("FALLO SONDA PIEL");
      FR("PANNE SONDE PEAU");
      PT("FALHA SONDA PELE");
      return "SKIN PROBE FAULT";
    case ALARM_FAN_FAILURE:
      ES("FALLO VENTILADOR");
      FR("PANNE VENTILATEUR");
      PT("FALHA VENTOINHA");
      return "FAN FAILURE";
    case ALARM_AIR_OUTLET_BLOCKED:
      ES("SALIDA DE AIRE OBSTRUIDA");
      FR("SORTIE D AIR OBSTRUEE");
      PT("SAIDA DE AR OBSTRUIDA");
      return "AIR OUTLET BLOCKED";
    // "CORTE DE CORRENTE" y no "FALHA DE REDE": en portugues "rede" se lee
    // antes como la red de datos que como la electrica, y esta alarma es
    // 201.12.3.103, corte de alimentacion.
    case ALARM_MAINS_INTERRUPTION:
      ES("CORTE DE RED");
      FR("COUPURE SECTEUR");
      PT("CORTE DE CORRENTE");
      return "MAINS INTERRUPTION";
    case ALARM_AIR_TEMP_DEVIATION_HIGH:
      ES("TEMP AIRE ALTA");
      FR("TEMP AIR ELEVEE");
      PT("TEMP AR ALTA");
      return "AIR TEMP HIGH";
    case ALARM_AIR_TEMP_DEVIATION_LOW:
      ES("TEMP AIRE BAJA");
      FR("TEMP AIR BASSE");
      PT("TEMP AR BAIXA");
      return "AIR TEMP LOW";
    case ALARM_SKIN_TEMP_DEVIATION_HIGH:
      ES("TEMP PIEL ALTA");
      FR("TEMP PEAU ELEVEE");
      PT("TEMP PELE ALTA");
      return "SKIN TEMP HIGH";
    case ALARM_SKIN_TEMP_DEVIATION_LOW:
      ES("TEMP PIEL BAJA");
      FR("TEMP PEAU BASSE");
      PT("TEMP PELE BAIXA");
      return "SKIN TEMP LOW";
    case ALARM_HEATER_FAULT:
      ES("FALLO CALENTADOR");
      FR("PANNE CHAUFFAGE");
      PT("FALHA AQUECEDOR");
      return "HEATER FAULT";
    case ALARM_SUPPLY_UNDERVOLTAGE:
      ES("TENSION BAJA");
      FR("TENSION BASSE");
      PT("TENSAO BAIXA");
      return "SUPPLY UNDERVOLTAGE";
    case ALARM_HMI_LINK_LOST:
      ES("SIN ENLACE PANTALLA");
      FR("LIAISON ECRAN PERDUE");
      PT("SEM LIGACAO AO ECRA");
      return "DISPLAY LINK LOST";
    case ALARM_SKIN_SENSOR_FAULT_AIR_MODE:
      ES("SONDA PIEL NO VALIDA");
      FR("SONDE PEAU INVALIDE");
      PT("SONDA PELE INVALIDA");
      return "SKIN PROBE UNUSABLE";
    case ALARM_HUMIDITY_DEVIATION:
      ES("DESVIACION HUMEDAD");
      FR("ECART HUMIDITE");
      PT("DESVIO DE HUMIDADE");
      return "HUMIDITY DEVIATION";
    case ALARM_HEATER_SENSOR_FAULT:
      ES("FALLO SENSOR CALENTADOR");
      FR("PANNE CAPTEUR CHAUFFAGE");
      PT("FALHA SENSOR AQUECEDOR");
      return "HEATER SENSOR FAULT";
    case ALARM_SENSORBOARD_LINK_LOST:
      ES("SIN ENLACE SENSORBOARD");
      FR("LIAISON SENSORBOARD PERDUE");
      PT("SEM LIGACAO SENSORBOARD");
      return "SENSORBOARD LINK LOST";
    case ALARM_SENSORBOARD_DOOR_FAULT:
      ES("SENSOR PUERTA SOSPECHOSO");
      FR("CAPTEUR PORTE SUSPECT");
      PT("SENSOR PORTA SUSPEITO");
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
      PT("AQUECEDOR CORTADO - VERIFICAR O BEBE - AVARIA: ALARME FIXO ATE REINICIAR");
      return "HEATER CUT - CHECK THE BABY - FAULT: ALARM STAYS UNTIL RESTART";
    case ALARM_AIR_SENSOR_FAULT:
      ES("CALEFACTOR CORTADO - SIN MEDIDA DE AIRE - REVISAR AL BEBE");
      FR("CHAUFFAGE COUPE - PAS DE MESURE D AIR - VERIFIER LE BEBE");
      PT("AQUECEDOR CORTADO - SEM MEDICAO DE AR - VERIFICAR O BEBE");
      return "HEATER CUT - NO AIR READING - CHECK THE BABY";
    case ALARM_SKIN_SENSOR_FAULT_SKIN_MODE:
      ES("CALEFACTOR CORTADO - REVISAR SONDA O PASAR A MODO AIRE");
      FR("CHAUFFAGE COUPE - VERIFIER LA SONDE OU PASSER EN MODE AIR");
      PT("AQUECEDOR CORTADO - VERIFICAR A SONDA OU PASSAR A MODO AR");
      return "HEATER CUT - CHECK PROBE OR SWITCH TO AIR MODE";
    case ALARM_FAN_FAILURE:
      ES("CALEFACTOR CORTADO - SIN CIRCULACION DE AIRE - REVISAR AL BEBE Y EL EQUIPO");
      FR("CHAUFFAGE COUPE - PAS DE CIRCULATION D AIR - VERIFIER LE BEBE ET L APPAREIL");
      PT("AQUECEDOR CORTADO - SEM CIRCULACAO DE AR - VERIFICAR O BEBE E O EQUIPAMENTO");
      return "HEATER CUT - NO AIR CIRCULATION - CHECK THE BABY AND SERVICE THE UNIT";
    case ALARM_AIR_OUTLET_BLOCKED:
      ES("CALEFACTOR CORTADO - DESPEJAR LA SALIDA DE AIRE");
      FR("CHAUFFAGE COUPE - DEGAGER LA SORTIE D AIR");
      PT("AQUECEDOR CORTADO - DESOBSTRUIR A SAIDA DE AR");
      return "HEATER CUT - CLEAR THE AIR OUTLET";
    case ALARM_MAINS_INTERRUPTION:
      ES("REVISAR LA CONEXION A LA RED");
      FR("VERIFIER LE RACCORDEMENT SECTEUR");
      PT("VERIFICAR A LIGACAO A REDE ELETRICA");
      return "CHECK THE MAINS CONNECTION";
    case ALARM_AIR_TEMP_DEVIATION_HIGH:
      ES("CALEFACTOR CORTADO - AIRE MAS DE 3 C SOBRE LA CONSIGNA");
      FR("CHAUFFAGE COUPE - AIR A PLUS DE 3 C AU DESSUS DE LA CONSIGNE");
      PT("AQUECEDOR CORTADO - AR MAIS DE 3 C ACIMA DO VALOR DEFINIDO");
      return "HEATER CUT - AIR OVER 3 C ABOVE SETPOINT";
    case ALARM_AIR_TEMP_DEVIATION_LOW:
      ES("AIRE MAS DE 3 C BAJO LA CONSIGNA");
      FR("AIR A PLUS DE 3 C SOUS LA CONSIGNE");
      PT("AR MAIS DE 3 C ABAIXO DO VALOR DEFINIDO");
      return "AIR OVER 3 C BELOW SETPOINT";
    case ALARM_SKIN_TEMP_DEVIATION_HIGH:
      ES("CALEFACTOR CORTADO - PIEL MAS DE 1 C SOBRE LA CONSIGNA");
      FR("CHAUFFAGE COUPE - PEAU A PLUS DE 1 C AU DESSUS DE LA CONSIGNE");
      PT("AQUECEDOR CORTADO - PELE MAIS DE 1 C ACIMA DO VALOR DEFINIDO");
      return "HEATER CUT - SKIN OVER 1 C ABOVE SETPOINT";
    case ALARM_SKIN_TEMP_DEVIATION_LOW:
      ES("PIEL MAS DE 1 C BAJO LA CONSIGNA");
      FR("PEAU A PLUS DE 1 C SOUS LA CONSIGNE");
      PT("PELE MAIS DE 1 C ABAIXO DO VALOR DEFINIDO");
      return "SKIN OVER 1 C BELOW SETPOINT";
    case ALARM_HEATER_FAULT:
      ES("EL EQUIPO NO CALIENTA - REVISAR AL BEBE Y EL EQUIPO");
      FR("L APPAREIL NE CHAUFFE PAS - VERIFIER LE BEBE ET L APPAREIL");
      PT("O EQUIPAMENTO NAO AQUECE - VERIFICAR O BEBE E O EQUIPAMENTO");
      return "UNIT NOT HEATING - CHECK THE BABY AND SERVICE THE UNIT";
    case ALARM_SUPPLY_UNDERVOLTAGE:
      ES("REVISAR FUENTE Y CABLEADO");
      FR("VERIFIER L ALIMENTATION ET LE CABLAGE");
      PT("VERIFICAR A FONTE E A CABLAGEM");
      return "CHECK SUPPLY AND WIRING";
    case ALARM_HMI_LINK_LOST:
      ES("DATOS NO FIABLES - REVISAR AL BEBE");
      FR("DONNEES NON FIABLES - VERIFIER LE BEBE");
      PT("DADOS NAO FIAVEIS - VERIFICAR O BEBE");
      return "DATA UNRELIABLE - CHECK THE BABY";
    case ALARM_SKIN_SENSOR_FAULT_AIR_MODE:
      ES("SIN TEMP DE PIEL - MODO AIRE ACTIVO");
      FR("PAS DE TEMP PEAU - MODE AIR ACTIF");
      PT("SEM TEMP DE PELE - MODO AR ATIVO");
      return "NO SKIN TEMP - AIR MODE ACTIVE";
    case ALARM_HUMIDITY_DEVIATION:
      ES("REVISAR DEPOSITO DE AGUA");
      FR("VERIFIER LE RESERVOIR D EAU");
      PT("VERIFICAR O DEPOSITO DE AGUA");
      return "CHECK THE WATER TANK";
    // Dice explicitamente que el calentador puede estar bien: es la diferencia
    // util frente a ALARM_HEATER_FAULT. Lo que hay que revisar es el conector
    // del sensor de corriente, no el del calefactor.
    case ALARM_HEATER_SENSOR_FAULT:
      ES("CALEFACTOR CORTADO - SIN MEDIDA DE CONSUMO - REVISAR SENSOR DE CORRIENTE");
      FR("CHAUFFAGE COUPE - PAS DE MESURE DE COURANT - VERIFIER LE CAPTEUR");
      PT("AQUECEDOR CORTADO - SEM MEDICAO DE CORRENTE - VERIFICAR O SENSOR");
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
      PT("SEM TEMPERATURA DE AR - VERIFICAR A LIGACAO DO SENSORBOARD");
      return "NO AIR TEMPERATURE - CHECK THE SENSORBOARD CONNECTION";
    case ALARM_SENSORBOARD_DOOR_FAULT:
      ES("SENSOR HALL POSIBLE AVERIA - NO USAR COMO ENTRADA DE CONTROL");
      FR("CAPTEUR HALL PANNE POSSIBLE - NE PAS UTILISER COMME ENTREE");
      PT("SENSOR HALL POSSIVEL AVARIA - NAO USAR COMO ENTRADA DE CONTROLO");
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
#undef PT
