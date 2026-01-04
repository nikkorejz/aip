#include <aip/core/fixed_string.hpp>
#include <aip/model/imodel.hpp>
#include <aip/model/piecewise_model.hpp>
#include <aip/params/macros.hpp>

#include <aip/params/control_param.hpp>
#include <aip/params/param_grid.hpp>
#include <aip/params/uniform_range.hpp>

#include <aip/core/orchestrator.hpp>
#include <aip/search/parallel_async.hpp>
#include <aip/search/index_space_from_grid.hpp>

#include <atomic>
#include <mutex>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>

namespace ex {

using In = double;
using Out = double;

struct Point {
    double x{};
    double y{};
};

// --- Domain (predicate) ---
struct SegDomain {
    enum class Kind { Left, Mid, Right } kind{};
    double x1{}, x2{};
    constexpr bool operator()(const double& x) const noexcept {
        if (kind == Kind::Left) return x < x1;
        if (kind == Kind::Mid) return (x >= x1) && (x < x2);
        return x >= x2;
    }
};

// --- Models (IModel-only) ---
struct Line final : aip::model::IModel<In, Out> {
    // Using macros

    AIP_DEFINE_CONTROL_PARAM(float, m);
    AIP_DEFINE_CONTROL_PARAM(double, c);

    [[nodiscard]] Out operator()(const In& x) const noexcept override { return m.value * x + c.value; }
};

using LineGrid = aip::params::ParamGrid<Line, aip::params::UniformRange, &Line::m, &Line::c>;

struct Parabola final : aip::model::IModel<In, Out> {
    // Using struct

    aip::params::ControlParam<double, aip::core::fixed_string{"a"}> a{};
    aip::params::ControlParam<double, aip::core::fixed_string{"b"}> b{};
    aip::params::ControlParam<double, aip::core::fixed_string{"c"}> c{};

    [[nodiscard]] Out operator()(const In& x) const noexcept override {
        return a.value * x * x + b.value * x + c.value;
    }
};

using ParGrid = aip::params::ParamGrid<Parabola, aip::params::UniformRange, &Parabola::a, &Parabola::b, &Parabola::c>;

struct Hyperbola final : aip::model::IModel<In, Out> {
    aip::params::ControlParam<float, aip::core::fixed_string{"a"}> a{};
    aip::params::ControlParam<float, aip::core::fixed_string{"b"}> b{};

    [[nodiscard]] Out operator()(const In& x) const noexcept override {
        // Domain is x >= x2, and we pick x2 > 0, so division is safe.
        return a.value / x + b.value;
    }
};

using HypGrid = aip::params::ParamGrid<Hyperbola, aip::params::UniformRange, &Hyperbola::a, &Hyperbola::b>;

double pearsonCorrelation(const std::vector<Point>& a, const std::vector<Point>& b) {
    if (a.size() != b.size() || a.empty()) throw std::invalid_argument("Arrays must have same non-zero size");

    const std::size_t n = a.size();

    double meanA = 0.0;
    double meanB = 0.0;

    for (std::size_t i = 0; i < n; ++i) {
        meanA += a[i].y;
        meanB += b[i].y;
    }

    meanA /= n;
    meanB /= n;

    double numerator = 0.0;
    double denomA = 0.0;
    double denomB = 0.0;

    for (std::size_t i = 0; i < n; ++i) {
        const double da = a[i].y - meanA;
        const double db = b[i].y - meanB;

        numerator += da * db;
        denomA += da * da;
        denomB += db * db;
    }

    const double denominator = std::sqrt(denomA * denomB);

    if (denominator == 0.0) throw std::runtime_error("Zero variance in data");

    return numerator / denominator;
}

}  // namespace ex

// --- Piecewise boundaries ---
const double x1 = -1.0;
const double x2 = +1.0;

auto generateOriginal() {
    using namespace ex;

    // True parameters (used only to synthesize the "observations")
    Line trueL;
    trueL.m = -0.8;
    trueL.c = 0.5;
    Parabola trueP;
    trueP.a = 1.2;
    trueP.b = 0.2;
    trueP.c = -0.3;
    Hyperbola trueH;
    trueH.a = 2.0;
    trueH.b = 0.1;

    std::vector<Point> data;
    data.reserve(201);
    for (double x = -5.0; x <= 5.0 + 1e-12; x += 0.05) {
        double y{};
        if (x < x1)
            y = trueL(x);
        else if (x < x2)
            y = trueP(x);
        else
            y = trueH(x);
        data.push_back({x, y});
    }

    return data;
}

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

