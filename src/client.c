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

#include "common.h"

// get current time in microseconds
uint64_t time_us()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

int main(int argc, char *argv[])
{
    char *ctrl_addr = NULL;
    int ctrl_port = 0;
    int exp_port = 0;
    int size = 0;
    int count = 0;
    char *output = NULL;

    static struct option long_options[] = {
        {"addr", required_argument, 0, 'a'},
        {"control-port", required_argument, 0, 'P'},
        {"exp-port", required_argument, 0, 'e'},
        {"size", required_argument, 0, 's'},
        {"count", required_argument, 0, 'c'},
        {"output", required_argument, 0, 'o'},
        {0, 0, 0, 0}};

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "a:P:e:s:c:o:", long_options, &option_index)) != -1)
    {
        switch (opt)
        {
        case 'a':
            ctrl_addr = optarg;
            break;
        case 'P':
            ctrl_port = atoi(optarg);
            break;
        case 'e':
            exp_port = atoi(optarg);
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
            fprintf(stderr, "Usage: %s -a <address> -P <control_port> [-e <exp_port>] -s <bytes> -c <number> -o <file>\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (!ctrl_addr || ctrl_port <= 0 || size <= 0 || count <= 0 || !output)
    {
        fprintf(stderr, "Usage: %s -a <address> -P <control_port> [-e <exp_port>] -s <bytes> -c <number> -o <file>\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (exp_port <= 0)
        exp_port = ctrl_port + 1;

    // Negotiate on control channel
    int ctrl_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctrl_fd < 0)
    {
        perror("socket control");
        return EXIT_FAILURE;
    }

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons(ctrl_port);
    if (inet_pton(AF_INET, ctrl_addr, &serv.sin_addr) <= 0)
    {
        perror("inet_pton control");
        return EXIT_FAILURE;
    }
    if (connect(ctrl_fd, (struct sockaddr *)&serv, sizeof(serv)) < 0)
    {
        perror("connect control");
        return EXIT_FAILURE;
    }

    negotiation_t neg_net;
    neg_net.size = htonl(size);
    neg_net.count = htonl(count);
    neg_net.exp_port = htons(exp_port);
    if (send_all(ctrl_fd, &neg_net, sizeof(neg_net)) < 0)
    {
        perror("send negotiation");
        return EXIT_FAILURE;
    }

    // receive server status
    uint32_t status_net;
    if (recv_all(ctrl_fd, &status_net, sizeof(status_net)) < 0)
    {
        perror("recv negotiation status");
        return EXIT_FAILURE;
    }
    close(ctrl_fd);
    uint32_t status = ntohl(status_net);
    if (status != NEG_STATUS_OK)
    {
        switch (status)
        {
        case NEG_STATUS_SOCKET:
            fprintf(stderr, "Server error: failed to create experiment socket\n");
            break;
        case NEG_STATUS_SETSOCKOPT:
            fprintf(stderr, "Server error: failed to set SO_REUSEADDR\n");
            break;
        case NEG_STATUS_BIND:
            fprintf(stderr, "Server error: failed to bind experiment port\n");
            break;
        case NEG_STATUS_LISTEN:
            fprintf(stderr, "Server error: failed to listen on experiment port\n");
            break;
        default:
            fprintf(stderr, "Server error: unknown status %u\n", status);
        }
        return EXIT_FAILURE;
    }

    // Experimental connection
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket experiment");
        return EXIT_FAILURE;
    }

    serv.sin_port = htons(exp_port);
    if (connect(sockfd, (struct sockaddr *)&serv, sizeof(serv)) < 0)
    {
        perror("connect experiment");
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
