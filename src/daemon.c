#include "daemon.h"

#include "storage.h"
#include "util.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int signo) {
    (void)signo;
    g_stop = 1;
}

static void setup_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static int acquire_lock(const char *path) {
    int fd = open(path, O_CREAT | O_RDWR, 0644);
    if (fd < 0) {
        return -1;
    }
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static const char *pick_best_mime(char **types, size_t count) {
    static const char *preferred[] = {
        "image/png",
        "image/jpeg",
        "image/webp",
        "text/plain;charset=utf-8",
        "text/plain",
    };
    size_t i;
    size_t j;

    for (i = 0; i < sizeof(preferred) / sizeof(preferred[0]); i++) {
        for (j = 0; j < count; j++) {
            if (strcmp(preferred[i], types[j]) == 0) {
                return types[j];
            }
        }
    }

    if (count > 0) {
        return types[0];
    }
    return NULL;
}

static int capture_current_clipboard(const RcopyConfig *cfg) {
    char **types = NULL;
    size_t count = 0;
    const char *mime_type = "text/plain";
    char *current = NULL;
    size_t current_len = 0;
    ClipItem last;
    int changed = 1;

    memset(&last, 0, sizeof(last));

    if (util_list_clipboard_types(&types, &count) == 0 && count > 0) {
        const char *picked = pick_best_mime(types, count);
        if (picked != NULL) {
            mime_type = picked;
            if (util_read_wl_paste_type(mime_type, &current, &current_len) != 0 || current_len == 0) {
                free(current);
                current = NULL;
                current_len = 0;
            }
        }
    }

    if (current == NULL || current_len == 0) {
        mime_type = "text/plain";
        if (util_read_wl_paste(&current, &current_len) != 0 || current_len == 0) {
            free(current);
            util_free_string_list(types, count);
            return 0;
        }
    }

    if (storage_get_last(cfg, &last) == 0 && last.content != NULL && last.mime_type != NULL) {
        if (strcmp(last.mime_type, mime_type) == 0 &&
            last.content_len == current_len &&
            memcmp(last.content, current, current_len) == 0) {
            changed = 0;
        }
    }

    if (changed) {
        storage_save(cfg, mime_type, current, current_len);
    }

    storage_free_item(&last);
    free(current);
    util_free_string_list(types, count);
    return 0;
}

int daemon_ingest_once(const RcopyConfig *cfg) {
    if (storage_init(cfg) != 0) {
        return 1;
    }
    return capture_current_clipboard(cfg);
}

static void poll_loop(const RcopyConfig *cfg) {
    struct timespec req;
    req.tv_sec = cfg->poll_ms / 1000;
    req.tv_nsec = (long)(cfg->poll_ms % 1000) * 1000000L;

    while (!g_stop) {
        capture_current_clipboard(cfg);
        nanosleep(&req, NULL);
    }
}

static int run_watch_once(const char *self_path) {
    pid_t pid = fork();
    int status = 0;

    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        execlp("wl-paste", "wl-paste", "--watch", self_path, "__ingest", (char *)NULL);
        _exit(127);
    }

    if (pid < 0) {
        return -1;
    }

    sleep(1);
    if (waitpid(pid, &status, WNOHANG) == 0) {
        while (!g_stop) {
            if (waitpid(pid, &status, 0) < 0) {
                if (errno == EINTR && g_stop) {
                    kill(pid, SIGTERM);
                    waitpid(pid, &status, 0);
                    return 0;
                }
                continue;
            }
            return 1;
        }
        kill(pid, SIGTERM);
        waitpid(pid, &status, 0);
        return 0;
    }

    return -1;
}

int daemon_run(const RcopyConfig *cfg, const char *self_path) {
    int lock_fd;

    if (storage_init(cfg) != 0) {
        fprintf(stderr, "failed to init storage\n");
        return 1;
    }

    lock_fd = acquire_lock(cfg->lock_file);
    if (lock_fd < 0) {
        fprintf(stderr, "daemon already running\n");
        return 0;
    }

    setup_signal_handlers();

    while (!g_stop) {
        int watch_rc = run_watch_once(self_path);
        if (g_stop) {
            break;
        }

        if (watch_rc < 0) {
            fprintf(stderr, "watch unsupported; using polling fallback (%d ms)\n", cfg->poll_ms);
            poll_loop(cfg);
            break;
        }

        sleep(1);
    }

    close(lock_fd);
    return 0;
}
