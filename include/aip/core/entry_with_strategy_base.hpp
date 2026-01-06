#pragma once

#include <aip/core/ientry.hpp>

namespace aip::core::detail {

template <class In, class Out, class Domain, class Grid, template <std::size_t> class StrategyT>
class EntryWithStrategyBase : public IEntry<In, Out, Domain> {
   protected:
    using Model = typename Grid::model_type;
    // В этой архитектуре Model обязана реализовывать IModel<In,Out>.
    static_assert(std::is_base_of_v<IM<In, Out, Domain>, Model>,
                  "Orchestrator: Grid::model_type must derive from IModel<In,Out>");

    static constexpr std::size_t N = Grid::N;
    using Strategy = StrategyT<N>;
    // Ограничение используемой стратегии концептом IndexStrategy
    static_assert(aip::search::IndexStrategy<Strategy, N>);
    using idx_type = std::array<std::size_t, N>;

    Domain domain_;
    Grid grid_;

    aip::search::IndexSpace<N> space_{};
    Strategy strat_{};
    std::optional<idx_type> current_{};

   public:
    EntryWithStrategyBase(Domain d, Grid g, std::string name = {}) : domain_(std::move(d)), grid_(std::move(g)) {
        this->model_name = std::move(name);
    }

    const Domain& getDomain() const noexcept override { return domain_; }
    
    std::size_t size() const noexcept override { return grid_.size(); }

    void reset() override {
        space_ = aip::search::make_index_space(grid_);
        strat_.reset(space_);
        current_.reset();
        // важно: “текущий” должен быть установлен
        current_ = strat_.next();
    }

    bool next() override {
        if (!current_) return false;
        current_ = strat_.next();
        return current_.has_value();
    }

    std::optional<std::vector<std::size_t>> currentIdx() const override {
        if (!current_) return std::nullopt;
        return std::vector<std::size_t>(current_->begin(), current_->end());
    }

    std::optional<std::size_t> currentLocal() const noexcept override {
        if (!current_) return std::nullopt;
        if constexpr (N == 0) return 0;

        std::size_t local = 0, mul = 1;
        for (std::size_t i = 0; i < N; ++i) {
            const std::size_t base = space_.bases[i];
            if (base == 0) return std::nullopt;
            const std::size_t v = (*current_)[i];
            if (v >= base) return std::nullopt;
            local += v * mul;
            mul *= base;
        }
        return local;
    }
};

}  // namespace aip::core::detail