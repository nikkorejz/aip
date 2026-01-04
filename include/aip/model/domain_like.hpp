#pragma once

#include <concepts>

namespace aip::model {

/**
 * @brief Концепт “домен-предикат”.
 *
 * Тип @p D считается доменом для входа типа @p In, если его можно вызвать
 * как функцию `d(x)` и результат приводится к `bool`.
 *
 * Используется в кусочно-заданных моделях (PiecewiseModel) для проверки,
 * принадлежит ли вход @p x домену сегмента.
 *
 * @tparam D  Тип домена (предиката).
 * @tparam In Тип входного значения.
 */
template <typename D, typename In>
concept DomainLike = requires(const D& d, const In& x) {
    { d(x) } -> std::convertible_to<bool>;
};

}  // namespace aip::model