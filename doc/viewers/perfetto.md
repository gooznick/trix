# Perfetto

Perfetto is a browser-based trace viewer and analysis platform developed by
Google. It runs entirely in the browser — trace files are never uploaded to
any server.

- [Official documentation](#official-documentation)
- [Supported trace formats](#supported-trace-formats)
- [Opening a trace](#opening-a-trace)
- [Navigation](#navigation)
- [Using on an internal network](#using-on-an-internal-network)

---

## Official documentation

| Resource | URL |
|----------|-----|
| Documentation | https://perfetto.dev/docs/ |
| UI (online) | https://ui.perfetto.dev |
| Source and releases | https://github.com/google/perfetto |

Press **?** inside the UI to show all keyboard shortcuts.

---

## Supported trace formats

| trix backend | File produced | Format | Conversion needed |
|---|---|---|---|
| `ftrace` | `trix_ftrace_*.txt` | Raw ftrace text | No — Perfetto reads it natively |
| `perf` | `trix_perf_*.pftrace` | Chrome trace JSON | Yes — run `scripts/perf_to_perfetto.py` first |
| `lttng` | `trix_lttng_*.pftrace` | Chrome trace JSON | Yes — run `scripts/lttng_to_perfetto.py` first |

For perf and lttng, run the converter before opening in Perfetto:

```bash
python3 scripts/perf_to_perfetto.py  trix_perf_*.txt   -o trace.pftrace
python3 scripts/lttng_to_perfetto.py trix_lttng_*.txt  -o trace.pftrace
```

---

## Opening a trace

1. Go to **https://ui.perfetto.dev** (or your internal instance — see below)
2. Click **Open trace**
3. Select or drag-and-drop the trace file

If the trace was captured on a remote machine, copy it first:

```bash
scp user@target:~/trix_ftrace_*.txt .
```

---

## Navigation

| Action | Keyboard / Mouse |
|--------|-----------------|
| Zoom in / out | **W** / **S** or scroll wheel |
| Pan left / right | **A** / **D** or click-and-drag |
| Select a span | Click it |
| Select and zoom to a time range | Click-and-drag on the timeline ruler |
| Search for a span or thread by name | **Ctrl+F** |
| Expand a track group | Click the **▶** arrow |
| Show all keyboard shortcuts | **?** |

After clicking a span, the **Details** panel at the bottom shows its name,
start time, duration, and depth. For counter tracks, clicking a data point
shows the exact value and timestamp.

### What trix traces look like

- **Thread rows** — one row per thread. Spans are nested: `frame_N` contains
  `algo_begin` regions which may contain further `algo_begin` regions.
- **Counter tracks** — appear below the thread row that emits them.
  `TRIX_DATA_INT` and `TRIX_DATA_FLOAT` values plot as step graphs.
- **String events** (`TRIX_DATA_STRING`) — appear as zero-duration spans
  named `key=value`, visible when zoomed in.
- **CPU lanes** — at the top of the trace, one row per logical CPU core.
  Coloured bars show which thread held each core. Requires scheduling events
  in the trace (enabled by default in the capture scripts).


---

## Using on an internal network

A `Dockerfile` that builds the Perfetto web UI is provided in `tools/perfetto-ui/`.
The build clones Perfetto v55.3 and compiles the frontend — it takes a few minutes
on first run.

```bash
docker build -t perfetto-ui tools/perfetto-ui/
```

Run the UI:

```bash
docker run -it --rm --network=host perfetto-ui:latest
```

Then open **http://localhost:10000** in a browser.
