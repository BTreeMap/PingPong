#ifndef __EVENT_DEFS_H
#define __EVENT_DEFS_H

#define EVENT_TYPE_TCP_SEND 1
#define EVENT_TYPE_TCP_RECV 2
#define EVENT_TYPE_TCP_SEND_EXIT 3
#define EVENT_TYPE_TCP_RECV_EXIT 4

// common max for IPv6 address
#define ADDR_V6_WORDS 4

struct event
{
    __u64 timestamp_ns;
    __u32 pid;
    __u16 sport;
    __u16 dport;
    __u8 event_type; // 1 for send, 2 for receive
    __u8 af;         // address family: AF_INET or AF_INET6
    union
    {
        __u32 v4;
        __u32 v6[ADDR_V6_WORDS];
    } saddr;
    union
    {
        __u32 v4;
        __u32 v6[ADDR_V6_WORDS];
    } daddr;
    __u64 sock_id; // NEW: cast of (u64) sk pointer
};

#endif /* __EVENT_DEFS_H */