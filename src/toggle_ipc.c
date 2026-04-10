#include "toggle_ipc.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int toggle_send(const RcopyConfig *cfg) {
    int fd;
    struct sockaddr_un addr;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return 0;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", cfg->socket_path);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return 0;
    }

    write(fd, "toggle", 6);
    close(fd);
    return 1;
}

int toggle_server_start(const RcopyConfig *cfg, int *server_fd) {
    int fd;
    struct sockaddr_un addr;

    unlink(cfg->socket_path);

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", cfg->socket_path);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }

    if (listen(fd, 4) != 0) {
        close(fd);
        return -1;
    }

    fcntl(fd, F_SETFL, O_NONBLOCK);
    *server_fd = fd;
    return 0;
}

int toggle_server_poll(int server_fd) {
    int client;
    char buf[16];

    client = accept(server_fd, NULL, NULL);
    if (client < 0) {
        return 0;
    }

    memset(buf, 0, sizeof(buf));
    read(client, buf, sizeof(buf) - 1);
    close(client);

    if (strncmp(buf, "toggle", 6) == 0) {
        return 1;
    }

    return 0;
}

void toggle_server_stop(const RcopyConfig *cfg, int server_fd) {
    if (server_fd >= 0) {
        close(server_fd);
    }
    unlink(cfg->socket_path);
}
