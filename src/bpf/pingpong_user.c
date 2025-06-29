#include <argp.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include "pingpong_kern.skel.h" // Generated by bpftool gen skeleton
#include "event_defs.h"         // Include the shared event definition

static struct pingpong_kern_bpf *skel = NULL;
static struct ring_buffer *rb = NULL;

static __u16 target_sport = 0; // Global variable for target sport
static __u16 target_dport = 0; // Global variable for target dport
static __u32 force_filter = 0; // Flag to force filtering by ports

static struct argp_option options[] = {
    {"sport", 's', "SPORT", 0, "Target source port to filter"},
    {"dport", 'd', "DPORT", 0, "Target destination port to filter"},
    // Note that some events may not necessarily have the port numbers set.
    // If the force filter is set, we skip events with unset port numbers.
    {"force-filter", 'f', 0, 0, "Force filtering by source and destination ports"},
    {0}};

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    switch (key)
    {
    case 's':
        if (arg)
        {
            char *end;
            long port = strtol(arg, &end, 10);
            if (*end != '\0' || port <= 0 || port > 65535)
            {
                fprintf(stderr, "Invalid sport: %s\n", arg);
                argp_usage(state);
            }
            target_sport = (__u16)port;
        }
        break;
    case 'd':
        if (arg)
        {
            char *end;
            long port = strtol(arg, &end, 10);
            if (*end != '\0' || port <= 0 || port > 65535)
            {
                fprintf(stderr, "Invalid dport: %s\n", arg);
                argp_usage(state);
            }
            target_dport = (__u16)port;
        }
        break;
    case 'f':
        force_filter = 1; // Enable force filtering
        break;
    case ARGP_KEY_ARG:
        argp_usage(state);
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static const char *const doc = "PingPong BPF User Program - Filter events by source and destination ports";

static struct argp argp = {options, parse_opt, 0, doc};

static int handle_event(void *ctx, void *data, size_t data_sz)
{
    const struct event *e = data;

    if (target_sport != 0)
    {
        if (e->sport == 0 && force_filter)
        {
            // If sport is 0 and force_filter is set, skip this event
            return 0;
        }
        if (e->sport != 0 && e->sport != target_sport)
        {
            // If sport is set and does not match target_sport, skip this event
            return 0;
        }
    }
    if (target_dport != 0)
    {
        if (e->dport == 0 && force_filter)
        {
            // If dport is 0 and force_filter is set, skip this event
            return 0;
        }
        if (e->dport != 0 && e->dport != target_dport)
        {
            // If dport is set and does not match target_dport, skip this event
            return 0;
        }
    }

    const char *type_str;
    switch (e->event_type)
    {
    case EVENT_TYPE_TCP_SEND:
        type_str = "send_entry";
        break;
    case EVENT_TYPE_TCP_RECV:
        type_str = "recv_entry";
        break;
    case EVENT_TYPE_TCP_SEND_EXIT:
        type_str = "send_exit";
        break;
    case EVENT_TYPE_TCP_RECV_EXIT:
        type_str = "recv_exit";
        break;
    default:
        type_str = "unknown";
    }

    char src[INET6_ADDRSTRLEN] = {0}, dst[INET6_ADDRSTRLEN] = {0};
    if (e->af == AF_INET)
    {
        struct in_addr ia;
        ia.s_addr = e->saddr.v4;
        inet_ntop(AF_INET, &ia, src, sizeof(src));
        ia.s_addr = e->daddr.v4;
        inet_ntop(AF_INET, &ia, dst, sizeof(dst));
    }
    else if (e->af == AF_INET6)
    {
        struct in6_addr ia6;
        for (int i = 0; i < ADDR_V6_WORDS; i++)
        {
            ia6.s6_addr32[i] = e->saddr.v6[i];
        }
        inet_ntop(AF_INET6, &ia6, src, sizeof(src));
        for (int i = 0; i < ADDR_V6_WORDS; i++)
        {
            ia6.s6_addr32[i] = e->daddr.v6[i];
        }
        inet_ntop(AF_INET6, &ia6, dst, sizeof(dst));
    }
    else
    {
        strncpy(src, "?", sizeof(src));
        strncpy(dst, "?", sizeof(dst));
    }

    // Print with direction depending on send/receive
    bool is_send = (e->event_type == EVENT_TYPE_TCP_SEND ||
                    e->event_type == EVENT_TYPE_TCP_SEND_EXIT);
    if (e->af == AF_INET)
    {
        if (is_send)
        {
            printf("ts:%llu sock:%llu pid:%u type:%s srtt:%u %s:%u -> %s:%u\n",
                   e->timestamp_ns, e->sock_id, e->pid, type_str, e->srtt_us, src, e->sport, dst, e->dport);
        }
        else
        {
            printf("ts:%llu sock:%llu pid:%u type:%s srtt:%u %s:%u -> %s:%u\n",
                   e->timestamp_ns, e->sock_id, e->pid, type_str, e->srtt_us, dst, e->dport, src, e->sport);
        }
    }
    else
    {
        if (is_send)
        {
            printf("ts:%llu sock:%llu pid:%u type:%s srtt:%u [%s]:%u -> [%s]:%u\n",
                   e->timestamp_ns, e->sock_id, e->pid, type_str, e->srtt_us, src, e->sport, dst, e->dport);
        }
        else
        {
            printf("ts:%llu sock:%llu pid:%u type:%s srtt:%u [%s]:%u -> [%s]:%u\n",
                   e->timestamp_ns, e->sock_id, e->pid, type_str, e->srtt_us, dst, e->dport, src, e->sport);
        }
    }
    fflush(stdout);
    return 0;
}

static void cleanup(void)
{
    int rb_cleanup = 0, skel_cleanup = 0;
    if (rb)
    {
        ring_buffer__free(rb);
        rb = NULL;
        rb_cleanup = 1;
    }
    if (skel)
    {
        pingpong_kern_bpf__destroy(skel);
        skel = NULL;
        skel_cleanup = 1;
    }
    if (rb_cleanup)
    {
        fprintf(stderr, "[INFO] Ring buffer cleaned up\n");
    }
    if (skel_cleanup)
    {
        fprintf(stderr, "[INFO] BPF skeleton cleaned up\n");
    }
}

static void fatal_handler(int sig)
{
    // Handle fatal signals by cleaning up
    fprintf(stderr, "Fatal signal %d received, unloading BPF programs\n", sig);
    cleanup();
    _exit(1);
}

static volatile bool exiting = false;

static void sig_handler(int sig)
{
    // Handle interrupt/termination signals by flushing logs
    exiting = true;
}

int main(int argc, char **argv)
{
    int err;

    atexit(cleanup);

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGSEGV, fatal_handler);
    signal(SIGABRT, fatal_handler);
    signal(SIGQUIT, fatal_handler);
    signal(SIGBUS, fatal_handler);

    // Parse command line arguments
    err = argp_parse(&argp, argc, argv, 0, 0, 0);
    if (err)
    {
        fprintf(stderr, "Failed to parse arguments\n");
        return 1;
    }

    // Open, load, and verify BPF application
    skel = pingpong_kern_bpf__open_and_load();
    if (!skel)
    {
        fprintf(stderr, "Failed to open and load BPF skeleton\n");
        return 1;
    }

    // Attach tracepoints or kprobes
    err = pingpong_kern_bpf__attach(skel);
    if (err)
    {
        fprintf(stderr, "Failed to attach BPF skeleton\n");
        goto cleanup;
    }

    // Set up ring buffer polling
    rb = ring_buffer__new(bpf_map__fd(skel->maps.events), handle_event, NULL, NULL);
    if (!rb)
    {
        err = -1;
        fprintf(stderr, "Failed to create ring buffer\n");
        goto cleanup;
    }

    fprintf(stderr, "Successfully started! Please run `sudo cat /sys/kernel/debug/tracing/trace_pipe` "
                    "to see output of the BPF programs.\n");

    while (!exiting)
    {
        err = ring_buffer__poll(rb, 100 /* timeout, ms */);
        // Ctrl-C will cause -EINTR
        if (err == -EINTR)
        {
            err = 0;
            break;
        }
        if (err < 0)
        {
            fprintf(stderr, "Error polling ring buffer: %d\n", err);
            break;
        }
    }

cleanup:
    cleanup();
    return -err;
}
