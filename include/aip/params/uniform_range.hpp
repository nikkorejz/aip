#pragma once

#include <cstddef>
#include <cmath>
#include <type_traits>

#include <aip/params/range_like.hpp>

namespace aip::params {

/**
 * @brief Равномерный диапазон значений (min/max/step).
 *
 * Представляет набор значений:
 *   min, min + step, min + 2*step, ... <= max
 *
 * Используется как простой источник кандидатов для перебора параметра.
 * Подходит для скалярных “арифметических” типов, которые можно безопасно
 * приводить к double для вычисления размера и индексации.
 *
 * @tparam T Тип значения (например, double, float, int).
 *
 * @note Для векторных/сложных типов рекомендуется предоставить собственный
 *       тип диапазона, удовлетворяющий концепту @ref aip::params::RangeLike.
 */
template <typename T>
struct UniformRange {
    /// Тип значения диапазона.
    using value_type = T;

    /// Минимальное значение (первый элемент диапазона).
    T min{};

    /// Максимальное значение (верхняя граница диапазона).
    T max{};

    /// Шаг (должен быть > 0).
    T step{};

    /**
     * @brief Количество значений в диапазоне.
     *
     * Возвращает 0, если:
     *  - step <= 0
     *  - max < min
     *
     * @return Число элементов, доступных через operator[].
     *
     * @warning Реализация использует приведение к double и std::floor, поэтому
     *          корректна только для “скалярных” типов.
     */
    [[nodiscard]] constexpr std::size_t size() const noexcept {
        // Базовые проверки на типе T
        if (!(step > T{})) return 0;
        if (max < min) return 0;

        // Вычисление через double (см. @warning)
        const double dmin  = static_cast<double>(min);
        const double dmax  = static_cast<double>(max);
        const double dstep = static_cast<double>(step);

        if (!(dstep > 0.0)) return 0;
        if (dmax < dmin) return 0;

        return static_cast<std::size_t>(std::floor((dmax - dmin) / dstep)) + 1;
    }

    /**
     * @brief Значение диапазона по индексу.
     *
     * @param i Индекс (0 <= i < size()).
     * @return min + i * step.
     *
     * @warning Не выполняет проверку границ; предполагается, что i валиден.
     */
    [[nodiscard]] constexpr T operator[](std::size_t i) const noexcept {
        const double dmin  = static_cast<double>(min);
        const double dstep = static_cast<double>(step);
        return static_cast<T>(dmin + static_cast<double>(i) * dstep);
    }
};

// Самопроверка: UniformRange должен удовлетворять RangeLike.
static_assert(RangeLike<UniformRange<double>>);

} // namespace aip::params