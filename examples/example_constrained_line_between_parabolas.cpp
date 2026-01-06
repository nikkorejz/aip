#include <aip/core/fixed_string.hpp>
#include <aip/model/imodel.hpp>
#include <aip/model/piecewise_model.hpp>

#include <aip/params/control_param.hpp>
#include <aip/params/param_grid.hpp>
#include <aip/params/uniform_range.hpp>
#include <aip/params/unit_grid.hpp>

#include <aip/core/orchestrator.hpp>

#include <cmath>
#include <iostream>

namespace ex {

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

// Binder: вписать прямую между (xL, yL) и (xR, yR)
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

} // namespace ex

int main() {
    using namespace ex;

    const double x1 = -1.0;
    const double x2 =  1.0;

    // --- Grids ---
    // Левую и правую параболу можно перебирать, но для примера возьмём фикс (size=1)
    using PGrid = aip::params::ParamGrid<Parabola, aip::params::UniformRange, &Parabola::a, &Parabola::b, &Parabola::c>;

    PGrid leftG;
    leftG.get<0>() = {1.0, 1.0, 1.0};   // a
    leftG.get<1>() = {0.0, 0.0, 1.0};   // b
    leftG.get<2>() = {0.0, 0.0, 1.0};   // c

    PGrid rightG;
    rightG.get<0>() = {0.5, 0.5, 1.0};  // a
    rightG.get<1>() = {0.0, 0.0, 1.0};  // b
    rightG.get<2>() = {0.0, 0.0, 1.0};  // c

    // Линия без перебора: UnitGrid
    aip::params::UnitGrid<Line> lineG;

    // --- Orchestrator ---
    aip::core::Orchestrator<In, Out, Domain> orch;

    orch.add(Domain{Domain::Kind::Left,  x1, x2}, leftG);

    // constrained линия: берём значения соседей на границах x1 и x2
    orch.addConstrained(
        Domain{Domain::Kind::Mid, x1, x2},
        lineG,
        /*leftBoundaryIn=*/x1,
        /*rightBoundaryIn=*/x2,
        FitLineBetween{x1, x2}
    );

    orch.add(Domain{Domain::Kind::Right, x1, x2}, rightG);

    // Единственная комбинация
    auto pm = orch.makePiecewise(0);

    const double yL = pm(x1);              // на x1 активен Mid, но она вписана по левому значению
    const double yR = pm(x2 + 1e-9);       // чуть правее x2 → Right
    const double yMid = pm(0.0);           // середина → Mid (линия)

    std::cout << "x1=" << x1 << "  pm(x1)=" << yL << "\n";
    std::cout << "x2=" << x2 << "  pm(x2+eps)=" << yR << "\n";
    std::cout << "pm(0)=" << yMid << "\n";

    // Визуальная проверка: линия между значениями парабол
    return 0;
}
