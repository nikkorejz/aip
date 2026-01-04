#include <gtest/gtest.h>

#include <vector>

#include <aip/search/enumeration_strategy.hpp>

TEST(EnumerationStrategy, enumerates_all_indices_in_order) {
    aip::search::IndexSpace<2> space;
    space.bases = {3, 2}; // i0: 0..2, i1: 0..1
    space.total = 6;

    aip::search::EnumerationStrategy<2> s;
    s.reset(space);

    std::vector<std::array<std::size_t,2>> out;
    while (auto idx = s.next()) {
        out.push_back(*idx);
    }

    ASSERT_EQ(out.size(), 6u);
    EXPECT_EQ(out[0], (std::array<std::size_t,2>{0,0}));
    EXPECT_EQ(out[1], (std::array<std::size_t,2>{1,0}));
    EXPECT_EQ(out[2], (std::array<std::size_t,2>{2,0}));
    EXPECT_EQ(out[3], (std::array<std::size_t,2>{0,1}));
    EXPECT_EQ(out[4], (std::array<std::size_t,2>{1,1}));
    EXPECT_EQ(out[5], (std::array<std::size_t,2>{2,1}));
}

TEST(EnumerationStrategy, empty_space_returns_nothing) {
    aip::search::IndexSpace<3> space;
    space.bases = {3, 0, 4};
    space.total = 0;

    aip::search::EnumerationStrategy<3> s;
    s.reset(space);

    EXPECT_EQ(s.next(), std::nullopt);
}
