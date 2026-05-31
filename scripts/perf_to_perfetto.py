#!/usr/bin/env python3
"""
perf_to_perfetto.py — Convert perf script output to Perfetto/Chrome trace JSON.

Input:  output of:
  perf script -i FILE.data -F comm,tid,cpu,time,event,trace --show-mmap-events

Output: Chrome trace format JSON, loadable in https://ui.perfetto.dev

Usage:
  python3 scripts/perf_to_perfetto.py INPUT.txt [-o OUTPUT.pftrace]

Track layout:
  pid=1  Thread spans   (frame_N, algo spans nested by thread)
  pid=2  CPU lanes      (which thread ran on which CPU, from sched_switch)
  pid=3  Counters       (data_float / data_int values)

NOTE — string encoding:
  The perf backend packs the first 8 chars of each string into a uint64
  (little-endian).  This converter decodes them back to readable names.
  Names longer than 8 chars are truncated (e.g. "correlate" -> "correlat").
"""

from __future__ import print_function

import sys
import re
import json
import struct
import argparse
from collections import defaultdict


# ---------------------------------------------------------------------------
# Parsing
# ---------------------------------------------------------------------------

# Matches:  comm  tid  [cpu]  timestamp:  event:  trace
_LINE_RE = re.compile(
    r'\s*(.+?)\s+(\d+)\s+\[(\d+)\]\s+(\d+\.\d+):\s+(\S+):\s*(.*)'
)


def parse_line(line):
    """Return parsed dict or None for unrecognised / MMAP lines."""
    if 'PERF_RECORD_MMAP' in line:
        return None
    m = _LINE_RE.match(line)
    if not m:
        return None
    comm, tid, cpu, ts_str, event, trace = m.groups()
    return {
        'comm':  comm.strip(),
        'tid':   int(tid),
        'cpu':   int(cpu),
        'ts':    float(ts_str),
        'event': event,
        'trace': trace.strip(),
    }


def parse_int_args(trace):
    """Return dict {arg_number: int_value} from 'arg1=N arg2=M ...' trace."""
    return {int(k): int(v) for k, v in re.findall(r'arg(\d+)=(-?\d+)', trace)}


def unpack_str8(val):
    """Decode a little-endian packed 8-char string from a uint64 integer."""
    chars = []
    for i in range(8):
        b = (val >> (i * 8)) & 0xff
        if b == 0:
            break
        chars.append(chr(b) if 32 <= b < 127 else '?')
    return ''.join(chars) if chars else str(val)


def bits_to_float(bits):
    """Reinterpret the lower 32 bits of an integer as an IEEE 754 float."""
    try:
        return struct.unpack('f', struct.pack('I', bits & 0xFFFFFFFF))[0]
    except Exception:
        return float(bits)


# ---------------------------------------------------------------------------
# Conversion
# ---------------------------------------------------------------------------

