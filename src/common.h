// filepath: /config/repositories/PingPong/src/common.h
#ifndef PINGPONG_COMMON_H
#define PINGPONG_COMMON_H

#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>

// send_all ensures all data is sent
typedef int (*send_all_fn)(int sockfd, const void *buf, size_t len);
int send_all(int sockfd, const void *buf, size_t len);

// recv_all ensures all data is received
int recv_all(int sockfd, void *buf, size_t len);

// Negotiation status codes used between client and server
enum neg_status
{
    NEG_STATUS_OK = 0,
    NEG_STATUS_SOCKET = 1,
    NEG_STATUS_SETSOCKOPT = 2,
    NEG_STATUS_BIND = 3,
    NEG_STATUS_LISTEN = 4,
};

#endif // PINGPONG_COMMON_H
