#include "common.h"

int send_all(int sockfd, const void *buf, size_t len)
{
    size_t total = 0;
    const char *p = buf;
    while (total < len)
    {
        ssize_t n = send(sockfd, p + total, len - total, 0);
        if (n <= 0)
            return -1;
        total += n;
    }
    return 0;
}

int recv_all(int sockfd, void *buf, size_t len)
{
    size_t total = 0;
    char *p = buf;
    while (total < len)
    {
        ssize_t n = recv(sockfd, p + total, len - total, 0);
        if (n <= 0)
            return -1;
        total += n;
    }
    return 0;
}
