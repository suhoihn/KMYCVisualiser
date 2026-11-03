#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

// gcc -c runtime.c -o runtime.o

// Prints one KMY integer followed by a newline.
void kmy_print_int(int64_t x) {
    printf("%" PRId64 "\n", x); // int64_t is long on Linux, long long on Windows.
    
    // Windows sometimes buffers standard output until the program 
    // cleanly exits. For compiler dev, force it to print immediately:
    fflush(stdout); 
}

// Prints one nullable, null-terminated KMY string.
void kmy_print_string(const char *value) {
    printf("%s\n", value ? value : "(null)");
    fflush(stdout);
}

// Compares two nullable KMY strings by contents.
int64_t kmy_string_equal(const char *left, const char *right) {
    if (left == NULL || right == NULL) return left == right;
    return strcmp(left, right) == 0;
}

// Allocates and returns the concatenation of two nullable KMY strings.
const char *kmy_string_concat(const char *left, const char *right) {
    if (left == NULL) left = "";
    if (right == NULL) right = "";

    size_t leftLen = strlen(left);
    size_t rightLen = strlen(right);
    char *result = malloc(leftLen + rightLen + 1);
    if (result == NULL) return NULL;

    memcpy(result, left, leftLen);
    memcpy(result + leftLen, right, rightLen);
    result[leftLen + rightLen] = '\0';
    return result;
}

// Returns the byte length of a nullable KMY string.
int64_t kmy_string_length(const char *value) {
    return value ? (int64_t)strlen(value) : 0;
}

// Returns one unsigned string byte, or -1 when the index is invalid.
int64_t kmy_string_byte_at(const char *value, int64_t index) {
    if (value == NULL || index < 0 || (size_t)index >= strlen(value)) {
        return -1;
    }
    return (unsigned char)value[index];
}

// Allocates a one-byte KMY string, or an empty string for an invalid byte.
char *kmy_string_from_byte(int64_t value) {
    char *result = malloc(2);
    if (result == NULL) return NULL;
    if (value < 0 || value > 255) {
        result[0] = '\0';
    } else {
        result[0] = (char)(unsigned char)value;
        result[1] = '\0';
    }
    return result;
}

// Reads an entire file into a newly allocated, null-terminated string.
char *kmy_read_file(const char *path) {
    if (path == NULL) return NULL;
    FILE *file = fopen(path, "rb");
    if (file == NULL) return NULL;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    char *contents = malloc((size_t)size + 1);
    if (contents == NULL) {
        fclose(file);
        return NULL;
    }

    size_t readCount = fread(contents, 1, (size_t)size, file);
    fclose(file);
    if (readCount != (size_t)size) {
        free(contents);
        return NULL;
    }
    contents[size] = '\0';
    return contents;
}

// Writes a complete string to a file and reports whether it succeeded.
int64_t kmy_write_file(const char *path, const char *contents) {
    if (path == NULL || contents == NULL) return 0;
    FILE *file = fopen(path, "wb");
    if (file == NULL) return 0;
    size_t length = strlen(contents);
    size_t written = fwrite(contents, 1, length, file);
    int closeResult = fclose(file);
    return written == length && closeResult == 0;
}

typedef void (*KMYDestructor)(void *value);

// Precedes every RC payload; KMY values point immediately after this header.
typedef struct KMYRcHeader {
    int64_t strongCount;
    KMYDestructor destructor;
} KMYRcHeader;

// Recovers the hidden RC header from a public payload pointer.
static KMYRcHeader *kmy_rc_header(void *value) {
    return ((KMYRcHeader *)value) - 1;
}

// Allocates an uninitialized RC payload with one strong owner.
void *kmy_rc_alloc(int64_t payloadSize, KMYDestructor destructor) {
    if (payloadSize < 0 ||
        (uint64_t)payloadSize > SIZE_MAX - sizeof(KMYRcHeader)) {
        return NULL;
    }

    size_t allocationSize = sizeof(KMYRcHeader) + (size_t)payloadSize;
    KMYRcHeader *mem = malloc(allocationSize);
    if (mem == NULL) return NULL;

    *mem = (struct KMYRcHeader) {
        .strongCount = 1,
        .destructor = destructor
    };
    return mem + 1;
}

// Adds one strong owner to a nullable RC payload.
void kmy_rc_retain(void *value) {
    if (value == NULL) return;

    KMYRcHeader *header = kmy_rc_header(value);
    if (header->strongCount <= 0 || header->strongCount == INT64_MAX) {
        abort();
    }
    header->strongCount++;
}

// Removes one strong owner and destroys the payload when the count reaches zero.
void kmy_rc_release(void *value) {
    if (value == NULL) return;

    KMYRcHeader *header = kmy_rc_header(value);
    if (header->strongCount <= 0) {
        abort();
    }

    header->strongCount--;
    if (header->strongCount != 0) return;

    if (header->destructor != NULL) {
        header->destructor(value);
    }
    free(header);
}
