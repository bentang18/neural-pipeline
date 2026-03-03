# neural-pipeline

Lock-free SPSC neural data pipeline with real-time spike detection in C++17.

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
I split this project into five quests. Each quests contains learning goals, tasks to complete, and understanding checks.

#### Quest 1: Read through the Science libndtp project to understand good C++ coding conventions, project organization, and also neural data transfer protocol.
`libndtp/include/science/libndtp/ndtp.h`
This writes the header for the serialized message sent from the Sci Fi headstage to the computer. The head includes bit width, number of channels, and sample rate. Then sends the entire message including header and payload.
`libndtp/test/test_ndtp.cpp`
This tests the ndtp protocol by packing and unpacking example data
`libndtp/CMakeLists.txt`
Uses C++17 and also includes building dependecies/others

Make configure read the project and creates a makefile
Make build actually builds the project
The neural data transport protocol exists to efficiently package/serialize the data recorded from the axon probe. It creates custom header and payload organization to most compactly deliver the data to the computer.

#### Quest 2: Understand how a C++ project is organized and make the first build of my neural-pipeline

Header files: These define the classes and methods the source files use. These don't have the specific implementation but outline the scope. Except for templates which do have the full implementation. When a header file is included, that code is just pasted in at the top of the source file. The header lays out what functions the source file could use.
->
Source files: The source files write out the implementations in the header files. The source files are compiled indepedently.
->
Library: The source files are packaged together together.
->
Executable files: Executable files use the library to run a program (main.cpp, could be a test or the actual pipeline). The linker ties everything together: Suppose one source files writes out the implementation for a function in a shared header - then the other source files could use that function.

In the current state of my project, the header file is `include/science/neural_pipeline/ring_buffer.h`. This will create the data structure for the ring buffer (the communication mode between the generated neural data and the consumer). The library makes the header files and the source files (which is currently empty) available to use for the executables. Then I have a test executable at `test/test_ring_buffer.cpp` which should test the functionality of my ring buffer. Finally I have my main executable at `/examples/main.cpp`. The executables are linked with the neural-proccesor library.

To put in simple words: headers create templates/classes that are commonly reused through multiple functions. Source files implement the actual functions. Each file is compiled indepedently, which is why the header needs to act as a place holder for the functions, or else the other files do not know the interface of classes/functions. The linker joins everything, and the entry point isthe main file can call the functions in the source files. Also you can link multiple libraries, that's why you need namespace conventions to prevent collisions of similar classes.

`ring_buffer.h` uses `#pragma once` to prevent it from being duplicated during compilation - suppose another header file includes it, and a source file calls both header files, you don't want to include the same code twice

Also the include path goes through `/include/science/neural_pipeline` instead of just `/include` to avoid collisions with other dependencies in the project/other projects

#### Quest 3: Implement the Ring Buffer and Write Tests
Before implementing let's draw out some cases:

```
                    R   W
[][][][]            0   0
push(A)
[A][][][]           0   1
push(B)
[A][B][][]          0   2
push(C)
[A][B][C][]         0   3
pop() → A
[_][B][C][]         1   3
push(D)
[_][B][C][D]        1   4
pop() → B
[_][_][C][D]        2   4
push(E)
[E][_][C][D]        2   5
push(F)
[E][F][C][D]        2   6  ← FULL (W-R = 4 = capacity)
pop() → C
[E][F][_][D]        3   6
pop() → D
[E][F][_][_]        4   6
pop() → E
[_][F][_][_]        5   6
pop() → F
[_][_][_][_]        6   6  ← EMPTY (W = R)
```

Here we can use atomics instead of mutexes since the threads are only updating their own variable (producer updates write_pos, and consumer updates read_pos). Thus since no two threads are writing to the same variable we do not need mutex, and atomics would be faster.

We need memory ordering since it's possible for compiler to reorder the code such that the producer could update write_pos before updating the data, which causes the consumer to read the new write_pos without the data having been updated yet. e.g. once you have updated the data in the buffer, you have to invoke release when updating the write_pos to make the ordering strict/push - the same for reading/acquire.

Also we need `alignas(64)` to prevent false sharing which happens when the producer and consumer are writing into the same cache line which means the cache would constantly be invalidated (because it thinks the memory is being shared) across threads, drastically reducing performance--even though they are writing into seperate variables on the same cache line.

Reiterating what I learned: Seperate threads on seperate cores of the CPU loads the RAM in cache lines, however an update in a different thread could outdate your current cache line. This is why we need the strict memory_release/acquire to make sure the cache lines are updated. Additionally we do the `alignas(64)` to make sure the read_pos / write_pos are on different cache lines such that they are not constantly invalidated since an update to one of them does not invalidate the other.

Final thoughts: the ring buffer is a template because we want just one class that works with any type. Also unsigned integers wrap to 0 when overflow so the indicies are always okay. Power of two capacity allows for efficent modulo. `memory_order_relase` and `memory_order_acquire` is valid since it enforces ordering between the paired producer and consumer, which is less work for the CPU.

#### Quest 4: Creating pipeline.cpp and running first tests
For this quest I need to create a pipeline object that starts both the consumer and producer threads, owns the buffer, and closes both threads during deconstruction. The pipeline class should be defined in a header file and the specific functions implemented in source file. Thus, main.cpp is able to create a pipeline object. Additionally, I need to create the sample class which contains the timestamp of when the sample is created and also a vector of floats for the channels. The buffer would then be instantiated with these samples as the object. Additionally for fast pushing into the buffer, instead of copying every vector of channels, we move the pointer/ownership of the channel object over to the buffer (which is O(1) instead of O(#of channels)). Finally main.cpp creates a pipeline object that runs until `Ctrl-C` is pressed which subsequently deconstructs the pipeline (and both threads). For testing, the consumer thread simply outputs the average latency of recieving each sample.

#### Quest 5: Create a Producer and Processor class, owned by Pipeline
Now that we have the pipeline skeleton working, we need to create specific Producer and Processor classes to handle generating noise + spikes, and threshold detection. The current functions are inline, we want to move them into seperate classes.
Quick note on constructor syntax: members are variables owned by a class. Initalizer list is in the format of `Class:Class(Config config):` `class->constructor->intializer list for members`
The producer class initalizes the channels with random gaussian noise and turns a few of those channels into spikes based off the `spike_rate_hz` and `sample_rate_hz`. The consumer class does simple threshholding to obtain the number of spikes per sample.
Finally I have added cumulative stats to the main.cpp output.

## Reflections
I think the most useful part of this project was learning how to cleanly organize a C++ project. In the past I've mainly written single file .cpp code where I did not have to consider multiple classes, threads, and linked functions. Now I have gained both a high-level understanding of library structure and also knowledge of the specifc syntax for implementation. Finally, it was really fun learning about multithreading and memory cache lines, something I did not have to consider during competitive programming. Overall, this project gave me the confidence to understand and begin to write production-level code!
