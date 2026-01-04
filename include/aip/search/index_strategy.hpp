#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <optional>

#include <aip/search/index_space.hpp>

namespace aip::search {

/**
 * @brief Концепт стратегии генерации индексов параметров.
 *
 * Стратегия описывает способ обхода дискретного пространства параметров,
 * заданного через IndexSpace. Стратегия является *stateful*:
 *  - сначала вызывается reset(space),
 *  - затем многократно вызывается next(),
 *  - next() возвращает следующую комбинацию индексов или std::nullopt,
 *    если перебор завершён.
 *
 * Стратегия может использовать всю информацию из IndexSpace (bases, total)
 * для реализации произвольной логики обхода (полный перебор, эвристики,
 * случайный поиск, МГС и т.п.).
 *
 * @tparam S Тип стратегии.
 * @tparam N Размерность пространства (число параметров).
 */
template <class S, std::size_t N>
concept IndexStrategy =
    requires(S s, const IndexSpace<N>& space) {
        // Тип, представляющий одну комбинацию индексов
        typename S::index_type;
        requires std::same_as<typename S::index_type, std::array<std::size_t, N>>;

        // Инициализация стратегии новым пространством
        { s.reset(space) } -> std::same_as<void>;

        // Получение следующего набора индексов
        { s.next() } -> std::same_as<std::optional<typename S::index_type>>;
    };

} // namespace aip::search
