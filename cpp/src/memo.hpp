#pragma once

#include <unordered_map>

#include "canonicalizer.hpp"
#include "common_types.hpp"
#include "semantic_state.hpp"

namespace conclusion_explorer {

struct memo_key {
  canonical_key c_key;
  uint16_t next_min_id = 0;
  bool operator==(const memo_key&) const = default;
};

struct memo_key_hash {
  size_t operator()(const memo_key&) const noexcept;
};

struct memo {
  const canon& c;
  std::unordered_map<memo_key, uint8_t, memo_key_hash> seen;
  bool should_prune(const semantic_state&, uint16_t next_min_id,
                    uint8_t depth_left, const semantic_space&) const;
  void record_seen(const semantic_state&, uint16_t next_min_id,
                   uint8_t depth_left, const semantic_space&);
};

}  // namespace conclusion_explorer