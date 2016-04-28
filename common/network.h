#ifndef NETWORK_H
#define NETWORK_H

#include "common.h"
#include <sys/types.h>
#include <sys/socket.h>

static inline ssize_t Send(int fd, void *buf, size_t size)
{
    ssize_t n = send(fd, buf, size, 0);
    if (n < size) {
        sys_panic("send");
    }
    return n;
}

static inline ssize_t Recv(int fd, void *buf, size_t size)
{
    ssize_t n = recv(fd, buf, size, 0);
    if (n == -1) {
        sys_panic("recv");
    }
    return n;
}

#endif  // NETWORK_H
