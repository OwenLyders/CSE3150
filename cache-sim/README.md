# Cache Simulator: FIFO vs LRU

CSE 3150, Fall 2026. A C++17 simulator that replays one memory-access trace through two
identical set-associative caches, one evicting with FIFO and one with LRU, and shows them
side by side. Each step splits the address into tag, index and offset, lights up the
matching set in both caches, and updates each policy's hit rate.

There are three programs, built on one core library that contains no graphics code:

| Program | What it does |
|---|---|
| `cachesim_gui` | Allegro 5 window: step through a trace and watch both caches |
| `cachesim_cli` | Prints accesses, hits, misses, evictions, write-backs and hit rate per policy |
| `cachesim_tests` | Unit tests for the core, run with `ctest` |

## Building

You need a C++17 compiler and CMake 3.16 or newer. The GUI also needs Allegro 5 and
pkg-config. The core, CLI and tests build without Allegro: if CMake can't find it, it
skips the GUI and prints a warning.

| Platform | Install |
|---|---|
| macOS | `brew install cmake pkgconf allegro` |
| Ubuntu | `sudo apt install build-essential cmake pkg-config liballegro5-dev` |
| Windows (not tested yet) | MSYS2 with its gcc, cmake, pkgconf and allegro packages, or vcpkg (`vcpkg install allegro5`) |

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build        # needs CMake 3.20+; on older CMake: cd build && ctest
```

With a Visual Studio generator on Windows, the programs end up in `build\Debug\` instead of
`build/`, and the tests run with `ctest --test-dir build -C Debug`.

## Running

### Command-line runner

```sh
./build/cachesim_cli                                   # all five built-in traces
./build/cachesim_cli --builtin hot-cold --ways 2       # one built-in, a 2-way cache
./build/cachesim_cli --trace traces/lru-wins.trace     # a trace file
./build/cachesim_cli --help
```

```
Cache:   64 B cache, 4 B blocks, 4-way set associative (4 sets x 4 ways)
Address: 16 bits = 12 tag + 2 index + 2 offset

hot-cold: A few blocks reused often, mixed with blocks used once (200 requests)
  Policy    Accesses    Hits  Misses  Evictions  Write-backs  Hit rate
  FIFO           200     106      94         78           18     53.0%
  LRU            200     124      76         60            8     62.0%
```

### GUI

```sh
./build/cachesim_gui                                   # starts on the first built-in trace
./build/cachesim_gui --builtin hot-cold
./build/cachesim_gui --cache 64 --block 4 --ways 2 --trace traces/fifo-wins.trace
```

| Key | Action |
|---|---|
| Space | Step one request (pauses playback) |
| P | Play or pause (at the end of a trace, plays it again from the start) |
| Up / Down | Faster or slower playback (0.5 to 128 requests per second) |
| R | Reset to the start of the trace |
| T | Next trace (a `--trace` file comes first, then the five built-ins) |
| Esc | Quit |

What the window shows:

- **Top bar:** the current request in hex, and its address in binary with the tag bits in
  blue, the index bits in lavender and the offset bits in cyan. On the right, whether each
  policy hit or missed, and what it evicted. Before the first step it previews the first request.
- **Left:** the trace, with the latest request highlighted, `>` on the next one, and H or M
  for each policy.
- **Middle:** one grid per policy, one row per set and one column per way. Each line shows its
  valid bit, tag (blue), dirty flag (pink `D`) and data bytes. The colors match the address:
  the current set is lit in lavender and the byte at the offset is cyan.
- **Feedback on each step:** the line that was touched flashes green (hit) or red (miss). A
  line that just evicted a block shows the old tag struck through in amber for one step, plus
  "written back" if it was dirty. In every full set (with 2 or more ways), a dashed amber
  outline marks the line that policy will evict next, so you can see FIFO and LRU disagree
  before it happens.
- **Bottom:** the counters and hit rate for each policy, and the keys.

Two extra flags help with the write-up: `--steps N` starts with N requests already processed,
and `--screenshot out.png` draws one frame, saves it and quits.

```sh
./build/cachesim_gui --builtin hot-cold --steps 40 --screenshot hot-cold-40.png
```

### Cache settings

Both programs take the same flags:

| Flag | Meaning | Default |
|---|---|---|
| `--cache BYTES` | total cache size (power of two) | 64 |
| `--block BYTES` | block size (power of two) | 4 |
| `--ways N` | associativity (power of two); 1 = direct-mapped | 4 |
| `--address-bits N` | address width, 1 to 24 bits | 16 |
| `--trace FILE` / `--builtin NAME` | which trace to run | all built-ins (CLI), first built-in (GUI) |

Main memory (2^address-bits bytes) must be at least twice the cache, and the cache can be at
most 64 KiB. The GUI draws at most 32 ways; the CLI takes any valid setting. Bad settings are
rejected at startup with a message such as `associativity must be a power of two (got 3)`.
Use 2 or more ways for the demo: with 1 way a block has only one possible line, so FIFO and
LRU always behave the same.

## Traces

A trace file has one request per line:

```
# R <address>   or   W <address> <byte>
R 0x1A2E
W 0x1A30 0x7F
R 0x1A2E
```

Numbers are hex (`0x1A2E`) or decimal. Blank lines and `#` comments are skipped, and a
malformed line stops loading with its line number, for example
`traces/bad.trace:4: expected R or W, got 'X'`. Save trace files as plain ASCII or UTF-8.
Windows PowerShell's `>` writes UTF-16, which the reader rejects with a message saying so.

