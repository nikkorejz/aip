#include <gtest/gtest.h>
#include <aip/params/control_param.hpp>

TEST(ControlParam, name_is_compile_time) {
    using K = aip::params::ControlParam<double, aip::core::fixed_string{"k"}>;
    static_assert(K::name.sv() == "k");
    EXPECT_EQ(K::name.sv(), "k");
}

TEST(ControlParam, default_value_and_assignment) {
    using K = aip::params::ControlParam<int, aip::core::fixed_string{"k"}>;
    K k;
    EXPECT_EQ(k.value, 0);

    k = 42;
    EXPECT_EQ(k.value, 42);

    int x = k;          // implicit conversion
    EXPECT_EQ(x, 42);
}