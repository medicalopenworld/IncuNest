/* Contrato de plat_fs, probado sobre el LittleFS REAL de la placa.
 *
 * POR QUE ESTO NO PUEDE VIVIR EN tools/host_tests: lo que se prueba no es
 * logica pura, es el comportamiento de la capa de compatibilidad sobre un
 * sistema de ficheros montado. En host no hay particion "spiffs" que montar.
 *
 * POR QUE EXISTE: el 2026-09-14 `FsFile::name()` devolvia la RUTA COMPLETA
 * ("/littlefs/pox_1.csv") en vez del nombre pelado ("pox_1.csv"), que es lo
 * que espera todo el firmware heredado de Arduino. No dio ningun error:
 * simplemente dejo mudos tres subsistemas a la vez —la limpieza de ventanas de
 * PPG, el tope de retencion de logs de crash y el historico de pesos—, la
 * particion se lleno y la placa acabo abortando. Arreglado en d324f9e.
 *
 * Cada TEST_CASE de aqui esta escrito para FALLAR si se reintroduce ese
 * defecto. Un test que pasa con el bug puesto no sirve de nada.
 */
#include "unity.h"
#include "platform/plat_fs.h"

#include <string.h>

namespace {

constexpr const char *kDir  = "/plat_fs_test";
constexpr const char *kFile = "/plat_fs_test/muestra.csv";

void ensureMounted()
{
    // begin(true): formatea si no se puede montar. En un test_app es lo que
    // queremos; en produccion tambien se llama asi (initDriveUpload).
    TEST_ASSERT_TRUE_MESSAGE(LittleFS.begin(true), "no se pudo montar LittleFS");
}

void writeSample()
{
    LittleFS.mkdir(kDir);
    FsFile f = LittleFS.open(kFile, "w", true);
    TEST_ASSERT_TRUE_MESSAGE((bool)f, "no se pudo crear el fichero de muestra");
    f.print("hola");
    f.close();
}

void cleanup()
{
    LittleFS.remove(kFile);
    LittleFS.rmdir(kDir);
}

} // namespace

/* ── El defecto de d324f9e, fijado ───────────────────────────────────────── */

TEST_CASE("name() devuelve el nombre pelado, no la ruta", "[plat_fs]")
{
    ensureMounted();
    writeSample();

    FsFile dir = LittleFS.open(kDir);
    TEST_ASSERT_TRUE(dir && dir.isDirectory());

    bool visto = false;
    FsFile f;
    while ((f = dir.openNextFile())) {
        if (f.isDirectory()) continue;
        const char *n = f.name();
        TEST_ASSERT_NOT_NULL(n);
        // Esto es lo que rompio: con la ruta completa, n empezaria por '/' y
        // el strncmp(n, "pox_", 4) de DriveUpload no casaria nunca.
        TEST_ASSERT_EQUAL_STRING_MESSAGE("muestra.csv", n,
            "name() debe devolver el nombre pelado; devolver la ruta deja mudos "
            "los strncmp() de DriveUpload y baby_profile_store");
        TEST_ASSERT_NOT_EQUAL_MESSAGE('/', n[0],
            "name() no debe empezar por '/'");
        TEST_ASSERT_NULL_MESSAGE(strchr(n, '/'),
            "name() no debe contener ninguna barra");
        visto = true;
    }
    dir.close();
    TEST_ASSERT_TRUE_MESSAGE(visto, "openNextFile() no devolvio el fichero creado");

    cleanup();
}

TEST_CASE("path() si devuelve la ruta completa", "[plat_fs]")
{
    ensureMounted();
    writeSample();

    FsFile f = LittleFS.open(kFile, "r");
    TEST_ASSERT_TRUE((bool)f);
    const char *p = f.path();
    TEST_ASSERT_NOT_NULL(p);
    // Quien necesite la ruta la tiene aqui: por eso name() puede ser el nombre
    // pelado sin perder informacion.
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(p, "muestra.csv"),
        "path() debe terminar en el nombre del fichero");
    TEST_ASSERT_EQUAL_MESSAGE('/', p[0], "path() debe ser absoluta");
    f.close();

    cleanup();
}

/* ── El prefijo de montaje ───────────────────────────────────────────────── */

TEST_CASE("las rutas del firmware no llevan el prefijo de montaje", "[plat_fs]")
{
    ensureMounted();
    writeSample();

    // El firmware abre "/baby_history.log", no "/littlefs/baby_history.log".
    // La capa antepone el prefijo por dentro. Si esto cambiara, una unidad ya
    // desplegada dejaria de encontrar sus perfiles de bebe tras el OTA.
    TEST_ASSERT_TRUE_MESSAGE(LittleFS.exists(kFile),
        "exists() debe aceptar la ruta SIN el prefijo de montaje");

    FsFile f = LittleFS.open(kFile, "r");
    TEST_ASSERT_TRUE_MESSAGE((bool)f,
        "open() debe aceptar la ruta SIN el prefijo de montaje");
    f.close();

    cleanup();
}

/* ── Contabilidad de espacio ─────────────────────────────────────────────── */

TEST_CASE("totalBytes/usedBytes son coherentes", "[plat_fs]")
{
    ensureMounted();

    const size_t total = LittleFS.totalBytes();
    const size_t used  = LittleFS.usedBytes();

    TEST_ASSERT_GREATER_THAN_MESSAGE(0, total, "totalBytes() no puede ser 0");
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(total, used,
        "usedBytes() no puede superar a totalBytes()");

    // De estas dos cifras depende el freno que impide que las ventanas de PPG
    // llenen la particion (DRIVE_MIN_FREE_BYTES). Si devolvieran 0, el freno
    // no actuaria nunca.
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, total - used,
        "no queda espacio libre: el test no puede continuar con la particion llena");
}

/* ── Borrado, que es lo que la limpieza de arranque necesita ─────────────── */

TEST_CASE("remove() borra un fichero listado por openNextFile()", "[plat_fs]")
{
    ensureMounted();
    writeSample();

    // Reproduce el patron exacto de initDriveUpload(): listar un directorio,
    // quedarse con el nombre y borrarlo componiendo "/" + nombre. Con name()
    // devolviendo la ruta, esto componia "//littlefs/..." y fallaba en
    // silencio.
    FsFile dir = LittleFS.open(kDir);
    char nombre[64] = {0};
    FsFile f;
    while ((f = dir.openNextFile())) {
        if (f.isDirectory()) continue;
        snprintf(nombre, sizeof(nombre), "%s", f.name());
        f.close();
        break;
    }
    dir.close();
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, nombre[0], "no se obtuvo ningun nombre");

    String ruta = String(kDir) + "/" + nombre;
    TEST_ASSERT_TRUE_MESSAGE(LittleFS.remove(ruta),
        "remove() fallo sobre la ruta compuesta a partir de name()");
    TEST_ASSERT_FALSE_MESSAGE(LittleFS.exists(kFile),
        "el fichero sigue existiendo despues de remove()");

    LittleFS.rmdir(kDir);
}
