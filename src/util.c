#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

static int read_stream_alloc(FILE *fp, char **out, size_t *len) {
    char buffer[4096];
    size_t cap = 0;
    size_t used = 0;
    char *data = NULL;

    while (!feof(fp)) {
        size_t n = fread(buffer, 1, sizeof(buffer), fp);
        if (n > 0) {
            if (used + n + 1 > cap) {
                size_t next = (cap == 0) ? 8192 : cap * 2;
                char *tmp;
                while (next < used + n + 1) {
                    next *= 2;
                }
                tmp = realloc(data, next);
                if (tmp == NULL) {
                    free(data);
                    return -1;
                }
                data = tmp;
                cap = next;
            }
            memcpy(data + used, buffer, n);
            used += n;
        }
        if (ferror(fp)) {
            free(data);
            return -1;
        }
    }

    if (data == NULL) {
        data = calloc(1, 1);
        if (data == NULL) {
            return -1;
        }
    }

    data[used] = '\0';
    *out = data;
    *len = used;
    return 0;
}

static int mime_is_safe(const char *mime_type) {
    size_t i;
    if (mime_type == NULL || mime_type[0] == '\0') {
        return 0;
    }
    for (i = 0; mime_type[i] != '\0'; i++) {
        unsigned char c = (unsigned char)mime_type[i];
        if (isalnum(c) || c == '/' || c == '.' || c == '+' || c == '-' || c == ';' || c == '=') {
            continue;
        }
        return 0;
    }
    return 1;
}

static char *dup_string(const char *src) {
    size_t n;
    char *out;

    if (src == NULL) {
        return NULL;
    }
    n = strlen(src);
    out = malloc(n + 1);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, src, n + 1);
    return out;
}

int util_read_wl_paste(char **out, size_t *len) {
    FILE *fp;
    int rc;

    if (out == NULL || len == NULL) {
        return -1;
    }

    fp = popen("wl-paste", "r");
    if (fp == NULL) {
        return -1;
    }

    rc = read_stream_alloc(fp, out, len);
    pclose(fp);
    return rc;
}

int util_read_wl_paste_type(const char *mime_type, char **out, size_t *len) {
    char command[256];
    FILE *fp;
    int rc;

    if (out == NULL || len == NULL || !mime_is_safe(mime_type)) {
        return -1;
    }

    snprintf(command, sizeof(command), "wl-paste --type %s", mime_type);
    fp = popen(command, "r");
    if (fp == NULL) {
        return -1;
    }

    rc = read_stream_alloc(fp, out, len);
    pclose(fp);
    return rc;
}

int util_read_stdin(char **out, size_t *len) {
    if (out == NULL || len == NULL) {
        return -1;
    }
    return read_stream_alloc(stdin, out, len);
}

int util_write_wl_copy(const char *data, size_t len) {
    FILE *fp = popen("wl-copy", "w");
    if (fp == NULL) {
        return -1;
    }

    if (len > 0 && data != NULL) {
        if (fwrite(data, 1, len, fp) != len) {
            pclose(fp);
            return -1;
        }
    }

    if (pclose(fp) == -1) {
        return -1;
    }

    return 0;
}

int util_write_wl_copy_type(const char *mime_type, const char *data, size_t len) {
    char command[256];
    FILE *fp;

    if (!mime_is_safe(mime_type)) {
        return -1;
    }

    snprintf(command, sizeof(command), "wl-copy --type %s", mime_type);
    fp = popen(command, "w");
    if (fp == NULL) {
        return -1;
    }

    if (len > 0 && data != NULL) {
        if (fwrite(data, 1, len, fp) != len) {
            pclose(fp);
            return -1;
        }
    }

    if (pclose(fp) == -1) {
        return -1;
    }

    return 0;
}

int util_list_clipboard_types(char ***types, size_t *count) {
    FILE *fp;
    char line[256];
    char **arr = NULL;
    size_t arr_count = 0;
    size_t arr_cap = 0;

    if (types == NULL || count == NULL) {
        return -1;
    }

    *types = NULL;
    *count = 0;

    fp = popen("wl-paste --list-types", "r");
    if (fp == NULL) {
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        size_t n = strlen(line);
        char *entry;

        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
            line[--n] = '\0';
        }
        if (n == 0) {
            continue;
        }

        if (arr_count == arr_cap) {
            size_t next = (arr_cap == 0) ? 8 : arr_cap * 2;
            char **tmp = realloc(arr, next * sizeof(char *));
            if (tmp == NULL) {
                util_free_string_list(arr, arr_count);
                pclose(fp);
                return -1;
            }
            arr = tmp;
            arr_cap = next;
        }

        entry = dup_string(line);
        if (entry == NULL) {
            util_free_string_list(arr, arr_count);
            pclose(fp);
            return -1;
        }
        arr[arr_count++] = entry;
    }

    pclose(fp);
    *types = arr;
    *count = arr_count;
    return 0;
}

void util_free_string_list(char **items, size_t count) {
    size_t i;
    if (items == NULL) {
        return;
    }
    for (i = 0; i < count; i++) {
        free(items[i]);
    }
    free(items);
}

int util_run_shell(const char *command) {
    int status;

    if (command == NULL) {
        return -1;
    }

    status = system(command);
    if (status == -1) {
        return -1;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    return -1;
}
