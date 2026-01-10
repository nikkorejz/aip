#pragma once

#include <sstream>

#include <aip/search/index_space.hpp>
#include <aip/search/index_strategy.hpp>
#include <aip/search/index_space_from_grid.hpp>
#include <aip/core/entry_with_strategy_base.hpp>

namespace aip::core::detail {
/**
 * @brief Реализация сегмента оркестратора для конкретного типа решётки параметров.
 *
 * FreeEntry<Grid> — это "один кусок" будущей piecewise-модели:
 *  - domain: предикат, определяющий область, где этот кусок активен
 *  - grid:   ParamGrid, задающий перебираемые параметры конкретного Model
 *
 * По локальному индексу local FreeEntry строит конкретный экземпляр Model
 * (через grid.makeModel(idx)) и добавляет его в PiecewiseModel.
 *
 * @tparam Grid Тип решётки параметров (ParamGrid<...>).
 */
template <typename In, typename Out, typename Domain, typename Grid, template <std::size_t> typename StrategyT>
struct FreeEntry final : EntryWithStrategyBase<In, Out, Domain, Grid, StrategyT> {
    using EntryWithStrategyBase<In, Out, Domain, Grid, StrategyT>::EntryWithStrategyBase;

    using Model = typename Grid::model_type;
    static constexpr std::size_t N = Grid::N;
    using idx_type = std::array<std::size_t, N>;

    std::shared_ptr<const IM<In, Out, Domain>> makeAt(std::size_t local,
                                                      const std::vector<std::shared_ptr<const IM<In, Out, Domain>>>&,
                                                      std::size_t) const override {
        const auto space = aip::search::make_index_space(this->grid_);

        idx_type idx{};
        for (std::size_t i = 0; i < N; ++i) {
            const std::size_t base = space.bases[i];
            idx[i] = (base > 0) ? (local % base) : 0;
            local = (base > 0) ? (local / base) : 0;
        }

        Model m = this->grid_.makeModel(idx);
        return std::make_shared<Model>(std::move(m));
    }

    bool isConstrained() const noexcept override { return false; }

    void forEachParamAt(std::size_t local,
                        const std::function<void(std::string_view label, std::string value)>& fn) const override {
        // N может быть 0 (UnitGrid-like) — тогда grid_.forEachParam просто ничего не вызовет.
        if constexpr (N == 0) {
            this->grid_.forEachParam([&](auto, const auto&) {});
            (void)local;
            return;
        } else {
            const auto space = aip::search::make_index_space(this->grid_);

            idx_type idx{};
            for (std::size_t i = 0; i < N; ++i) {
                const std::size_t base = space.bases[i];
                idx[i] = (base > 0) ? (local % base) : 0;
                local = (base > 0) ? (local / base) : 0;
            }

            this->grid_.forEachParam([&](auto meta, const auto& range) {
                const std::size_t pi = meta.index;

                std::ostringstream oss;
                oss << range[idx[pi]];

                fn(meta.label, oss.str());
            });
        }
    }

    std::optional<std::size_t> localFromIdx(const std::vector<std::size_t>& idx) const noexcept override {
        if constexpr (N == 0) return 0;

        if (idx.size() != N) return std::nullopt;

        std::size_t local = 0;
        std::size_t mul = 1;

        const auto space = aip::search::make_index_space(this->grid_);

        for (std::size_t i = 0; i < N; ++i) {
            const std::size_t base = space.bases[i];
            if (base == 0) return std::nullopt;
            if (idx[i] >= base) return std::nullopt;
            local += idx[i] * mul;
            mul *= base;
        }
        return local;
    }
};
}  // namespace aip::core::detail