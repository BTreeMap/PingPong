#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BACKLOG 1
#define BUFSIZE 65536

// send_all ensures all data is sent
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

int main(int argc, char *argv[])
{
    if (argc != 3 || strcmp(argv[1], "--port") != 0)
    {
        fprintf(stderr, "Usage: %s --port <port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    int port = atoi(argv[2]);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
    {
        perror("socket");
        return EXIT_FAILURE;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return EXIT_FAILURE;
    }
    if (listen(listen_fd, BACKLOG) < 0)
    {
        perror("listen");
        return EXIT_FAILURE;
    }

    printf("Listening on port %d...\n", port);
    int conn_fd = accept(listen_fd, NULL, NULL);
    if (conn_fd < 0)
    {
        perror("accept");
        return EXIT_FAILURE;
    }
    printf("Client connected\n");

    char buf[BUFSIZE];
    ssize_t n;
    while ((n = recv(conn_fd, buf, BUFSIZE, 0)) > 0)
    {
        if (send_all(conn_fd, buf, n) < 0)
        {
            perror("send");
            break;
        }
    }

    close(conn_fd);
    close(listen_fd);
    return EXIT_SUCCESS;
}