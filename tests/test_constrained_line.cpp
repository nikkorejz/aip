#include <gtest/gtest.h>

#include <aip/core/fixed_string.hpp>
#include <aip/model/imodel.hpp>
#include <aip/model/piecewise_model.hpp>

#include <aip/params/control_param.hpp>
#include <aip/params/param_grid.hpp>
#include <aip/params/uniform_range.hpp>
#include <aip/params/unit_grid.hpp>

#include <aip/core/orchestrator.hpp>

#include <cmath>

namespace {

using In = double;
using Out = double;

struct Domain {
    enum class Kind { Left, Mid, Right } kind{};
    double x1{}, x2{};

    constexpr bool operator()(const double& x) const noexcept {
        if (kind == Kind::Left)  return x < x1;
        if (kind == Kind::Mid)   return (x >= x1) && (x < x2);
        return x >= x2;
    }
};

struct Parabola final : aip::model::IModel<In, Out> {
    aip::params::ControlParam<double, aip::core::fixed_string{"a"}> a{};
    aip::params::ControlParam<double, aip::core::fixed_string{"b"}> b{};
    aip::params::ControlParam<double, aip::core::fixed_string{"c"}> c{};

    [[nodiscard]] Out operator()(const In& x) const noexcept override {
        return a.value * x * x + b.value * x + c.value;
    }
};

struct Line final : aip::model::IModel<In, Out> {
    aip::params::ControlParam<double, aip::core::fixed_string{"k"}> k{};
    aip::params::ControlParam<double, aip::core::fixed_string{"m"}> m{};

    [[nodiscard]] Out operator()(const In& x) const noexcept override {
        return k.value * x + m.value;
    }
};

struct FitLineBetween {
    double xL{};
    double xR{};
    void operator()(Line& line, const Out& yL, const Out& yR) const noexcept {
        const double k = (yR - yL) / (xR - xL);
        const double m = yL - k * xL;
        line.k = k;
        line.m = m;
    }
};

} // namespace

TEST(Constrained, line_between_two_parabolas_matches_boundaries) {
    const double x1 = -1.0;
    const double x2 =  1.0;

    using PGrid = aip::params::ParamGrid<Parabola, aip::params::UniformRange, &Parabola::a, &Parabola::b, &Parabola::c>;
    PGrid leftG;
    leftG.get<0>() = {1.0, 1.0, 1.0}; // a
    leftG.get<1>() = {0.0, 0.0, 1.0}; // b
    leftG.get<2>() = {0.0, 0.0, 1.0}; // c

    PGrid rightG;
    rightG.get<0>() = {0.5, 0.5, 1.0};
    rightG.get<1>() = {0.0, 0.0, 1.0};
    rightG.get<2>() = {0.0, 0.0, 1.0};

    aip::params::UnitGrid<Line> lineG;

    aip::core::Orchestrator<In, Out, Domain> orch;
    orch.add(Domain{Domain::Kind::Left,  x1, x2}, leftG);
    orch.addConstrained(Domain{Domain::Kind::Mid, x1, x2}, lineG, x1, x2, FitLineBetween{x1, x2});
    orch.add(Domain{Domain::Kind::Right, x1, x2}, rightG);

    auto pm = orch.makePiecewise(0);

    // Ожидаемые значения соседей на границах:
    // left parabola: y = 1*x^2
    // right parabola: y = 0.5*x^2
    const double yL_expected = 1.0 * x1 * x1;     // 1
    const double yR_expected = 0.5 * x2 * x2;     // 0.5

    // На x1 мы попадаем в Mid-домен (линия), но она должна совпасть с yL_expected
    EXPECT_NEAR(pm(x1), yL_expected, 1e-12);

    // Чуть правее x2 — Right-домен
    EXPECT_NEAR(pm(x2 + 1e-9), yR_expected, 2e-9);

    // Проверим середину для линии: она должна быть линейной интерполяцией
    // между (x1,yL) и (x2,yR).
    const double k = (yR_expected - yL_expected) / (x2 - x1);
    const double m = yL_expected - k * x1;
    const double yMid_expected = k * 0.0 + m;

    EXPECT_NEAR(pm(0.0), yMid_expected, 1e-12);
}
