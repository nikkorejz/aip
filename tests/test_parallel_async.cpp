#include <gtest/gtest.h>
#include <atomic>

#include <aip/search/parallel_async.hpp>

TEST(ParallelAsync, preserves_order_and_reports_progress) {
    std::atomic<std::size_t> lastDone{0};

    auto out = aip::search::parallelForIndicesAsync(
        10, 110,
        [](std::size_t g) { return static_cast<int>(g * 2); },
        8,
        [&](std::size_t done, std::size_t total) {
            (void)total;
            lastDone.store(done, std::memory_order_relaxed);
        }
    );

    ASSERT_EQ(out.size(), 100u);
    EXPECT_EQ(out.front(), 20);
    EXPECT_EQ(out.back(), 218);

    EXPECT_EQ(lastDone.load(std::memory_order_relaxed), 100u);
}
