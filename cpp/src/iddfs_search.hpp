#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

#include "canonicalizer.hpp"
#include "common_types.hpp"
#include "memo.hpp"
#include "profiler.hpp"
#include "prune_rules.hpp"
#include "puzzle_collector.hpp"
#include "semantic.hpp"
#include "semantic_state.hpp"
#include "syntax.hpp"

namespace conclusion_explorer {
void install_sigint_handler() noexcept;
bool stop_requested() noexcept;

void run_iddfs(const semantic_space&, const syntax_space&, const prune_rules&,
               memo&, semantic_state, premise_path&, uint8_t max_depth,
               collector&, profiler&);

}  // namespace conclusion_explorer