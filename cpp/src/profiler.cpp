#include "profiler.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace conclusion_explorer {

static void atomic_fetch_max(std::atomic<std::uint64_t>& a, std::uint64_t v) {
  std::uint64_t cur = a.load(std::memory_order_relaxed);
  while (cur < v &&
         !a.compare_exchange_weak(cur, v, std::memory_order_relaxed)) {
    // cur updated by compare_exchange_weak
  }
}

void profiler::begin_term_run(std::uint8_t term_count, std::uint8_t max_depth) {
  start_time = std::chrono::steady_clock::now();
  current_limit.store(0, std::memory_order_relaxed);
  current_stack_size.store(0, std::memory_order_relaxed);

  nodes_considered.store(0, std::memory_order_relaxed);
  conflicts_pruned.store(0, std::memory_order_relaxed);
  inconsistent_pruned.store(0, std::memory_order_relaxed);
  memo_pruned.store(0, std::memory_order_relaxed);
  should_expand_pruned.store(0, std::memory_order_relaxed);
  leaf_reached.store(0, std::memory_order_relaxed);
  leaf_missing_coverage.store(0, std::memory_order_relaxed);
  leaf_non_unique.store(0, std::memory_order_relaxed);
  leaf_taste_banned.store(0, std::memory_order_relaxed);
  solutions_emitted.store(0, std::memory_order_relaxed);
  solutions_accepted.store(0, std::memory_order_relaxed);

  apply_calls.store(0, std::memory_order_relaxed);
  entails_calls.store(0, std::memory_order_relaxed);
  unique_conclusion_scans.store(0, std::memory_order_relaxed);
  redunant.store(0, std::memory_order_relaxed);
  // subsumed.store(0, std::memory_order_relaxed);
  partial.store(0, std::memory_order_relaxed);
  toomanyconc.store(0, std::memory_order_relaxed);
  max_stack_depth_observed.store(0, std::memory_order_relaxed);

  nodes_by_limit.assign(static_cast<std::size_t>(max_depth) + 1, 0);
  leaves_by_limit.assign(static_cast<std::size_t>(max_depth) + 1, 0);
  solutions_by_limit.assign(static_cast<std::size_t>(max_depth) + 1, 0);

  // k is in [0..max_depth]
  leaves_by_k.assign(static_cast<std::size_t>(max_depth) + 1, 0);
  solutions_by_k.assign(static_cast<std::size_t>(max_depth) + 1, 0);
}

void profiler::end_term_run() { end_time = std::chrono::steady_clock::now(); }

std::uint64_t profiler::elapsed_ms() const {
  const auto t1 = (end_time.time_since_epoch().count() == 0)
                      ? std::chrono::steady_clock::now()
                      : end_time;
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(t1 - start_time)
          .count());
}

void profiler::begin_depth_limit(std::uint8_t limit) {
  current_limit.store(limit, std::memory_order_relaxed);
}

void profiler::on_stack_depth(std::size_t depth) {
  current_stack_size.store(depth, std::memory_order_relaxed);
  atomic_fetch_max(max_stack_depth_observed, static_cast<std::uint64_t>(depth));
}

void profiler::write_stats_file(std::uint8_t term_count,
                                std::uint8_t max_depth) const {
  fs::create_directories(output_root);

  fs::path p = fs::path(output_root) /
               (std::to_string(term_count) + "-term-statistics.txt");
  std::ofstream out(p, std::ios::out | std::ios::binary);

  auto load64 = [](const std::atomic<std::uint64_t>& a) {
    return a.load(std::memory_order_relaxed);
  };
  auto loadu8 = [](const std::atomic<std::uint8_t>& a) {
    return a.load(std::memory_order_relaxed);
  };
  auto loadsz = [](const std::atomic<std::size_t>& a) {
    return a.load(std::memory_order_relaxed);
  };

  out << "[run]\n";
  out << "elapsed " << format_elapsed_ms(elapsed_ms()) << "\n";
  out << "term_count " << int(term_count) << "\n";
  out << "max_depth " << int(max_depth) << "\n\n";

  out << "[sizes]\n";
  out << "premise_seen_size " << load64(premise_seen_size) << "\n";
  out << "premise_seen_buckets " << load64(premise_seen_buckets) << "\n\n";

  out << "[totals]\n";
  out << "nodes_considered " << load64(nodes_considered) << "\n";
  out << "solutions_emitted " << load64(solutions_emitted) << "\n";
  out << "solutions_accepted " << load64(solutions_accepted) << "\n";
  out << "max_stack_depth_observed " << load64(max_stack_depth_observed)
      << "\n\n";

  out << "[leaf]\n";
  out << "reached " << load64(leaf_reached) << "\n";
  out << "missing_coverage " << load64(leaf_missing_coverage) << "\n";
  out << "non_unique " << load64(leaf_non_unique) << "\n";
  out << "taste_banned " << load64(leaf_taste_banned) << "\n\n";

  out << "[conclusion_filters]\n";
  out << "redundant " << load64(redunant) << "\n";
  // out << "subsumed " << load64(subsumed) << "\n";
  out << "partial " << load64(partial) << "\n";
  out << "too_many " << load64(toomanyconc) << "\n\n";

  out << "[prune]\n";
  out << "conflicts " << load64(conflicts_pruned) << "\n";
  out << "inconsistent " << load64(inconsistent_pruned) << "\n";
  out << "memo " << load64(memo_pruned) << "\n";
  out << "should_expand " << load64(should_expand_pruned) << "\n\n";

  out << "[optional]\n";
  out << "apply_calls " << load64(apply_calls) << "\n";
  out << "entails_calls " << load64(entails_calls) << "\n";
  out << "unique_conclusion_scans " << load64(unique_conclusion_scans)
      << "\n\n";

  out << "[by_limit]\n";
  for (std::size_t l = 0; l < nodes_by_limit.size(); ++l) {
    if (nodes_by_limit[l] == 0 && leaves_by_limit[l] == 0 &&
        solutions_by_limit[l] == 0) {
      continue;
    }
    out << l << " nodes " << nodes_by_limit[l] << " leaves "
        << leaves_by_limit[l] << " solutions " << solutions_by_limit[l] << "\n";
  }
  out << "\n";

  out << "[by_k]\n";
  for (std::size_t k = 0; k < leaves_by_k.size(); ++k) {
    if (leaves_by_k[k] == 0 && solutions_by_k[k] == 0) continue;
    out << k << " leaves " << leaves_by_k[k] << " solutions "
        << solutions_by_k[k] << "\n";
  }
}

}  // namespace conclusion_explorer
