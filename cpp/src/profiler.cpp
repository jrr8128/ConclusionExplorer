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

  candidates_considered.store(0, std::memory_order_relaxed);
  descended.store(0, std::memory_order_relaxed);
  leaves_reached.store(0, std::memory_order_relaxed);

  prune_conflict.store(0, std::memory_order_relaxed);
  prune_no_change.store(0, std::memory_order_relaxed);
  prune_inconsistent.store(0, std::memory_order_relaxed);
  prune_memo_dead.store(0, std::memory_order_relaxed);
  prune_dominance.store(0, std::memory_order_relaxed);
  prune_should_expand.store(0, std::memory_order_relaxed);
  prune_premise_seen.store(0, std::memory_order_relaxed);

  leaf_prune_missing_coverage.store(0, std::memory_order_relaxed);
  leaf_prune_no_unique_conclusion.store(0, std::memory_order_relaxed);
  leaf_prune_taste_banned.store(0, std::memory_order_relaxed);
  leaf_prune_present.store(0, std::memory_order_relaxed);
  leaf_prune_requires_all_failed.store(0, std::memory_order_relaxed);
  leaf_prune_too_many_conclusions.store(0, std::memory_order_relaxed);

  solutions_emitted.store(0, std::memory_order_relaxed);
  solutions_accepted.store(0, std::memory_order_relaxed);


  apply_premise_calls.store(0, std::memory_order_relaxed);
  entails_calls.store(0, std::memory_order_relaxed);
  leaf_unique_scans.store(0, std::memory_order_relaxed);
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

  out << "[run]\n";
  out << "elapsed " << format_elapsed_ms(elapsed_ms()) << "\n";
  out << "term_count " << int(term_count) << "\n";
  out << "max_depth " << int(max_depth) << "\n";
  out << "max_stack_depth_observed " << load64(max_stack_depth_observed) << "\n\n";

  out << "[flow summary / survival rates per stage]\n";
  out << std::left << std::setw(18) << "stage"
      << std::right << std::setw(10) << "count"
      << std::setw(8) << "%all"
      << std::setw(8) << "%prev" << "\n";

  out << std::left << std::setw(18) << "0. candidates"
    << std::right << std::setw(10) << load64(candidates_considered)
    << std::setw(8) << "100.0"
    << std::setw(8) << "-" << "\n";

  out << std::left << std::setw(18) << "1. preleaf"
      << std::right << std::setw(10) << load64(descended)
      << std::setw(8) << pct_str(load64(descended), load64(candidates_considered))
      << std::setw(8) << pct_str(load64(descended), load64(candidates_considered))
      << "\n";

  out << std::left << std::setw(18) << "2. leaf_reached"
      << std::right << std::setw(10) << load64(leaves_reached)
      << std::setw(8) << pct_str(load64(leaves_reached), load64(candidates_considered))
      << std::setw(8) << pct_str(load64(leaves_reached), load64(descended))
      << "\n";

  out << std::left << std::setw(18) << "3. solutions_found"
      << std::right << std::setw(10) << load64(solutions_emitted)
      << std::setw(8) << pct_str(load64(solutions_emitted), load64(candidates_considered))
      << std::setw(8) << pct_str(load64(solutions_emitted), load64(leaves_reached))
      << "\n";

  out << std::left << std::setw(18) << "4. sols recorded"
      << std::right << std::setw(10) << load64(solutions_accepted)
      << std::setw(8) << pct_str(load64(solutions_accepted), load64(candidates_considered))
      << std::setw(8) << pct_str(load64(solutions_accepted), load64(solutions_emitted))
      << "\n\n";

  out << "[preleaf_prune | try_descend]\n";
  out << "1. conflict " << load64(prune_conflict) << "\n";
  out << "2. no_change " << load64(prune_no_change) << "\n";
  out << "3. inconsistent " << load64(prune_inconsistent) << "\n";
  out << "4. memo_dead " << load64(prune_memo_dead) << "\n";
  out << "5. dominance " << load64(prune_dominance) << "\n";
  out << "6. should_expand " << load64(prune_should_expand) << "\n";
  out << "7. premise_seen " << load64(prune_premise_seen) << "\n\n";

  out << "[leaf_prune | handle_leaf]\n";
  out << "1. missing_coverage " << load64(leaf_prune_missing_coverage) << "\n";
  out << "2. no_unique_conclusion " << load64(leaf_prune_no_unique_conclusion) << "\n";
  out << "3. taste_banned " << load64(leaf_prune_taste_banned) << "\n";
  out << "4. requires_all_failed " << load64(leaf_prune_requires_all_failed) << "\n";
  out << "5. too_many_conclusions " << load64(leaf_prune_too_many_conclusions) << "\n";
  out << "6. already_present " << load64(leaf_prune_present) << "\n\n";

  out << "[workload]\n";
  out << "apply_premise_calls " << load64(apply_premise_calls) << "\n";
  out << "entails_calls " << load64(entails_calls) << "\n";
  out << "leaf_unique_scans " << load64(leaf_unique_scans) << "\n\n";

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
