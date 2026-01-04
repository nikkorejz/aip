#include <gtest/gtest.h>

#include <type_traits>

#include <aip/params/param_grid.hpp>
#include <aip/params/control_param.hpp>
#include <aip/params/uniform_range.hpp>

using namespace aip::params;

namespace {

struct M {
    double k;
    ControlParam<int, "b"> b;
};

using Grid = ParamGrid<M, UniformRange, &M::k, &M::b>;

}

TEST(ParamGridStage1, get_allows_setting_ranges) {
    Grid g;

    g.get<0>() = {0.0, 1.0, 0.5};   // for k
    g.get<1>() = {1, 5, 1};         // for b.value (int)

    EXPECT_EQ(g.get<0>().size(), 3u); // 0, 0.5, 1.0
    EXPECT_EQ(g.get<1>().size(), 5u); // 1,2,3,4,5
}

TEST(ParamGridStage1, range_types_are_deduced_via_param_traits) {
    Grid g;

    using R0 = std::remove_reference_t<decltype(g.get<0>())>;
    using R1 = std::remove_reference_t<decltype(g.get<1>())>;

    static_assert(std::is_same_v<typename R0::value_type, double>);
    static_assert(std::is_same_v<typename R1::value_type, int>);

    EXPECT_TRUE((std::is_same_v<typename R0::value_type, double>));
    EXPECT_TRUE((std::is_same_v<typename R1::value_type, int>));
}

TEST(ParamGridStage2, size_is_product_of_ranges) {
    Grid g;

    g.get<0>() = {0.0, 1.0, 0.5}; // size = 3
    g.get<1>() = {1, 4, 1};       // size = 4

    EXPECT_EQ(g.size(), 12u);
}

TEST(ParamGridStage2, size_is_zero_if_any_range_empty) {
    Grid g;

    g.get<0>() = {0.0, 1.0, 0.5}; // size = 3
    g.get<1>() = {1, 1, 0};       // step=0 => size = 0

    EXPECT_EQ(g.size(), 0u);
}

TEST(ParamGridStage3, makeModel_assigns_plain_and_control_param_fields) {
    Grid g;

    g.get<0>() = {0.0, 1.0, 0.5}; // k: 0, 0.5, 1.0
    g.get<1>() = {10, 12, 1};     // b: 10, 11, 12

    // Возьмём k = 1.0 (index 2), b = 11 (index 1)
    const auto m = g.makeModel(std::array<std::size_t, 2>{2, 1});

    EXPECT_DOUBLE_EQ(m.k, 1.0);
    EXPECT_EQ(m.b.value, 11);
}

TEST(ParamGridStage4, getByLabel_returns_range_for_named_param) {
    Grid g;

    // Доступ по имени "b" должен указывать на тот же диапазон, что и get<1>()
    g.getByLabel<aip::core::fixed_string{"b"}>() = {10, 12, 1};

    EXPECT_EQ(g.get<1>().size(), 3u);
    EXPECT_EQ(g.get<1>()[0], 10);
    EXPECT_EQ(g.get<1>()[2], 12);
}

TEST(ParamGridStage4, getByLabel_can_be_used_to_modify_range) {
    Grid g;

    auto& rb = g.getByLabel<aip::core::fixed_string{"b"}>();
    rb = {1, 5, 2};

    EXPECT_EQ(g.get<1>().size(), 3u); // 1,3,5
}

TEST(ParamGridStage5, forEachParam_reports_meta_and_allows_access_to_ranges) {
    Grid g;
    g.get<0>() = {0.0, 1.0, 0.5};
    g.get<1>() = {10, 12, 1};

    std::vector<std::string> labels;
    std::vector<std::size_t> indices;
    std::vector<bool> named;

    g.forEachParam([&](Grid::ParamMeta meta, auto& r) {
        labels.emplace_back(meta.label);
        indices.push_back(meta.index);
        named.push_back(meta.is_named);

        // просто проверим, что range доступен
        (void)r.size();
    });

    ASSERT_EQ(labels.size(), 2u);
    EXPECT_EQ(indices[0], 0u);
    EXPECT_EQ(indices[1], 1u);

    EXPECT_FALSE(named[0]);          // k неименованный
    EXPECT_TRUE(named[1]);           // b именованный

    EXPECT_EQ(labels[0], "");        // пусто
    EXPECT_EQ(labels[1], "b");
}

TEST(ParamGridStage6, find_returns_pointer_for_matching_label_and_type) {
    Grid g;
    g.get<1>() = {10, 12, 1};

    auto* r = g.find<int>("b");
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->size(), 3u);
    EXPECT_EQ((*r)[0], 10);
}

TEST(ParamGridStage6, find_returns_nullptr_on_type_mismatch) {
    Grid g;
    g.get<1>() = {10, 12, 1};

    auto* r = g.find<double>("b"); // b is int
    EXPECT_EQ(r, nullptr);
}

TEST(ParamGridStage6, find_returns_nullptr_for_unknown_or_unnamed) {
    Grid g;
    g.get<0>() = {0.0, 1.0, 0.5};

    EXPECT_EQ(g.find<double>("k"), nullptr); // k неименованный
    EXPECT_EQ(g.find<double>("nope"), nullptr);
}
