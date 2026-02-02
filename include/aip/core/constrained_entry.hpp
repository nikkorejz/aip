#pragma once

#include <array>
#include <cassert>
#include <functional>
#include <optional>
#include <sstream>
#include <tuple>
#include <vector>
#include <cstddef>
#include <utility>

#include <aip/core/entry_with_strategy_base.hpp>

namespace aip::core::detail {

template <typename T, typename In>
concept BoundaryPairLike = requires(T&& t) {
    { std::get<0>(std::forward<T>(t)) } -> std::convertible_to<In>;
    { std::get<1>(std::forward<T>(t)) } -> std::convertible_to<In>;
};

template <typename Binder, typename Model, typename Out>
concept BoundaryBinderLegacy = requires(Binder b, Model& m, const Out& l, const Out& r) {
    { b(m, l, r) } -> std::same_as<void>;
};

template <typename Binder, typename Model, typename In, typename Out>
concept BoundaryBinderWithInputs = requires(Binder b, Model& m, const In& lIn, const Out& lOut, const In& rIn,
                                            const Out& rOut) {
    { b(m, lIn, lOut, rIn, rOut) } -> std::same_as<void>;
};

template <typename BoundarySelector, typename In, typename Out, typename Domain>
concept BoundarySelectorNoContext = requires(BoundarySelector s) {
    { s() } -> BoundaryPairLike<In>;
};

template <typename BoundarySelector, typename In, typename Out, typename Domain>
concept BoundarySelectorWithContext = requires(BoundarySelector s,
                                               const std::vector<std::shared_ptr<const IM<In, Out, Domain>>>& built,
                                               std::size_t self, const Domain& domain) {
    { s(built, self, domain) } -> BoundaryPairLike<In>;
};

template <typename In, typename Out, typename Domain, typename Grid, template <std::size_t> typename StrategyT,
          typename Binder, typename BoundarySelector>
struct ConstrainedEntry final : EntryWithStrategyBase<In, Out, Domain, Grid, StrategyT> {
    using Model = typename Grid::model_type;
    static constexpr std::size_t N = Grid::N;
    using idx_type = std::array<std::size_t, N>;

    static_assert(BoundaryBinderLegacy<Binder, Model, Out> || BoundaryBinderWithInputs<Binder, Model, In, Out>,
                  "Binder must be callable either as binder(Model&, const Out&, const Out&) or "
                  "binder(Model&, const In&, const Out&, const In&, const Out&)");
    static_assert(BoundarySelectorNoContext<BoundarySelector, In, Out, Domain> ||
                      BoundarySelectorWithContext<BoundarySelector, In, Out, Domain>,
                  "Boundary selector must be callable as selector() or "
                  "selector(const vector<shared_ptr<const IModel<In,Out>>>& built, size_t self, const Domain&)");

    Binder binder_;
    BoundarySelector boundarySelector_;

    ConstrainedEntry(Domain d, Grid g, BoundarySelector boundarySelector, Binder binder, std::string name = {})
        : EntryWithStrategyBase<In, Out, Domain, Grid, StrategyT>(std::move(d), std::move(g), std::move(name)),
          binder_(std::move(binder)),
          boundarySelector_(std::move(boundarySelector)) {
    }

    std::shared_ptr<const IM<In, Out, Domain>> makeAt(
        std::size_t local, const std::vector<std::shared_ptr<const IM<In, Out, Domain>>>& built,
        std::size_t self) const override {
        // constrained не может быть первым/последним
        assert(self != 0 && self + 1 < built.size() && "A constrained model must be between two free models.");

        const auto& leftM = built[self - 1];
        const auto& rightM = built[self + 1];
        if (!leftM || !rightM) return {};

        const auto boundaries = [&]() {
            if constexpr (BoundarySelectorWithContext<BoundarySelector, In, Out, Domain>) {
                return boundarySelector_(built, self, this->domain_);
            } else {
                return boundarySelector_();
            }
        }();

        const In leftIn = std::get<0>(boundaries);
        const In rightIn = std::get<1>(boundaries);
        const Out leftOut = (*leftM)(leftIn);
        const Out rightOut = (*rightM)(rightIn);

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
        if constexpr (BoundaryBinderLegacy<Binder, Model, Out>) {
            binder_(m, leftOut, rightOut);
        } else {
            binder_(m, leftIn, leftOut, rightIn, rightOut);
        }

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
