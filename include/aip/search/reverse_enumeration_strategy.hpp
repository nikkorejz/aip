#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include <aip/search/index_space.hpp>

namespace aip::search {

/**
 * @brief Стратегия перебора индексов в обратном порядке.
 *
 * Генерирует комбинации индексов так, будто мы перечисляем local = total-1 ... 0,
 * а затем раскладываем local по базам (смешанная система).
 *
 * Нужна как простой пример альтернативной стратегии.
 *
 * @tparam N Размерность пространства.
 */
template <std::size_t N>
class ReverseEnumerationStrategy {
public:
    using index_type = std::array<std::size_t, N>;

    void reset(const IndexSpace<N>& s) noexcept {
        space = &s;
        if (s.total == 0) {
            finished = true;
            currentLocal = 0;
        } else {
            finished = false;
            currentLocal = s.total - 1;
        }
    }

    [[nodiscard]] std::optional<index_type> next() noexcept {
        if (!space || finished) return std::nullopt;

        index_type idx{};
        std::size_t local = currentLocal;

        for (std::size_t i = 0; i < N; ++i) {
            const std::size_t base = space->bases[i];
            if (base == 0) { finished = true; return std::nullopt; }
            idx[i] = local % base;
            local /= base;
        }

        if (currentLocal == 0) finished = true;
        else --currentLocal;

        return idx;
    }

private:
    const IndexSpace<N>* space{nullptr};
    std::size_t currentLocal{0};
    bool finished{true};
};

} // namespace aip::search