struct Best {
    double score{-1e300};
    std::size_t global{0};
};

int main() {
    using namespace ex;

    // --- Generate target data (discrete points) ---
    const auto original = generateOriginal();

    // --- Prepare grids (all params are named ControlParam) ---
    LineGrid gL;
    // 10x10 = 100
    gL.getByLabel<"m">() = {-1.25, -0.35, 0.05};  // m: 20 values
    gL.get<1>() = {0.05, 0.95, 0.05};             // c: 20 values

    ParGrid gP;
    // 10x5x1 = 50
    gP.get<0>() = {0.75, 1.65, 0.05};  // a: 20 values
    gP.get<1>() = {-0.2, 0.2, 0.1};    // b: 5 values
    gP.get<2>() = {-0.3, -0.3, 1.0};   // c: 1 value (фикс)

    HypGrid gH;
    // 10x2 = 20
    gH.get<0>() = {1.5, 2.4, 0.1};  // a: 10 values
    gH.get<1>() = {0.0, 0.1, 0.1};  // b: 2 values

    // --- Orchestrator (stateless makePiecewise(global) is what we parallelize later) ---
    aip::core::Orchestrator<In, Out, SegDomain> orch;
    orch.add(SegDomain{SegDomain::Kind::Left, x1, x2}, gL);
    orch.add(SegDomain{SegDomain::Kind::Mid, x1, x2}, gP);
    orch.add(SegDomain{SegDomain::Kind::Right, x1, x2}, gH);

    // --- Brute-force search (measure only enumeration time) ---
    const std::size_t total = orch.size();

    std::size_t threadCount = std::thread::hardware_concurrency();
    if (threadCount == 0) threadCount = 1;
    threadCount = std::min(threadCount, total);

    std::atomic<std::size_t> done{0};
    std::mutex io;

    auto t0 = std::chrono::steady_clock::now();

    const std::size_t chunk = (total + threadCount - 1) / threadCount;
    std::vector<std::future<Best>> futs;
    futs.reserve(threadCount);

    for (std::size_t t = 0; t < threadCount; ++t) {
        const std::size_t begin = t * chunk;
        const std::size_t end   = std::min(total, begin + chunk);
        if (begin >= end) break;

        futs.push_back(std::async(std::launch::async, [&, begin, end] {
            Best best{};
            for (std::size_t g = begin; g < end; ++g) {
                auto pm = orch.makePiecewise(g);
                
                std::vector<Point> model_data;
                model_data.resize(original.size());
                
                for (std::size_t i = 0; i < original.size(); ++i) {
                    auto x = original[i].x;
                    model_data[i] = {x, pm(x)};
                }

                const double s = pearsonCorrelation(model_data, original);

                if (std::isfinite(s) && s > best.score) {
                    best.score = s;
                    best.global = g;
                }

                const auto now = done.fetch_add(1, std::memory_order_relaxed) + 1;
                // прогресс печатаем редко
                if (total >= 100 && (now % (total / 100)) == 0) {
                    std::lock_guard<std::mutex> lock(io);
                    std::cout << "\rProgress: " << (100 * now / total) << "% (" << now << "/" << total << ")" << std::flush;
                }
            }
            return best;
        }));
    }

    Best best{};
    for (auto& f : futs) {
        Best b = f.get();
        if (b.score > best.score) best = b;
    }

    auto t1 = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    std::cout << "\rProgress: 100% (" << total << "/" << total << ")\n";
    std::cout << "Elapsed (parallel): " << ms << " ms\n";
    std::cout << "Best score: " << best.score << "\n";
    std::cout << "Best global: " << best.global << "\n";

    // --- Decode bestGlobal into per-entry locals and print params via forEachParam (no hardcode) ---
    // IMPORTANT: Orchestrator’s global decomposition must match makePiecewise(global).
    // Here: entry 0 changes fastest => local0 = global % size0; global /= size0; etc.

    std::size_t g = best.global;

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

    return 0;
}