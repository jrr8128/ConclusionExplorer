#pragma once
#include <chrono>
#include <cstdint>
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

struct profiler {
  std::chrono::steady_clock::time_point start_time{};
  std::chrono::steady_clock::time_point end_time{};
  std::atomic<std::uint32_t> limit_epoch{0};
  // --- totals ---
  std::atomic<std::uint8_t> current_limit{0};
  std::atomic<std::size_t> current_stack_size{0};

  std::atomic<std::uint64_t> nodes_considered{0};
  std::atomic<std::uint64_t> conflicts_pruned{0};
  std::atomic<std::uint64_t> inconsistent_pruned{0};
  std::atomic<std::uint64_t> memo_pruned{0};
  std::atomic<std::uint64_t> should_expand_pruned{0};
  std::atomic<std::uint64_t> leaf_reached{0};
  std::atomic<std::uint64_t> leaf_missing_coverage{0};
  std::atomic<std::uint64_t> leaf_non_unique{0};
  std::atomic<std::uint64_t> leaf_taste_banned{0};
  std::atomic<std::uint64_t> solutions_emitted{0};
  std::atomic<std::uint64_t> solutions_accepted{0};

  // optional
  std::atomic<std::uint64_t> apply_calls{0};
  std::atomic<std::uint64_t> entails_calls{0};
  std::atomic<std::uint64_t> unique_conclusion_scans{0};
  std::atomic<std::uint64_t> redunant{0};
  // std::atomic<std::uint64_t> subsumed{0};
  std::atomic<std::uint64_t> partial{0};
  std::atomic<std::uint64_t> toomanyconc{0};
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
  // config (optional; set from main)
  std::string output_root = "recipes";  // where stats file is written
};

}  // namespace conclusion_explorer
