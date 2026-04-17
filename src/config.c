#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int rcopy_load_config(RcopyConfig *cfg) {
    const char *home = getenv("HOME");
    const char *runtime = getenv("XDG_RUNTIME_DIR");
    const char *uid = getenv("UID");

    if (home == NULL || cfg == NULL) {
        return -1;
    }

    if (runtime != NULL && runtime[0] != '\0') {
        snprintf(cfg->data_dir, sizeof(cfg->data_dir), "%s/rcopy", runtime);
    } else if (uid != NULL && uid[0] != '\0') {
        snprintf(cfg->data_dir, sizeof(cfg->data_dir), "/tmp/rcopy-%s", uid);
    } else {
        snprintf(cfg->data_dir, sizeof(cfg->data_dir), "/tmp/rcopy");
    }

    snprintf(cfg->items_dir, sizeof(cfg->items_dir), "%s/items", cfg->data_dir);
    snprintf(cfg->index_file, sizeof(cfg->index_file), "%s/index.txt", cfg->data_dir);
    snprintf(cfg->lock_file, sizeof(cfg->lock_file), "%s/daemon.lock", cfg->data_dir);

    if (runtime != NULL && runtime[0] != '\0') {
        snprintf(cfg->socket_path, sizeof(cfg->socket_path), "%s/rcopy-toggle.sock", runtime);
    } else {
        snprintf(cfg->socket_path, sizeof(cfg->socket_path), "/tmp/rcopy-toggle.sock");
    }

    snprintf(cfg->paste_command, sizeof(cfg->paste_command), "wtype -M ctrl -k v -m ctrl");
    cfg->poll_ms = 10000;
    cfg->max_items = 500;
    return 0;
}
