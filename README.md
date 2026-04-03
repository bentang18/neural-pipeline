# neural-pipeline

Lock-free SPSC neural data pipeline with real-time spike detection in C++17.

## Architecture

Two separate processes communicate over POSIX shared memory. The producer writes samples into a ring buffer backed by `mmap`'d pages. The consumer reads directly from the same physical memory. The IPC is one atomic store (`publish`) and one atomic load (`read_slot`), O(1) regardless of message size.

```
[Producer Process] ──shared memory──→ [Processor Process]
  generate() →                         read directly from
  write into slot                      shared memory slot
  publish() (1 atomic store)           consume() (1 atomic store)
```

Previously used Unix domain sockets (~235us latency). Shared memory brings this down to ~4us.

## Build & Run

```sh
make all && make test

# Terminal 1:
./build/producer_node

# Terminal 2:
./build/processor_node
```

There is also a single-process demo: `./build/pipeline_demo`

## Design Decisions

- **Shared memory over sockets** — `shm_open` + `mmap` maps the same physical pages into both processes. No kernel copies, no syscalls for data transfer. Latency drops from ~235us to ~4us.
- **Fixed-size `SharedSample`** — `float channels[64]` instead of `std::vector<float>`. Vectors store heap pointers that are meaningless across process boundaries. Fixed arrays are flat and contiguous.
- **`alignas(64)` on atomic counters** — `write_pos` and `read_pos` each get their own cache line. Prevents false sharing between producer and consumer cores. Measurable at high throughput (~25% faster at 20M samples/sec), negligible at real neural data rates (30kHz).
- **Ever-increasing monotonic counters** — `write_pos` and `read_pos` never wrap. Occupancy is always `write_pos - read_pos`. Array index is `pos & (capacity - 1)`. Avoids the ambiguity of wrapped counters where empty and full look identical.
- **Placement new for shared memory** — `mmap` returns raw bytes. Atomics need proper construction via `new (ptr) ShmHeader{}`. The client side does NOT use placement new since the server already initialized the header.
- **`is_always_lock_free` static assert** — mutex-based atomics would use process-local memory and silently break across processes. The assert catches this at compile time.

## Project Structure

```
include/science/neural_pipeline/
  shared_sample.h    — fixed-size sample struct (no pointers, safe for shm)
  shm_transport.h    — ShmHeader layout + ShmTransport class
  ring_buffer.h      — template SPSC ring buffer (single-process)
  sample.h           — std::vector-based sample (single-process)
  pipeline.h         — single-process pipeline (threads + ring buffer)
  producer.h         — synthetic neural data generator
  processor.h        — threshold spike detector

src/science/neural_pipeline/
  shm_transport.cpp  — create/open/write_slot/publish/read_slot/consume/close
  pipeline.cpp       — single-process producer/consumer loop
  producer.cpp       — gaussian noise + spike injection
  processor.cpp      — -Nσ threshold detection

examples/
  producer_node.cpp  — shared memory producer binary
  processor_node.cpp — shared memory consumer binary
  shm_bench.cpp      — raw throughput benchmark
  main.cpp           — single-process demo

test/
  test_ring_buffer.cpp     — ring buffer unit tests
  test_shm_transport.cpp   — 6 tests including fork()-based cross-process
```
