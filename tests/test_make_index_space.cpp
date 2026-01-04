#include <gtest/gtest.h>

#include <aip/core/fixed_string.hpp>
#include <aip/params/control_param.hpp>
#include <aip/params/uniform_range.hpp>
#include <aip/params/param_grid.hpp>
#include <aip/search/index_space_from_grid.hpp>

namespace {

struct M {
    double k{};
    aip::params::ControlParam<int, aip::core::fixed_string{"b"}> b{};
};

using Grid = aip::params::ParamGrid<M, aip::params::UniformRange, &M::k, &M::b>;

} // namespace

TEST(MakeIndexSpace, computes_bases_and_total) {
    Grid g;
    g.get<0>() = {0.0, 1.0, 0.5}; // 3
    g.get<1>() = {10, 12, 1};     // 3

    const auto s = aip::search::make_index_space(g);

    EXPECT_EQ(s.bases[0], 3u);
    EXPECT_EQ(s.bases[1], 3u);
    EXPECT_EQ(s.total, 9u);
    EXPECT_FALSE(s.empty());
}

TEST(MakeIndexSpace, empty_if_any_range_empty) {
    Grid g;
    g.get<0>() = {0.0, 1.0, 0.5}; // 3
    g.get<1>() = {1, 1, 0};       // step=0 => 0

    const auto s = aip::search::make_index_space(g);

    EXPECT_EQ(s.bases[0], 3u);
    EXPECT_EQ(s.bases[1], 0u);
    EXPECT_EQ(s.total, 0u);
    EXPECT_TRUE(s.empty());
}
