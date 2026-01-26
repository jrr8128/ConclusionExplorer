#include "semantic_state.hpp"

#include <assert.h>

namespace conclusion_explorer {

static bool req_is_impossible(const region_mask& empty_mask,
                              const region_mask& req_mask,
                              const semantic_space& sem_space) {
  for (uint8_t word_index = 0; word_index < sem_space.active_words;
       ++word_index)
    if (((~empty_mask.w[word_index]) & sem_space.all_regions.w[word_index] &
         req_mask.w[word_index]) != 0)
      return false;
  return true;
}

static bool update_empty(region_mask& out_new_empty, bool& out_changed,
                         const region_mask& old_empty,
                         const region_mask& forb_mask,
                         const semantic_space& sem_space) {
  bool any_allowed = false;
  out_changed = false;
  for (uint8_t word_index = 0; word_index < sem_space.active_words;
       ++word_index) {
    const word_t new_word = old_empty.w[word_index] | forb_mask.w[word_index];
    out_new_empty.w[word_index] = new_word;
    out_changed |= (new_word != old_empty.w[word_index]);
    any_allowed |= (((~new_word) & sem_space.all_regions.w[word_index]) != 0);
  }
  return any_allowed;
}

static bool add_req_bit(std::array<uint64_t, MAX_REQ_WORDS>& bits,
                        int16_t req_id) {
  if (req_id < 0) return false;
  const uint16_t word = static_cast<uint16_t>(req_id) >> 6, bit = req_id & 63;
  const uint64_t old = bits[word];
  bits[word] |= (1ull << bit);
  return bits[word] != old;
}

static bool any_req_impossible(const semantic_state& state,
                               const region_mask& empty_view,
                               const semantic_space& sem_space) {
  for (uint16_t word_index = 0; word_index < sem_space.req_words; ++word_index)
    for (uint64_t bits = state.req_bits[word_index]; bits; bits &= (bits - 1)) {
      const uint16_t r =
          static_cast<uint16_t>(word_index * 64 + __builtin_ctzll(bits));
      if (r < sem_space.req_mask_by_req_index.size() &&
          req_is_impossible(empty_view, sem_space.req_mask_by_req_index[r],
                            sem_space))
        return true;
    }
  return false;
}

apply_result apply_premise(semantic_state& state, class_id cid,
                           const semantic_space& sem_space) {
  const region_mask& forb_mask = sem_space.forbid_mask_by_class_id[cid.id];
  region_mask new_empty{};
  bool changed = false;
  if (!update_empty(new_empty, changed, state.empty, forb_mask, sem_space))
    return apply_result::inconsistent;
  const bool req_added =
      add_req_bit(state.req_bits, sem_space.req_index_by_class_id[cid.id]);
  const region_mask& result_mask = changed ? new_empty : state.empty;
  if ((changed || req_added) &&
      any_req_impossible(state, result_mask, sem_space))
    return apply_result::inconsistent;
  if (changed) state.empty = new_empty;
  return (changed || req_added) ? apply_result::changed
                                : apply_result::no_change;
}

static bool any_allowed_regions(const region_mask& empty,
                                const semantic_space& sem_space) {
  for (uint8_t word_index = 0; word_index < sem_space.active_words;
       ++word_index)
    if (((~empty.w[word_index]) & sem_space.all_regions.w[word_index]) != 0)
      return true;
  return false;
}

bool is_inconsistent(const semantic_state& state,
                     const semantic_space& sem_space) {
  if (!any_allowed_regions(state.empty, sem_space)) return true;
  return any_req_impossible(state, state.empty, sem_space);
}

bool entails(const semantic_state& state, class_id conclusion_cid,
             const semantic_space& sem_space) {
  if (is_inconsistent(state, sem_space)) {
    return false;
  }
  region_mask allowed{};
  for (uint8_t w = 0; w < sem_space.active_words; ++w)
    allowed.w[w] = (~state.empty.w[w]) & sem_space.all_regions.w[w];

  if (sem_space.kind_by_class_id[conclusion_cid.id] ==
      constraint_kind::forbid) {
    const region_mask& forb_mask =
        sem_space.forbid_mask_by_class_id[conclusion_cid.id];
    for (uint8_t word_index = 0; word_index < sem_space.active_words;
         word_index++) {
      if ((allowed.w[word_index] & forb_mask.w[word_index]) != 0) {
        return false;
      }
    }
    return true;
  }

  const int16_t conc_req_id =
      sem_space.req_index_by_class_id[conclusion_cid.id];
  if (conc_req_id < 0) {
    return false;
  }
  const region_mask& conc_mask =
      sem_space.req_mask_by_req_index[static_cast<size_t>(conc_req_id)];

  for (uint16_t word_index = 0; word_index < sem_space.req_words;
       word_index++) {
    for (uint64_t bits = state.req_bits[word_index]; bits; bits &= (bits - 1)) {
      const uint16_t req_index =
          static_cast<uint16_t>(word_index * 64 + __builtin_ctzll(bits));
      if (req_index >= sem_space.req_mask_by_req_index.size()) {
        continue;
      }
      bool any = false;
      bool subset_ok = true;
      for (uint8_t word = 0; word < sem_space.active_words; word++) {
        const word_t allowed_in_req_word =
            allowed.w[word] &
            sem_space.req_mask_by_req_index[req_index].w[word];
        any |= (allowed_in_req_word != 0);
        subset_ok &= ((allowed_in_req_word & ~conc_mask.w[word]) == 0);
      }
      if (any && subset_ok) {
        return true;
      }
    }
  }
  return false;
}
}  // namespace conclusion_explorer