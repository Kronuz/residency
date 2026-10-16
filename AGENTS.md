# AGENTS.md — residency

## Repo map

```
residency.hh     file (mmap + views), pinned (verified copies), backend concept, sync_backend.
test/test.cc     Views and bounds, pinned verification, prefetch from a task, move semantics.
docs/residency.d2     README diagram; render: d2 --layout elk --sketch --theme 302 docs/residency.d2 docs/residency.svg
```

## Invariants

- `view()` is bounds-checked; never hand out pointers past the mapping.
- `pinned` verifies CRC-32C before exposing bytes.
- Every backend returns `task::task<void>` from `prefetch`; the concept is the contract.

## Verify a change

```sh
cmake -S . -B build && cmake --build build && ctest --test-dir build
```
