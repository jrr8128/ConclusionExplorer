#include "memo.hpp"

#include <iostream>

#include "canonicalizer.hpp"

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

bool keys_equal(const canonical_key& a, const canonical_key& b,
                const semantic_space& sem) {
  if (a.base_terms_mask != b.base_terms_mask) return false;
  for (std::uint8_t w = 0; w < sem.active_words; ++w)
    if (a.empty.w[w] != b.empty.w[w]) return false;
  for (std::uint8_t w = 0; w < sem.req_words; ++w)
    if (a.req_bits[w] != b.req_bits[w]) return false;
  return true;
}

bool memo::should_prune_dominance(const semantic_state& state,
                                  std::uint8_t depth_left,
                                  const syntax_space& syn_space,
                                  const semantic_space& sem_space) const {
  return should_prune_dominance_key(c.make_iso_key(state, syn_space, sem_space),
                                    depth_left, sem_space);
}

bool memo::should_prune_dominance_key(const canonical_key& k,
                                      std::uint8_t depth_left,
                                      const semantic_space& sem_space) const {
  const auto& vec = dom[k.base_terms_mask];
  for (const dom_entry& e : vec) {
    if (keys_equal(e.key, k, sem_space) && e.best_depth_left >= depth_left) {
      return true;
    }
  }
  return false;
}

void memo::record_dominance(const semantic_state& state,
                            std::uint8_t depth_left,
                            const syntax_space& syn_space,
                            const semantic_space& sem_space) {
  return record_dominance_key(c.make_iso_key(state, syn_space, sem_space),
                              depth_left, sem_space);
}

void memo::record_dominance_key(const canonical_key& k, std::uint8_t depth_left,
                                const semantic_space& sem_space) {
  auto& vec = dom[k.base_terms_mask];

  for (dom_entry& e : vec) {
    if (keys_equal(e.key, k, sem_space)) {
      if (e.best_depth_left < depth_left) e.best_depth_left = depth_left;
      return;
    }
  }
  vec.push_back(dom_entry{.key = k, .best_depth_left = depth_left});
}

bool memo::is_dead(const semantic_state& state, const syntax_space& syn_space,
                   const semantic_space& sem_space) const {
  if (dead.empty()) {
    return false;
  }
  return is_dead_key(c.make_iso_key(state, syn_space, sem_space));
}

bool memo::is_dead_key(const canonical_key& k) const {
  if (dead.empty()) {
    return false;
  }
  const bool r = (dead.find(memo_key{k}) != dead.end());
  return r;
}

void memo::record_dead(const semantic_state& state,
                       const syntax_space& syn_space,
                       const semantic_space& sem_space) {
  dead.insert(memo_key{c.make_iso_key(state, syn_space, sem_space)});
}
}  // namespace conclusion_explorer