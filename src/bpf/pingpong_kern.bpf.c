/* SPDX-License-Identifier: MIT OR GPL-2.0 */

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_endian.h>

#include "event_defs.h" // Include the shared event definition

// Define address family constants <https://github.com/torvalds/linux/blob/master/include/linux/socket.h>
#define AF_INET 2
#define AF_INET6 10

// Ring buffer map to send events to user space
struct
{
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 16 * 1024 * 1024); // 16 MiB
} events SEC(".maps");

static __always_inline void trace_sock_event(struct pt_regs *ctx, struct sock *sk, __u8 evt_type)
{
    struct event *e;
    __u64 ts = bpf_ktime_get_ns();
    __u32 pid = bpf_get_current_pid_tgid() & 0xFFFFFFFF;

    // Reserve space in the ring buffer
    e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (!e)
        return;

    // Populate only the minimal fields:
    e->timestamp_ns = ts;
    e->pid = pid;
    e->event_type = evt_type;
    e->af = BPF_CORE_READ(sk, __sk_common.skc_family);

    // NEW: emit sock_id (pointer value) for user-space pairing
    e->sock_id = (u64)sk;

    struct tcp_sock *ts_ptr = bpf_skc_to_tcp_sock(sk);
    if (ts_ptr)
    {
        e->srtt_us = BPF_CORE_READ(ts_ptr, srtt_us) >> 3;
    }

    // ports in host order
    e->sport = BPF_CORE_READ(sk, __sk_common.skc_num);
    e->dport = bpf_ntohs(BPF_CORE_READ(sk, __sk_common.skc_dport));

    if (e->af == AF_INET)
    {
        // IPv4
        e->saddr.v4 = BPF_CORE_READ(sk, __sk_common.skc_rcv_saddr);
        e->daddr.v4 = BPF_CORE_READ(sk, __sk_common.skc_daddr);
    }
    else if (e->af == AF_INET6)
    {
        // IPv6
        __u32 *s6 = (__u32 *)&e->saddr.v6;
        __u32 *d6 = (__u32 *)&e->daddr.v6;
        BPF_CORE_READ_INTO(&s6[0], sk, __sk_common.skc_v6_rcv_saddr.in6_u.u6_addr32[0]);
        BPF_CORE_READ_INTO(&s6[1], sk, __sk_common.skc_v6_rcv_saddr.in6_u.u6_addr32[1]);
        BPF_CORE_READ_INTO(&s6[2], sk, __sk_common.skc_v6_rcv_saddr.in6_u.u6_addr32[2]);
        BPF_CORE_READ_INTO(&s6[3], sk, __sk_common.skc_v6_rcv_saddr.in6_u.u6_addr32[3]);
        BPF_CORE_READ_INTO(&d6[0], sk, __sk_common.skc_v6_daddr.in6_u.u6_addr32[0]);
        BPF_CORE_READ_INTO(&d6[1], sk, __sk_common.skc_v6_daddr.in6_u.u6_addr32[1]);
        BPF_CORE_READ_INTO(&d6[2], sk, __sk_common.skc_v6_daddr.in6_u.u6_addr32[2]);
        BPF_CORE_READ_INTO(&d6[3], sk, __sk_common.skc_v6_daddr.in6_u.u6_addr32[3]);
    }

    bpf_ringbuf_submit(e, 0);
}

SEC("fentry/tcp_sendmsg")
int BPF_PROG(handle_tcp_sendmsg, struct sock *sk)
{
    trace_sock_event((struct pt_regs *)ctx, sk, EVENT_TYPE_TCP_SEND);
    return 0;
}

// Capture send exit
SEC("fexit/tcp_sendmsg")
int BPF_PROG(handle_tcp_sendmsg_ret, struct sock *sk)
{
    trace_sock_event((struct pt_regs *)ctx, sk, EVENT_TYPE_TCP_SEND_EXIT);
    return 0;
}

SEC("fentry/tcp_rcv_established")
int BPF_PROG(handle_tcp_rcv, struct sock *sk)
{
    trace_sock_event((struct pt_regs *)ctx, sk, EVENT_TYPE_TCP_RECV);
    return 0;
}

// Capture receive exit (deliver to user space)
SEC("fexit/tcp_recvmsg")
int BPF_PROG(handle_tcp_recvmsg_ret, struct sock *sk)
{
    trace_sock_event((struct pt_regs *)ctx, sk, EVENT_TYPE_TCP_RECV_EXIT);
    return 0;
}

// Only GPL-compatible licenses can use all BPF features <https://github.com/torvalds/linux/blob/master/include/linux/license.h>
char LICENSE[] SEC("license") = "GPL";
