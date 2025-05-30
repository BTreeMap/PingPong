#ifndef __EVENT_DEFS_H
#define __EVENT_DEFS_H

#define EVENT_TYPE_TCP_SEND 1
#define EVENT_TYPE_TCP_RECV 2

// Define the same event structure as in the BPF program
struct event
{
    __u64 timestamp_ns;
    __u32 pid;
    __u16 sport;
    __u16 dport;
    __u8 event_type; // 1 for send, 2 for receive
    // Add other fields as needed
};

#endif /* __EVENT_DEFS_H */