#include <gtest/gtest.h>
#include <aip/params/uniform_range.hpp>

TEST(UniformRange, size_basic) {
    aip::params::UniformRange<double> r{0.0, 1.0, 0.25};
    EXPECT_EQ(r.size(), 5u); // 0, 0.25, 0.5, 0.75, 1.0
}

TEST(UniformRange, indexing) {
    aip::params::UniformRange<double> r{1.0, 2.0, 0.5};
    EXPECT_DOUBLE_EQ(r[0], 1.0);
    EXPECT_DOUBLE_EQ(r[1], 1.5);
    EXPECT_DOUBLE_EQ(r[2], 2.0);
}

TEST(UniformRange, invalid_cases) {
    aip::params::UniformRange<double> r1{0.0, 1.0, 0.0};
    EXPECT_EQ(r1.size(), 0u);

    aip::params::UniformRange<double> r2{2.0, 1.0, 0.1};
    EXPECT_EQ(r2.size(), 0u);
}
