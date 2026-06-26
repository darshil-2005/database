#include "./utils/catch.hpp"
#include "../src/include/bufferpool/bufferpool.h"
#include "../src/include/storageManager/storageManager.h"
#include <vector>

TEST_CASE("BufferPool Core Operations", "[bufferpool]") {
    if (fs::exists(DATA_DIR)) {
        fs::remove_all(DATA_DIR);
    }
    StorageManager sm;
    REQUIRE(sm.Bootstrap() == true);
    BufferPool bp(sm);
    SECTION("Requesting new pages allocates and caches properly") {
        Result<NewPage> page_res1 = bp.AllocateNewPage();
        REQUIRE(page_res1.err == ErrType::None);
        REQUIRE(page_res1.value.ptr != nullptr);

        PageID pid1 = page_res1.value.pid;
        std::memset(page_res1.value.ptr, 'A', PAGE_SIZE);
        Result<Byte*> page_res2 = bp.RequestPage(pid1);
        REQUIRE(page_res2.err == ErrType::None);
        REQUIRE(page_res2.value[0] == static_cast<Byte>('A'));
    }
};

TEST_CASE("AllocateNewPage produces unique PageIDs with isolated memory", "[bufferpool]") {
    if (fs::exists(DATA_DIR)) {
        fs::remove_all(DATA_DIR);
    }
    StorageManager sm;
    REQUIRE(sm.Bootstrap() == true);
    BufferPool bp(sm);

    Result<NewPage> p1 = bp.AllocateNewPage();
    Result<NewPage> p2 = bp.AllocateNewPage();
    Result<NewPage> p3 = bp.AllocateNewPage();
    REQUIRE(p1.err == ErrType::None);
    REQUIRE(p2.err == ErrType::None);
    REQUIRE(p3.err == ErrType::None);

    REQUIRE(p1.value.pid != p2.value.pid);
    REQUIRE(p2.value.pid != p3.value.pid);
    REQUIRE(p1.value.pid != p3.value.pid);

    std::memset(p1.value.ptr, 'X', PAGE_SIZE);
    std::memset(p2.value.ptr, 'Y', PAGE_SIZE);
    std::memset(p3.value.ptr, 'Z', PAGE_SIZE);

    // Each allocation must occupy its own frame; writes to one must not
    // bleed into another.
    REQUIRE(p1.value.ptr[0] == static_cast<Byte>('X'));
    REQUIRE(p1.value.ptr[PAGE_SIZE - 1] == static_cast<Byte>('X'));
    REQUIRE(p2.value.ptr[0] == static_cast<Byte>('Y'));
    REQUIRE(p3.value.ptr[0] == static_cast<Byte>('Z'));
};

TEST_CASE("RequestPage returns the cached frame on repeated access", "[bufferpool]") {
    if (fs::exists(DATA_DIR)) {
        fs::remove_all(DATA_DIR);
    }
    StorageManager sm;
    REQUIRE(sm.Bootstrap() == true);
    BufferPool bp(sm);

    Result<NewPage> alloc_res = bp.AllocateNewPage();
    REQUIRE(alloc_res.err == ErrType::None);
    PageID pid = alloc_res.value.pid;
    std::memset(alloc_res.value.ptr, 'C', PAGE_SIZE);

    Result<Byte*> hit1 = bp.RequestPage(pid);
    REQUIRE(hit1.err == ErrType::None);
    REQUIRE(hit1.value == alloc_res.value.ptr);
    REQUIRE(hit1.value[0] == static_cast<Byte>('C'));

    Result<Byte*> hit2 = bp.RequestPage(pid);
    REQUIRE(hit2.err == ErrType::None);
    REQUIRE(hit2.value == alloc_res.value.ptr);
};

TEST_CASE("ReleasePage updates pin state and rejects unknown pages", "[bufferpool]") {
    if (fs::exists(DATA_DIR)) {
        fs::remove_all(DATA_DIR);
    }
    StorageManager sm;
    REQUIRE(sm.Bootstrap() == true);
    BufferPool bp(sm);

    SECTION("Releasing a tracked page succeeds") {
        Result<NewPage> alloc_res = bp.AllocateNewPage();
        REQUIRE(alloc_res.err == ErrType::None);

        Result<bool> release_res = bp.ReleasePage(alloc_res.value.pid, true);
        REQUIRE(release_res.err == ErrType::None);
        REQUIRE(release_res.value == true);
    }

    SECTION("Releasing a page never requested returns PageNotFoundInBufferPool") {
        Result<bool> release_res = bp.ReleasePage(12345, false);
        REQUIRE(release_res.err == ErrType::PageNotFoundInBufferPool);
        REQUIRE(release_res.value == false);
    }
};

