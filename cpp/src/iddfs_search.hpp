#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "canonicalizer.hpp"
#include "common_types.hpp"
#include "memo.hpp"
#include "prune_rules.hpp"
#include "puzzle_collector.hpp"
#include "semantic.hpp"
#include "semantic_state.hpp"

namespace conclusion_explorer {

struct search_result {};

void run_iddfs(const semantic_space& sem_space, semantic_state root,
               uint16_t premise_count, uint8_t max_depth, prune_rules&, memo&,
               canon*, collector&);

}  // namespace conclusion_explorer