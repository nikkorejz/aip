#include <gtest/gtest.h>

#include <aip/params/unit_grid.hpp>

struct M {
    int a{7};
};

TEST(UnitGrid, basic) {
    aip::params::UnitGrid<M> g;

    EXPECT_EQ(g.size(), 1u);

    auto m = g.makeModel(std::array<std::size_t, 0>{});
    // default-constructed
    EXPECT_EQ(m.a, 7);
}
