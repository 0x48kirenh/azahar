# Fortunella

**Fortunella** is a highly experimental 3DS emulator with a multithreaded CPU architecture, descended from Azahar.

## What is this?

Fortunella is not a general-purpose emulator. It is an experimental fork that rewrites the core execution model to run all four 3DS CPU cores in parallel on separate host threads, rather than sequentially on a single thread like its predecessors.

This project exists to explore whether a multithreaded emulation architecture can deliver meaningful performance improvements on modern multi-core host CPUs.

## Architecture

- **Parallel CPU cores**: All four 3DS cores (quad-core N3DS titles) execute simultaneously on dedicated worker threads
- **Big Kernel Lock**: HLE kernel operations are serialized via a recursive mutex, but JIT execution (`cpu->Run()`) runs lock-free for true parallelism
- **TLS-based core state**: Each worker thread maintains its own `current_cpu`, `current_process`, `current_timer`, and `page_table` via thread-local storage
- **SpinBarrier synchronization**: Custom spin-wait barriers minimize sync overhead between slices
- **Pipelined rendering**: SwapBuffers from the previous frame overlaps with CPU execution of the next frame
- **VBlank-driven frame loop**: The orchestrator runs CPU slices until VBlank fires, then presents

## Lineage

```
Citra → Azahar → Fortunella
```

Fortunella is built on top of Azahar, which itself is a fork of Citra. The vast majority of the codebase remains Azahar/Citra code. The multithreaded CPU architecture is the primary contribution of Fortunella.

## Status

**Experimental.** Do not expect games to work correctly. Do not expect stability. This is a research project.

## Build

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target fortunella.exe -- -j 8
```

Requires MSYS2/Clang64 on Windows. See Azahar's build wiki for dependency details.

## License

GPLv2 or later. See `license.txt`.
