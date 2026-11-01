# residency

Part of [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries), a collection of small, reusable C++ libraries.

[![CI](https://github.com/Kronuz/residency/actions/workflows/ci.yml/badge.svg)](https://github.com/Kronuz/residency/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![Header-only](https://img.shields.io/badge/header--only-yes-green.svg)

Resident file regions for C++20 event loops: mmap views, prefetch as a coroutine, and CRC-32C
verified pinned copies. Map a file, ask a backend for the byte ranges a computation is about to
read, and `co_await` until they are resident, so the loop reads memory instead of blocking on a
page fault. Reach for it when event-loop code reads index segments or posting lists from mapped
files.

## ✨ Highlights

- **Read-only mappings.** `residency::file` maps a file and hands out bounds-checked
  `view(range)` spans.
- **Prefetch as a task.** A backend's `prefetch(file, ranges)` returns a `task::task<void>`; the
  caller `co_await`s it and reads the ranges after it resumes.
- **Pluggable backends.** `residency::backend` is the concept a backend implements.
  `sync_backend` advises the kernel (`madvise(MADV_WILLNEED)`) and returns without suspending; an
  asynchronous backend plugs in through the same concept.
- **Pinned copies.** `pinned::copy(file, range, crc32c)` copies a region into owned memory and
  throws on a CRC-32C mismatch, for structures that must never fault or be read corrupt: term
  indexes, skip data, document lengths.
- **Verified on first touch.** `verified_regions` holds the CRC-32C of each region of mapped
  bytes; `verify(range)` checks every region the range overlaps the first time a read touches
  it, with one atomic state per region, and returns false on a mismatch from then on.

## 🚀 Quick start

```cpp
#include "residency.hh"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

residency::sync_backend io;

task::task<std::uint64_t> sum_lengths(const residency::file& f, std::vector<residency::range> ranges) {
	co_await io.prefetch(f, ranges);  // resident, as far as the backend promises
	std::uint64_t n = 0;
	for (auto r : ranges) { n += f.view(r).size(); }  // reads memory; view() checks bounds
	co_return n;
}

int main() {
	std::ofstream("segment.bin", std::ios::binary) << std::string(65536, 'x');
	auto f = residency::file::open("segment.bin");

	task::inline_scheduler loop;
	std::printf("%llu\n", (unsigned long long)task::run(loop, sum_lengths(f, {{0, 4096}, {8192, 100}})));
	// 4196
}
```

## 📦 Installation

Requires a C++20 compiler, a POSIX system (`mmap`, `madvise`), and CMake 3.14 or newer. CMake
fetches [task](https://github.com/Kronuz/task) and [crc32c](https://github.com/Kronuz/crc32c) at
pinned commits. CI builds and tests it on Linux (x86_64 with GCC and Clang, arm64), macOS (arm64,
x86_64) and FreeBSD, plus ASan + UBSan and TSan builds.

```cmake
include(FetchContent)
FetchContent_Declare(residency
  GIT_REPOSITORY https://github.com/Kronuz/residency.git
  GIT_TAG        f4206c7679e0ea1e8f3d04a7b2a8a41df0fa02ad)  # or main to track the tip
FetchContent_MakeAvailable(residency)

target_link_libraries(my_app PRIVATE residency::residency)
```

```cpp
#include "residency.hh"
```

## 🎯 When to use it

Event-loop code that reads mapped files and must not stall the loop on a page fault: index
segments, posting lists, skip data.

Not for:

- Writable mappings: `residency::file` maps read-only.
- Windows: it needs `mmap` and `madvise`.
- A hard no-fault guarantee from `sync_backend` alone: it only advises the kernel, so a first
  touch can still fault and block. Pin the regions that must never fault.

## 📚 Documentation

- [AGENTS.md](AGENTS.md): repository map and invariants.
- [Catalog entry](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md#residency) in Kronuz
  C++ Libraries.

## 🛠️ Building and testing

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

## 🤝 Contributing

Issues and pull requests are welcome. Read [AGENTS.md](AGENTS.md) and the catalog's
[rules of engagement](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md#rules-of-engagement)
first; CI must pass on every platform.

## 📄 License

MIT. See [LICENSE](LICENSE).
