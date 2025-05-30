// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_endian.h>

#include "event_defs.h" // Include the shared event definition

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
    e->event_type = EVENT_TYPE_TCP_SEND; // Mark this as a send event

    // Populate sport, dport, etc. from struct sock *sk
    // This requires careful handling of kernel struct access
    // e.g., e->sport = BPF_CORE_READ(sk, __sk_common.skc_num);
    // Get the socket structure from the first argument of tcp_sendmsg
    struct sock *sk = (struct sock *)PT_REGS_PARM1(ctx);

    // Extract source port (in host byte order)
    e->sport = BPF_CORE_READ(sk, __sk_common.skc_num);

    // Extract destination port (convert from network to host byte order)
    e->dport = bpf_ntohs(BPF_CORE_READ(sk, __sk_common.skc_dport));

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

    e = bpf_ringbuf_reserve(&events, sizeof(struct event), 0);
    if (!e)
    {
        return 0;
    }

    e->timestamp_ns = ts;
    e->pid = pid;
    e->event_type = EVENT_TYPE_TCP_RECV; // Mark this as a receive event

    // Populate details from sk
    struct sock *sk = (struct sock *)PT_REGS_PARM1(ctx);
    if (!sk)
    {
        return 0;
    }

    // Extract source port (network byte order, will be converted in userspace)
    e->sport = BPF_CORE_READ(sk, __sk_common.skc_num);
    // Extract destination port (network byte order, will be converted in userspace)
    e->dport = BPF_CORE_READ(sk, __sk_common.skc_dport);

    bpf_ringbuf_submit(e, 0);
    return 0;
}

char LICENSE[] SEC("license") = "MIT";
