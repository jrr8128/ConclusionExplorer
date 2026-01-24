#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "common_types.hpp"
#include "semantic.hpp"

namespace conclusion_explorer {

constexpr uint16_t MAX_PREMISES = 24;
constexpr uint16_t REQ_WORDS = (MAX_PREMISES + 63) / 64;

enum class apply_result : int8_t {
  inconsistent = -1,
  no_change = 0,
  changed = 1
};

struct semantic_state {
  region_mask allowed;
  std::array<uint64_t, REQ_WORDS> req_bits{};
};

[[nodiscard]] apply_result apply_premise(semantic_state& state,
                                         premise_id prm_id,
                                         const semantic_space& sem_space);
[[nodiscard]] bool is_inconsistent(const semantic_state& state,
                                   const semantic_space& sem_space);
[[nodiscard]] bool entails(const semantic_state& state,
                           statement_id conclusion_id,
                           const semantic_space& sem_space);

}  // namespace conclusion_explorer