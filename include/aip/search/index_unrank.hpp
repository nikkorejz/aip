#pragma once

#include <array>
#include <cstddef>

#include <aip/search/index_space.hpp>

namespace aip::search {

/**
 * @brief Преобразовать линейный индекс в многомерный индекс (смешанная система счисления).
 *
 * Пусть bases = {b0,b1,...}. Тогда:
 *   idx[0] = linear % b0
 *   linear /= b0
 *   idx[1] = linear % b1
 *   ...
 *
 * @tparam N Число параметров.
 * @param space Пространство индексов.
 * @param linear Линейный индекс (ожидается < space.total).
 * @return Массив индексов по каждому параметру.
 */
template <std::size_t N>
[[nodiscard]] constexpr auto linear_to_multi_index(
    const IndexSpace<N>& space,
    std::size_t linear
) noexcept -> std::array<std::size_t, N> {
    std::array<std::size_t, N> out{};

    for (std::size_t i = 0; i < N; ++i) {
        const std::size_t b = space.bases[i];
        // Если b == 0, пространство пустое, но тогда total==0 и сюда лучше не заходить.
        out[i] = (b == 0) ? 0 : (linear % b);
        linear = (b == 0) ? 0 : (linear / b);
    }
    return out;
}

} // namespace aip::search
