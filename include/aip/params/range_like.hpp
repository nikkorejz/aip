#pragma once

#include <concepts>
#include <cstddef>

namespace aip::params {

/**
 * @brief Концепт индексируемого диапазона значений параметра.
 *
 * Тип @p R считается диапазоном (RangeLike), если:
 *  - у него есть `size()`, возвращающий размер (std::size_t),
 *  - у него есть `operator[](std::size_t)`, возвращающий значение по индексу,
 *  - определён тип значения `value_type`.
 *
 * Такой контракт позволяет алгоритмам перебора работать как с простыми
 * скалярными диапазонами (min/max/step), так и с пользовательскими диапазонами
 * (например, для векторов), без виртуальности и динамического полиморфизма.
 */
template <typename R>
concept RangeLike =
    requires(const R& r, std::size_t i) {
        typename R::value_type;
        { r.size() } -> std::convertible_to<std::size_t>;
        { r[i] } -> std::convertible_to<typename R::value_type>;
    };

} // namespace aip::params
