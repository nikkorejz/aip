#pragma once

#include <array>
#include <memory>
#include <vector>
#include <cassert>
#include <cstddef>
#include <utility>
#include <optional>
#include <typeindex>
#include <type_traits>

#include <aip/core/ientry.hpp>
#include <aip/core/free_entry.hpp>
#include <aip/core/constrained_entry.hpp>
#include <aip/params/param_grid.hpp>
#include <aip/model/piecewise_model.hpp>
#include <aip/search/index_strategy.hpp>
#include <aip/search/enumeration_strategy.hpp>
#include <aip/search/index_space_from_grid.hpp>

namespace aip::core {

using namespace aip::model;

template <typename In, typename Out, typename Domain,
          template <std::size_t> typename StrategyT = aip::search::EnumerationStrategy>
class Orchestrator final {
   private:
    using IM = aip::model::IModel<In, Out>;
    using PM = aip::model::PiecewiseModel<In, Out, Domain>;

    template <typename Grid>
    using FEntryT = detail::FreeEntry<In, Out, Domain, Grid, StrategyT>;

    std::vector<std::unique_ptr<detail::IEntry<In, Out, Domain>>> entries;

    bool iterate_ready = false;
    bool iterate_finished = false;
    std::size_t step{0};

   public:
    struct Snapshot {
        std::size_t step{};
        std::vector<std::optional<std::vector<std::size_t>>> indices;  // per-entry
    };

    Orchestrator() = default;

    void clear() {
        entries.clear();
        iterate_ready = false;
        iterate_finished = false;
    }

    void removeEntry(size_t idx) {
        assert(idx < entries.size() && "Out of range: idx must be less than entries size");
        if (idx >= entries.size()) {
            return;
        }

        entries.erase(entries.begin() + idx);
        iterate_ready = false;
        iterate_finished = false;
    }

    template <class Model, template <class> class RangeT, auto... Members>
    void add(Domain d, aip::params::ParamGrid<Model, RangeT, Members...> grid) {
        using G = aip::params::ParamGrid<Model, RangeT, Members...>;
        entries.push_back(std::make_unique<FEntryT<G>>(std::move(d), std::move(grid), "Unnamed"));
        iterate_ready = false;
        iterate_finished = false;
    }

    template <class Model, template <class> class RangeT, auto... Members>
    void add(Domain d, aip::params::ParamGrid<Model, RangeT, Members...> grid, std::string name) {
        using G = aip::params::ParamGrid<Model, RangeT, Members...>;
        entries.push_back(std::make_unique<FEntryT<G>>(std::move(d), std::move(grid), std::move(name)));
        iterate_ready = false;
        iterate_finished = false;
    }

    /**
     * @brief Добавить "связанный" сегмент: модель подгоняется по двум граничным значениям Out от соседей.
     *
     * Сегмент должен иметь соседа слева и соседа справа (не может быть первым/последним).
     * Orchestrator при сборке вычисляет:
     *  - leftOut  = leftModel(leftBoundaryIn)
     *  - rightOut = rightModel(rightBoundaryIn)
     * и вызывает binder(model, leftOut, rightOut).
     *
     * @tparam Grid   Решётка параметров (ParamGrid или UnitGrid), задаёт свободные степени.
     * @tparam Binder Callable вида void(Model& m, const Out& leftOut, const Out& rightOut).
     */
    template <class Grid, class Binder>
    void addConstrained(Domain domain, Grid grid, const In& leftBoundaryIn, const In& rightBoundaryIn, Binder binder) {
        using EntryT = detail::ConstrainedEntry<In, Out, Domain, Grid, StrategyT, Binder>;
        entries.push_back(std::make_unique<EntryT>(std::move(domain), std::move(grid), leftBoundaryIn, rightBoundaryIn,
                                                   std::move(binder)));
        iterate_ready = false;
        iterate_finished = false;
    }