Sample files in `traces/` (run them with the default settings):

| File | Shows |
|---|---|
| `example.trace` | the example from the design plan |
| `lru-wins.trace` | the plan's worked example on a 4-way set: FIFO 1 hit, LRU 2 hits |
| `fifo-wins.trace` | LRU doesn't always win: FIFO 4 hits, LRU 1 hit |
| `write-back.trace` | a dirty block written back on eviction, then read back from memory |

### Built-in traces

Each built-in sizes itself from the cache settings. Results with the default settings
(64 B cache, 4 B blocks, 4-way):

| Trace | Pattern | Plan's hypothesis | FIFO | LRU |
|---|---|---|---|---|
| `sequential` | reads 0, 1, 2, ... over twice the cache size | identical; hits come from neighbors in a block | 75.0% | 75.0% |
| `small-loop` | 8 passes over exactly as many blocks as the cache holds | both near 100% after warm-up | 87.5% | 87.5% |
| `oversized-loop` | 16 passes over ways + 1 blocks that share a set | both miss on almost every request | 0.0% | 0.0% |
| `hot-cold` | 3 in 4 requests hit a few hot blocks (a quarter of them writes); the rest read cold blocks once | LRU ahead | 53.0% | 62.0% |
| `random` | uniform random addresses over 4x the cache size, 1 in 4 writes, fixed seed | roughly equal | 19.5% | 19.0% |

The small loop's 87.5% is 16 cold misses out of 128 requests; after warm-up it never misses.
`hot-cold` and `random` get longer as the cache grows (25 requests per hot block and 12 per
cache block, from 200 up to 20,000), so a big cache still gets past warm-up. With a 1 KiB
cache, `hot-cold` gives FIFO 59.0% and LRU 69.9%.

## How the core works

The code is in `src/core`. Every file is small enough to walk through in a review.

| File | Responsibility |
|---|---|
| `CacheConfig` | the four settings, `validate()`, bit widths, `split(address)` |
| `MemRequest` | one trace entry: read or write, address, value |
| `MainMemory` | the backing store, a `std::vector<uint8_t>` |
| `CacheLine` | valid, dirty, tag, stamp, data bytes |
| `ReplacementPolicy` | abstract base: `onLoad`, `onHit`, `chooseVictim` |
| `FifoPolicy`, `LruPolicy` | one header each |
| `PolicyFactory` | `PolicyKind` and `makePolicy()` |
| `Cache` | sets of lines plus one policy (`std::unique_ptr`); `access()` returns an `AccessResult` |
| `AccessResult`, `CacheStats` | what one request did; the running counters |
| `TraceReader`, `TraceGenerator` | read trace files; build the five built-in traces |
| `Simulator` | owns the trace and one cache per policy; `step()`, `run()`, `reset()` |
| `Options`, `Format` | command-line flags and text helpers shared by both programs |

`Cache::access` follows the plan's six steps:

1. Split the address into tag, set index and offset.
2. Search that set for a valid line with a matching tag.
3. On a hit, tell the policy (LRU re-stamps the line).
4. On a miss, use a free line if there is one.
5. Otherwise ask the policy for a victim and write it back to memory first if it's dirty, then
   load the new block.
6. Read or write the byte at the offset (a write marks the line dirty) and update the counters.

FIFO and LRU differ by one line: both stamp a line from a counter when a block is loaded and
evict the smallest stamp, but only `LruPolicy::onHit` re-stamps the line.

