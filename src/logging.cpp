#include <cstdio>

#include "phylum.h"
#include "task_stack.h"

namespace phylum {

task_stack tasks;

int32_t debugf(const char *f, ...) {
    va_list args;
    va_start(args, f);
    printf("%s ", tasks.get());
    vprintf(f, args);
    va_end(args);
    return 0;
}

void fk_dump_memory(const char *prefix, const uint8_t *p, size_t size, ...) {
    va_list args;
    va_start(args, size);

    vprintf(prefix, args);
    for (auto i = 0u; i < size; ++i) {
        printf("%02x ", p[i]);
        if ((i + 1) % 32 == 0) {
            if (i + 1 < size) {
                printf("\n");
                vprintf(prefix, args);
            }
        }
    }
    printf(" (%zu bytes)\n", size);

    va_end(args);
}

} // namespace phylum
