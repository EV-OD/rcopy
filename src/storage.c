#include "storage.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    char *id;
    char *mime_type;
    char *ext;
} IndexMeta;

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

static int ensure_dir(const char *path) {
    struct stat st;

    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }

    if (mkdir(path, 0755) == 0) {
        return 0;
    }

    return (errno == EEXIST) ? 0 : -1;
}

static int read_file_alloc(const char *path, char **out, size_t *len) {
    FILE *fp;
    long size;
    char *buf;
    size_t got;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }

    buf = malloc((size_t)size + 1);
    if (buf == NULL) {
        fclose(fp);
        return -1;
    }

    got = fread(buf, 1, (size_t)size, fp);
    fclose(fp);
    if (got != (size_t)size) {
        free(buf);
        return -1;
    }

    buf[got] = '\0';
    *out = buf;
    *len = got;
    return 0;
}

static const char *ext_from_mime(const char *mime_type) {
    if (mime_type == NULL) {
        return "bin";
    }
    if (strncmp(mime_type, "image/png", 9) == 0) {
        return "png";
    }
    if (strncmp(mime_type, "image/jpeg", 10) == 0) {
        return "jpg";
    }
    if (strncmp(mime_type, "image/webp", 10) == 0) {
        return "webp";
    }
    if (strncmp(mime_type, "text/", 5) == 0) {
        return "txt";
    }
    return "bin";
}

static int append_index(const RcopyConfig *cfg, const char *id, const char *mime_type, const char *ext) {
    FILE *fp = fopen(cfg->index_file, "a");
    if (fp == NULL) {
        return -1;
    }
    fprintf(fp, "%s\t%s\t%s\n", id, mime_type, ext);
    fclose(fp);
    return 0;
}

static int parse_index_line(const char *line_in, char **id, char **mime_type, char **ext) {
    char line[512];
    char *a;
    char *b;
    char *c;

    snprintf(line, sizeof(line), "%s", line_in);
    a = strtok(line, "\t");
    b = strtok(NULL, "\t");
    c = strtok(NULL, "\t");

    if (a == NULL) {
        return -1;
    }

    if (b == NULL || c == NULL) {
        *id = dup_string(a);
        *mime_type = dup_string("text/plain");
        *ext = dup_string("txt");
        return (*id && *mime_type && *ext) ? 0 : -1;
    }

    *id = dup_string(a);
    *mime_type = dup_string(b);
    *ext = dup_string(c);
    return (*id && *mime_type && *ext) ? 0 : -1;
}

static int build_item_path(const RcopyConfig *cfg, const char *id, const char *ext, char *out, size_t out_size) {
    if (id == NULL || ext == NULL || out == NULL || out_size == 0) {
        return -1;
    }
    snprintf(out, out_size, "%s/%s.%s", cfg->items_dir, id, ext);
    return 0;
}

int storage_init(const RcopyConfig *cfg) {
    FILE *fp;
    if (ensure_dir(cfg->data_dir) != 0) {
        return -1;
    }
    if (ensure_dir(cfg->items_dir) != 0) {
        return -1;
    }

    fp = fopen(cfg->index_file, "a");
    if (fp == NULL) {
        return -1;
    }
    fclose(fp);
    return 0;
}

int storage_save(const RcopyConfig *cfg, const char *mime_type, const char *data, size_t len) {
    struct timespec ts;
    const char *ext;
    char id[64];
    char path[PATH_MAX];
    FILE *fp;

    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return -1;
    }

    ext = ext_from_mime(mime_type);
    snprintf(id, sizeof(id), "%lld%09ld", (long long)ts.tv_sec, ts.tv_nsec);
    build_item_path(cfg, id, ext, path, sizeof(path));

    fp = fopen(path, "wb");
    if (fp == NULL) {
        return -1;
    }

    if (len > 0 && data != NULL) {
        if (fwrite(data, 1, len, fp) != len) {
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);
    return append_index(cfg, id, mime_type, ext);
}

