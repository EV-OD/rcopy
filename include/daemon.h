#ifndef RCOPY_DAEMON_H
#define RCOPY_DAEMON_H

#include "config.h"

int daemon_run(const RcopyConfig *cfg, const char *self_path);
int daemon_ingest_once(const RcopyConfig *cfg);

#endif
