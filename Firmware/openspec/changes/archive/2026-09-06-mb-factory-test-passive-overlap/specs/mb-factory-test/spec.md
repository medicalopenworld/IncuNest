## ADDED Requirements

### Requirement: Orden de ejecución con solape cooperativo

La tarea `FTEST` SHALL clasificar cada test como **pasivo** (solo observa
estado cacheado o una petición asíncrona: `charger`, `env_sensor`,
`sb_status`, `sb_camera`, `gsm_at`, `gsm_sim`, `gsm_signal`, `gsm_net`,
`wifi`, `tb_provision`, `time`, y los instantáneos `sysinfo`, `ina3221`,
`skin_adc`, `hmi_link`, `nvs`, `littlefs`, `power_src`, `humid_usb`,
`sb_door`, `afe_probe`) o **activo** (`standby`, `actuators`, `fan_rpm`,
`buzzer`, `afe_spi`, `sb_env`, `sb_light`). Al arrancar la batería SHALL
emitir RUNNING de todos los pasivos y arrancar un cronómetro común; SHALL
sondearlos cada 250 ms, incluidos los tramos de espera de los activos, y
emitir el resultado final de cada uno en cuanto se resuelva o venza su plazo
propio. Los activos SHALL ejecutarse secuencialmente, en el orden de la
tabla, uno a la vez y nunca solapados entre sí. Las cascadas SHALL
respetarse (`gsm_signal`/`gsm_net` esperan a `gsm_sim`; `sb_status`,
`sb_camera` y `sb_env` esperan a `env_sensor`). Tras el último activo la
tarea SHALL seguir sondeando hasta resolver todos los pasivos. ABORT,
dead-man y tope de batería SHALL cerrar los pasivos pendientes como SKIP con
el motivo. La cota de 90 s por test aplica a los activos; los pasivos se
acotan por su plazo propio. Todo ello sin crear tareas FreeRTOS nuevas: un
único escritor hacia el HMI y ningún acceso concurrente al bus.

#### Scenario: Fábrica sin cobertura ni AP
- **WHEN** no hay SIM registrable, ni AP, ni servidor, ni hora de red
- **THEN** la batería completa termina en menos de 60 s (frente a varios
  minutos en secuencial), con los mismos resultados WARN que antes
- *(Verificación manual en banco con cronómetro.)*

#### Scenario: Los activos nunca se solapan
- **WHEN** `actuators` está midiendo corriente
- **THEN** ningún otro activo está en RUNNING y los pasivos solo leen estado
  cacheado, sin tocar el bus I2C ni los actuadores
- *(Verificación manual en banco: monitor serie, un solo activo RUNNING a la vez.)*

#### Scenario: ABORT con pasivos pendientes
- **WHEN** llega `HMI,FTEST,ABORT` con `wifi` y `tb_provision` aún en RUNNING
- **THEN** ambos terminan como SKIP `abort`, se emite `FTEST_DONE` y se
  restaura el estado seguro
- *(Verificación manual en banco.)*