def convert(input_path):
    # --- read and parse all lines ---
    rows = []
    with open(input_path) as fh:
        for line in fh:
            r = parse_line(line)
            if r is not None:
                rows.append(r)

    if not rows:
        sys.exit("ERROR: no events parsed from " + input_path)

    min_ts = min(r['ts'] for r in rows)
    max_ts = max(r['ts'] for r in rows)

    def us(ts):
        return (ts - min_ts) * 1e6

    # --- per-TID stacks for begin/end matching ---
    frame_stack = defaultdict(list)   # tid -> [(ts, frame_num)]
    algo_stack  = defaultdict(list)   # tid -> [(ts, ptr)]

    # --- CPU sched tracking ---
    cpu_on = {}   # cpu -> (ts, tid, comm)  — the thread currently running

    # --- collected Perfetto events ---
    trace_events = []

    def X(name, pid, tid, begin_ts, end_ts, extra_args=None):
        """Append a complete (X) duration event."""
        dur = us(end_ts) - us(begin_ts)
        if dur < 0:
            dur = 0.001
        ev = {
            "name": name, "ph": "X",
            "pid": pid, "tid": tid,
            "ts": us(begin_ts), "dur": max(0.001, dur),
        }
        if extra_args:
            ev["args"] = extra_args
        trace_events.append(ev)

    thread_comms = {}   # tid -> most-recent comm

    # --- main loop ---
    for r in rows:
        tid   = r['tid']
        cpu   = r['cpu']
        ts    = r['ts']
        comm  = r['comm']
        event = r['event']
        trace = r['trace']

        thread_comms[tid] = comm

        if event == 'sdt_trix:frame_begin':
            args = parse_int_args(trace)
            frame_stack[tid].append((ts, args.get(1, 0)))

        elif event == 'sdt_trix:frame_end':
            if frame_stack[tid]:
                begin_ts, fnum = frame_stack[tid].pop()
                X("frame_{0}".format(fnum), 1, tid, begin_ts, ts,
                  {"frame": fnum})

        elif event == 'sdt_trix:algo_begin':
            args = parse_int_args(trace)
            algo_stack[tid].append((ts, args.get(1, 0)))

        elif event == 'sdt_trix:algo_end':
            if algo_stack[tid]:
                begin_ts, packed = algo_stack[tid].pop()
                X(unpack_str8(packed), 1, tid, begin_ts, ts)

        elif event == 'sched:sched_switch':
            m_prev = re.search(r'prev_pid=(\d+)', trace)
            m_next = re.search(r'next_pid=(\d+)', trace)
            m_nc   = re.search(r'next_comm=(\S+)', trace)
            if not (m_prev and m_next):
                continue
            next_pid  = int(m_next.group(1))
            next_comm = m_nc.group(1) if m_nc else str(next_pid)

            # close the slice for whatever was running on this CPU
            if cpu in cpu_on:
                sched_ts, sched_tid, sched_comm = cpu_on.pop(cpu)
                if sched_tid != 0:   # skip idle
                    X(sched_comm, 2, cpu, sched_ts, ts,
                      {"tid": sched_tid})

            # start a new slice for the incoming thread
            if next_pid != 0:
                cpu_on[cpu] = (ts, next_pid, next_comm)

        elif event == 'sdt_trix:data_int':
            args  = parse_int_args(trace)
            name  = unpack_str8(args.get(1, 0))
            value = args.get(2, 0)
            trace_events.append({
                "name": name, "ph": "C",
                "pid": 1, "tid": tid,
                "ts": us(ts),
                "args": {name: value},
            })

        elif event == 'sdt_trix:data_float':
            args  = parse_int_args(trace)
            name  = unpack_str8(args.get(1, 0))
            value = bits_to_float(args.get(2, 0))
            trace_events.append({
                "name": name, "ph": "C",
                "pid": 1, "tid": tid,
                "ts": us(ts),
                "args": {name: value},
            })

        elif event == 'sdt_trix:data_string':
            args  = parse_int_args(trace)
            name  = unpack_str8(args.get(1, 0))
            value = unpack_str8(args.get(2, 0))
            # Use pid=4, tid=hash(name) so each key gets its own visible row
            row_tid = abs(hash(name)) % 100000
            trace_events.append({
                "name": value, "ph": "X",
                "pid": 4, "tid": row_tid,
                "ts": us(ts), "dur": 15000,
                "args": {name: value},
            })


    seen_cpus = set()
    for cpu_id, (sched_ts, sched_tid, sched_comm) in cpu_on.items():
        seen_cpus.add(cpu_id)
        if sched_tid != 0:
            X(sched_comm, 2, cpu_id, sched_ts, max_ts,
              {"tid": sched_tid})

    # collect all CPUs seen in any sched_switch
    for ev in trace_events:
        if ev.get("pid") == 2 and ev.get("ph") == "X":
            seen_cpus.add(ev["tid"])

    # --- metadata: thread names ---
    for tid, comm in thread_comms.items():
        trace_events.append({
            "name": "thread_name", "ph": "M",
            "pid": 1, "tid": tid,
            "args": {"name": comm},
        })

    # --- metadata: CPU lane row names ---
    for cpu_id in sorted(seen_cpus):
        trace_events.append({
            "name": "thread_name", "ph": "M",
            "pid": 2, "tid": cpu_id,
            "args": {"name": "CPU {0}".format(cpu_id)},
        })

    # --- metadata: process names ---
    for pid, pname in [(1, "Threads"), (2, "CPU Lanes"), (4, "Data")]:
        trace_events.append({
            "name": "process_name", "ph": "M",
            "pid": pid, "tid": 0,
            "args": {"name": pname},
        })

    # --- metadata: Data row names (one row per string key) ---
    seen_data_rows = {}
    for ev in trace_events:
        if ev.get("pid") == 4 and ev.get("ph") == "X":
            row_tid = ev["tid"]
            if row_tid not in seen_data_rows:
                key = list(ev.get("args", {}).keys())
                seen_data_rows[row_tid] = key[0] if key else str(row_tid)
    for row_tid, key_name in seen_data_rows.items():
        trace_events.append({
            "name": "thread_name", "ph": "M",
            "pid": 4, "tid": row_tid,
            "args": {"name": key_name},
        })

    return {"traceEvents": trace_events}


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Convert perf script text to Perfetto JSON (.pftrace)"
    )
    parser.add_argument("input",
                        help="perf script text file "
                             "(-F comm,tid,cpu,time,event,trace --show-mmap-events)")
    parser.add_argument("-o", "--output",
                        help="output file (default: input.pftrace)")
    args = parser.parse_args()

    out_path = args.output
    if out_path is None:
        out_path = re.sub(r'\.[^.]+$', '', args.input) + '.pftrace'

    result = convert(args.input)

    with open(out_path, 'w') as fh:
        json.dump(result, fh, separators=(',', ':'))

    n = len(result['traceEvents'])
    try:
        import os
        size_kb = os.path.getsize(out_path) // 1024
    except Exception:
        size_kb = 0
    print("Written: {0}  ({1} events, {2} KB)".format(out_path, n, size_kb))


if __name__ == '__main__':
    main()
