#pragma once

#include "../core/fixed_string.hpp"

namespace aip::params {

/**
 * @brief Управляемый параметр с compile-time именем.
 *
 * Связывает:
 *  - значение параметра (runtime) в поле @ref value,
 *  - имя параметра (compile-time) в шаблонном параметре @p Name
 *    и в статическом поле @ref name.
 *
 * За счёт того, что имя является частью типа, разные имена образуют разные типы,
 * даже если @p T совпадает. Это позволяет безопасно адресоваться к параметрам
 * “по имени” на этапе компиляции (например, при хранении параметров в std::tuple).
 *
 * @tparam T    Тип значения параметра.
 * @tparam Name Имя параметра (compile-time строка).
 */
template <typename T, core::fixed_string Name>
struct ControlParam {
    /// Тип значения параметра.
    using value_type = T;

    /// Имя параметра на уровне типа (compile-time).
    static constexpr auto name = Name;

    /// Текущее значение параметра (runtime).
    T value{};

    /// Конструктор по умолчанию: value = T{}.
    constexpr ControlParam() noexcept = default;

    /// Конструктор от значения.
    constexpr ControlParam(const T& v) noexcept(std::is_nothrow_copy_constructible_v<T>) : value(v) {}

    /**
     * @brief Неявное преобразование к базовому типу.
     *
     * Удобно для использования в формулах, где параметр ведёт себя как значение @p T.
     * @warning Может скрывать ошибки (например, нежелательные неявные преобразования).
     */
    constexpr operator T() const noexcept(std::is_nothrow_copy_constructible_v<T>) { return value; }

    /**
     * @brief Присваивание из базового типа.
     * @param v Новое значение параметра.
     * @return *this
     */
    constexpr ControlParam& operator=(const T& v) noexcept(std::is_nothrow_copy_assignable_v<T>) {
        value = v;
        return *this;
    }
};

}  // namespace aip::params