#include "semantic_state.hpp"

#include <assert.h>

#include <iostream>

#include "canonicalizer.hpp"
#include "profiler.hpp"
#include "semantic.hpp"
#include "syntax.hpp"

namespace conclusion_explorer {

static bool get_region_bit(const region_mask& mask, std::uint16_t idx) {
  const std::uint16_t word = idx >> 6;
  const std::uint16_t bit = idx & 63;
  return (mask.w[word] >> bit) & 1ull;
}

static void set_region_bit(region_mask& mask, std::uint16_t idx) {
  const std::uint16_t word = idx >> 6;
  const std::uint16_t bit = idx & 63;
  mask.w[word] |= (1ull << bit);
}

static region_mask permute_empty_mask(const region_mask& in_empty,
                                      std::size_t perm_i,
                                      const semantic_space& sem_space) {
  region_mask output{};
  const std::uint16_t region_count = sem_space.region_count;

  for (std::uint16_t region = 0; region < region_count; ++region) {
    if (!get_region_bit(in_empty, region)) {
      continue;
    }

    const std::uint16_t region2 =
        sem_space
            .permuted_region_index_by_perm_and_region[perm_i * region_count +
                                                      region];
    set_region_bit(output, region2);
  }
  return output;
}

static void set_req_bit(std::array<std::uint64_t, MAX_REQ_WORDS>& bits,
                        std::uint16_t req) {
  const std::uint16_t word = req >> 6;
  const std::uint16_t bit = req & 63;
  bits[word] |= (1ull << bit);
}

static std::array<std::uint64_t, MAX_REQ_WORDS> permute_req_bits(
    const std::array<std::uint64_t, MAX_REQ_WORDS>& input, std::size_t perm_i,
    const semantic_space& sem_space) {
  std::array<std::uint64_t, MAX_REQ_WORDS> output{};
  const std::uint16_t req_count = sem_space.req_count;

  for (std::uint16_t word = 0; word < sem_space.req_words; ++word) {
    for (std::uint64_t bits = input[word]; bits; bits &= (bits - 1)) {
      const std::uint16_t req =
          static_cast<std::uint16_t>(word * 64 + __builtin_ctzll(bits));
      if (req >= req_count) {
        continue;
      }
      const std::uint16_t req2 =
          sem_space
              .permuted_req_index_by_perm_and_req[perm_i * req_count + req];
      set_req_bit(output, req2);
    }
  }
  return output;
}

static std::uint8_t permute_base_terms_mask(std::uint8_t mask,
                                            const term_perm& perm,
                                            std::uint8_t term_count) {
  std::uint8_t output = 0;
  for (std::uint8_t term = 0; term < term_count; ++term) {
    if (mask & static_cast<std::uint8_t>(1u << term)) {
      output |= static_cast<std::uint8_t>(1u << perm.p[term]);
    }
  }
  return output;
}

semantic_state permuted_state(const semantic_state& state, std::size_t perm_i,
                              const syntax_space& syn_space,
                              const semantic_space& sem_space) {
  semantic_state output{};
  output.base_terms_mask = permute_base_terms_mask(
      state.base_terms_mask, syn_space.perms[perm_i], syn_space.term_count);
  output.empty = permute_empty_mask(state.empty, perm_i, sem_space);
  output.req_bits = permute_req_bits(state.req_bits, perm_i, sem_space);
  return output;
}

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
             const semantic_space& sem_space, profiler& prof) {
  prof.entails_calls.fetch_add(1, std::memory_order_relaxed);
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