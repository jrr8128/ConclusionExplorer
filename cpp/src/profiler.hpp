#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace conclusion_explorer {

inline std::string format_elapsed_ms(std::uint64_t ms) {
  const std::uint64_t s = ms / 1000;
  const std::uint64_t rem = ms % 1000;
  std::ostringstream oss;
  oss << s << "." << std::setw(3) << std::setfill('0') << rem << "s";
  return oss.str();
}

static std::string pct_str(std::uint64_t num, std::uint64_t den) {
  double p = den ? (100.0 * double(num) / double(den)) : 0.0;
  std::ostringstream oss;
  if (p >= 1.0) {
    oss << std::fixed << std::setprecision(1);
  } else if (p >= 0.1) {
    oss << std::fixed << std::setprecision(2);
  } else {
    oss << std::fixed << std::setprecision(3);
  }
  oss << p << "%";
  return oss.str();
}

struct profiler {
  std::chrono::steady_clock::time_point start_time{};
  std::chrono::steady_clock::time_point end_time{};
  std::atomic<std::uint32_t> limit_epoch{0};
  // --- totals ---
  std::atomic<std::uint8_t> current_limit{0};
  std::atomic<std::size_t> current_stack_size{0};

  // flow
  std::atomic<std::uint64_t> candidates_considered{0};
  std::atomic<std::uint64_t> descended{0};
  std::atomic<std::uint64_t> leaves_reached{0};

  // preleaf prune
  std::atomic<std::uint64_t> prune_conflict{0};
  std::atomic<std::uint64_t> prune_no_change{0};
  std::atomic<std::uint64_t> prune_inconsistent{0};
  std::atomic<std::uint64_t> prune_memo_dead{0};
  std::atomic<std::uint64_t> prune_dominance{0};
  std::atomic<std::uint64_t> prune_should_expand{0};
  std::atomic<std::uint64_t> prune_premise_seen{0};

  // leaf prune
  std::atomic<std::uint64_t> leaf_prune_missing_coverage{0};
  std::atomic<std::uint64_t> leaf_prune_no_unique_conclusion{0};
  std::atomic<std::uint64_t> leaf_prune_taste_banned{0};
  std::atomic<std::uint64_t> leaf_prune_present{0};
  std::atomic<std::uint64_t> leaf_prune_requires_all_failed{0};
  std::atomic<std::uint64_t> leaf_prune_too_many_conclusions{0};

  // outcomes
  std::atomic<std::uint64_t> solutions_emitted{0};
  std::atomic<std::uint64_t> solutions_accepted{0};

  // workload
  std::atomic<std::uint64_t> apply_premise_calls{0};
  std::atomic<std::uint64_t> entails_calls{0};
  std::atomic<std::uint64_t> leaf_unique_scans{0};
  std::atomic<std::uint64_t> max_stack_depth_observed{0};

  // --- per limit / per k ---
  std::vector<std::uint64_t> nodes_by_limit;
  std::vector<std::uint64_t> leaves_by_limit;
  std::vector<std::uint64_t> solutions_by_limit;

  std::vector<std::uint64_t> leaves_by_k;
  std::vector<std::uint64_t> solutions_by_k;

  std::atomic<std::uint64_t> premise_seen_size{0};
  std::atomic<std::uint64_t> premise_seen_buckets{0};

  // lifecycle
  void begin_term_run(std::uint8_t term_count, std::uint8_t max_depth);
  void end_term_run();
  std::uint64_t elapsed_ms() const;
  void begin_depth_limit(std::uint8_t limit);
  void on_stack_depth(std::size_t depth);

  void write_stats_file(std::uint8_t term_count, std::uint8_t max_depth) const;
  void print_snapshot() const;

  // snapshot rate tracking (mutable so print_snapshot() can update)
  mutable std::uint64_t rate_window_ms = 4000;
  mutable std::uint64_t rate_acc_ms = 0;
  mutable std::uint64_t rate_acc_nodes = 0;
  mutable std::uint64_t last_snapshot_ms = 0;
  mutable std::uint64_t last_snapshot_nodes = 0;
  mutable double last_rate = 0.0;
  mutable double min_rate = 0.0;
  mutable double max_rate = 0.0;
  mutable bool rate_initialized = false;

  // config (optional; set from main)
  std::string output_root = "recipes";  // where stats file is written
};

}  // namespace conclusion_explorer
