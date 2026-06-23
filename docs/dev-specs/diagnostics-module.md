# Diagnostics Module Spec - nordic-wifi-audio

## Document Information

| Field | Value |
|---|---|
| Project | Nordic Wi-Fi Audio Demo |
| Version | 2026-06-22-15-18 |
| PRD Version | 2026-06-22-15-18 |
| NCS Version | v3.3.0 |
| Target Board(s) | Board-agnostic (kernel-only, no hardware peripherals) |
| Status | In Review |

## Changelog

| Version | Summary of changes |
|---|---|
| 2026-06-22-15-18 | Initial spec: memonitor brick consumption + status shell command |

---

## Overview

Memory and thread diagnostics are provided by the `zego/memonitor` brick, which
replaces the custom `src/debug/heaps_monitor.c`. The app adds a thin `status` shell
command that reads the memonitor cache and prints a formatted report.

| Component | Role |
|---|---|
| `zego/bricks/memonitor` | Periodic heap/stack watermark sampler, MEMONITOR_CHAN |
| `status` shell command | Reads cache via `memonitor_get_heaps/threads()`; prints |

---

## zego/memonitor Brick

**Spec**: `zego/bricks/memonitor/docs/memonitor-spec.md`

The brick fires on the system workqueue every `CONFIG_ZEGO_MEMONITOR_INTERVAL_MS`
(default 5000 ms) and:
1. Iterates `_k_heap_list` linker section — captures free/used/watermark for each named heap.
2. Iterates live threads via `k_thread_foreach()` — captures stack HWM via 0xAA fill counting.
3. Caches results in a spinlock-protected static buffer.
4. Publishes a 12-byte `MEMONITOR_CHAN` notification.

Consumers call `memonitor_get_heaps()` / `memonitor_get_threads()` for thread-safe copies.

---

## Status Shell Command

A thin `status` shell command registered in `src/debug/` (or `wifi_audio_gateway/main.c`):

```c
static int cmd_status(const struct shell *sh, size_t argc, char **argv)
{
    struct memonitor_heap_entry heaps[MEMONITOR_MAX_HEAPS];
    struct memonitor_thread_entry threads[MEMONITOR_MAX_THREADS];
    int n_heaps, n_threads;

    memonitor_get_heaps(heaps, ARRAY_SIZE(heaps), &n_heaps);
    memonitor_get_threads(threads, ARRAY_SIZE(threads), &n_threads);

    shell_print(sh, "=== Heap watermarks ===");
    for (int i = 0; i < n_heaps; i++) {
        shell_print(sh, "  %-28s free=%5u  used=%5u  hwm=%5u  total=%5u",
            heaps[i].name, heaps[i].free, heaps[i].used,
            heaps[i].watermark, heaps[i].total);
    }
    shell_print(sh, "=== Thread stack HWMs ===");
    for (int i = 0; i < n_threads; i++) {
        shell_print(sh, "  %-20s %-10s  hwm=%4u / %4u",
            threads[i].name, threads[i].state,
            threads[i].stack_hwm, threads[i].stack_size);
    }
    return 0;
}
SHELL_CMD_REGISTER(status, NULL, "Print memory/thread diagnostics", cmd_status);
```

---

## Zbus Integration

| Channel | Direction | Notes |
|---|---|---|
| `MEMONITOR_CHAN` | Subscribe (implicit) | Subscriber can use channel as a "snapshot ready" notification; data read via API |

---

## Kconfig Flags

| Symbol | Description | Default |
|---|---|---|
| `CONFIG_ZEGO_MEMONITOR` | Enable memonitor brick | y |
| `CONFIG_ZEGO_MEMONITOR_INTERVAL_MS` | Sampling interval (ms) | 5000 |
| `CONFIG_ZEGO_MEMONITOR_HEAP_MONITOR` | Sample heaps (requires `SYS_HEAP_RUNTIME_STATS`) | y |
| `CONFIG_ZEGO_MEMONITOR_THREAD_MONITOR` | Sample threads (requires `INIT_STACKS`) | y |
| `CONFIG_ZEGO_MEMONITOR_ZVIEW` | Auto-select ZView Kconfig deps | y |
| `CONFIG_ZEGO_MEMONITOR_LOG_PERIODIC` | Log to UART on each sample (flash-expensive) | n |
| `CONFIG_INIT_STACKS` | Fill stacks with 0xaa (required for HWM) | y |
| `CONFIG_STACK_SENTINEL` | **Must be n** — sentinel overwrites 0xaa fill | n |

---

## Memory

- Heap cache: `struct memonitor_heap_entry[8]` = 384 B BSS
- Thread cache: `struct memonitor_thread_entry[32]` = 1664 B BSS
- Total brick BSS: ~4 KB constant (board-agnostic)

---

## Test Points

| UART log string | Expected condition |
|---|---|
| `[memonitor] Heap: _system_heap free=...` | Logged if `CONFIG_ZEGO_MEMONITOR_LOG_PERIODIC=y` |
| `status` shell output: heap and thread table | `status` shell command executed |
