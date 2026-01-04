#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>

#include <aip/model/imodel.hpp>
#include <aip/model/piecewise_model.hpp>

// Простой домен-интервал: домен = предикат
struct Interval {
    double a{}, b{};
    constexpr bool operator()(const double& x) const noexcept {
        return a <= x && x <= b;
    }
};

// Простая модель-наследник IModel
struct LinearModel final : aip::model::IModel<double, double> {
    double k{}, b{};
    LinearModel(double kk, double bb) : k(kk), b(bb) {}
    [[nodiscard]] double operator()(const double& x) const noexcept override {
        return k * x + b;
    }
};

TEST(PiecewiseModel, selects_first_matching_segment) {
    aip::model::PiecewiseModel<double, double, Interval> pm;

    auto m1 = std::make_shared<LinearModel>(1.0, 0.0);   // y = x
    auto m2 = std::make_shared<LinearModel>(10.0, 0.0);  // y = 10x

    pm.add(Interval{0.0, 1.0}, m1);
    pm.add(Interval{0.5, 2.0}, m2); // пересекается с первым

    // 0.75 попадает в оба, но должен выбрать первый (first match wins)
    EXPECT_DOUBLE_EQ(pm(0.75), 0.75);
}

TEST(PiecewiseModel, uses_second_segment_when_first_not_matching) {
    aip::model::PiecewiseModel<double, double, Interval> pm;

    auto m1 = std::make_shared<LinearModel>(1.0, 0.0);   // [0,1]
    auto m2 = std::make_shared<LinearModel>(10.0, 0.0);  // [2,3]

    pm.add(Interval{0.0, 1.0}, m1);
    pm.add(Interval{2.0, 3.0}, m2);

    EXPECT_DOUBLE_EQ(pm(2.5), 25.0);
}

TEST(PiecewiseModel, returns_nan_when_no_segment_matches) {
    aip::model::PiecewiseModel<double, double, Interval> pm;

    auto m1 = std::make_shared<LinearModel>(1.0, 0.0);
    pm.add(Interval{0.0, 1.0}, m1);

    const double y = pm(10.0);
    EXPECT_TRUE(std::isnan(y));
}
