#pragma once

#include <array>
#include <memory>
#include <vector>
#include <cassert>
#include <cstddef>
#include <utility>
#include <optional>
#include <type_traits>

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
    using PM = aip::model::PiecewiseModel<In, Out, Domain>;
    using IM = aip::model::IModel<In, Out>;

    /**
     * @brief Внутренний type-erasure интерфейс для сегмента оркестратора.
     *
     * Каждый сегмент = (domain + grid + strategy-state).
     * Нужен, чтобы хранить разные Grid-типы в одном std::vector.
     *
     * IEntry — строго внутренняя деталь реализации (не часть публичного API).
     */
    struct IEntry {
        // Type-erasure — это способ хранить и вызывать объекты разных типов так, будто они одного типа.

        virtual ~IEntry() = default;

        /// @brief Число локальных комбинаций параметров в этом сегменте.
        [[nodiscard]] virtual std::size_t size() const noexcept = 0;

        /// @brief Перейти к следующему варианту. false если вариантов больше нет.
        virtual bool next() = 0;

        /// @brief Подготовить стратегию и установить "текущий" вариант на первый (если он существует).
        virtual void reset() = 0;

        /// @brief Добавить текущую модель сегмента в piecewise-модель.
        virtual void addTo(PM& pm) const = 0;

        /**
         * @brief Добавить в piecewise-модель вариант сегмента по локальному индексу (stateless).
         *
         * Не использует состояние стратегии (reset/next/current).
         * Используется методом Orchestrator::makePiecewise(global) и подходит для параллелизма.
         *
         * @param pm    Целевая piecewise-модель.
         * @param local Локальный индекс (0..size()-1) внутри сегмента.
         */
        virtual void addAt(PM& pm, std::size_t local) const = 0;
    };

    /**
     * @brief Реализация сегмента оркестратора для конкретного типа решётки параметров.
     *
     * Entry<Grid> — это "один кусок" будущей piecewise-модели:
     *  - domain: предикат, определяющий область, где этот кусок активен
     *  - grid:   ParamGrid, задающий перебираемые параметры конкретного Model
     *
     * По локальному индексу local Entry строит конкретный экземпляр Model
     * (через grid.makeModel(idx)) и добавляет его в PiecewiseModel.
     *
     * @tparam Grid Тип решётки параметров (ParamGrid<...>).
     */
    template <typename Grid>
    struct Entry final : IEntry {
        using Model = typename Grid::model_type;
        static constexpr std::size_t N = Grid::N;

        // В этой архитектуре Model обязана реализовывать IModel<In,Out>.
        static_assert(std::is_base_of_v<IM, Model>, "Orchestrator: Grid::model_type must derive from IModel<In,Out>");

        using Strategy = StrategyT<N>;
        // Ограничение используемой стратегии концептом IndexStrategy
        static_assert(aip::search::IndexStrategy<Strategy, N>);

        using idx_type = std::array<std::size_t, N>;

        Domain domain;
        Grid grid;

        search::IndexSpace<N> space{};
        Strategy strat{};
        std::optional<idx_type> current{};

        Entry(Domain d, Grid g) : domain(std::move(d)), grid(std::move(g)) {}

        [[nodiscard]] std::size_t size() const noexcept override { return grid.size(); }

        bool next() override {
            if (!current) return false;

            current = strat.next();
            return current.has_value();
        }

        void reset() override {
            space = aip::search::make_index_space(grid);
            strat.reset(space);

            current = strat.next();
        }

        void addTo(PM& pm) const override {
            assert(!!current && "Orchestrator's entry: index must be initialized");
            if (!current) {
                return;
            }
            // current обязательно должен быть установлен
            Model m = grid.makeModel(*current);
            pm.add(domain, std::make_shared<Model>(std::move(m)));
        }

        void addAt(PM& pm, std::size_t local) const override {
            const auto space = aip::search::make_index_space(grid);

            idx_type idx{};
            for (std::size_t i = 0; i < N; ++i) {
                const std::size_t base = space.bases[i];
                idx[i] = (base > 0) ? (local % base) : 0;
                local = (base > 0) ? (local / base) : 0;
            }

            Model m = grid.makeModel(idx);
            pm.add(domain, std::make_shared<Model>(std::move(m)));
        }
    };

    std::vector<std::unique_ptr<IEntry>> entries;

    bool iterate_ready = false;
    bool iterate_finished = false;

   public:
    Orchestrator() = default;

    void clear() {
        entries.clear();
        iterate_ready = false;
        iterate_finished = false;
    }

    template <class Model, template <class> class RangeT, auto... Members>
    void add(Domain d, aip::params::ParamGrid<Model, RangeT, Members...> grid) {
        using G = aip::params::ParamGrid<Model, RangeT, Members...>;
        entries.push_back(std::make_unique<Entry<G>>(std::move(d), std::move(grid)));
        iterate_ready = false;
        iterate_finished = false;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        if (entries.empty()) return 0;
        std::size_t total = 1;
        for (const auto& e : entries) total *= e->size();
        return total;
    }

    /**
     * @brief Сбросить stateful-перебор (для next()).
     *
     * После reset() первый вызов next() вернёт первую piecewise-модель (если size() > 0).
     */
    void reset() {
        iterate_ready = true;
        iterate_finished = false;

        if (entries.empty()) {
            iterate_finished = true;
            return;
        }

        // reset каждого сегмента
        for (auto& e : entries) e->reset();

        // если хотя бы один сегмент пуст -> всё пространство пустое
        for (const auto& e : entries) {
            // после reset() у пустого сегмента addTo() будет no-op, но размер = 0,
            // а значит общая size() будет 0. Для next() это считаем завершением.
            if (e->size() == 0) {
                iterate_finished = true;
                return;
            }
        }
    }

    /**
     * @brief Получить следующую piecewise-модель согласно StrategyT.
     *
     * @return PiecewiseModel или std::nullopt, если перебор завершён.
     */
    [[nodiscard]] std::optional<PM> next() {
        if (!iterate_ready) reset();
        if (iterate_finished) return std::nullopt;

        // 1) собрать текущую модель
        PM pm;
        for (const auto& e : entries) e->addTo(pm);

        // 2) продвинуть "одометр" по сегментам
        // entry0 меняется быстрее всего.
        for (std::size_t i = 0; i < entries.size(); ++i) {
            if (entries[i]->next()) {
                // удалось продвинуться без переноса
                return pm;
            }
            // перенос: сбросить этот сегмент на начало
            entries[i]->reset();
            // если i был последним — всё закончилось
            if (i + 1 == entries.size()) {
                iterate_finished = true;
                break;
            }
            // иначе перенос пойдёт на следующий i+1 (цикл продолжится)
        }

        return pm;  // возвращаем последнюю собранную перед завершением
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
        PM pm;

        for (std::size_t i = 0; i < entries.size(); ++i) {
            const std::size_t sz = entries[i]->size();
            const std::size_t local = (sz > 0) ? (global % sz) : 0;
            global = (sz > 0) ? (global / sz) : 0;

            entries[i]->addAt(pm, local);
        }

        return pm;
    }
};

}  // namespace aip::core