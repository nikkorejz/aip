#pragma once

#include <atomic>
#include <cstddef>
#include <future>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
#include <algorithm>

namespace aip::search {

/**
 * @brief Параллельно обработать диапазон индексов [begin, end) с помощью std::async.
 *
 * Функция разбивает диапазон на чанки и запускает по одному async-задаче на чанк.
 * Это снижает накладные расходы по сравнению с запуском async на каждый индекс.
 *
 * @tparam Worker      Callable вида Result(std::size_t global).
 * @tparam OnProgress  Callable вида void(std::size_t done, std::size_t total).
 *
 * @param begin         Начальный глобальный индекс (включительно).
 * @param end           Конечный глобальный индекс (исключая).
 * @param worker        Функция обработки одного global индекса.
 * @param threadCount   Число параллельных задач (по умолчанию hardware_concurrency()).
 * @param onProgress    Опциональный callback прогресса (можно передать nullptr).
 *
 * @return std::vector<Result> размера (end-begin), в исходном порядке по global.
 *
 * @note Если worker бросает исключение, оно пробросится при get() соответствующего future.
 */
template <typename Worker, typename OnProgress = std::nullptr_t>
auto parallelForIndicesAsync(std::size_t begin,
                             std::size_t end,
                             Worker&& worker,
                             std::size_t threadCount = std::thread::hardware_concurrency(),
                             OnProgress onProgress = nullptr)
    -> std::vector<std::invoke_result_t<Worker&, std::size_t>>
{
    using Result = std::invoke_result_t<Worker&, std::size_t>;

    const std::size_t total = (end > begin) ? (end - begin) : 0;
    std::vector<Result> results;
    results.resize(total);

    if (total == 0) return results;

    if (threadCount == 0) threadCount = 1;
    threadCount = std::min(threadCount, total);

    // Размер чанка: равномерно распределяем работу
    const std::size_t chunk = (total + threadCount - 1) / threadCount;

    std::atomic<std::size_t> done{0};
    std::vector<std::future<void>> futs;
    futs.reserve(threadCount);

    for (std::size_t t = 0; t < threadCount; ++t) {
        const std::size_t chunkBegin = t * chunk;
        const std::size_t chunkEnd = std::min(total, chunkBegin + chunk);
        if (chunkBegin >= chunkEnd) break;

        futs.push_back(std::async(std::launch::async,
            [&, chunkBegin, chunkEnd]() {
                for (std::size_t off = chunkBegin; off < chunkEnd; ++off) {
                    const std::size_t global = begin + off;
                    results[off] = std::forward<Worker>(worker)(global);

                    const std::size_t now = done.fetch_add(1, std::memory_order_relaxed) + 1;
                    if constexpr (!std::is_same_v<OnProgress, std::nullptr_t>) {
                        onProgress(now, total);
                    }
                }
            }
        ));
    }

    for (auto& f : futs) f.get();
    return results;
}

} // namespace aip::search
