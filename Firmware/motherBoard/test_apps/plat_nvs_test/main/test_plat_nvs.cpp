/* Contrato de plat_nvs, probado sobre la NVS REAL de la placa.
 *
 * AQUI VIVEN LOS DATOS DE PACIENTE. En esta NVS estan la consigna, el modo de
 * control, el umbral del corte termico, el idioma y el numero de serie; y la
 * misma capa sirve a los perfiles de bebe. Un cambio de formato no da error:
 * deja a una unidad YA DESPLEGADA sin sus datos despues de un OTA.
 *
 * EL CONTRATO QUE HAY QUE NO ROMPER: `putFloat`/`putDouble` guardan un **BLOB**
 * de sizeof(float)/sizeof(double), no un tipo numerico de NVS. Es lo que hacia
 * Arduino y por eso lo reproduce la capa. Si alguien lo "arregla" pasandolo a
 * nvs_set_u32 o similar, compilara, los tests de escritura+lectura nuevos
 * pasaran... y las unidades en campo perderan sus valores, porque lo que hay
 * grabado en su flash es un blob.
 *
 * Por eso estos tests NO se limitan a escribir y releer: comprueban el
 * FORMATO FISICO con getBytesLength(), que es lo unico que distingue un blob
 * de un entero.
 */
#include "unity.h"
#include "platform/plat_nvs.h"

#include <math.h>
#include <string.h>

namespace {

constexpr const char *kNs = "plat_nvs_test";

void wipe()
{
    NvsPrefs p;
    p.begin(kNs, false);
    p.clear();
    p.end();
}

} // namespace

/* ── El contrato que protege los datos en campo ──────────────────────────── */

TEST_CASE("putFloat guarda un BLOB de 4 bytes, no un entero", "[plat_nvs]")
{
    wipe();
    NvsPrefs p;
    TEST_ASSERT_TRUE(p.begin(kNs, false));
    p.putFloat("f", 36.5f);

    // ESTO es lo que distingue el formato. Un nvs_set_u32 daria 0 aqui, porque
    // no seria un blob, y el test caeria — que es justo lo que se quiere.
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(float), p.getBytesLength("f"),
        "putFloat debe guardar un BLOB de sizeof(float): es el formato que ya "
        "tienen grabado las unidades desplegadas");
    p.end();
}

TEST_CASE("putDouble guarda un BLOB de 8 bytes", "[plat_nvs]")
{
    wipe();
    NvsPrefs p;
    TEST_ASSERT_TRUE(p.begin(kNs, false));
    p.putDouble("d", 37.25);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(double), p.getBytesLength("d"),
        "putDouble debe guardar un BLOB de sizeof(double)");
    p.end();
}

TEST_CASE("un float grabado se relee exacto tras cerrar y reabrir", "[plat_nvs]")
{
    wipe();
    {
        NvsPrefs w;
        TEST_ASSERT_TRUE(w.begin(kNs, false));
        w.putFloat("consigna", 36.5f);
        w.putDouble("corte", 40.0);
        w.end();                      // cerrar es lo que fuerza el commit
    }
    {
        NvsPrefs r;
        TEST_ASSERT_TRUE(r.begin(kNs, true));
        // Sin tolerancia: es el mismo patron de bits, no una conversion.
        TEST_ASSERT_EQUAL_FLOAT(36.5f, r.getFloat("consigna", -1.0f));
        TEST_ASSERT_EQUAL_DOUBLE(40.0, r.getDouble("corte", -1.0));
        r.end();
    }
}

/* ── El valor por defecto cuando la clave no existe ──────────────────────── */

TEST_CASE("una clave ausente devuelve el valor por defecto, no basura", "[plat_nvs]")
{
    wipe();
    NvsPrefs p;
    TEST_ASSERT_TRUE(p.begin(kNs, true));
    // De esto depende recapVariables(): si una clave nueva no existe todavia
    // en una unidad ya desplegada, tiene que caer al valor compilado y no a 0
    // ni a un valor inventado.
    TEST_ASSERT_EQUAL_FLOAT(12.34f, p.getFloat("no_existe", 12.34f));
    TEST_ASSERT_EQUAL_DOUBLE(56.78, p.getDouble("tampoco", 56.78));
    TEST_ASSERT_EQUAL_UINT8(7, p.getUChar("ni_esta", 7));
    p.end();
}

/* ── Solo lectura de verdad ──────────────────────────────────────────────── */

TEST_CASE("abrir en solo lectura no crea el espacio de nombres", "[plat_nvs]")
{
    NvsPrefs p;
    // begin(..., true) = readOnly. Sobre un namespace que no existe debe
    // fallar, no crearlo: crearlo al vuelo enmascararia un nombre mal escrito.
    const bool abierto = p.begin("ns_que_no_existe_jamas", true);
    if (abierto) p.end();
    TEST_ASSERT_FALSE_MESSAGE(abierto,
        "begin() en solo lectura no debe crear un namespace inexistente");
}

/* ── Cadenas, que es como se guardan los nombres de bebe ─────────────────── */

TEST_CASE("una cadena se relee igual, con su terminador", "[plat_nvs]")
{
    wipe();
    {
        NvsPrefs w;
        TEST_ASSERT_TRUE(w.begin(kNs, false));
        w.putString("nombre", "Zoe");
        w.end();
    }
    {
        NvsPrefs r;
        TEST_ASSERT_TRUE(r.begin(kNs, true));
        String s = r.getString("nombre", "");
        TEST_ASSERT_EQUAL_STRING("Zoe", s.c_str());
        r.end();
    }
}

/* ── Borrado ─────────────────────────────────────────────────────────────── */

TEST_CASE("remove() quita la clave y vuelve el valor por defecto", "[plat_nvs]")
{
    wipe();
    NvsPrefs p;
    TEST_ASSERT_TRUE(p.begin(kNs, false));
    p.putFloat("temporal", 1.5f);
    TEST_ASSERT_EQUAL_FLOAT(1.5f, p.getFloat("temporal", -1.0f));
    p.remove("temporal");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(-1.0f, p.getFloat("temporal", -1.0f),
        "tras remove() debe devolverse el valor por defecto");
    p.end();
    wipe();
}
