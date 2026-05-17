#!/usr/bin/env python3
"""
lttng_to_perfetto.py — Convert LTTng-UST babeltrace2 text output to Perfetto JSON.

Input:  output of:
  babeltrace2 /path/to/lttng/session/

  For best results, capture with vpid/vtid/procname context:
    lttng add-context -u -t vpid -t vtid -t procname

Output: Chrome trace format JSON, loadable in https://ui.perfetto.dev

Usage:
  babeltrace2 /path/to/session/ | python3 scripts/lttng_to_perfetto.py [-o out.pftrace]
  python3 scripts/lttng_to_perfetto.py session.txt [-o out.pftrace]

Track layout:
  pid=1  Thread spans   (frame_N, named algo spans, nested durations)
  pid=3  Counters       (data_float / data_int values, with real key names)

Note: unlike the perf converter, all string names are fully resolved because
LTTng-UST copies string values into the trace buffer at record time.
"""

from __future__ import print_function

import sys
import re
import json
import argparse
from collections import defaultdict


# ---------------------------------------------------------------------------
# Parsing
# ---------------------------------------------------------------------------
#
# babeltrace2 text line format (with vpid/vtid/procname context):
#   [HH:MM:SS.nnnnnnnnn] (+delta) hostname event: { cpu_id = N }, { vpid = P, vtid = T, procname = "name" }, { field = val, ... }
#
# without context (no add-context):
#   [HH:MM:SS.nnnnnnnnn] (+delta) hostname event: { cpu_id = N }, { field = val, ... }

_TS_RE   = re.compile(r'\[(\d+):(\d+):(\d+)\.(\d+)\]')
_EVT_RE  = re.compile(r'\]\s+\S+\s+\S+\s+([\w:]+):\s+(.+)')
_INT_RE  = re.compile(r'(\w+)\s*=\s*(-?\d+)')
_STR_RE  = re.compile(r'(\w+)\s*=\s*"([^"]*)"')


def parse_ts(line):
    """Return timestamp in seconds (float) or None."""
    m = _TS_RE.search(line)
    if not m:
        return None
    h, mn, s, ns_str = m.groups()
    ns = int(ns_str.ljust(9, '0')[:9])
    return int(h) * 3600 + int(mn) * 60 + int(s) + ns * 1e-9


def parse_fields(text):
    """Return dict of all int and string fields from babeltrace2 output."""
    fields = {}
    for k, v in _INT_RE.findall(text):
        fields[k] = int(v)
    for k, v in _STR_RE.findall(text):
        fields[k] = v
    return fields


def parse_line(line):
    """Return (ts, event_name, fields_dict) or None."""
    ts = parse_ts(line)
    if ts is None:
        return None
    m = _EVT_RE.search(line)
    if not m:
        return None
    event = m.group(1)
    fields = parse_fields(m.group(2))
    return ts, event, fields


# ---------------------------------------------------------------------------
# Conversion
# ---------------------------------------------------------------------------