    /**
     * @brief Кол-во комбинаций
     *
     * @note Если кол-во комбинаций == 0, то это не значит, что оркестратор пустой, так как 0 может дать неправильно
     * настроенная сетка.
     */
    [[nodiscard]] std::size_t size() const noexcept {
        if (entries.empty()) return 0;
        std::size_t total = 1;
        for (const auto& e : entries) total *= e->size();
        return total;
    }

    [[nodiscard]] inline constexpr bool empty() const noexcept { return entries.empty(); }

    [[nodiscard]] inline constexpr std::size_t entryCount() const noexcept { return entries.size(); }

    /**
     * @brief Сбросить stateful-перебор (для next()).
     *
     * После reset() первый вызов next() вернёт первую piecewise-модель (если size() > 0).
     */
    void reset() {
        iterate_ready = true;
        iterate_finished = false;
        step = 0;

        if (entries.empty()) {
            iterate_finished = true;
            return;
        }

        // reset каждого сегмента
        for (auto& e : entries) e->reset();

        // если хотя бы один сегмент пуст -> всё пространство пустое
        for (const auto& e : entries) {
            if (e->size() == 0) {
                iterate_finished = true;
                return;
            }
        }
    }

    PM buildAtLocals(const std::vector<std::size_t>& locals) const {
        const std::size_t K = entries.size();
        PM pm;
        if (K == 0) return pm;

        std::vector<std::shared_ptr<const IM>> built(K);

        // pass A: free
        for (std::size_t i = 0; i < K; ++i) {
            if (!entries[i]->isConstrained()) {
                built[i] = entries[i]->makeAt(locals[i], built, i);
            }
        }
        // pass B: constrained
        for (std::size_t i = 0; i < K; ++i) {
            if (entries[i]->isConstrained()) {
                built[i] = entries[i]->makeAt(locals[i], built, i);
            }
        }

        for (std::size_t i = 0; i < K; ++i) {
            if (built[i]) pm.add(entries[i]->getDomain(), built[i]);
        }
        return pm;
    }

    /**
     * @brief Получить следующую piecewise-модель согласно StrategyT.
     *
     * @return PiecewiseModel или std::nullopt, если перебор завершён.
     */
    [[nodiscard]] std::optional<PM> next() {
        if (!iterate_ready) reset();
        if (iterate_finished) return std::nullopt;

        // 1) locals из текущего состояния стратегии
        std::vector<std::size_t> locals;
        locals.reserve(entries.size());
        for (const auto& e : entries) {
            auto cl = e->currentLocal();
            if (!cl) {
                iterate_finished = true;
                return std::nullopt;
            }
            locals.push_back(*cl);
        }

        PM pm = buildAtLocals(locals);
        ++step;

        // 2) двигаем одометр (как было)
        for (std::size_t i = 0; i < entries.size(); ++i) {
            if (entries[i]->next()) {
                return pm;
            }
            entries[i]->reset();
            if (i + 1 == entries.size()) {
                iterate_finished = true;
                break;
            }
        }
        return pm;
    }

    /**
     * @brief Построить piecewise-модель по глобальному индексу (stateless).
     *
     * Глобальный индекс трактуется как число в смешанной системе счисления,
     * где основания — sizes сегментов. Сегмент 0 меняется быстрее всего.
     *
     * Этот метод не использует reset/next и безопасен для параллельной обработки.
     */
    [[nodiscard]] PM makePiecewise(std::size_t global) const {
        const std::size_t K = entries.size();
        std::vector<std::size_t> locals(K, 0);

        for (std::size_t i = 0; i < K; ++i) {
            const std::size_t sz = entries[i]->size();
            locals[i] = (sz > 0) ? (global % sz) : 0;
            global = (sz > 0) ? (global / sz) : 0;
        }
        return buildAtLocals(locals);
    }

    [[nodiscard]] Snapshot snapshot() const {
        Snapshot s;
        s.step = step;
        s.indices.reserve(entries.size());
        for (const auto& e : entries) s.indices.push_back(e->currentIdx());
        return s;
    }

    inline const detail::IEntry<In, Out, Domain>& operator[](size_t idx) const { return *entries[idx]; };
};

}  // namespace aip::core