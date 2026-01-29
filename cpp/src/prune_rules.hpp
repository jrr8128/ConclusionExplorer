#pragma once

#include <optional>
#include <vector>
#include <unordered_map>

#include "common_types.hpp"
#include "profiler.hpp"
#include "semantic.hpp"
#include "semantic_state.hpp"
#include "puzzle_collector.hpp"

namespace conclusion_explorer {
struct region_mask_hash {
  size_t operator()(const region_mask& r) const noexcept {
    size_t h = 0;
    for (uint16_t i = 0; i < MASK_WORDS; ++i) {
      const size_t x = static_cast<size_t>(r.w[i]);
      h ^= x + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    }
    return h;
  }
};


struct leaf_ctx{
  const semantic_space& sem_space;
  const class_bitset& present_bits;
  const premise_path& path;
  profiler& prof;
};

struct premise_aggregate{
  std::vector<region_mask> empty_prefix;
  std::vector<region_mask> empty_suffix;

  std::vector<std::array<std::uint64_t, MAX_REQ_WORDS>> req_prefix;
  std::vector<std::array<std::uint64_t, MAX_REQ_WORDS>> req_suffix;
};

struct prune_rules {
  prune_rules(uint8_t term_count, const syntax_space&, const semantic_space&);
  uint8_t goal_mask;
  std::vector<uint8_t> suffix_union_mask;
  std::vector<uint8_t> base_terms_mask_by_cid;
  std::vector<class_bitset> conflict_bits_by_cid;
  mutable std::unordered_map<region_mask, std::vector<class_id>,
                             region_mask_hash> req_superset_cache;
  mutable std::vector<class_id> entailed_req_cids_scratch;
  mutable premise_aggregate aggregate_scratch;

  bool should_expand(const semantic_state&, uint16_t next_min_id,
                     uint8_t depth_left, const semantic_space&) const;
  std::optional<class_id> unique_interesting_conclusion(
      const semantic_state&, const leaf_ctx&) const;
  bool is_banned_with_path(class_id, const premise_path&) const;
};

}  // namespace conclusion_explorer