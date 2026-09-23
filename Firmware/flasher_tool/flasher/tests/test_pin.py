"""Ancla de binarios: data/firmware/<placa>/NO_SOBRESCRIBIR.txt.

Dos rutas rellenan data/firmware/ sin preguntar --la descarga de la ultima
release y el boton "Actualizar binarios locales"-- y las dos dejan la pantalla
del flasher exactamente igual. Una tanda entera se puede flashear con un
firmware que no es el que se creia:

  - el build de fabrica (ONOMONDO_API_KEY dentro, nunca publicado) sustituido
    por el de distribucion, y las placas salen sin activacion de SIM;
  - un build recien compilado y aun sin release sustituido por el de la ultima
    release, que es mas viejo.

El marcador corta las dos rutas. Estos tests protegen esa garantia.
"""
import pytest

from main import FACTORY_ENV_SUFFIX, pick_pio_env_dir
from updater import PIN_MARKER, read_pin, pinned_folders


def _env(pio_dir, name):
    d = pio_dir / name
    d.mkdir(parents=True)
    (d / 'firmware.bin').write_bytes(b'\xff' * 16)
    return d


def _pin(firmware_base, folder, body):
    d = firmware_base / folder
    d.mkdir(parents=True, exist_ok=True)
    (d / PIN_MARKER).write_text(body, encoding='utf-8')


@pytest.fixture
def pio_dir(tmp_path):
    return tmp_path / '.pio' / 'build'


# ------------------------------------------------------------------ #
# Lectura del marcador
# ------------------------------------------------------------------ #

def test_sin_marcador_no_hay_ancla(tmp_path):
    (tmp_path / 'motherboard').mkdir()
    assert read_pin(tmp_path, 'motherboard') is None
    assert pinned_folders(tmp_path) == {}


def test_el_marcador_declara_el_entorno(tmp_path):
    _pin(tmp_path, 'motherboard', 'env=IncuNest_V18_factory\n\nlo que sea\n')
    assert read_pin(tmp_path, 'motherboard') == 'IncuNest_V18_factory'
    assert pinned_folders(tmp_path) == {'motherboard': 'IncuNest_V18_factory'}


# Un marcador puesto a mano y sin la linea env= sigue anclando: quien lo
# escribio quiso decir "no toques esto", y esa intencion pesa mas que la
# comodidad de poder refrescar.
def test_marcador_sin_entorno_ancla_igual(tmp_path):
    _pin(tmp_path, 'display_hmi', 'no me sobrescribas\n')
    assert read_pin(tmp_path, 'display_hmi') == ''
    assert pinned_folders(tmp_path) == {'display_hmi': ''}


def test_ancla_varias_placas_a_la_vez(tmp_path):
    _pin(tmp_path, 'motherboard', 'env=IncuNest_V18_factory\n')
    _pin(tmp_path, 'display_hmi', 'env=main\n')
    assert pinned_folders(tmp_path) == {
        'motherboard': 'IncuNest_V18_factory',
        'display_hmi': 'main',
    }


# ------------------------------------------------------------------ #
# Seleccion del entorno con ancla
# ------------------------------------------------------------------ #

# El ancla es la UNICA forma de que el flasher copie un entorno de fabrica, y
# solo porque alguien lo ha escrito con ese nombre en el marcador.
def test_con_ancla_se_copia_el_entorno_de_fabrica(pio_dir):
    _env(pio_dir, 'IncuNest_V18')
    factory = 'IncuNest_V18' + FACTORY_ENV_SUFFIX
    _env(pio_dir, factory)
    assert pick_pio_env_dir(pio_dir, factory).name == factory


# Lo contrario de lo que hacia antes: si el entorno anclado no esta compilado,
# no se cae al de distribucion. Ese silencio --copiar otro firmware y decir
# "copiado"-- es justo el fallo que el ancla existe para evitar.
def test_entorno_anclado_sin_compilar_no_cae_al_de_distribucion(pio_dir):
    _env(pio_dir, 'IncuNest_V18')
    assert pick_pio_env_dir(pio_dir, 'IncuNest_V18' + FACTORY_ENV_SUFFIX) is None


def test_sin_ancla_el_comportamiento_no_cambia(pio_dir):
    _env(pio_dir, 'IncuNest_V18')
    _env(pio_dir, 'IncuNest_V18' + FACTORY_ENV_SUFFIX)
    assert pick_pio_env_dir(pio_dir).name == 'IncuNest_V18'
    assert pick_pio_env_dir(pio_dir, None).name == 'IncuNest_V18'
    assert pick_pio_env_dir(pio_dir, '').name == 'IncuNest_V18'
