#pragma once

#include <array>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

#include <aip/search/index_space.hpp>

namespace aip::search {

/**
 * @brief Построить IndexSpace из решётки параметров.
 *
 * Извлекает размеры диапазонов (range.size()) для каждого параметра решётки
 * и вычисляет общее число комбинаций (произведение bases).
 *
 * Если хотя бы один диапазон пуст (size() == 0), то total будет 0.
 *
 * @tparam Grid Тип решётки (например, aip::params::ParamGrid<...>).
 * @param grid  Экземпляр решётки параметров.
 * @return IndexSpace<Grid::N>
 */
template <typename Grid>
[[nodiscard]] constexpr auto make_index_space(const Grid& grid) noexcept -> IndexSpace<Grid::N> {
    constexpr std::size_t N = Grid::N;
    IndexSpace<N> space{};

    std::size_t total = 1;

    // Заполняем bases[i] и считаем произведение
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        ((space.bases[I] = static_cast<std::size_t>(grid.template get<I>().size())), ...);
    }(std::make_index_sequence<N>{});

    for (std::size_t i = 0; i < N; ++i) {
        const std::size_t b = space.bases[i];
        if (b == 0) {
            total = 0;
            break;
        }
        total *= b;
    }

    space.total = total;
    return space;
}

}  // namespace aip::search
