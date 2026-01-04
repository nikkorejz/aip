#pragma once

#include <cstddef>
#include <string_view>

namespace aip::core {

/**
 * @brief Compile-time оболочка с фиксированной строкой (NTTP-helper).
 *
 * Представляет собой строковый литерал, хранящийся как тип значения и используемый в compile-time (например, в качестве
 * параметра шаблона).
 *
 * Предназначен для присвоения имен и идентификации сущностей, таких как параметры модели, типобезопасным способом и во
 * время компиляции
 *
 * @tparam N Размер строки, включая нулевой разделитель ('\0').
 */
template <std::size_t N>
struct fixed_string {
    char value[N]{};

    /// @brief Construct from a string literal.
    constexpr fixed_string(const char (&str)[N]) noexcept {
        for (std::size_t i = 0; i < N; ++i) value[i] = str[i];
    }

    /**
     * @brief Get string view without the null terminator.
     * @return String view of the stored literal.
     */
    [[nodiscard]] constexpr std::string_view sv() const noexcept { return std::string_view(value, N ? (N - 1) : 0); }

    // template <std::size_t N, std::size_t M>
    // [[nodiscard]] constexpr bool operator==(const fixed_string<N>& a, const fixed_string<M>& b) noexcept;
};

template <std::size_t N, std::size_t M>
[[nodiscard]] constexpr bool operator==(const fixed_string<N>& a, const fixed_string<M>& b) noexcept {
    return a.sv() == b.sv();
}

template <std::size_t N, std::size_t M>
[[nodiscard]] constexpr bool operator!=(const fixed_string<N>& a, const fixed_string<M>& b) noexcept {
    return !(a == b);
}

}  // namespace aip::core