#pragma once

#include <unordered_set>

#include "canonicalizer.hpp"
#include "common_types.hpp"
#include "semantic_state.hpp"

namespace conclusion_explorer {

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
  bool is_dead(const semantic_state&, const syntax_space&,
               const semantic_space&) const;
  void record_dead(const semantic_state&, const syntax_space&,
                   const semantic_space&);
};

}  // namespace conclusion_explorer