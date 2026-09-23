#include "sensorBoard_json.h"

size_t sb_json_escape(char *dst, size_t dst_size, const char *src, size_t src_max)
{
    if (dst == NULL || dst_size == 0) {
        return 0;
    }

    size_t pos = 0;
    for (size_t i = 0; src != NULL && src[i] != '\0' && i < src_max; i++) {
        char c = src[i];
        int needs_escape = (c == '"' || c == '\\');
        size_t needed = needs_escape ? 2u : 1u;
        if (pos + needed >= dst_size) {
            break;
        }
        if (needs_escape) {
            dst[pos++] = '\\';
        }
        dst[pos++] = ((unsigned char)c < 0x20) ? ' ' : c;
    }
    dst[pos] = '\0';
    return pos;
}
