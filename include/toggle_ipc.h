#ifndef RCOPY_TOGGLE_IPC_H
#define RCOPY_TOGGLE_IPC_H

#include "config.h"

int toggle_send(const RcopyConfig *cfg);
int toggle_server_start(const RcopyConfig *cfg, int *server_fd);
int toggle_server_poll(int server_fd);
void toggle_server_stop(const RcopyConfig *cfg, int server_fd);

#endif
