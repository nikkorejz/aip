#pragma once

#include <array>
#include <cassert>
#include <optional>
#include <vector>
#include <cstddef>

#include <aip/core/entry_with_strategy_base.hpp>

namespace aip::core::detail {

template <typename Binder, typename Model, typename Out>
concept BoundaryBinder = requires(Binder b, Model& m, const Out& l, const Out& r) {
    { b(m, l, r) } -> std::same_as<void>;
};

template <typename In, typename Out, typename Domain, typename Grid, template <std::size_t> typename StrategyT,
          typename Binder>
struct ConstrainedEntry final : EntryWithStrategyBase<In, Out, Domain, Grid, StrategyT> {
    using Model = typename Grid::model_type;
    static constexpr std::size_t N = Grid::N;
    using idx_type = std::array<std::size_t, N>;

    static_assert(BoundaryBinder<Binder, Model, Out>,
                  "Binder must be callable as binder(Model&, const Out&, const Out&)");

    In leftIn_;
    In rightIn_;
    Binder binder_;

    ConstrainedEntry(Domain d, Grid g, In leftIn, In rightIn, Binder binder, std::string name = {})
        : EntryWithStrategyBase<In, Out, Domain, Grid, StrategyT>(std::move(d), std::move(g), std::move(name)),
          leftIn_(std::move(leftIn)),
          rightIn_(std::move(rightIn)),
          binder_(std::move(binder)) {
    }

    std::shared_ptr<const IM<In, Out, Domain>> makeAt(
        std::size_t local, const std::vector<std::shared_ptr<const IM<In, Out, Domain>>>& built,
        std::size_t self) const override {
        // constrained не может быть первым/последним
        assert(self != 0 && self + 1 < built.size() && "A constrained model must be between two free models.");

        const auto& leftM = built[self - 1];
        const auto& rightM = built[self + 1];
        if (!leftM || !rightM) return {};

        const Out leftOut = (*leftM)(leftIn_);
        const Out rightOut = (*rightM)(rightIn_);

        // 1) делаем "черновую" модель из grid (или default, если UnitGrid)
        Model m{};
        if constexpr (N == 0) {
            m = this->grid_.makeModel(std::array<std::size_t, 0>{});
        } else {
            const auto space = aip::search::make_index_space(this->grid_);
            idx_type idx{};
            for (std::size_t i = 0; i < N; ++i) {
                const std::size_t base = space.bases[i];
                idx[i] = (base > 0) ? (local % base) : 0;
                local = (base > 0) ? (local / base) : 0;
            }
            m = this->grid_.makeModel(idx);
        }

        // 2) подгоняем по границам
        binder_(m, leftOut, rightOut);

        return std::make_shared<Model>(std::move(m));
    }

    bool isConstrained() const noexcept override { return true; }

    void forEachParamAt(std::size_t local,
                        const std::function<void(std::string_view label, std::string value)>& fn) const override {
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