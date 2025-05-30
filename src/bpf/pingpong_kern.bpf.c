// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

// Define a structure for the data we want to send to user space
struct event
{
    __u64 timestamp_ns;
    __u32 pid;
    __u16 sport;
    __u16 dport;
    // Add other fields as needed, e.g., saddr, daddr, event_type
};

// Ring buffer map to send events to user space
struct
{
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256 * 1024); // 256 KB
} events SEC(".maps");

// Placeholder for kprobe on tcp_sendmsg or similar
SEC("kprobe/tcp_sendmsg")
int handle_tcp_sendmsg(struct pt_regs *ctx)
{
    struct event *e;
    __u64 ts = bpf_ktime_get_ns();
    __u32 pid = bpf_get_current_pid_tgid() >> 32;

    // Reserve space in the ring buffer
    e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (!e)
    {
        return 0;
    }

    e->timestamp_ns = ts;
    e->pid = pid;

    // Populate sport, dport, etc. from struct sock *sk
    // This requires careful handling of kernel struct access
    // e.g., e->sport = BPF_CORE_READ(sk, __sk_common.skc_num);
    // e->dport = BPF_CORE_READ(sk, __sk_common.skc_dport);
    // Ensure dport is in host byte order if needed: bpf_ntohs()

    bpf_ringbuf_submit(e, 0);
    return 0;
}
// Placeholder for kprobe on tcp_rcv_established or similar for received packets
SEC("kprobe/tcp_rcv_established")
int handle_tcp_rcv(struct pt_regs *ctx)
{
    struct event *e;
    __u64 ts = bpf_ktime_get_ns();
    __u32 pid = bpf_get_current_pid_tgid() >> 32;

    e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (!e)
    {
        return 0;
    }

    e->timestamp_ns = ts;
    e->pid = pid;

    // Populate details from sk

    bpf_ringbuf_submit(e, 0);
    return 0;
}

char LICENSE[] SEC("license") = "Dual BSD/GPL";
