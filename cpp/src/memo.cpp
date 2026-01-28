#include "memo.hpp"

#include <iostream>

namespace conclusion_explorer {

struct region_mask_hash {
  size_t operator()(const region_mask& r_mask) const noexcept {
    size_t h = 0;
    for (uint16_t index = 0; index < MASK_WORDS; index++) {
      const size_t x = static_cast<size_t>(r_mask.w[index]);
      h ^= x + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    }
    return h;
  }
};

struct canonical_key_hash {
  size_t operator()(const canonical_key& k) const noexcept {
    size_t h = region_mask_hash{}(k.empty);
    for (uint16_t i = 0; i < MAX_REQ_WORDS; ++i) {
      const size_t x = static_cast<size_t>(k.req_bits[i]);
      h ^= x + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    }
    const size_t b = static_cast<size_t>(k.base_terms_mask);
    h ^= b + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    return h;
  }
};

size_t memo_key_hash::operator()(const memo_key& k) const noexcept {
  size_t h = canonical_key_hash{}(k.c_key);
  return h;
}

bool memo::is_dead(const semantic_state& state, const syntax_space& syn_space,
                   const semantic_space& sem_space) const {
  const memo_key k{c.make_iso_key(state, syn_space, sem_space)};
  const bool r = (dead.find(k) != dead.end());
  return r;
}

void memo::record_dead(const semantic_state& state,
                       const syntax_space& syn_space,
                       const semantic_space& sem_space) {
  dead.insert(memo_key{c.make_iso_key(state, syn_space, sem_space)});
}
}  // namespace conclusion_explorer