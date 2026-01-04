#pragma once

#include <vector>
#include <memory>
#include <limits>
#include <type_traits>

#include <aip/model/imodel.hpp>
#include <aip/model/domain_like.hpp>

namespace aip::model {

/**
 * @brief Кусочно-заданная модель (piecewise model).
 *
 * Состоит из набора сегментов (домен, модель). При вычислении значения
 * выбирается первый сегмент, домен которого содержит вход @p x.
 *
 * @tparam In     Тип входа.
 * @tparam Out    Тип выхода.
 * @tparam Domain Тип домена. Должен предоставлять `bool operator()(const In&) -> bool`.
 */
template <typename In, typename Out, typename Domain>
requires DomainLike<Domain, In>
class PiecewiseModel final : public IModel<In, Out> {
   private:
    struct Entry {
        Domain domain;
        std::shared_ptr<const IModel<In, Out>> model;
    };

    std::vector<Entry> entries;

   public:
    PiecewiseModel() = default;

    /**
     * @brief Добавить сегмент (домен + модель).
     *
     * @note Порядок добавления важен: при пересечении доменов выигрывает первый подходящий.
     */
    void add(Domain d, std::shared_ptr<const IModel<In, Out>> m) { entries.push_back({std::move(d), std::move(m)}); }

    [[nodiscard]] Out operator()(const In& x) const noexcept override {
        for (const auto& e : entries) {
            if (e.domain(x)) {
                return (*e.model)(x);
            }
        }

        if constexpr (std::is_floating_point_v<Out>) {
            return std::numeric_limits<Out>::quiet_NaN();
        } else {
            return Out{};
        }
    }
};

}  // namespace aip::model
