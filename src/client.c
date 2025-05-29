#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <getopt.h>

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

    static struct option long_options[] = {
        {"addr", required_argument, 0, 'a'},
        {"port", required_argument, 0, 'p'},
        {"size", required_argument, 0, 's'},
        {"count", required_argument, 0, 'c'},
        {"output", required_argument, 0, 'o'},
        {0, 0, 0, 0}};

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "a:p:s:c:o:", long_options, &option_index)) != -1)
    {
        switch (opt)
        {
        case 'a':
            addr = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 's':
            size = atoi(optarg);
            break;
        case 'c':
            count = atoi(optarg);
            break;
        case 'o':
            output = optarg;
            break;
        default:
            fprintf(stderr, "Usage: %s -a <address> -p <port> -s <bytes> -c <number> -o <file>\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (!addr || port <= 0 || size <= 0 || count <= 0 || !output)
    {
        fprintf(stderr, "Usage: %s -a <address> -p <port> -s <bytes> -c <number> -o <file>\n", argv[0]);
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
