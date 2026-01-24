#include "semantic_state.hpp"

#include <assert.h>

namespace conclusion_explorer {

// Return true if the intersection of two masks is empty.
//
// Intersection = bitwise AND.
// - If (mask_a & mask_b) has any 1-bit, that means there exists at least one
// region
//   that is included in both sets → intersection is NOT empty.
// - If all words AND to 0, there is no region shared → intersection IS empty.
//
// In this project,this is used to detect “no possible witness region”
// situations: e.g., state.allowed has no overlap with a required-nonempty
// constraint mask.
static bool intersects_empty(const region_mask& mask_a,
                             const region_mask& mask_b, uint8_t active_words) {
  for (uint32_t index = 0; index < active_words; index++) {
    if ((mask_a.w[index] & mask_b.w[index]) != 0) {
      return false;
    }
  }
  return true;
}

// Apply one premise to the current semantic_state.
//
// State meaning:
// - state.allowed: bitset of regions (candidate worlds) still possible.
// - state.req_bits: bitset over premise IDs. Bit i = 1 means premise i
// contributes an
//   I/O-style “must have a witness” constraint that must remain satisfiable.
//
// Return value:
// - inconsistent: premise makes the state impossible (prune this branch)
// - no_change: premise adds no new information (skip/memoize efficiently)
// - changed: premise narrowed allowed regions and/or added a new witness
// requirement
apply_result apply_premise(semantic_state& state, premise_id prm_id,
                           const semantic_space& sem_space) {
  assert(prm_id.id < MAX_PREMISES);
  // forbidden mask: regions that would violate this premise (must be removed)
  const region_mask& forb_mask =
      sem_space.forbidden_regions_by_statement[prm_id.id];

  // One pass over the active mask words computes:
  // - new_allowed = old_allowed \ forb_mask
  // - any_bit_set: fast check for “did we eliminate all regions?”
  // - allowed_change: whether this premise actually narrowed the allowed set
  region_mask new_allowed{};
  bool any_bit_set = false;
  bool allowed_change = false;

  for (uint8_t index = 0; index < sem_space.active_words; index++) {
    const word_t old_word = state.allowed.w[index];
    const word_t new_word = old_word & ~forb_mask.w[index];
    new_allowed.w[index] = new_word;
    any_bit_set |= (new_word != 0);
    allowed_change |= (new_word != old_word);
  }
  if (!any_bit_set) {
    return apply_result::inconsistent;
  }

  // Some premises (I/O-style) also require existence of at least one witness
  // region inside a specific subset. This subset is precomputed as
  // required_nonempty_by_statement[premise].
  //
  // If this requirement exists, we record it by setting a bit in
  // state.req_bits. That bitset is canonical (order-independent) and avoids
  // vector insertion/sorting in the hot path.
  bool req_added = false;
  const region_mask& req_mask =
      sem_space.required_nonempty_by_statement[prm_id.id];

  // Detect whether req_mask is nonzero (i.e., this premise contributes a
  // witness requirement). Scan only active_words because the remaining words
  // are unused for this term_count.
  bool has_req = false;
  for (uint8_t index = 0; index < sem_space.active_words; index++) {
    if (req_mask.w[index] != 0) {
      has_req = true;
      break;
    }
  }

  if (has_req) {
    // Map premise_id.id to a bit position in req_bits.
    // With MAX_PREMISES <= 64, this is a single uint64_t word (word == 0).
    const uint16_t id = prm_id.id;
    const uint16_t word = id >> 6;
    const uint16_t bit = id & 63;
    const uint64_t old = state.req_bits[word];
    state.req_bits[word] |= (1ull << bit);
    req_added = (state.req_bits[word] != old);
  }

  // For consistency checks, use the narrowed mask if it changed, otherwise
  // reuse current allowed. This avoids copying new_allowed into state.allowed
  // on branches that end up inconsistent anyway.
  const region_mask& allowed_view =
      allowed_change ? new_allowed : state.allowed;

  // Only re-check witness requirements if something could have affected
  // satisfiability:
  // - allowed_change might remove the last possible witness for an existing
  // requirement
  // - req_added introduces a new requirement that must be satisfied immediately
  if (allowed_change || req_added) {
    uint64_t bits = state.req_bits[0];
    for (uint16_t id = 0; id < MAX_PREMISES; ++id) {
      if ((bits >> id) & 1ull) {
        if (intersects_empty(allowed_view,
                             sem_space.required_nonempty_by_statement[id],
                             sem_space.active_words)) {
          return apply_result::inconsistent;
        }
      }
    }
  }

  if (allowed_change) {
    state.allowed = new_allowed;
  }

  // Report whether the state changed in a way that matters for
  // search/memoization.
  return (allowed_change || req_added) ? apply_result::changed
                                       : apply_result::no_change;
}

// Return true if the semantic_state is impossible (should be pruned).
//
// The state can become inconsistent in two ways:
//
// 1) allowed is empty:
//    - state.allowed is a bitset of regions (candidate worlds) still possible.
//    - If it has no 1-bits, there are zero candidate worlds left.
//
// 2) some “must have a witness” requirement is impossible:
//    - For I/O-style premises, semantic_space precomputes a required_nonempty
//    mask
//      (regions where a witness is allowed to exist).
//    - state.req_bits tracks which of those requirements are currently active
//    in this state.
//    - A requirement is violated if:
//         allowed ∩ required_mask == empty
//      meaning: there is no remaining region that could serve as the required
//      witness.
bool is_inconsistent(const semantic_state& state,
                     const semantic_space& sem_space) {
  // Fast check: does state.allowed contain ANY allowed region bit?
  // We scan word-by-word because region_mask is stored as uint64_t chunks.
  bool any_allowed = false;
  for (uint8_t i = 0; i < sem_space.active_words; ++i) {
    if (state.allowed.w[i] != 0) {
      any_allowed = true;
      break;
    }
  }
  if (!any_allowed) return true;  // no candidate worlds remain

  // Check all active “nonempty” requirements (I/O constraints).
  // req_bits is a compact set over premise IDs (MAX_PREMISES <= 64 here).
  uint64_t bits = state.req_bits[0];
  for (uint16_t id = 0; id < MAX_PREMISES; ++id) {
    if ((bits >> id) & 1ull) {
      if (intersects_empty(state.allowed,
                           sem_space.required_nonempty_by_statement[id],
                           sem_space.active_words)) {
        return true;  // this existence constraint can no longer be satisfied
      }
    }
  }
  return false;  // at least one world remains and all existence constraints are
                 // satisfiable
}

// Return true if the current semantic_state *forces* the conclusion to be true.
//
// Representation recap:
// - state.allowed: bitset of regions (candidate worlds) still possible.
// - state.req_bits: bitset over premise IDs with active I/O-style “witness must
// exist” constraints.
// - semantic_space provides, per statement:
//   * forbidden_regions_by_statement[s]: regions where the statement is false
//   (A/E-style)
//   * required_nonempty_by_statement[s]: regions where a witness must exist
//   (I/O-style)
//
// Entailment strategy:
// - For A/E-style conclusions: entailed if no remaining allowed region
// falsifies it.
// - For I/O-style conclusions: entailed if some already-required witness is
// *forced* to lie
//   within the conclusion’s witness region set, given current pruning.
bool entails(const semantic_state& state, statement_id conclusion_id,
             const semantic_space& sem_space) {
  if (is_inconsistent(state, sem_space)) {
    return false;
  }

  const region_mask& forb_conc =
      sem_space.forbidden_regions_by_statement[conclusion_id.id];
  const region_mask& rec_constraint =
      sem_space.required_nonempty_by_statement[conclusion_id.id];

  // Case 1: Universal-style conclusion (A/E).
  //
  // If allowed ∩ forbidden == empty, then none of the remaining candidate
  // worlds violate the conclusion → conclusion is forced true.
  if (!is_empty(forb_conc, sem_space.active_words)) {
    return (intersects_empty(state.allowed, forb_conc, sem_space.active_words));
  }

  // Case 2: Existential-style conclusion (I/O).
  //
  // Having some allowed regions is not enough; we need that an existence
  // requirement already present in the state is “pinned down” into the
  // conclusion’s witness set.
  //
  // Iterate each active witness requirement (bitset over premise IDs).
  else if (!is_empty(rec_constraint, sem_space.active_words)) {
    const uint64_t bits = state.req_bits[0];
    for (uint16_t id = 0; id < MAX_PREMISES; ++id) {
      if (((bits >> id) & 1ull) == 0) continue;

      // For this requirement id, let req_mask be the set of regions where its
      // witness may live.
      // possible = allowed ∩ req_mask narrows that to witnesses still possible
      // after pruning.
      //
      // We want to know whether:
      // - possible is nonempty (the requirement is still satisfiable), AND
      // - possible ⊆ rec_constraint (every still-possible witness also
      // satisfies the conclusion),
      //   which means the conclusion is forced by this requirement.
      bool any = false;       // becomes true if possible has any 1-bit
      bool subset_ok = true;  // stays true only if possible has no bits outside
                              // rec_constraint
      for (uint8_t w = 0; w < sem_space.active_words; ++w) {
        const word_t p = state.allowed.w[w] &
                         sem_space.required_nonempty_by_statement[id].w[w];
        any |= (p != 0);
        subset_ok &= ((p & ~rec_constraint.w[w]) == 0);
      }

      // If some witness is still possible AND all possible witnesses lie inside
      // the conclusion’s witness set, then the conclusion must be true.
      if (any && subset_ok) return true;
    }
    return false;
  }
  // If neither encoding is present, this statement form isn’t represented here.
  else {
    return false;
  }
}
}  // namespace conclusion_explorer