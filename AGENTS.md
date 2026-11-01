# AGENTS.md — residency

## Repo map

```
residency.hh     file (mmap + views), pinned (verified copies), backend concept, sync_backend.
test/test.cc     Views and bounds, pinned verification, prefetch from a task, move semantics.
```

## Invariants

- `view()` is bounds-checked; never hand out pointers past the mapping.
- `pinned` verifies CRC-32C before exposing bytes.
- Every backend returns `task::task<void>` from `prefetch`; the concept is the contract.

## Verification and benchmarks

CI (`.github/workflows/ci.yml`) calls the catalog's shared
[`cmake.yml`](https://github.com/Kronuz/cpp-libraries/blob/main/.github/workflows/cmake.yml) on every push and nightly:
CTest on Linux (x86-64 GCC and Clang, arm64), macOS (arm64, x86-64) and FreeBSD, then again under
ASan+UBSan and TSan. The shared method is the catalog's
[PERFORMANCE.md](https://github.com/Kronuz/cpp-libraries/blob/main/docs/PERFORMANCE.md).

```sh
cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure
# ASan+UBSan as CI's linux-asan-ubsan job runs it (Linux, Clang)
CC=clang CXX=clang++ cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -D_GLIBCXX_ASSERTIONS" \
  "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined"
cmake --build build-asan && ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-asan --output-on-failure
# TSan as CI's linux-tsan job runs it
CC=clang CXX=clang++ cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  "-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=-O1 -g -fsanitize=thread -fno-omit-frame-pointer" \
  "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread"
cmake --build build-tsan && TSAN_OPTIONS="halt_on_error=1 second_deadlock_stack=1" ctest --test-dir build-tsan --output-on-failure
```

`residency_test` checks views and their bounds, `pinned::copy` throwing on a CRC-32C mismatch,
prefetch through `sync_backend` from a task, moves, and `verified_regions` against a model: 50
rounds of random bytes, regions in spans that start mid-word, random corruptions and 300 random
ranges each, where a range verifies only if every region it reaches is intact and a bad region
stays bad. The expected checksums come from the same crc32c library the header uses, so the
test checks the region bookkeeping, not CRC-32C itself. The tests run on one thread: TSan builds
them, but no test races two `verify` calls on one region. There is no bench, fuzz target or
mutation script: the library makes no performance claim.