int storage_get_last(const RcopyConfig *cfg, ClipItem *item) {
    FILE *fp;
    char line[512];
    char last[512] = {0};
    char path[PATH_MAX];
    char *id = NULL;
    char *mime_type = NULL;
    char *ext = NULL;

    if (item == NULL) {
        return -1;
    }

    item->id = NULL;
    item->mime_type = NULL;
    item->content = NULL;
    item->content_len = 0;

    fp = fopen(cfg->index_file, "r");
    if (fp == NULL) {
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
            line[--n] = '\0';
        }
        if (n > 0) {
            snprintf(last, sizeof(last), "%s", line);
        }
    }
    fclose(fp);

    if (last[0] == '\0') {
        return 0;
    }

    if (parse_index_line(last, &id, &mime_type, &ext) != 0) {
        free(id);
        free(mime_type);
        free(ext);
        return -1;
    }

    build_item_path(cfg, id, ext, path, sizeof(path));
    if (read_file_alloc(path, &item->content, &item->content_len) != 0) {
        free(id);
        free(mime_type);
        free(ext);
        return -1;
    }

    item->id = id;
    item->mime_type = mime_type;
    free(ext);
    return 0;
}

void storage_free_item(ClipItem *item) {
    if (item == NULL) {
        return;
    }
    free(item->id);
    free(item->mime_type);
    free(item->content);
    item->id = NULL;
    item->mime_type = NULL;
    item->content = NULL;
    item->content_len = 0;
}

int storage_load_all(const RcopyConfig *cfg, ClipList *list, size_t limit) {
    FILE *fp;
    IndexMeta *meta = NULL;
    size_t meta_count = 0;
    size_t meta_cap = 0;
    char line[512];
    size_t i;
    size_t out_count;

    list->items = NULL;
    list->count = 0;

    fp = fopen(cfg->index_file, "r");
    if (fp == NULL) {
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        size_t n = strlen(line);
        IndexMeta m;
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
            line[--n] = '\0';
        }
        if (n == 0) {
            continue;
        }

        if (meta_count == meta_cap) {
            size_t next = (meta_cap == 0) ? 64 : meta_cap * 2;
            IndexMeta *tmp = realloc(meta, next * sizeof(IndexMeta));
            if (tmp == NULL) {
                fclose(fp);
                return -1;
            }
            meta = tmp;
            meta_cap = next;
        }

        memset(&m, 0, sizeof(m));
        if (parse_index_line(line, &m.id, &m.mime_type, &m.ext) != 0) {
            fclose(fp);
            return -1;
        }
        meta[meta_count++] = m;
    }
    fclose(fp);

    if (meta_count == 0) {
        free(meta);
        return 0;
    }

    out_count = (meta_count < limit) ? meta_count : limit;
    list->items = calloc(out_count, sizeof(ClipItem));
    if (list->items == NULL) {
        for (i = 0; i < meta_count; i++) {
            free(meta[i].id);
            free(meta[i].mime_type);
            free(meta[i].ext);
        }
        free(meta);
        return -1;
    }

    for (i = 0; i < out_count; i++) {
        size_t rev = meta_count - 1 - i;
        char path[PATH_MAX];
        list->items[i].id = dup_string(meta[rev].id);
        list->items[i].mime_type = dup_string(meta[rev].mime_type);
        build_item_path(cfg, meta[rev].id, meta[rev].ext, path, sizeof(path));
        if (read_file_alloc(path, &list->items[i].content, &list->items[i].content_len) != 0) {
            list->items[i].content = dup_string("");
            list->items[i].content_len = 0;
        }
    }

    list->count = out_count;

    for (i = 0; i < meta_count; i++) {
        free(meta[i].id);
        free(meta[i].mime_type);
        free(meta[i].ext);
    }
    free(meta);

    return 0;
}

void storage_free_list(ClipList *list) {
    size_t i;
    if (list == NULL || list->items == NULL) {
        return;
    }
    for (i = 0; i < list->count; i++) {
        free(list->items[i].id);
        free(list->items[i].mime_type);
        free(list->items[i].content);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
}
