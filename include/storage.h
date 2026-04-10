#ifndef RCOPY_STORAGE_H
#define RCOPY_STORAGE_H

#include <stddef.h>

#include "config.h"

typedef struct {
    char *id;
    char *mime_type;
    char *content;
    size_t content_len;
} ClipItem;

typedef struct {
    ClipItem *items;
    size_t count;
} ClipList;

int storage_init(const RcopyConfig *cfg);
int storage_save(const RcopyConfig *cfg, const char *mime_type, const char *data, size_t len);
int storage_get_last(const RcopyConfig *cfg, ClipItem *item);
void storage_free_item(ClipItem *item);
int storage_load_all(const RcopyConfig *cfg, ClipList *list, size_t limit);
void storage_free_list(ClipList *list);

#endif
