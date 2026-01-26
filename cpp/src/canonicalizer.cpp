#include "canonicalizer.hpp"

namespace conclusion_explorer {

canonical_key canon::make_key(const semantic_state& state,
                              const semantic_space& sem_space) const {
  canonical_key k{};
  for (size_t word = 0; word < sem_space.active_words; word++) {
    k.empty.w[word] = state.empty.w[word] & sem_space.all_regions.w[word];
  }

  for (size_t index = 0; index < MAX_REQ_WORDS; index++) {
    k.req_bits[index] =
        (index < sem_space.req_words) ? state.req_bits[index] : 0;
  }
  const size_t req_count = sem_space.req_mask_by_req_index.size();
  const size_t tail = req_count & 63;
  if (tail && sem_space.req_words) {
    k.req_bits[sem_space.req_words - 1] &= ((1ULL << tail) - 1ULL);
  }

  k.base_terms_mask = state.base_terms_mask;
  return k;
}

}  // namespace conclusion_explorer