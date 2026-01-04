#pragma once

#include "./control_param.hpp"

namespace aip::params {

/**
 * @brief Трейты доступа к полю параметра.
 *
 * Класс-трейты описывает, как обобщённому коду (оркестратору/поиску) работать
 * с “полем параметра” независимо от его представления.
 *
 * Базовая реализация предполагает, что поле @p Field само является значением
 * параметра (например, `double`, `int`), и доступ/установка выполняются напрямую.
 *
 * Для обёрток (например, `ControlParam<T, Name>`) предусматриваются специализации,
 * которые могут:
 *  - выбирать другой тип перебора (например, `T` вместо `ControlParam<T, Name>`),
 *  - предоставлять доступ к внутреннему значению (например, `value`),
 *  - отмечать наличие compile-time имени параметра.
 *
 * @tparam Field Тип поля, в котором хранится параметр.
 */
template <typename Field>
struct ParamTraits {
    /// Тип значения, с которым работает перебор/поиск (по умолчанию совпадает с Field).
    using range_type = Field;

    /// Тип самого поля.
    using field_type = Field;

    /// Признак наличия compile-time имени у параметра (по умолчанию нет).
    static constexpr bool is_named = false;

    /**
     * @brief Получить ссылку на значение параметра для чтения/изменения.
     * @param f Поле параметра.
     * @return Ссылка на значение параметра.
     */
    [[nodiscard]] static constexpr range_type& ref(Field& f) noexcept { return f; }

    /**
     * @brief Получить константную ссылку на значение параметра.
     * @param f Поле параметра.
     * @return Константная ссылка на значение параметра.
     */
    [[nodiscard]] static constexpr const range_type& ref(const Field& f) noexcept { return f; }

    /**
     * @brief Установить значение параметра.
     * @param f Поле параметра.
     * @param v Новое значение.
     */
    static constexpr void set(Field& f, const range_type& v) noexcept(std::is_nothrow_copy_assignable_v<Field>) {
        f = v;
    }
};

/**
 * @brief Специализация ParamTraits для ControlParam.
 *
 * См. описание общего шаблона @ref aip::params::ParamTraits.
 *
 * Данная специализация описывает правила доступа к именованному
 * управляемому параметру @ref ControlParam, где значение хранится
 * во внутреннем поле @c value, а имя параметра известно на этапе компиляции.
 */
template <typename T, core::fixed_string Name>
struct ParamTraits<ControlParam<T, Name>> {
    using range_type = T;
    using field_type = ControlParam<T, Name>;

    static constexpr bool is_named = true;
    static constexpr auto name = Name;

    [[nodiscard]] static constexpr range_type& ref(field_type& f) noexcept { return f.value; }
    [[nodiscard]] static constexpr const range_type& ref(const field_type& f) noexcept { return f.value; }

    static constexpr void set(field_type& f, const range_type& v) noexcept(std::is_nothrow_copy_assignable_v<T>) {
        f.value = v;
    }
};

}  // namespace aip::params