#include <gtest/gtest.h>
#include <type_traits>

#include <aip/params/param_traits.hpp>

TEST(ParamTraits, plain_field) {
    using Traits = aip::params::ParamTraits<double>;

    static_assert(std::is_same_v<Traits::range_type, double>);
    static_assert(std::is_same_v<Traits::field_type, double>);
    static_assert(Traits::is_named == false);

    double x = 1.0;
    Traits::set(x, 2.5);
    EXPECT_EQ(x, 2.5);

    Traits::ref(x) = 3.0;
    EXPECT_EQ(x, 3.0);
}

TEST(ParamTraits, control_param_field) {
    using P = aip::params::ControlParam<int, aip::core::fixed_string{"k"}>;
    using Traits = aip::params::ParamTraits<P>;

    static_assert(std::is_same_v<Traits::range_type, int>);
    static_assert(std::is_same_v<Traits::field_type, P>);
    static_assert(Traits::is_named == true);
    static_assert(Traits::name.sv() == "k");

    P p{};
    Traits::set(p, 69);
    EXPECT_EQ(p.value, 69);

    Traits::ref(p) = 7;
    EXPECT_EQ(p.value, 7);

    int v = Traits::ref(p);
    EXPECT_EQ(v, 7);
}