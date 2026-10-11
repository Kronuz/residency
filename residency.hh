/*
 * Copyright (c) 2026 Germán Méndez Bravo (Kronuz)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// Read-only file regions that code on an event loop can make resident before it
// touches them, so evaluation reads memory instead of faulting on the loop.
//
//   file      a memory-mapped, read-only file and views into it
//   prefetch  co_await backend.prefetch(file, ranges): the ranges are resident after
//   pinned    a small region copied into owned memory and checksum-verified, for hot
//             structures that must never fault (term indexes, skip data, lengths)
//
// Backends implement prefetch; this header ships the synchronous one, which advises
// the kernel and returns without suspending.

#pragma once

#include "crc32c.hh"
#include "task.hh"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace residency {

// A byte range within a file.
struct range {
	std::uint64_t offset = 0;
	std::uint64_t length = 0;
};

// A read-only memory mapping of a whole file.
class file {
public:
	file() = default;

	static file open(const std::string& path) {
		int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
		if (fd < 0) { throw std::system_error(errno, std::generic_category(), "residency: open " + path); }
		struct stat st {};
		if (::fstat(fd, &st) != 0) {
			int e = errno;
			::close(fd);
			throw std::system_error(e, std::generic_category(), "residency: fstat " + path);
		}
		file f;
		f.fd_ = fd;
		f.size_ = std::uint64_t(st.st_size);
		if (f.size_) {
			void* p = ::mmap(nullptr, f.size_, PROT_READ, MAP_SHARED, fd, 0);
			if (p == MAP_FAILED) {
				int e = errno;
				::close(fd);
				throw std::system_error(e, std::generic_category(), "residency: mmap " + path);
			}
			f.data_ = static_cast<const std::byte*>(p);
		}
		return f;
	}

	file(file&& o) noexcept : data_(std::exchange(o.data_, nullptr)), size_(std::exchange(o.size_, 0)), fd_(std::exchange(o.fd_, -1)) {}
	file& operator=(file&& o) noexcept {
		if (this != &o) {
			close();
			data_ = std::exchange(o.data_, nullptr);
			size_ = std::exchange(o.size_, 0);
			fd_ = std::exchange(o.fd_, -1);
		}
		return *this;
	}
	~file() { close(); }

	std::uint64_t size() const noexcept { return size_; }
	int fd() const noexcept { return fd_; }

	// The bytes of r; valid while the file is open. Throws if r is out of bounds.
	std::span<const std::byte> view(range r) const {
		if (r.offset > size_ || r.length > size_ - r.offset) { throw std::out_of_range("residency: range outside file"); }
		return {data_ + r.offset, std::size_t(r.length)};
	}

private:
	void close() noexcept {
		if (data_) { ::munmap(const_cast<std::byte*>(data_), size_); }
		if (fd_ >= 0) { ::close(fd_); }
		data_ = nullptr;
		fd_ = -1;
	}

	const std::byte* data_ = nullptr;
	std::uint64_t size_ = 0;
	int fd_ = -1;
};

// A region copied into owned memory and verified against its CRC-32C.
class pinned {
public:
	pinned() = default;

	// Copies r out of f and checks it; throws on a checksum mismatch.
	static pinned copy(const file& f, range r, std::uint32_t expected_crc32c) {
		auto bytes = f.view(r);
		pinned p;
		p.bytes_.assign(bytes.begin(), bytes.end());
		if (hashes::crc32c(p.bytes_.data(), p.bytes_.size()) != expected_crc32c) {
			throw std::runtime_error("residency: checksum mismatch in pinned region");
		}
		return p;
	}

	std::span<const std::byte> bytes() const noexcept { return bytes_; }

private:
	std::vector<std::byte> bytes_;
};

// A backend makes ranges resident: after co_await b.prefetch(f, ranges), reading
// them does not block on I/O (as far as the backend can promise).
template <typename B>
concept backend = requires(B& b, const file& f, std::span<const range> ranges) {
	{ b.prefetch(f, ranges) } -> std::same_as<task::task<void>>;
};

// Advises the kernel to read the ranges ahead and returns without suspending. Page
// faults can still block on first touch; asynchronous backends remove that.
class sync_backend {
public:
	task::task<void> prefetch(const file& f, std::span<const range> ranges) {
		advise(f, ranges);
		co_return;
	}

	static void advise(const file& f, std::span<const range> ranges) noexcept {
		static const std::uint64_t page = std::uint64_t(::sysconf(_SC_PAGESIZE));
		for (const range& r : ranges) {
			if (!r.length || r.offset >= f.size()) { continue; }
			auto bytes = f.view({r.offset, std::min(r.length, f.size() - r.offset)});
			auto start = reinterpret_cast<std::uintptr_t>(bytes.data()) & ~(page - 1);
			auto end = reinterpret_cast<std::uintptr_t>(bytes.data() + bytes.size());
			::madvise(reinterpret_cast<void*>(start), end - start, MADV_WILLNEED);
		}
	}
};

static_assert(backend<sync_backend>);

}  // namespace residency
