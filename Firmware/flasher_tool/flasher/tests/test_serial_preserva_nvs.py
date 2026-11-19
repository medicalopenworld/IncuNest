"""Reflashear una unidad no puede borrarle la identidad.

`flasher_config.json` trae `force_serial_number: true`, asi que el flasher
pide el serial SIEMPRE, tambien al reflashear una placa que ya viene de
fabrica. Con eso, flash_board escribia la imagen de NVS que genera
nvs_gen.generate_serial_nvs, que es del tamano de la particion entera y solo
lleva mb_cfg/serial: se llevaba por delante el token de ThingsBoard y la marca
de provisionado (mb_gprs) y las credenciales WiFi (mb_wifi).

Consecuencia observada el 2026-09-20: una unidad reflasheada con la
herramienta dejo de conectarse a ThingsBoard. Sin WiFi guardado cae al SSID
por defecto compilado, que fuera del banco no existe; y aunque llegue a la
red, el perfil `IncuNest` provisiona con ALLOW_CREATE_NEW_DEVICES, que
rechaza un nombre ya existente. El firmware reintenta como IncuNest-<n>_1.._3
(PROVISION_MAX_RETRIES=3) y se rinde: con el serial 1 esos cuatro nombres ya
estaban cogidos en el servidor.

La regla es simple: solo se escribe la NVS si el serial cambia de verdad.
"""
import pytest

from flasher import serial_to_write


def test_mismo_serial_no_escribe_la_nvs():
    assert serial_to_write(350, 350) is None


def test_serial_distinto_escribe():
    assert serial_to_write(350, 351) == 351


# Lo que no se ha podido leer no se puede dar por bueno: una placa virgen y una
# lectura fallida son el mismo caso, y ahi si hay que escribir (y avisar).
def test_serial_desconocido_escribe():
    assert serial_to_write(None, 350) == 350


# El 0 es un serial valido (0-9999) y no debe confundirse con "sin serial".
def test_el_cero_es_un_serial_como_otro_cualquiera():
    assert serial_to_write(0, 0) is None
    assert serial_to_write(0, 1) == 1
    assert serial_to_write(None, 0) == 0


@pytest.mark.parametrize('serial', [0, 1, 350, 9999])
def test_reflasheo_idempotente(serial):
    assert serial_to_write(serial, serial) is None
