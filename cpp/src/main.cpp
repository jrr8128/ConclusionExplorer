#include <iostream>

#include "canonicalizer.hpp"
#include "iddfs_search.hpp"
#include "memo.hpp"
#include "profiler.hpp"
#include "prune_rules.hpp"
#include "puzzle_collector.hpp"
#include "semantic.hpp"
#include "syntax.hpp"
using namespace ::conclusion_explorer;

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>

static void live_printer(std::stop_token st,
                         const conclusion_explorer::profiler& p) {
  using clock = std::chrono::steady_clock;
  using namespace std::chrono_literals;

  std::uint32_t last_epoch = p.limit_epoch.load(std::memory_order_relaxed);
  clock::time_point last_print = clock::now();

  while (!st.stop_requested()) {
    const std::uint32_t e = p.limit_epoch.load(std::memory_order_relaxed);
    const bool epoch_changed = (e != last_epoch);
    const bool time_due = (clock::now() - last_print) >= 4s;

    if (epoch_changed || time_due) {
      last_epoch = e;
      last_print = clock::now();

      p.print_snapshot();
    }

    std::this_thread::sleep_for(2000ms);  // responsiveness to epoch bumps
  }
}

static std::uint8_t parse_term_count(int argc, char** argv) {
  if (argc < 2) {
    return 3;
  }
  const long v = std::strtol(argv[1], nullptr, 10);
  if (v < 3 || v > 8) {
    std::cerr << "usage: app.exe [term_count 3..8]\n";
    std::exit(1);
  }
  return static_cast<std::uint8_t>(v);
}

int main(int argc, char** argv) {
  const std::uint8_t term_count = parse_term_count(argc, argv);
  conclusion_explorer::install_sigint_handler();
  syntax_space syn_space = build_syntax_space(term_count);
  semantic_space sem_space = build_semantic_space(syn_space);
  prune_rules rules{term_count, syn_space, sem_space};
  canon canonicalizer;
  memo memo_table{canonicalizer};
  collector puzzle_collector{collector_config{}};

  puzzle_collector.begin_term_run(term_count, syn_space);
  uint8_t max_depth = term_count + 1;
  premise_path path{};
  semantic_state root{};

  profiler prof;
  prof.begin_term_run(term_count, max_depth);
  std::jthread printer(live_printer, std::cref(prof));

  run_iddfs(sem_space, syn_space, rules, memo_table, root, path, max_depth,
            puzzle_collector, prof);
  printer.request_stop();
  prof.end_term_run();
  prof.write_stats_file(term_count, max_depth);

  puzzle_collector.finalize();

  return 0;
}