#include <aip/core/fixed_string.hpp>
#include <aip/model/imodel.hpp>
#include <aip/model/piecewise_model.hpp>
#include <aip/params/control_param.hpp>
#include <aip/params/param_grid.hpp>
#include <aip/params/uniform_range.hpp>
#include <aip/core/orchestrator.hpp>

#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <vector>

namespace ex {

struct In {
    double x{};
};

struct Out {
    double y{};
    double dy{}; // допустим, “производная” или любая доп. величина
};

struct Domain {
    double x1{}, x2{};
    enum class Kind { Left, Mid, Right } kind{Kind::Left};

    constexpr bool operator()(const In& in) const noexcept {
        if (kind == Kind::Left)  return in.x < x1;
        if (kind == Kind::Mid)   return (in.x >= x1) && (in.x < x2);
        return in.x >= x2;
    }
};

struct Line final : aip::model::IModel<In, Out> {
    aip::params::ControlParam<double, aip::core::fixed_string{"m"}> m{};
    aip::params::ControlParam<double, aip::core::fixed_string{"c"}> c{};

    [[nodiscard]] Out operator()(const In& in) const noexcept override {
        return Out{ m.value * in.x + c.value, m.value };
    }
};

struct Parabola final : aip::model::IModel<In, Out> {
    aip::params::ControlParam<double, aip::core::fixed_string{"a"}> a{};
    aip::params::ControlParam<double, aip::core::fixed_string{"b"}> b{};
    aip::params::ControlParam<double, aip::core::fixed_string{"c"}> c{};

    [[nodiscard]] Out operator()(const In& in) const noexcept override {
        const double x = in.x;
        return Out{ a.value * x * x + b.value * x + c.value, 2.0 * a.value * x + b.value };
    }
};

struct Hyperbola final : aip::model::IModel<In, Out> {
    aip::params::ControlParam<double, aip::core::fixed_string{"a"}> a{};
    aip::params::ControlParam<double, aip::core::fixed_string{"b"}> b{};

    [[nodiscard]] Out operator()(const In& in) const noexcept override {
        const double x = in.x;
        return Out{ a.value / x + b.value, -a.value / (x * x) };
    }
};

double score_mse_y(const std::vector<Out>& pred, const std::vector<Out>& obs) {
    if (pred.size() != obs.size() || pred.empty()) return std::numeric_limits<double>::infinity();
    double s = 0.0;
    for (std::size_t i = 0; i < pred.size(); ++i) {
        const double d = pred[i].y - obs[i].y;
        s += d * d;
    }
    return s / static_cast<double>(pred.size());
}

} // namespace ex

// Helper: local -> idx[] for a grid (mixed radix by bases)
template <class Grid>
auto local_to_idx(const Grid& grid, std::size_t local) {
    constexpr std::size_t N = Grid::N;
    std::array<std::size_t, N> idx{};

    const auto space = aip::search::make_index_space(grid);
    for (std::size_t i = 0; i < N; ++i) {
        const std::size_t base = space.bases[i];
        idx[i] = (base > 0) ? (local % base) : 0;
        local = (base > 0) ? (local / base) : 0;
    }
    return idx;
}

template <class Grid>
void print_params_for_grid(const char* title, const Grid& grid, std::size_t local) {
    const auto idx = local_to_idx(grid, local);

    std::cout << "  [" << title << "]\n";
    grid.forEachParam([&](typename Grid::ParamMeta meta, const auto& r) {
        const auto i = meta.index;
        const auto v = r[idx[i]];
        std::cout << "    " << meta.label << " = " << v << "\n";
    });
}

int main() {
    using namespace ex;

    const double x1 = -1.0;
    const double x2 =  1.0;

    // синтетические наблюдения
    Line tl; tl.m = -0.8; tl.c = 0.5;
    Parabola tp; tp.a = 1.2; tp.b = 0.2; tp.c = -0.3;
    Hyperbola th; th.a = 2.0; th.b = 0.1;

    std::vector<In> xs;
    for (double x = -5.0; x <= 5.0 + 1e-12; x += 0.05) xs.push_back({x});

    std::vector<Out> obs; obs.reserve(xs.size());
    for (auto in : xs) {
        if (in.x < x1) obs.push_back(tl(in));
        else if (in.x < x2) obs.push_back(tp(in));
        else obs.push_back(th(in));
    }

    using LineGrid = aip::params::ParamGrid<Line, aip::params::UniformRange, &Line::m, &Line::c>;
    using ParGrid  = aip::params::ParamGrid<Parabola, aip::params::UniformRange, &Parabola::a, &Parabola::b, &Parabola::c>;
    using HypGrid  = aip::params::ParamGrid<Hyperbola, aip::params::UniformRange, &Hyperbola::a, &Hyperbola::b>;

    LineGrid gL;
    gL.get<0>() = {-1.25, -0.35, 0.1}; // 10
    gL.get<1>() = { 0.05,  0.95, 0.1}; // 10

    ParGrid gP;
    gP.get<0>() = {0.75, 1.65, 0.1};   // 10
    gP.get<1>() = {-0.2, 0.2, 0.1};    // 5
    gP.get<2>() = {-0.3, -0.3, 1.0};   // 1

    HypGrid gH;
    gH.get<0>() = {1.5, 2.4, 0.1};     // 10
    gH.get<1>() = {0.0, 0.1, 0.1};     // 2

    aip::core::Orchestrator<In, Out, Domain> orch;
    orch.add(Domain{x1, x2, Domain::Kind::Left},  gL);
    orch.add(Domain{x1, x2, Domain::Kind::Mid},   gP);
    orch.add(Domain{x1, x2, Domain::Kind::Right}, gH);

    const std::size_t total = orch.size();
    std::cout << "Total combos: " << total << "\n";

    // однопоточно через makePiecewise(global)
    double best = std::numeric_limits<double>::infinity();
    std::size_t bestGlobal = 0;

    std::vector<Out> pred; pred.resize(xs.size());

    for (std::size_t g = 0; g < total; ++g) {
        auto pm = orch.makePiecewise(g);
        for (std::size_t i = 0; i < xs.size(); ++i) pred[i] = pm(xs[i]);

        const double s = score_mse_y(pred, obs);
        if (s < best) { best = s; bestGlobal = g; }
    }

    std::cout << "Best MSE(y): " << best << "\n";
    std::cout << "Best global: " << bestGlobal << "\n";

    // --- Decode bestGlobal into per-entry locals and print params via forEachParam (no hardcode) ---

    std::size_t g = bestGlobal;

    const std::size_t szL = gL.size();
    const std::size_t localL = (szL > 0) ? (g % szL) : 0;
    g = (szL > 0) ? (g / szL) : 0;

    const std::size_t szP = gP.size();
    const std::size_t localP = (szP > 0) ? (g % szP) : 0;
    g = (szP > 0) ? (g / szP) : 0;

    const std::size_t szH = gH.size();
    const std::size_t localH = (szH > 0) ? (g % szH) : 0;

    std::cout << "Best parameters (via forEachParam):\n";
    print_params_for_grid("Line (x < x1)", gL, localL);
    print_params_for_grid("Parabola (x1..x2)", gP, localP);
    print_params_for_grid("Hyperbola (x >= x2)", gH, localH);
}
