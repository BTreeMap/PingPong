#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdint.h>
#include <errno.h>
#include <arpa/inet.h>

#include "common.h"

#define BACKLOG 1
#define BUFSIZE 65536

int main(int argc, char *argv[])
{
    if (argc != 3 || strcmp(argv[1], "--port") != 0)
    {
        fprintf(stderr, "Usage: %s --port <control_port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    int control_port = atoi(argv[2]);

    // Control listener setup
    int control_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (control_fd < 0)
    {
        perror("socket");
        return EXIT_FAILURE;
    }

    int opt = 1;
    setsockopt(control_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(control_port);

    if (bind(control_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return EXIT_FAILURE;
    }
    if (listen(control_fd, BACKLOG) < 0)
    {
        perror("listen");
        return EXIT_FAILURE;
    }
    printf("Control listening on port %d...\n", control_port);

    // Negotiation phase
    int conn_fd = accept(control_fd, NULL, NULL);
    if (conn_fd < 0)
    {
        perror("accept");
        return EXIT_FAILURE;
    }
    printf("Client connected for negotiation\n");

    negotiation_t neg_net;
    if (recv_all(conn_fd, &neg_net, sizeof(neg_net)) < 0)
    {
        perror("recv negotiation");
        return EXIT_FAILURE;
    }
    uint32_t size = ntohl(neg_net.size);
    uint32_t count = ntohl(neg_net.count);
    uint16_t exp_port = ntohs(neg_net.exp_port);

    // Attempt to set up experiment listener and notify client on control channel
    int exp_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (exp_listen_fd < 0)
    {
        uint32_t sn = htonl(NEG_STATUS_SOCKET);
        send_all(conn_fd, &sn, sizeof(sn));
        close(conn_fd);
        close(control_fd);
        return EXIT_FAILURE;
    }
    if (setsockopt(exp_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        uint32_t sn = htonl(NEG_STATUS_SETSOCKOPT);
        send_all(conn_fd, &sn, sizeof(sn));
        close(conn_fd);
        close(control_fd);
        return EXIT_FAILURE;
    }
    addr.sin_port = htons(exp_port);
    if (bind(exp_listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        uint32_t sn = htonl(NEG_STATUS_BIND);
        send_all(conn_fd, &sn, sizeof(sn));
        close(conn_fd);
        close(control_fd);
        return EXIT_FAILURE;
    }
    if (listen(exp_listen_fd, BACKLOG) < 0)
    {
        uint32_t sn = htonl(NEG_STATUS_LISTEN);
        send_all(conn_fd, &sn, sizeof(sn));
        close(conn_fd);
        close(control_fd);
        return EXIT_FAILURE;
    }
    // success, notify client
    {
        uint32_t sn = htonl(NEG_STATUS_OK);
        send_all(conn_fd, &sn, sizeof(sn));
    }
    close(conn_fd);
    close(control_fd);

    printf("Experiment listening on port %u...\n", exp_port);
    int exp_fd = accept(exp_listen_fd, NULL, NULL);
    if (exp_fd < 0)
    {
        perror("accept experiment");
        return EXIT_FAILURE;
    }
    printf("Experiment connection established\n");

    // Read exactly size bytes and echo back
    char *buf = malloc(size);
    if (!buf)
    {
        perror("malloc");
        return EXIT_FAILURE;
    }
    for (uint32_t i = 0; i < count; i++)
    {
        if (recv_all(exp_fd, buf, size) < 0)
        {
            perror("recv experiment");
            break;
        }
        if (send_all(exp_fd, buf, size) < 0)
        {
            perror("send experiment");
            break;
        }
    }
    free(buf);
    close(exp_fd);
    close(exp_listen_fd);
    return EXIT_SUCCESS;
}