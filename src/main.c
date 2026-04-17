#include "config.h"
#include "daemon.h"
#include "storage.h"
#include "toggle_ipc.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

static void print_usage(void) {
    printf("rcopy daemon\n");
    printf("rcopy toggle\n");
    printf("rcopy picker\n");
}

int main(int argc, char **argv) {
    RcopyConfig cfg;

    if (rcopy_load_config(&cfg) != 0) {
        fprintf(stderr, "failed to load config\n");
        return 1;
    }

    if (argc < 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "daemon") == 0) {
        return daemon_run(&cfg, argv[0]);
    }

    if (strcmp(argv[1], "__ingest") == 0) {
        return daemon_ingest_once(&cfg);
    }

    if (strcmp(argv[1], "toggle") == 0) {
        if (toggle_send(&cfg)) {
            return 0;
        }
        return ui_run(&cfg, 0);
    }

    if (strcmp(argv[1], "picker") == 0) {
        return ui_run(&cfg, 1);
    }

    print_usage();
    return 1;
}
