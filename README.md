# residency

Read-only file regions for C++20 code that runs on an event loop: map a file, ask
for the byte ranges a computation is about to read, and `co_await` until they are
resident, so the loop reads memory instead of blocking on a page fault. Small hot
regions can be pinned into owned memory and checksum-verified. Header-only; depends
on [task](https://github.com/Kronuz/task) and [hashes](https://github.com/Kronuz/hashes)
(CRC-32C).

## What it is

- `residency::file`: a read-only memory mapping with bounds-checked `view(range)`.
- `residency::pinned::copy(file, range, crc32c)`: copies a region into owned memory
  and verifies its CRC-32C, throwing on a mismatch. For structures that must never
  fault or be read corrupt: term indexes, skip data, document lengths.
- `residency::verified_regions(bytes, regions)`: checksummed regions of mapped bytes;
  `verify(range)` checks each region it overlaps the first time any read touches it
  (one atomic state per region) and returns false on a mismatch, then and after.
- `residency::backend`: the concept every backend implements,
  `task::task<void> prefetch(const file&, std::span<const range>)`.
- `residency::sync_backend`: advises the kernel (`madvise(MADV_WILLNEED)`) and returns
  without suspending. Asynchronous backends (io_uring on Linux, POSIX AIO on macOS) and
  a verified block cache implement the same concept.

## Usage

```cpp
#include "residency.hh"

auto f = residency::file::open("segment.sx");
residency::sync_backend io;

task::task<std::uint64_t> evaluate(const residency::file& f, std::vector<residency::range> postings) {
    co_await io.prefetch(f, postings);   // resident (as far as the backend promises)
    std::uint64_t n = 0;
    for (auto r : postings) { n += f.view(r).size(); }
    co_return n;
}
```

## Limits

- The synchronous backend only advises: a first touch can still fault and block.
- POSIX only (mmap, madvise).

## License

MIT. See [LICENSE](LICENSE).
