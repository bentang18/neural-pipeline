# neural-pipeline

Lock-free SPSC neural data pipeline with real-time spike detection in C++17.

## Zero copy IPC: O(1) transmission (as a function of message size)




## Architecture

```
Producer Thread ──→ [Ring Buffer] ──→ Consumer Thread
(30kHz synthetic     (lock-free       (threshold spike
 neural data)         SPSC, 4096)      detection)
```

## Build & Run

```sh
make all && make test
./build/pipeline_demo    # Ctrl+C to stop
```

## Design Decisions

- **Lock-free ring buffer** — predictable microsecond latency; mutexes can block and cause priority inversion in real-time systems
- **Power-of-two capacity** — enables bitmask indexing (`index & (capacity - 1)`) instead of expensive modulo division
- **Threshold spike detection** — simple `-Nσ` voltage threshold as a first pass, matching real-system approaches before full spike sorting
- **`alignas(64)` on atomic positions** — prevents false sharing between producer/consumer cache lines

## Example Output
```
Config: 32 channels @ 30000 Hz, buffer capacity: 4096

[1s] produced: 22530 | consumed: 22530 | dropped: 0 | spikes: 123 | latency: 2.97us avg, 28us max
[2s] produced: 45060 | consumed: 45060 | dropped: 0 | spikes: 246 | latency: 2.94us avg, 123us max
[3s] produced: 67620 | consumed: 67620 | dropped: 0 | spikes: 353 | latency: 2.92us avg, 123us max
^C
Final stats:
  Total samples:   67620
  Spikes detected: 353
  Samples dropped: 0
  Avg latency:     2.92 us
  Spike rate:      117.7 Hz (expected: ~160 Hz)
```
## Quests
I split this project into five quests. Each quest contains learning goals, tasks to complete, and understanding checks.

#### Quest 1: Read through the Science libndtp project to understand good C++ coding conventions, project organization, and also neural data transfer protocol.
`libndtp/include/science/libndtp/ndtp.h`
This writes the header for the serialized message sent from the Sci Fi headstage to the computer. The head includes bit width, number of channels, and sample rate. Then sends the entire message including header and payload.
`libndtp/test/test_ndtp.cpp`
This tests the ndtp protocol by packing and unpacking example data
`libndtp/CMakeLists.txt`
Uses C++17 and also includes building dependencies/others

`make configure` reads the project and creates a makefile.
`make build` actually builds the project.
The neural data transport protocol exists to efficiently package/serialize the data recorded from the axon probe. It creates custom header and payload organization to most compactly deliver the data to the computer.

#### Quest 2: Understand C++ project organization and make the first build

Learned the build pipeline: Headers (declare classes/interfaces) → Source files (implement them, compiled independently) → Library (packaged together) → Executables (linked with the library to run).

Key takeaways: `#pragma once` prevents duplicate includes. Namespaced include paths (`include/science/neural_pipeline/`) avoid collisions with other libraries. Templates must be fully implemented in headers since the compiler needs the full code to generate type-specific versions.

#### Quest 3: Implement the Ring Buffer and Write Tests

```
             R  W
[_][_][_][_] 0  0  empty
[A][B][C][_] 0  3  push A, B, C
[_][B][C][D] 1  4  pop A, push D
[E][F][C][D] 2  6  pop B, push E, F → FULL (W-R = 4)
[_][_][_][_] 6  6  pop all → EMPTY (W = R)
```

Here we can use atomics instead of mutexes since the threads are only updating their own variable (producer updates write_pos, and consumer updates read_pos). Thus since no two threads are writing to the same variable we do not need mutex, and atomics would be faster.

We need memory ordering since it's possible for the compiler to reorder the code such that the producer could update write_pos before updating the data, which causes the consumer to read the new write_pos without the data having been updated yet. e.g. once you have updated the data in the buffer, you have to invoke release when updating the write_pos to make the ordering strict/push - the same for reading/acquire.

Also we need `alignas(64)` to prevent false sharing which happens when the producer and consumer are writing into the same cache line which means the cache would constantly be invalidated (because it thinks the memory is being shared) across threads, drastically reducing performance--even though they are writing into separate variables on the same cache line.

Separate threads on separate cores of the CPU load RAM in cache lines, however an update in a different thread could outdate your current cache line. This is why we need the strict memory_release/acquire to make sure the cache lines are updated. Additionally we do the `alignas(64)` to make sure the read_pos / write_pos are on different cache lines such that they are not constantly invalidated since an update to one of them does not invalidate the other.

The ring buffer is a template because we want just one class that works with any type. Unsigned integers wrap to 0 on overflow so the indices are always valid. Power of two capacity allows for efficient bitmask modulo. `memory_order_release` and `memory_order_acquire` is valid since it enforces ordering between the paired producer and consumer, which is less work for the CPU.

#### Quest 4: Creating pipeline.cpp and running first tests
For this quest I need to create a pipeline object that starts both the consumer and producer threads, owns the buffer, and closes both threads during destruction. The pipeline class should be defined in a header file and the specific functions implemented in source file. Thus, main.cpp is able to create a pipeline object. Additionally, I need to create the sample class which contains the timestamp of when the sample is created and also a vector of floats for the channels. The buffer would then be instantiated with these samples as the object. Additionally for fast pushing into the buffer, instead of copying every vector of channels, we move the pointer/ownership of the channel object over to the buffer (which is O(1) instead of O(#of channels)). Finally main.cpp creates a pipeline object that runs until `Ctrl-C` is pressed which subsequently destructs the pipeline (and both threads). For testing, the consumer thread simply outputs the average latency of receiving each sample.

#### Quest 5: Create a Producer and Processor class, owned by Pipeline
Now that we have the pipeline skeleton working, we need to create specific Producer and Processor classes to handle generating noise + spikes, and threshold detection. The current functions are inline, we want to move them into separate classes.
Quick note on constructor syntax: members are variables owned by a class. Initializer list is in the format of `Class::Class(Config config):` `class->constructor->initializer list for members`
The producer class initializes the channels with random Gaussian noise and turns a few of those channels into spikes based off the `spike_rate_hz` and `sample_rate_hz`. The processor class does simple thresholding to obtain the number of spikes per sample.
Finally I have added cumulative stats to the main.cpp output.

## Reflections
I think the most useful part of this project was learning how to cleanly organize a C++ project. In the past I've mainly written single file .cpp code where I did not have to consider multiple classes, threads, and linked functions. Now I have gained both a high-level understanding of library structure and also knowledge of the specific syntax for implementation. Finally, it was really fun learning about multithreading and memory cache lines, something I did not have to consider during competitive programming. Overall, this project gave me the confidence to understand and begin to write production-level code!
