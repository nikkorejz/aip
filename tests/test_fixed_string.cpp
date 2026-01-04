#include <gtest/gtest.h>
#include <aip/core/fixed_string.hpp>

TEST(fixed_string, sv_and_size) {
    constexpr aip::core::fixed_string s{"k"};
    static_assert(s.sv().size() == 1);
    EXPECT_EQ(s.sv(), "k");
}

TEST(fixed_string, equality) {
    constexpr aip::core::fixed_string a{"alpha"};
    constexpr aip::core::fixed_string b{"alpha"};
    constexpr aip::core::fixed_string c{"beta"};
    static_assert(a == b);
    static_assert(!(a == c));
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}
