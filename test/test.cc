// residency: file views and bounds, pinned copies with checksum verification, and
// prefetch through the synchronous backend from a task.
#include "residency.hh"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>

static int failures = 0;
#define CHECK(cond) \
	do { \
		if (!(cond)) { std::fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); ++failures; } \
	} while (0)

task::task<std::uint64_t> sum_after_prefetch(residency::sync_backend& b, const residency::file& f, std::vector<residency::range> ranges) {
	co_await b.prefetch(f, ranges);
	std::uint64_t sum = 0;
	for (const auto& r : ranges) {
		for (std::byte x : f.view(r)) { sum += std::uint64_t(x); }
	}
	co_return sum;
}

int main() {
	const char* path = "residency_test.bin";
	std::vector<unsigned char> data(1 << 20);
	std::mt19937_64 rng(1);
	for (auto& c : data) { c = static_cast<unsigned char>(rng()); }
	std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));

	auto f = residency::file::open(path);
	CHECK(f.size() == data.size());
	auto v = f.view({100, 10});
	CHECK(std::memcmp(v.data(), data.data() + 100, 10) == 0);
	bool threw = false;
	try { (void)f.view({data.size() - 5, 10}); } catch (const std::out_of_range&) { threw = true; }
	CHECK(threw);
	CHECK(f.view({data.size(), 0}).empty());

	auto crc = hashes::crc32c(data.data() + 4096, 8192);
	auto p = residency::pinned::copy(f, {4096, 8192}, crc);
	CHECK(p.bytes().size() == 8192);
	CHECK(std::memcmp(p.bytes().data(), data.data() + 4096, 8192) == 0);
	threw = false;
	try { (void)residency::pinned::copy(f, {4096, 8192}, crc ^ 1); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw);

	residency::sync_backend backend;
	task::inline_scheduler s;
	std::vector<residency::range> ranges{{0, 1000}, {500000, 70000}, {data.size() - 1, 1}, {data.size(), 0}};
	std::uint64_t want = 0;
	for (const auto& r : ranges) { for (std::uint64_t i = 0; i < r.length; ++i) { want += data[r.offset + i]; } }
	CHECK(task::run(s, sum_after_prefetch(backend, f, ranges)) == want);

	// Regions verify on first touch; a corrupted region stays bad.
	{
		std::vector<std::byte> bytes(reinterpret_cast<const std::byte*>(data.data()), reinterpret_cast<const std::byte*>(data.data()) + data.size());
		std::vector<residency::verified_regions::region> regions;
		for (std::uint64_t at = 0; at < 300000; at += 65536) {
			const std::uint64_t n = std::min<std::uint64_t>(65536, 300000 - at);
			regions.push_back({at, n, hashes::crc32c(bytes.data() + at, n)});
		}
		bytes[200000] ^= std::byte{1};  // region 3 (196608..262143)
		residency::verified_regions v(bytes, regions);
		CHECK(v.size() == 5 && v.verified() == 0);
		CHECK(v.verify({0, 10}));
		CHECK(v.verified() == 1);
		CHECK(v.verify({65530, 20}));  // spans regions 0 and 1
		CHECK(v.verified() == 2);
		CHECK(!v.verify({100000, 150000}));  // reaches the corrupted region
		CHECK(!v.verify({200000, 1}));
		CHECK(v.verify({262144, 1000}));     // region 4 is fine
		CHECK(v.verify({400000, 100}));      // outside every region: unchecked
		CHECK(v.verify({0, 0}));
		threw = false;
		try { residency::verified_regions bad(bytes, {{0, 10, 0}, {5, 10, 0}}); } catch (const std::invalid_argument&) { threw = true; }
		CHECK(threw);
	}

	auto moved = std::move(f);
	CHECK(moved.size() == data.size());
	CHECK(f.size() == 0);
	std::remove(path);

	threw = false;
	try { (void)residency::file::open("does-not-exist.bin"); } catch (const std::system_error&) { threw = true; }
	CHECK(threw);

	std::printf("residency: %s\n", failures ? "FAILED" : "ok");
	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
