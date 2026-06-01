# trix in Docker / containerised environments

This page explains how to use trix when your application runs inside Docker
containers — including multi-container (microservice) setups.

---

## Two kinds of tracing

Before choosing a backend it helps to be clear about what you want to observe:

| Kind | What it captures | Who writes it |
|------|-----------------|---------------|
| **System tracing** | CPU scheduling, context switches, syscalls, IRQs, kernel events | The kernel — no code changes needed |
| **trix user tracing** | Your `TRIX_FRAME_*`, `TRIX_ALGO_*`, `TRIX_DATA_*` spans and counters | Your application, via `libtrix.so` |

Most backends deliver **both** in the same trace file — system events give
you the full CPU picture; trix events tell you what your code was doing.
In a Docker setup the key question is which side of the container boundary
each half lives on.

---

## Backend comparison for Docker

### ftrace ✅ Recommended

ftrace lives in the **host kernel**. Every process — whether running bare-metal
or inside a container — writes to the same kernel ring-buffer via
`/sys/kernel/tracing/trace_marker`.

```
┌─────────────────────────────────────────────────────┐
│  Host kernel (ftrace ring-buffer)                   │
│  ← service-A (container 1)  tracing_mark_write      │
│  ← service-B (container 2)  tracing_mark_write      │
│  ← service-C (container 3)  tracing_mark_write      │
│  ← kernel    sched_switch / syscalls / IRQs         │
└─────────────────────────────────────────────────────┘
```

**Requirements:**

- Run the capture scripts (`capture_ftrace_pre.sh` / `capture_ftrace_post.sh`)
  on the **host**, not inside the containers.
- Each container needs `tracefs` accessible. Either:
  - Run `capture_ftrace_pre.sh` on the host (mounts tracefs there), **or**
  - Add `--privileged` or `--cap-add SYS_ADMIN` to the container that needs
    to write markers (standard trix usage already works without this for
    `trace_marker` writes if the host script has opened the buffer).
- `libtrix.so` must be present inside each container (copy it in, or mount
  it via a volume).

**What you get:** a single trace file with all services and the host kernel
on a **unified timeline** — context switches between containers are visible,
idle time is accounted for, and trix spans from every service appear together.

---

### perf (SDT uprobes) ⚠️ More complex

perf SDT probes are registered against a specific file path **on the host**.
Inside a container, `libtrix.so` lives under the container's overlay
filesystem (e.g. `/var/lib/docker/overlay2/<hash>/merged/usr/lib/libtrix.so`).

Options:

1. **Register probes against the host path** — find the overlay path and run
   `perf probe -x <host_overlay_path_to_libtrix.so> "sdt_trix:<name>"`.
   Fragile; the path changes on every container restart.

2. **Privileged container** — run the container with `--pid=host --privileged`,
   then run `capture_perf_pre.sh` from inside the container.  This works but
   gives the container full host access.

3. **Use ftrace instead** — for containerised workloads, ftrace is strictly
   simpler.

**System tracing with perf:** `perf record -a -e sched:sched_switch` captures
host-wide scheduling regardless of containers — this part works fine.
The complexity is only in registering SDT (user-space) probes.

---

### ITT / VTune ⚠️ Attach mode required

VTune must attach to (or launch) the process it traces. In a container context:

- Run VTune on the **host** and attach by PID:
  ```bash
  vtune -collect hotspots -target-pid <container_pid> -r result/
  ```
- The container PID is the PID visible on the host (containers share the
  host PID namespace by default unless `--pid=host` is used).
- VTune HW sampling requires `--cap-add SYS_PTRACE --cap-add PERFMON` or
  `--privileged` on the container.

**System tracing with VTune:** VTune's platform profiler captures OS
scheduler data host-wide. ITT task spans from `libtrix.so` only appear for
the attached process.

---

### ETW (Windows) — not applicable

ETW is host-wide by design on Windows. Container support depends on the
Windows container type (process-isolated vs Hyper-V). Process-isolated
containers share the host ETW session — trix ETW events appear in the host
trace automatically.

---

## Recommended setup for microservices

1. **Use the ftrace backend** in every service.
2. Mount `libtrix.so` into each container via a volume, or bake it into your
   base image.
3. Run `capture_ftrace_pre.sh` on the **host** before starting (or after
   starting) the containers.
4. Exercise your workload.
5. Run `capture_ftrace_post.sh` on the **host**.
6. Open the resulting `.txt` file in [Perfetto UI](https://ui.perfetto.dev) —
   it is native ftrace format, drag-and-drop works directly.

```bash
# Host — start capture
sudo sh scripts/capture_ftrace_pre.sh

# Containers run normally — trix writes trace_marker through the host kernel

# Host — stop and collect
sudo sh scripts/capture_ftrace_post.sh
# → trix_ftrace_YYYYMMDD_HHMMSS.txt

# Open in Perfetto UI (no conversion needed)
```

The resulting trace contains:
- **trix spans** from every container, labelled by process name and PID
- **Context switches** showing when each service was scheduled in/out
- **Syscall latency** (if enabled in the capture scripts)
- All events on a **single shared clock** — inter-service latency is directly
  measurable

---

## Caveats

- **PID namespacing:** inside a container, `getpid()` returns the container-local
  PID. The host kernel timestamps events with the **host PID**. In Perfetto
  the process will appear under the host PID, not the container-local one.
  Use process names (set via `pthread_setname_np` or the binary name) to
  identify services.

- **Clock sync:** all backends use `CLOCK_MONOTONIC` or the kernel's ftrace
  clock — containers share these with the host, so timestamps are directly
  comparable.

- **Buffer size:** with many services writing simultaneously, increase the
  ftrace ring-buffer: set `TRIX_BUFFER_KB=65536` (per CPU) before running
  `capture_ftrace_pre.sh`.
