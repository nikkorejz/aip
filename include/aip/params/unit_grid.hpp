#pragma once

#include <array>
#include <cstddef>

namespace aip::params {

/**
 * @brief Решётка из одного варианта (для моделей без перебираемых параметров).
 * 
 * @example Может быть полезно для модели линии (kx + b) с 0 степенью свободы (заданы граничные точки)
 */
template <class Model>
struct UnitGrid {
    using model_type = Model;
    static constexpr std::size_t N = 0;

    [[nodiscard]] constexpr std::size_t size() const noexcept { return 1; }

    [[nodiscard]] constexpr Model makeModel(const std::array<std::size_t, 0>&) const noexcept { return Model{}; }
};

}  // namespace aip::params