Each cache has its own copy of main memory, so one policy's write-backs never change what the
other loads. Every memory byte starts as the low byte of its own address (`0x1A2E` holds
`0x2E`), so a loaded block shows where it came from and a written value stands out.

### Adding a third policy

1. Write `src/core/RandomPolicy.h` (or LFU), modeled on `FifoPolicy.h`.
2. Add a value to `PolicyKind` and a case to `makePolicy()` in `PolicyFactory`.
3. Pass it to the `Simulator` (the default list is `{Fifo, Lru}`). The CLI prints one row per
   policy and the GUI draws one grid per policy.

`chooseVictim()` is `const` because the GUI calls it to draw the next-victim outline, and asking
must not change the answer. A Random policy can still reuse the stamp: give each line a random
stamp in `onLoad()`, do nothing in `onHit()`, and keep evicting the smallest stamp. Each block
then leaves in a random order fixed when it was loaded.

## Tests

`ctest --test-dir build` runs ten areas, one per file in `tests/`. You can also run one area
directly, for example `./build/cachesim_tests cache`. They cover every test in the design plan:

| Plan test | Where |
|---|---|
| Address split (0x1A2E gives tag 0x1A2, set 3, offset 2) | `test_config.cpp` |
| Config validation | `test_config.cpp`, `test_options.cpp` |
| Load then hit | `test_cache.cpp` |
| FIFO and LRU sequences (A B C A D A on 3 lines) | `test_policies.cpp`; the 4-way version in `test_cache.cpp` |
| FIFO ahead (A B A C B on 2 lines) | `test_policies.cpp`, `test_cache.cpp` |
| Direct-mapped: FIFO and LRU identical on a random trace | `test_simulator.cpp` |
| Write-back | `test_cache.cpp`, `test_simulator.cpp` |
| Counters: hits + misses = accesses on every trace | `test_simulator.cpp` |
| Trace parsing and line numbers | `test_trace_reader.cpp` |

The tests also check that each sample trace's stated result is true, that each cache has its own
memory, and that `reset()` replays identically. The test framework is the short
`tests/TestFramework.h`. Unlike `assert`, its `CHECK` keeps going after a failure and still runs
in release builds.

The plan's manual check, "the GUI and CLI report the same counts", is quick: run
`cachesim_gui --trace traces/lru-wins.trace` to the end and compare the bottom bar with
`cachesim_cli --trace traces/lru-wins.trace`.

## Choices made where the plan was open

- **Address width** is range-checked (1 to 24 bits) instead of power-of-two checked. It's a bit
  count, and each cache allocates all of memory, so the cap keeps that at 16 MiB.
- **Memory must be at least twice the cache.** Otherwise the loop traces can't reach enough
  distinct blocks to evict anything. The cache is capped at 64 KiB because the built-in
  traces grow with it.
- **The GUI refuses more than 32 ways**, which don't fit in a grid panel. The CLI has no such limit.
- **Write policy** is write-back with write-allocate: a write miss loads the block, then writes it.
- **The 3-line worked example** can't be a real config, because 3 ways isn't a power of two. It's
  tested against the policies directly, and a 4-way version runs through the full cache
  (`traces/lru-wins.trace`).
- **The random trace** covers 4x the cache size. Over the whole 64 KiB address space both
  policies would sit near 0%.
- **Random numbers** come from `std::mt19937` with seed 3150, reduced with `%`.
  `std::uniform_int_distribution` gives different results on different compilers, and we want
  the same traces on everyone's machine.
- **Allegro** is found through pkg-config first. Homebrew's Allegro CMake package points at a
  folder that doesn't exist, so it's only the fallback (for vcpkg), with that path filtered out.

## Not done yet

- The stretch goals: hit-rate graph, sweep mode, CSV export, a third policy, stepping backward.
- M4: the results write-up and a rehearsed demo. The numbers above are a starting point.
- The plan's open questions (due date, who takes which workstream, demo settings) are still open.
- The Windows build hasn't been tried.

## Layout

```
cache-sim/
  CMakeLists.txt
  README.md
  AI_USAGE.md
  src/
    core/     CacheConfig, MainMemory, Cache, policies, Simulator, trace code
    cli/      main.cpp
    gui/      main.cpp, App (playback state), Renderer, Input
  tests/      one file per area, plus TestFramework.h and test_main.cpp
  traces/     sample .trace files
  assets/     JetBrains Mono font (SIL Open Font License, see JetBrainsMono-OFL.txt)
```
