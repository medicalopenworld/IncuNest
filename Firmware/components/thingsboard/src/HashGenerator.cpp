// Header include.
#include "HashGenerator.h"

#if THINGSBOARD_ENABLE_OTA

HashGenerator::~HashGenerator(void) {
    free();
}

bool HashGenerator::start(mbedtls_md_type_t const & type) {
    // Clear the internal structure of any previous attempt, because if we do not the init function will not work correctly
    free();
    // Initialize the context
    mbedtls_md_init(&m_ctx);
    m_started = true; // PARCHE INCUNEST (ver PATCHES.md)
    // Choose the hash function
    bool init_result = mbedtls_md_setup(&m_ctx, mbedtls_md_info_from_type(type), 0) == 0;
    // Start the hash
    init_result = init_result && (mbedtls_md_starts(&m_ctx) == 0);
    return init_result;
}

bool HashGenerator::update(uint8_t const * const data, size_t const & length) {
    return mbedtls_md_update(&m_ctx, data, length) == 0;
}

bool HashGenerator::finish(unsigned char * hash) {
    return mbedtls_md_finish(&m_ctx, hash) == 0;
}

void HashGenerator::free() {
    // PARCHE INCUNEST (ver PATCHES.md).
    //
    // Upstream miraba los campos internos del contexto (hmac_ctx, md_ctx,
    // md_info) para no liberar un contexto que nunca se inicializo. Con la
    // mbedtls 4 que trae ESP-IDF 6 eso ya no compila: hmac_ctx solo existe
    // bajo MBEDTLS_MD_C, que IDF 6 ya no activa, y los otros dos son
    // privados. Se sustituye por una bandera propia, que es lo que la
    // condicion queria saber en realidad.
    //
    // La guarda sigue siendo necesaria, no es defensiva de mas: llamar a
    // mbedtls_md_free() sobre un contexto sin inicializar revienta, y free()
    // se invoca desde start() antes del primer mbedtls_md_init().
    if (!m_started) {
        return;
    }
    mbedtls_md_free(&m_ctx);
    m_started = false;
}

#endif // THINGSBOARD_ENABLE_OTA
