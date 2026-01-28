#pragma once

#include <optional>
#include <vector>

#include "common_types.hpp"
#include "profiler.hpp"
#include "puzzle_collector.hpp"
#include "semantic.hpp"
#include "semantic_state.hpp"

namespace conclusion_explorer {
struct prune_rules {
  prune_rules(uint8_t term_count, const syntax_space&, const semantic_space&);
  uint8_t goal_mask;
  std::vector<uint8_t> suffix_union_mask;
  std::vector<uint8_t> base_terms_mask_by_cid;
  std::vector<class_bitset> conflict_bits_by_cid;
  bool should_expand(const semantic_state&, uint16_t next_min_id,
                     uint8_t depth_left, const semantic_space&) const;
  std::optional<class_id> unique_interesting_conclusion(
      const semantic_state&, const premise_path& path, const class_bitset&,
      const semantic_space&, profiler&) const;
  bool is_banned_with_path(class_id, const premise_path&) const;
};

}  // namespace conclusion_explorer