TEST_CASE("AllocateNewPage fails with BufferPoolFull when every frame is pinned", "[bufferpool]") {
    if (fs::exists(DATA_DIR)) {
        fs::remove_all(DATA_DIR);
    }
    StorageManager sm;
    REQUIRE(sm.Bootstrap() == true);
    BufferPool bp(sm);

    // Fill every frame and never release any of them.
    for (int i = 0; i < POOL_SIZE; i++) {
        Result<NewPage> alloc_res = bp.AllocateNewPage();
        REQUIRE(alloc_res.err == ErrType::None);
    }

    Result<NewPage> overflow_res = bp.AllocateNewPage();
    REQUIRE(overflow_res.err == ErrType::BufferPoolFull);
    REQUIRE(overflow_res.value.ptr == nullptr);
};

TEST_CASE("Buffer pool evicts unpinned page and preserves data", "[bufferpool]") {
    if (fs::exists(DATA_DIR)) {
        fs::remove_all(DATA_DIR);
    }

    StorageManager sm;
    REQUIRE(sm.Bootstrap() == true);

    BufferPool bp(sm);

    std::vector<PageID> pids;
    pids.reserve(POOL_SIZE);

    // Fill entire buffer pool.
    for (int i = 0; i < POOL_SIZE; i++) {
        auto alloc_res = bp.AllocateNewPage();

        REQUIRE(alloc_res.err == ErrType::None);

        PageID pid = alloc_res.value.pid;

        std::memset(
            alloc_res.value.ptr,
            static_cast<uint8_t>(pid & 0xFF),
            PAGE_SIZE
        );

        pids.push_back(pid);
    }

    // Make exactly one frame evictable.
    PageID victim_pid = pids.back();

    auto release_res = bp.ReleasePage(victim_pid, true);
    REQUIRE(release_res.err == ErrType::None);

    // Force eviction.
    auto extra_page = bp.AllocateNewPage();

    REQUIRE(extra_page.err == ErrType::None);
    REQUIRE(extra_page.value.ptr != nullptr);

    auto release_2 = bp.ReleasePage(extra_page.value.pid, false);
    REQUIRE(release_2.err == ErrType::None);

    // Victim should have been written to disk and removed from memory.
    auto reload_res = bp.RequestPage(victim_pid);

    REQUIRE(reload_res.err == ErrType::None);
    REQUIRE(reload_res.value != nullptr);

    // Verify persisted contents.
    REQUIRE(
        static_cast<uint8_t>(reload_res.value[0]) ==
        static_cast<uint8_t>(victim_pid & 0xFF)
    );

    REQUIRE(
        static_cast<uint8_t>(reload_res.value[PAGE_SIZE - 1]) ==
        static_cast<uint8_t>(victim_pid & 0xFF)
    );
}

TEST_CASE("Dirty page data survives eviction pressure", "[bufferpool]") {
    if (fs::exists(DATA_DIR)) {
        fs::remove_all(DATA_DIR);
    }

    StorageManager sm;
    REQUIRE(sm.Bootstrap());

    BufferPool bp(sm);

    auto target = bp.AllocateNewPage();
    REQUIRE(target.err == ErrType::None);

    PageID target_pid = target.value.pid;

    std::memset(target.value.ptr, 'D', PAGE_SIZE);
    REQUIRE(static_cast<char>(target.value.ptr[2]) == 'D');
    REQUIRE(static_cast<char>((bp.buffer_pool + (PAGE_SIZE * 99))[2]) == 'D');

    REQUIRE(bp.ReleasePage(target_pid, true).err == ErrType::None);
    Result<NewPage> page;

    // Generate enough pressure to force at least one eviction.
    for (int i = 0; i < POOL_SIZE; ++i) {
        page = bp.AllocateNewPage();
        REQUIRE(page.err == ErrType::None);

        std::memset(page.value.ptr, 'C', PAGE_SIZE);
    };

    REQUIRE(bp.ReleasePage(page.value.pid, false).err == ErrType::None);

    // bp.DumpCurrBufferPool();

    auto result = bp.RequestPage(target_pid);

    // bp.DumpCurrBufferPool();

    REQUIRE(result.err == ErrType::None);
    REQUIRE(result.value != nullptr);

    REQUIRE(static_cast<char>(result.value[0]) == 'D');
    REQUIRE(static_cast<char>(result.value[PAGE_SIZE / 2]) == 'D');
    REQUIRE(static_cast<char>(result.value[PAGE_SIZE - 1]) == 'D');
};
