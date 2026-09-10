"""Seleccion del entorno de PlatformIO del que el flasher copia los binarios.

Lo que se protege aqui no es una preferencia de comodidad: los entornos
`*_factory` compilan la activacion de SIM del test de fabrica y por tanto
llevan ONOMONDO_API_KEY dentro del firmware.bin. `data/firmware/` acaba como
asset de GitHub Releases y el repo es publico, asi que empaquetar ese binario
publicaria la credencial que controla TODAS las SIM de la organizacion.
"""
import os
import time

import pytest

from main import FACTORY_ENV_SUFFIX, pick_pio_env_dir


def _env(pio_dir, name, mtime=None):
    d = pio_dir / name
    d.mkdir(parents=True)
    f = d / 'firmware.bin'
    f.write_bytes(b'\xff' * 16)
    if mtime is not None:
        os.utime(f, (mtime, mtime))
    return d


@pytest.fixture
def pio_dir(tmp_path):
    return tmp_path / '.pio' / 'build'


def test_prefiere_la_revision_de_hardware_mas_alta(pio_dir):
    _env(pio_dir, 'IncuNest_V17')
    _env(pio_dir, 'IncuNest_V18')
    assert pick_pio_env_dir(pio_dir).name == 'IncuNest_V18'


def test_a_igualdad_de_version_gana_el_build_mas_reciente(pio_dir):
    now = time.time()
    _env(pio_dir, 'IncuNest_V18', mtime=now - 3600)
    _env(pio_dir, 'IncuNest_V18b', mtime=now)
    assert pick_pio_env_dir(pio_dir).name == 'IncuNest_V18b'


# El caso que de verdad importa. IncuNest_V18_factory casa con [Vv](\d+) y
# puntua 18 igual que IncuNest_V18, asi que antes de existir el filtro el
# desempate era el mtime: se empaquetaba el que se hubiera compilado mas tarde.
def test_nunca_elige_un_entorno_de_fabrica_aunque_sea_el_mas_reciente(pio_dir):
    now = time.time()
    _env(pio_dir, 'IncuNest_V18', mtime=now - 3600)
    _env(pio_dir, 'IncuNest_V18' + FACTORY_ENV_SUFFIX, mtime=now)
    assert pick_pio_env_dir(pio_dir).name == 'IncuNest_V18'


# Ni aunque el de fabrica sea de una revision mas alta: la version no puede
# comprar el derecho a publicar la clave.
def test_no_elige_fabrica_ni_con_version_mayor(pio_dir):
    _env(pio_dir, 'IncuNest_V17')
    _env(pio_dir, 'IncuNest_V18' + FACTORY_ENV_SUFFIX)
    assert pick_pio_env_dir(pio_dir).name == 'IncuNest_V17'


# Mejor no copiar nada que copiar el binario de fabrica: sin candidato valido
# el llamante deja los binarios que ya hubiera en data/firmware/.
def test_sin_candidatos_validos_devuelve_none(pio_dir):
    _env(pio_dir, 'IncuNest_V18' + FACTORY_ENV_SUFFIX)
    assert pick_pio_env_dir(pio_dir) is None


def test_directorio_vacio_devuelve_none(pio_dir):
    pio_dir.mkdir(parents=True)
    assert pick_pio_env_dir(pio_dir) is None


# Un entorno sin V<n> en el nombre puntua -1: vale como ultimo recurso, pero
# nunca por delante de uno con revision.
def test_entorno_sin_version_va_al_final(pio_dir):
    _env(pio_dir, 'generico')
    _env(pio_dir, 'IncuNest_V17')
    assert pick_pio_env_dir(pio_dir).name == 'IncuNest_V17'
