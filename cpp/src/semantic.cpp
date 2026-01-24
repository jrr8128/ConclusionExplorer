#include "semantic.hpp"

#include <assert.h>

#include "syntax.hpp"

namespace conclusion_explorer {

bool is_empty(const region_mask& mask, uint8_t active_words) {
  for (uint8_t index = 0; index < active_words; index++) {
    if (mask.w[index] != 0) {
      return false;
    }
  }
  return true;
}

region_mask build_all_region_mask(const std::uint16_t region_count) {
  // Construct the “universe” mask for this run: bits [0, region_count) set,
  // others clear.
  //
  // Why this exists:
  // - Compile masks sized for MAX_TERMS (fixed MASK_WORDS) to avoid
  // allocations.
  // - But each run may use fewer terms, so only the first 2^term_count regions
  // are real.
  // - all_regions is the authoritative boundary that keeps computations
  // confined to
  //   the active region space (e.g., when forming complements -> must not turn
  //   on bits for non-existent regions).

  region_mask regions{};  // zero-initialize all words so unused words stay 0 by
                          // default

  // Split the “set the first region_count bits” problem into:
  // - full_words: whole 64-bit blocks to fill with 1s
  // - tail_bits: remaining low bits in the next block
  int full_words = region_count / 64;
  int tail_bits = region_count % 64;

  // Safety: if region_count corresponds to MAX_REGIONS, full_words may equal
  // MASK_WORDS
  // and tail_bits must then be 0 (otherwise it'd write past the array).
  assert(full_words < MASK_WORDS ||
         (full_words == MASK_WORDS && tail_bits == 0));
  for (int i = 0; i < full_words; ++i) {
    regions.w[i] = ~0ULL;
  }
  if (tail_bits != 0) {
    regions.w[full_words] = (1ULL << tail_bits) - 1ULL;
  }
  return regions;
}

void build_term_region_masks(const syntax_space& syn_space,
                             semantic_space& sem_space) {
  // Model semantics as a finite “region space” (Venn-diagram cells):
  // each base term is either true or false, so there are 2^term_count regions.
  // A region is identified by an integer whose binary digits encode that truth
  // assignment.
  //
  // Use bitmasks over regions because “applying a premise” becomes fast boolean
  // algebra: intersections/emptiness checks reduce to word-wise AND/OR and a
  // few tests, which is much cheaper than iterating over explicit sets of
  // regions or objects during search.
  for (std::uint8_t term_ind = 0; term_ind < syn_space.term_count; term_ind++) {
    region_mask& true_mask = sem_space.regions_where_term_true[term_ind];

    // Store masks in fixed-size blocks (sized for MAX_TERMS) to avoid heap
    // allocation in hot paths. When term_count < MAX_TERMS, the “extra” high
    // words must remain 0; keeping that rule prevents accidental leakage of
    // irrelevant bits into later logic (and makes equality/hash stable if ever
    // compared to full masks).
    for (int i = 0; i < MASK_WORDS; i++) {
      true_mask.w[i] = 0;
    }

    // Build: regions_where_term_true[t] = { regions r where term t is true }.
    // Because region_ind’s bit t encodes the truth value of term t in that
    // region, membership is a single bit-test on region_ind.
    for (std::uint16_t region_ind = 0; region_ind < sem_space.region_count;
         region_ind++) {
      if (((region_ind >> static_cast<unsigned>(term_ind)) & 1u) != 0) {
        // Set the bit for this region in the mask. The mask is a bitset packed
        // into u64s:word selects which u64, bit selects which bit within that
        // u64.
        std::uint16_t word = region_ind >> 6;  // region_ind / 64
        int bit = region_ind & 63;             // region_ind % 64
        // DEBUG
        assert(word < sem_space.active_words);
        true_mask.w[word] |= (1ULL << bit);
      }
    }
  }

  // “False” masks are complements of the “true” masks, but only within the
  // active
  // region universe for this run. Using all_regions & ~true (instead of plain
  // ~true) ensures bits outside region_count never become set.
  for (std::uint8_t term_ind = 0; term_ind < syn_space.term_count; term_ind++) {
    region_mask& false_mask = sem_space.regions_where_term_false[term_ind];
    region_mask& true_mask = sem_space.regions_where_term_true[term_ind];
    for (int active_ind = 0; active_ind < MASK_WORDS; active_ind++) {
      false_mask.w[active_ind] =
          (active_ind < sem_space.active_words)
              ? (sem_space.all_regions.w[active_ind] & ~true_mask.w[active_ind])
              : 0;
    }
  }
}

const region_mask& true_mask_for(const term_literal literal,
                                 const semantic_space& sem_space) {
  return literal.is_complement
             ? sem_space.regions_where_term_false[literal.term]
             : sem_space.regions_where_term_true[literal.term];
}

const region_mask& false_mask_for(const term_literal literal,
                                  const semantic_space& sem_space) {
  return literal.is_complement
             ? sem_space.regions_where_term_true[literal.term]
             : sem_space.regions_where_term_false[literal.term];
}

void build_forbidden_by_statement(const syntax_space& syn_space,
                                  semantic_space& sem_space) {
  sem_space.forbidden_regions_by_statement.resize(
      syn_space.all_statements.size());
  for (size_t index = 0;
       index < sem_space.forbidden_regions_by_statement.size(); index++) {
    const statement& stmt = syn_space.all_statements[index];
    if (stmt.f != form::A && stmt.f != form::E) {
      continue;
    }

    const region_mask& subject_mask = true_mask_for(stmt.subject, sem_space);
    const region_mask& predicate_mask =
        (stmt.f == form::A) ? false_mask_for(stmt.predicate, sem_space)
                            : true_mask_for(stmt.predicate, sem_space);

    region_mask forbidden_mask{};
    for (int word_index = 0; word_index < sem_space.active_words;
         word_index++) {
      forbidden_mask.w[word_index] =
          subject_mask.w[word_index] & predicate_mask.w[word_index];
    }
    sem_space.forbidden_regions_by_statement[index] = forbidden_mask;
  }
}

void build_nonempty_by_statement(const syntax_space& syn_space,
                                 semantic_space& sem_space) {
  sem_space.required_nonempty_by_statement.resize(
      syn_space.all_statements.size());
  for (size_t index = 0;
       index < sem_space.required_nonempty_by_statement.size(); index++) {
    const statement& stmt = syn_space.all_statements[index];
    if (stmt.f != form::I && stmt.f != form::O) {
      continue;
    }

    const region_mask& subject_mask = true_mask_for(stmt.subject, sem_space);
    const region_mask& predicate_mask =
        (stmt.f == form::I) ? true_mask_for(stmt.predicate, sem_space)
                            : false_mask_for(stmt.predicate, sem_space);

    region_mask required_mask{};
    for (int word_index = 0; word_index < sem_space.active_words;
         word_index++) {
      required_mask.w[word_index] =
          subject_mask.w[word_index] & predicate_mask.w[word_index];
    }
    sem_space.required_nonempty_by_statement[index] = required_mask;
  }
}

semantic_space build_semantic_space(const syntax_space& syn_space) {
  // DEBUG
  assert(syn_space.term_count <= MAX_TERMS);
  semantic_space sem_space{};
  sem_space.region_count =
      static_cast<std::uint16_t>(1u << syn_space.term_count);
  sem_space.all_regions = build_all_region_mask(sem_space.region_count);
  sem_space.active_words = (sem_space.region_count + 63) / 64;

  build_term_region_masks(syn_space, sem_space);
  build_forbidden_by_statement(syn_space, sem_space);
  build_nonempty_by_statement(syn_space, sem_space);

  return sem_space;
}
}  // namespace conclusion_explorer