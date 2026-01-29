#pragma once

#include <unordered_set>

#include "canonicalizer.hpp"
#include "common_types.hpp"
#include "semantic_state.hpp"

namespace conclusion_explorer {
struct dom_entry {
  canonical_key key;
  std::uint8_t best_depth_left;
};

struct memo_key {
  canonical_key c_key;
  bool operator==(const memo_key&) const = default;
};

struct memo_key_hash {
  size_t operator()(const memo_key&) const noexcept;
};

struct memo {
  const canon& c;
  std::unordered_set<memo_key, memo_key_hash> dead;

  std::array<std::vector<dom_entry>, 256> dom;

  bool should_prune_dominance(const semantic_state&, std::uint8_t depth_leth,
                              const syntax_space&, const semantic_space&) const;
  bool should_prune_dominance_key(const canonical_key&, std::uint8_t depth_left,
                                  const semantic_space&) const;

  void record_dominance(const semantic_state&, std::uint8_t depth_left,
                        const syntax_space&, const semantic_space&);
  void record_dominance_key(const canonical_key&, std::uint8_t depth_left,
                            const semantic_space&);
  bool is_dead(const semantic_state&, const syntax_space&,
               const semantic_space&) const;
  bool is_dead_key(const canonical_key&) const;

  void record_dead(const semantic_state&, const syntax_space&,
                   const semantic_space&);
};

}  // namespace conclusion_explorer