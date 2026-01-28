#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "common_types.hpp"
#include "semantic.hpp"

namespace conclusion_explorer {
struct profiler;
struct term_perm;

struct semantic_state {
  region_mask empty{};
  std::array<std::uint64_t, MAX_REQ_WORDS> req_bits{};
  uint8_t base_terms_mask = 0;
};

semantic_state permuted_state(const semantic_state&, const std::size_t perm_i,
                              const syntax_space&, const semantic_space&);

using class_bitset = std::array<std::uint64_t, MAX_CLASS_WORDS>;

enum class apply_result : std::int8_t {
  inconsistent = -1,
  no_change = 0,
  changed = 1
};

[[nodiscard]] apply_result apply_premise(semantic_state&, class_id,
                                         const semantic_space&);
[[nodiscard]] bool is_inconsistent(const semantic_state&,
                                   const semantic_space&);
[[nodiscard]] bool entails(const semantic_state&, class_id,
                           const semantic_space&, profiler&);

}  // namespace conclusion_explorer