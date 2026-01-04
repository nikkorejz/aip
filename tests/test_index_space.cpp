#include <gtest/gtest.h>
#include <aip/search/index_space.hpp>

TEST(IndexSpace, empty_works) {
    aip::search::IndexSpace<3> s;
    EXPECT_TRUE(s.empty());

    s.total = 10;
    EXPECT_FALSE(s.empty());
}
