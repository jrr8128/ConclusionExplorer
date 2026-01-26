#pragma once

#include <optional>

namespace conclusion_explorer {
struct prune_rules {
  prune_rules(uint8_t term_count, const syntax_space& syn);
  uint8_t goal_mask;
  std::vector<uint8_t> suffix_union_mask;
  bool should_expand(const semantic_state&, uint16_t next_min_id,
                     uint8_t depth_left, const semantic_space&) const;
  std::optional<class_id> unique_interesting_conclusion(
      const semantic_state&, const premise_path& path,
      const semantic_space&) const;
};

}  // namespace conclusion_explorer