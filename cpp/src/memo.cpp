#include "memo.hpp"

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
  const size_t x = static_cast<size_t>(k.next_min_id);
  h ^= x + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
  return h;
}

bool memo::should_prune(const semantic_state& state, uint16_t next_min_id,
                        uint8_t depth_left,
                        const semantic_space& sem_space) const {
  memo_key key{c.make_key(state, sem_space), next_min_id};
  auto iter = seen.find(key);
  if (iter == seen.end()) return false;
  return depth_left <= iter->second;
}

void memo::record_seen(const semantic_state& state, uint16_t next_min_id,
                       uint8_t depth_left, const semantic_space& sem_space) {
  memo_key key{c.make_key(state, sem_space), next_min_id};
  auto [iter, inserted] = seen.emplace(key, depth_left);
  if (!inserted && depth_left > iter->second) {
    iter->second = depth_left;
  }
}
}  // namespace conclusion_explorer