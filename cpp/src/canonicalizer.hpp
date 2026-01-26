#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "common_types.hpp"
#include "semantic.hpp"
#include "semantic_state.hpp"

namespace conclusion_explorer {
struct canonical_key {
  region_mask empty;
  std::array<uint64_t, MAX_REQ_WORDS> req_bits{};
  uint8_t base_terms_mask = 0;
  bool operator==(const canonical_key&) const = default;
};

struct canon {
  canonical_key make_key(const semantic_state&, const semantic_space&) const;
};
}  // namespace conclusion_explorer