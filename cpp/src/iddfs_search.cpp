#include "iddfs_search.hpp"

namespace conclusion_explorer {
struct iddfs_frame {
  semantic_state state;
  uint16_t next_id;
  uint8_t depth_left;
};

search_result run_iddfs(const semantic_space& sem_space, semantic_state root,
                        uint16_t premise_count, uint8_t max_depth) {
  return {};
}
}  // namespace conclusion_explorer