def convert(lines):
    rows = []
    for line in lines:
        r = parse_line(line.rstrip('\n'))
        if r is not None:
            rows.append(r)

    if not rows:
        sys.exit("ERROR: no trix events parsed. "
                 "Check that the input is babeltrace2 text output.")

    min_ts = min(r[0] for r in rows)
    max_ts = max(r[0] for r in rows)

    def us(ts):
        return (ts - min_ts) * 1e6

    # per-tid stacks for begin/end matching (LIFO)
    frame_stack = defaultdict(list)   # tid -> [(ts, frame_num)]
    algo_stack  = defaultdict(list)   # tid -> [(ts, name)]

    # CPU lane tracking (from kernel sched_switch)
    cpu_on   = {}   # cpu -> (ts, tid, comm)
    seen_cpus = set()

    trace_events = []
    thread_names = {}   # tid -> procname

    def X(name, pid, tid, begin_ts, end_ts, extra_args=None):
        dur = max(0.001, us(end_ts) - us(begin_ts))
        ev = {
            "name": name, "ph": "X",
            "pid": pid, "tid": tid,
            "ts": us(begin_ts), "dur": dur,
        }
        if extra_args:
            ev["args"] = extra_args
        trace_events.append(ev)

    for ts, event, fields in rows:
        # resolve tid: prefer vtid (thread id), fall back to cpu_id
        tid = fields.get('vtid', fields.get('cpu_id', 0))
        pid = fields.get('vpid', 1)
        procname = fields.get('procname', '')
        if procname and tid not in thread_names:
            thread_names[tid] = procname

        if event == 'trix:frame_begin':
            frame_stack[tid].append((ts, fields.get('frame_num', 0)))

        elif event == 'trix:frame_end':
            if frame_stack[tid]:
                begin_ts, fnum = frame_stack[tid].pop()
                X("frame_{0}".format(fnum), 1, tid, begin_ts, ts,
                  {"frame": fnum})

        elif event == 'trix:algo_begin':
            name = fields.get('name', 'algo')
            algo_stack[tid].append((ts, name))

        elif event == 'trix:algo_end':
            if algo_stack[tid]:
                begin_ts, name = algo_stack[tid].pop()
                # prefer end-event name if available
                end_name = fields.get('name', name)
                X(end_name or name, 1, tid, begin_ts, ts)

        elif event in ('trix:data_float', 'trix:data_int'):
            key   = fields.get('key', 'value')
            value = fields.get('value', 0)
            trace_events.append({
                "name": key, "ph": "C",
                "pid": 1, "tid": tid,
                "ts": us(ts),
                "args": {key: value},
            })

        elif event == 'trix:data_string':
            key   = fields.get('key', 'string')
            value = fields.get('value', '')
            # emit as an instant event (annotation)
            trace_events.append({
                "name": key, "ph": "i",
                "pid": 1, "tid": tid,
                "ts": us(ts), "s": "t",
                "args": {"value": value},
            })

        elif event == 'sched_switch':
            cpu        = fields.get('cpu_id', 0)
            next_tid   = fields.get('next_tid', 0)
            next_comm  = fields.get('next_comm', str(next_tid))
            seen_cpus.add(cpu)

            # close slice for thread leaving this CPU
            if cpu in cpu_on:
                sched_ts, sched_tid, sched_comm = cpu_on.pop(cpu)
                if sched_tid != 0:
                    X(sched_comm, 2, cpu, sched_ts, ts, {"tid": sched_tid})

            # open slice for incoming thread
            if next_tid != 0:
                cpu_on[cpu] = (ts, next_tid, next_comm)

    # close unclosed spans at end of trace (e.g. truncated capture)
    for tid, stack in frame_stack.items():
        for begin_ts, fnum in reversed(stack):
            X("frame_{0}".format(fnum), 1, tid, begin_ts, max_ts)
    for tid, stack in algo_stack.items():
        for begin_ts, name in reversed(stack):
            X(name, 1, tid, begin_ts, max_ts)

    # close open CPU slices
    for cpu_id, (sched_ts, sched_tid, sched_comm) in cpu_on.items():
        if sched_tid != 0:
            X(sched_comm, 2, cpu_id, sched_ts, max_ts, {"tid": sched_tid})

    # metadata: thread names
    for tid, name in thread_names.items():
        trace_events.append({
            "name": "thread_name", "ph": "M",
            "pid": 1, "tid": tid,
            "args": {"name": name},
        })

    # metadata: CPU lane row names (only if sched_switch was captured)
    for cpu_id in sorted(seen_cpus):
        trace_events.append({
            "name": "thread_name", "ph": "M",
            "pid": 2, "tid": cpu_id,
            "args": {"name": "CPU {0}".format(cpu_id)},
        })

    # metadata: process names
    pnames = [(1, "Threads")]
    if seen_cpus:
        pnames.append((2, "CPU Lanes"))
    for pid, pname in pnames:
        trace_events.append({
            "name": "process_name", "ph": "M",
            "pid": pid, "tid": 0,
            "args": {"name": pname},
        })

    return {"traceEvents": trace_events}


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Convert LTTng-UST babeltrace2 text to Perfetto JSON (.pftrace)"
    )
    parser.add_argument("input", nargs='?',
                        help="babeltrace2 text file (omit to read from stdin)")
    parser.add_argument("-o", "--output",
                        help="output file (default: input.pftrace or stdout)")
    args = parser.parse_args()

    if args.input:
        with open(args.input) as fh:
            lines = fh.readlines()
        out_path = args.output or re.sub(r'\.[^.]+$', '', args.input) + '.pftrace'
    else:
        lines = sys.stdin.readlines()
        out_path = args.output or 'lttng_trace.pftrace'

    result = convert(lines)

    with open(out_path, 'w') as fh:
        json.dump(result, fh, separators=(',', ':'))

    import os
    n = len(result['traceEvents'])
    size_kb = os.path.getsize(out_path) // 1024
    print("Written: {0}  ({1} events, {2} KB)".format(out_path, n, size_kb))


if __name__ == '__main__':
    main()
