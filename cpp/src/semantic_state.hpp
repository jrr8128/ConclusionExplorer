#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "common_types.hpp"
#include "semantic.hpp"

namespace conclusion_explorer {

constexpr uint16_t MAX_REQS = 6 * MAX_TERMS * (MAX_TERMS - 1);
constexpr uint16_t MAX_REQ_WORDS = (MAX_REQS + 63) / 64;

enum class apply_result : int8_t {
  inconsistent = -1,
  no_change = 0,
  changed = 1
};

struct semantic_state {
  region_mask empty{};
  std::array<uint64_t, MAX_REQ_WORDS> req_bits{};
  uint8_t base_terms_mask = 0;
};

[[nodiscard]] apply_result apply_premise(semantic_state&, class_id,
                                         const semantic_space&);
[[nodiscard]] bool is_inconsistent(const semantic_state&,
                                   const semantic_space&);
[[nodiscard]] bool entails(const semantic_state&, class_id,
                           const semantic_space&);

}  // namespace conclusion_explorer