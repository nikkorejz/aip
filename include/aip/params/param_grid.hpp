#pragma once

#include <array>
#include <tuple>
#include <utility>
#include <string_view>
#include <type_traits>

#include <aip/params/range_like.hpp>
#include <aip/params/param_traits.hpp>
#include <aip/params/control_param.hpp>

namespace aip::params {

/**
 * @brief Решётка (grid) управляемых параметров модели.
 *
 * ParamGrid описывает пространство параметров модели в виде декартова
 * произведения диапазонов значений. Каждый параметр модели задаётся
 * указателем на поле (`pointer-to-member`) и связан с диапазоном значений
 * соответствующего типа.
 *
 * Основные идеи:
 *  - Набор управляемых параметров (`Members...`) фиксируется на этапе компиляции.
 *  - Тип модели (`Model`) и список параметров являются частью типа ParamGrid.
 *  - Значения диапазонов (min/max/step и т.п.) настраиваются во время выполнения.
 *
 * ParamGrid позволяет:
 *  - настраивать диапазоны параметров,
 *  - вычислять общее число комбинаций параметров,
 *  - конструировать экземпляры модели по набору индексов,
 *  - получать доступ к диапазонам по индексу или имени параметра,
 *  - выполнять runtime-интроспекцию параметров (для UI/обобщённого кода).
 *
 * Тип диапазона параметра задаётся шаблонным параметром RangeT и должен
 * удовлетворять концепту RangeLike.
 *
 * Доступ к полям модели и их значениям осуществляется через ParamTraits,
 * что позволяет одинаково работать как с "голыми" полями (например, double),
 * так и с обёртками вроде ControlParam.
 *
 * @tparam Model   Тип модели, параметры которой перебираются.
 * @tparam RangeT  Шаблон типа диапазона значений (например, UniformRange).
 * @tparam Members Список указателей на поля модели (pointer-to-member),
 *                 определяющих управляемые параметры.
 *
 * @note Модель должна быть default-constructible.
 * @note Изменение списка Members требует перекомпиляции.
 * @note Изменение диапазонов параметров допускается во время выполнения.
 */
template <typename Model, template <typename> typename RangeT, auto... Members>
struct ParamGrid {
    using model_type = Model;

    /// @brief Кол-во параметров в сетке
    static constexpr std::size_t N = sizeof...(Members);

    static_assert(N > 0, "ParamGrid: at least one parameter member is required");

    struct ParamMeta {
        std::string_view label;  // пусто, если параметр неименованный
        std::size_t index{};
        bool is_named{};
    };

   private:
    /**
     * Позиция несуществующего элемета
     */
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    // Извлечение типа поля модели по pointer-to-member (&Model::field)
    // через идиому decltype(std::declval<T&>().*M).
    template <auto M>
    using member_field_type = std::remove_reference_t<decltype(std::declval<Model&>().*M)>;

    // Тип значения, используемый в диапазоне для данного элемента M (через ParamTraits)
    template <auto M>
    using range_value_type = typename ParamTraits<member_field_type<M>>::range_type;

    // Проверяем, что RangeT<T> удовлетворяет RangeLike для всех параметров.
    static_assert((RangeLike<RangeT<range_value_type<Members>>> && ...),
                  "ParamGrid: RangeT<T> must satisfy RangeLike for all parameter types");

    template <std::size_t I>
    static consteval auto memberPtrAt() {
        return std::get<I>(std::tuple{Members...});
    }

    template <std::size_t I>
    using field_at = member_field_type<memberPtrAt<I>()>;

    template <std::size_t I>
    using traits_at = ParamTraits<field_at<I>>;

    /// Установить один параметр модели
    template <std::size_t I>
    constexpr void assignOne(Model& m, std::size_t i) const noexcept {
        constexpr auto mp = memberPtrAt<I>();
        using Traits = traits_at<I>;

        const auto& r = std::get<I>(ranges);
        const auto v = r[i];    // значение из диапазона
        Traits::set(m.*mp, v);  // присваивание в поле модели (через ParamTraits)
    }

    /// Установить все параметры модели
    template <std::size_t... I>
    constexpr void assignAll(Model& m, const std::array<std::size_t, N>& idx,
                             std::index_sequence<I...>) const noexcept {
        (assignOne<I>(m, idx[I]), ...);
    }

    template <aip::core::fixed_string Label, std::size_t... I>
    static consteval std::size_t indexByLabelImpl(std::index_sequence<I...>) {
        std::size_t out = npos;

        (([&] {
             using Traits = traits_at<I>;
             if constexpr (Traits::is_named) {
                 if constexpr (Traits::name == Label) {
                     if (out == npos) out = I;  // first match wins
                 }
             }
         }()),
         ...);

        return out;
    }

    template <aip::core::fixed_string Label>
    static consteval std::size_t indexByLabel() {
        constexpr std::size_t out = indexByLabelImpl<Label>(std::make_index_sequence<N>{});
        static_assert(out != npos,
                      "ParamGrid::getByLabel: no parameter with such label (or field is not a named ControlParam)");
        return out;
    }

    template <std::size_t I>
    static constexpr std::string_view labelAt() noexcept {
        if constexpr (traits_at<I>::is_named) {
            return traits_at<I>::name.sv();
        } else {
            return {};
        }
    }

    template <typename Fn, std::size_t... I>
    constexpr void forEachParamImpl(Fn&& fn, std::index_sequence<I...>) noexcept {
        (std::forward<Fn>(fn)(ParamMeta{labelAt<I>(), I, traits_at<I>::is_named}, std::get<I>(ranges)), ...);
    }

