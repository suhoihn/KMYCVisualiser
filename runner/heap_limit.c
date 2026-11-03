/* GNU ld --wrap applies to both generated KMY assembly and runtime.c.
 * This is a KMY allocation quota, not a sandbox for unsafe native code. */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef KMY_HEAP_BYTES
#define KMY_HEAP_BYTES (4u * 1024u * 1024u)
#endif

typedef union {
    max_align_t alignment;
    size_t charge;
} Allocation;

static size_t live_bytes;
void *__real_malloc(size_t size);
void __real_free(void *ptr);

static void fail_quota(void) {
    static const char message[] = "KMY program exceeded its heap limit.\n";
    (void)write(STDERR_FILENO, message, sizeof(message) - 1);
    _exit(86); /* Reported by job.py as heap_limit. */
}

void *__wrap_malloc(size_t size) {
    const size_t remaining = (size_t)KMY_HEAP_BYTES - live_bytes;
    if (remaining < sizeof(Allocation) || size > remaining - sizeof(Allocation)) {
        fail_quota();
    }
    const size_t charge = sizeof(Allocation) + size;
    Allocation *allocation = __real_malloc(charge);
    if (!allocation) {
        static const char message[] = "KMY program could not allocate memory.\n";
        (void)write(STDERR_FILENO, message, sizeof(message) - 1);
        _exit(87);
    }
    allocation->charge = charge;
    live_bytes += charge;
    return allocation + 1;
}

void *__wrap_calloc(size_t count, size_t size) {
    if (size && count > SIZE_MAX / size) {
        fail_quota();
    }
    const size_t bytes = count * size;
    return memset(__wrap_malloc(bytes), 0, bytes);
}

void __wrap_free(void *ptr) {
    if (!ptr) return;
    Allocation *allocation = (Allocation *)ptr - 1;
    live_bytes -= allocation->charge;
    __real_free(allocation);
}
