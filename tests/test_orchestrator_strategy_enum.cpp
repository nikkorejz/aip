#include <gtest/gtest.h>

#include <aip/core/fixed_string.hpp>
#include <aip/model/imodel.hpp>
#include <aip/params/control_param.hpp>
#include <aip/params/param_grid.hpp>
#include <aip/params/uniform_range.hpp>
#include <aip/core/orchestrator.hpp>
#include <aip/search/reverse_enumeration_strategy.hpp>

namespace {
struct Always {
    constexpr bool operator()(const double&) const noexcept { return true; }
};

struct M final : aip::model::IModel<double, double> {
    double k{};
    aip::params::ControlParam<int, aip::core::fixed_string{"b"}> b{};

    [[nodiscard]] double operator()(const double&) const noexcept override {
        return k + 1000.0 * static_cast<double>(b.value);
    }
};

using Grid = aip::params::ParamGrid<M, aip::params::UniformRange, &M::k, &M::b>;
}

TEST(OrchestratorStrategy, enumeration_order) {
    aip::core::Orchestrator<double, double, Always> orch;

    Grid g;
    g.get<0>() = {0.0, 1.0, 1.0}; // k: 0,1
    g.get<1>() = {5, 6, 1};       // b: 5,6
    orch.add(Always{}, g);

    orch.reset();

    // enumeration: idx0 fast -> (k,b):
    // (0,5)=5000, (1,5)=5001, (0,6)=6000, (1,6)=6001
    std::array<double, 4> expected{5000.0, 5001.0, 6000.0, 6001.0};

    for (double exp : expected) {
        auto pm = orch.next();
        ASSERT_TRUE(pm.has_value());
        EXPECT_DOUBLE_EQ((*pm)(0.0), exp);
    }

    EXPECT_FALSE(orch.next().has_value());
}

TEST(OrchestratorStrategy, reverse_enumeration_order) {
    aip::core::Orchestrator<double, double, Always, aip::search::ReverseEnumerationStrategy> orch;

    Grid g;
    g.get<0>() = {0.0, 1.0, 1.0}; // k: 0,1
    g.get<1>() = {5, 6, 1};       // b: 5,6
    orch.add(Always{}, g);

    orch.reset();

    // reverse local order: last -> first:
    // local 3: (1,6)=6001, local 2: (0,6)=6000, local 1: (1,5)=5001, local 0: (0,5)=5000
    std::array<double, 4> expected{6001.0, 6000.0, 5001.0, 5000.0};

    for (double exp : expected) {
        auto pm = orch.next();
        ASSERT_TRUE(pm.has_value());
        EXPECT_DOUBLE_EQ((*pm)(0.0), exp);
    }

    EXPECT_FALSE(orch.next().has_value());
}
