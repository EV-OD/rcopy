#ifndef RCOPY_CONFIG_H
#define RCOPY_CONFIG_H

#include <limits.h>

typedef struct {
    char data_dir[PATH_MAX];
    char items_dir[PATH_MAX];
    char index_file[PATH_MAX];
    char socket_path[PATH_MAX];
    char lock_file[PATH_MAX];
    char paste_command[256];
    int poll_ms;
    int max_items;
} RcopyConfig;

int rcopy_load_config(RcopyConfig *cfg);

#endif
