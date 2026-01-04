#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include <aip/search/index_space.hpp>

namespace aip::search {

/**
 * @brief Стратегия полного перебора индексов (смешанная система счисления).
 *
 * Генерирует все комбинации индексов в лексикографическом порядке,
 * где индекс 0 меняется быстрее всего.
 *
 * Использование:
 *  - вызвать reset(space)
 *  - затем вызывать next() до std::nullopt
 *
 * @tparam N Размерность пространства (число параметров).
 */
template <std::size_t N>
class EnumerationStrategy {
public:
    using index_type = std::array<std::size_t, N>;

    EnumerationStrategy() = default;

    /**
     * @brief Инициализировать стратегию новым пространством индексов.
     *
     * @param s Пространство индексов (bases + total).
     */
    void reset(const IndexSpace<N>& s) noexcept {
        space = &s;
        current.fill(0);
        finished = (s.total == 0);
        first = true;
    }

    /**
     * @brief Получить следующую комбинацию индексов.
     *
     * @return Массив индексов или std::nullopt, если перебор завершён.
     */
    [[nodiscard]] std::optional<index_type> next() noexcept {
        if (!space || finished) return std::nullopt;

        if (first) {
            first = false;
            return current;
        }

        // Инкремент в смешанной системе счисления
        for (std::size_t i = 0; i < N; ++i) {
            const std::size_t base = space->bases[i];
            if (base == 0) { finished = true; return std::nullopt; }

            if (++current[i] < base) {
                return current;
            }
            current[i] = 0; // перенос
        }

        finished = true;
        return std::nullopt;
    }

private:
    const IndexSpace<N>* space{nullptr};
    index_type current{};
    bool first{true};
    bool finished{true};
};

} // namespace aip::search
