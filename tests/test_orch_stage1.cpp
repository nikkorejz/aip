#include <gtest/gtest.h>

#include <aip/core/fixed_string.hpp>
#include <aip/model/imodel.hpp>
#include <aip/model/piecewise_model.hpp>
#include <aip/params/control_param.hpp>
#include <aip/params/uniform_range.hpp>
#include <aip/params/param_grid.hpp>
#include <aip/core/orchestrator.hpp>

using namespace aip::core;

namespace {
// домен-предикат
struct Always {
    constexpr bool operator()(const double&) const noexcept { return true; }
};

// модель обязана наследовать IModel
struct M final : aip::model::IModel<double, double> {
    double k{};
    aip::params::ControlParam<int, aip::core::fixed_string{"b"}> b{};

    [[nodiscard]] double operator()(const double&) const noexcept override {
        return k + 1000.0 * static_cast<double>(b.value);
    }
};

using Grid = aip::params::ParamGrid<M, aip::params::UniformRange, &M::k, &M::b>;
}  // namespace

TEST(Orchestrator_IModelOnly, makePiecewise_works) {
    Orchestrator<double, double, Always> orch;

    Grid g;
    g.get<0>() = {0.0, 1.0, 1.0};  // k: {0,1}
    g.get<1>() = {5, 6, 1};        // b: {5,6}
    orch.add(Always{}, g);

    auto pm = orch.makePiecewise(3);  // idx {1,1} => k=1, b=6
    EXPECT_DOUBLE_EQ(pm(0.0), 6001.0);
}
