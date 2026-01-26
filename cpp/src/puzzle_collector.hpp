#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "common_types.hpp"
#include "iddfs_search.hpp"
#include "semantic.hpp"
#include "semantic_state.hpp"

namespace conclusion_explorer {

using premise_path = std::vector<class_id>;

struct collector {
  void add_solution(std::uint8_t term_count, class_id conclusion_cid,
                    const premise_path&, const semantic_state&,
                    const semantic_space&);
};
}  // namespace conclusion_explorer