    template <typename Fn, std::size_t... I>
    constexpr void forEachParamImplConst(Fn&& fn, std::index_sequence<I...>) const noexcept {
        (std::forward<Fn>(fn)(ParamMeta{labelAt<I>(), I, traits_at<I>::is_named}, std::get<I>(ranges)), ...);
    }

   public:
    /**
     * @brief Набор диапазонов для всех управляемых параметров.
     *
     * Элемент i соответствует i-му указателю на поле из списка `Members...`.
     * Тип каждого диапазона выводится через ParamTraits:
     *  - для поля `double` хранится UniformRange<double>
     *  - для поля `ControlParam<double, "...">` тоже хранится UniformRange<double>
     */
    std::tuple<RangeT<range_value_type<Members>>...> ranges{};

    /**
     * @brief Доступ к диапазону параметра по индексу (0..N-1).
     *
     * Используется для настройки диапазонов до запуска перебора.
     *
     * @tparam I Индекс параметра (compile-time).
     * @return Ссылка на соответствующий диапазон.
     */
    template <std::size_t I>
    [[nodiscard]] constexpr auto& get() noexcept {
        static_assert(I < N, "ParamGrid::get<I>: index out of range");
        return std::get<I>(ranges);
    }

    /**
     * @brief Константный доступ к диапазону параметра по индексу (0..N-1).
     *
     * @tparam I Индекс параметра (compile-time).
     * @return Константная ссылка на соответствующий диапазон.
     */
    template <std::size_t I>
    [[nodiscard]] constexpr const auto& get() const noexcept {
        static_assert(I < N, "ParamGrid::get<I>: index out of range");
        return std::get<I>(ranges);
    }

    /**
     * @brief Общее число комбинаций параметров (размер решётки).
     *
     * Вычисляется как произведение размеров всех диапазонов.
     * Если хотя бы один диапазон пуст, результат будет 0.
     */
    [[nodiscard]] constexpr std::size_t size() const noexcept {
        std::size_t total = 1;
        std::apply([&](const auto&... r) { ((total *= r.size()), ...); }, ranges);
        return total;
    }

    /**
     * @brief Сконструировать модель, выбрав значения параметров по индексам.
     *
     * @param idx Массив из N индексов, по одному на каждый параметр (в порядке Members...).
     * @return Экземпляр модели с установленными параметрами.
     *
     * @note Границы не проверяются: предполагается, что 0 <= idx[j] < ranges[j].size().
     */
    [[nodiscard]] constexpr Model makeModel(const std::array<std::size_t, N>& idx) const noexcept {
        static_assert(std::is_default_constructible_v<Model>,
                      "ParamGrid::makeModel: Model must be default-constructible");

        Model m{};
        assignAll(m, idx, std::make_index_sequence<N>{});
        return m;
    }

    /**
     * @brief Доступ к диапазону по compile-time имени параметра.
     *
     * Работает только для именованных параметров (например, ControlParam<T, Name>).
     * Если параметр с таким именем не найден — ошибка компиляции.
     *
     * @tparam Label Имя параметра (compile-time строка).
     */
    template <aip::core::fixed_string Label>
    [[nodiscard]] constexpr auto& getByLabel() noexcept {
        constexpr std::size_t i = indexByLabel<Label>();
        return std::get<i>(ranges);
    }

    /**
     * @brief Константный доступ к диапазону по compile-time имени параметра.
     *
     * @tparam Label Имя параметра (compile-time строка).
     */
    template <aip::core::fixed_string Label>
    [[nodiscard]] constexpr const auto& getByLabel() const noexcept {
        constexpr std::size_t i = indexByLabel<Label>();
        return std::get<i>(ranges);
    }

    /**
     * @brief Вызвать функцию для каждого параметра (runtime-интроспекция).
     *
     * fn(meta, range) вызывается для каждого параметра в порядке Members...
     * range — ссылка на реальный RangeT<T>, хранимый внутри `ranges`.
     */
    template <typename Fn>
    constexpr void forEachParam(Fn&& fn) noexcept {
        forEachParamImpl(std::forward<Fn>(fn), std::make_index_sequence<N>{});
    }

    /**
     * @brief Константный вариант forEachParam.
     */
    template <typename Fn>
    constexpr void forEachParam(Fn&& fn) const noexcept {
        forEachParamImplConst(std::forward<Fn>(fn), std::make_index_sequence<N>{});
    }

    /**
     * @brief Найти диапазон по runtime-имени (только для именованных параметров).
     *
     * Возвращает указатель на диапазон RangeT<T>, если найден именованный параметр
     * с заданным label и его value_type совпадает с T. Иначе возвращает nullptr.
     *
     * @tparam T Ожидаемый тип значения диапазона.
     * @param label Имя параметра (runtime строка).
     */
    template <typename T>
    [[nodiscard]] constexpr RangeT<T>* find(std::string_view label) noexcept {
        RangeT<T>* out = nullptr;

        forEachParam([&](ParamMeta meta, auto& r) {
            if (out) return;
            if (!meta.is_named) return;
            if (meta.label != label) return;

            using R = std::remove_reference_t<decltype(r)>;
            using V = typename R::value_type;

            if constexpr (std::is_same_v<V, T>) {
                out = &r;
            }
        });

        return out;
    }

    /**
     * @brief Константный вариант find().
     */
    template <typename T>
    [[nodiscard]] constexpr const RangeT<T>* find(std::string_view label) const noexcept {
        const RangeT<T>* out = nullptr;

        forEachParam([&](ParamMeta meta, const auto& r) {
            if (out) return;
            if (!meta.is_named) return;
            if (meta.label != label) return;

            using R = std::remove_reference_t<decltype(r)>;
            using V = typename R::value_type;

            if constexpr (std::is_same_v<V, T>) {
                out = &r;
            }
        });

        return out;
    }
};

}  // namespace aip::params