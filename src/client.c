#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

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

// recv_all ensures all data is received
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

// get current time in microseconds
uint64_t time_us()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

int main(int argc, char *argv[])
{
    char *addr = NULL;
    int port = 0;
    int size = 0;
    int count = 0;
    char *output = NULL;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--addr") == 0 && i + 1 < argc)
        {
            addr = argv[++i];
        }
        else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc)
        {
            port = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--size") == 0 && i + 1 < argc)
        {
            size = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--count") == 0 && i + 1 < argc)
        {
            count = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc)
        {
            output = argv[++i];
        }
        else
        {
            fprintf(stderr, "Unknown or incomplete argument: %s\n", argv[i]);
            return EXIT_FAILURE;
        }
    }
    if (!addr || port <= 0 || size <= 0 || count <= 0 || !output)
    {
        fprintf(stderr, "Usage: %s --addr <address> --port <port> --size <bytes> --count <number> --output <file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    if (inet_pton(AF_INET, addr, &serv.sin_addr) <= 0)
    {
        perror("inet_pton");
        return EXIT_FAILURE;
    }
    if (connect(sockfd, (struct sockaddr *)&serv, sizeof(serv)) < 0)
    {
        perror("connect");
        return EXIT_FAILURE;
    }

    FILE *fp = fopen(output, "w");
    if (!fp)
    {
        perror("fopen");
        return EXIT_FAILURE;
    }
    fprintf(fp, "seq,send_entry_us,send_exit_us,recv_entry_us\n");

    char *buf = malloc(size);
    if (!buf)
    {
        perror("malloc");
        return EXIT_FAILURE;
    }
    memset(buf, 'P', size);

    for (int i = 0; i < count; i++)
    {
        uint64_t ts1 = time_us();
        if (send_all(sockfd, buf, size) < 0)
        {
            perror("send");
            break;
        }
        uint64_t ts2 = time_us();
        if (recv_all(sockfd, buf, size) < 0)
        {
            perror("recv");
            break;
        }
        uint64_t ts3 = time_us();
        fprintf(fp, "%d,%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n", i, ts1, ts2, ts3);
    }

    fclose(fp);
    free(buf);
    close(sockfd);
    return EXIT_SUCCESS;
}