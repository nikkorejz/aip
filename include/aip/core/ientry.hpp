#pragma once

#include <memory>
#include <vector>
#include <cstddef>
#include <optional>
#include <typeindex>
#include <string_view>

#include <aip/model/imodel.hpp>
#include <aip/model/piecewise_model.hpp>

namespace aip::core::detail {

template <class In, class Out, class Domain>
using IM = aip::model::IModel<In, Out>;

template <class In, class Out, class Domain>
using PM = aip::model::PiecewiseModel<In, Out, Domain>;

/**
 * @brief Внутренний type-erasure интерфейс для сегмента оркестратора.
 *
 * Каждый сегмент = (domain + grid + strategy-state).
 * Нужен, чтобы хранить разные Grid-типы в одном std::vector.
 *
 * IEntry — строго внутренняя деталь реализации (не часть публичного API).
 */
template <class In, class Out, class Domain>
struct IEntry {
    // Type-erasure — это способ хранить и вызывать объекты разных типов так, будто они одного типа.

    using M = IM<In, Out, Domain>;

   protected:
    std::string model_name;

   public:
    virtual ~IEntry() = default;

    /// @brief Число локальных комбинаций параметров в этом сегменте.
    [[nodiscard]] virtual std::size_t size() const noexcept = 0;

    /// @brief Перейти к следующему варианту. false если вариантов больше нет.
    virtual bool next() = 0;

    /// @brief Подготовить стратегию и установить "текущий" вариант на первый (если он существует).
    virtual void reset() = 0;

    virtual std::optional<std::vector<std::size_t>> currentIdx() const = 0;

    virtual std::optional<std::size_t> currentLocal() const noexcept = 0;

    virtual const Domain& getDomain() const noexcept = 0;

    /**
     * @brief Добавить в piecewise-модель вариант сегмента по локальному индексу (stateless).
     *
     * Не использует состояние стратегии (reset/next/current).
     * Используется методом Orchestrator::makePiecewise(global) и подходит для параллелизма.
     *
     * @param pm    Целевая piecewise-модель.
     * @param local Локальный индекс (0..size()-1) внутри сегмента.
     */
    virtual std::shared_ptr<const M> makeAt(std::size_t local, const std::vector<std::shared_ptr<const M>>& built,
                                            std::size_t self) const = 0;

    virtual std::type_index modelType() const noexcept { return typeid(M); };

    virtual std::string_view modelName() const noexcept { 
        if (!model_name.empty()) {
            return model_name;
        }
        return typeid(M).name();
     };

    virtual bool isConstrained() const noexcept = 0;
};
};  // namespace aip::core::detail