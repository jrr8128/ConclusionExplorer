#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "common_types.hpp"

namespace conclusion_explorer {
// Forward decl to keep semantics.hpp independent of syntax.hpp; included
// syntax.hpp in .cpp.
struct syntax_space;

// Compile-time ceiling on the number of base terms supported by this build.
// Region-space size grows as 2^term_count;
// 8 terms => 256 regions (4x u64 words).
constexpr int MAX_TERMS = 8;

// Upper bound on regions for MAX_TERMS; actual runs use
// region_count = 2^term_count from syntax_space.
constexpr int MAX_REGIONS = 1 << MAX_TERMS;

// Fixed block count for region bitsets. Using a fixed-size array avoids
// per-state heap allocations during search (critical for performance and cache
// locality).
constexpr int MASK_WORDS = (MAX_REGIONS + 63) / 64;

using word_t = std::uint64_t;

// Bitset over Venn regions. Bit i represents whether region i is included.
// Only the lowest region_count bits are meaningful for a given run; higher bits
// must remain 0.
struct region_mask {
  std::array<word_t, MASK_WORDS> w{};
};

// Precomputed lookup tables that map syntax (terms/statements) to fast
// region-mask effects. This is read-only during search; the search manipulates
// *states* using these tables.
struct semantic_space {
  // Actual number of regions for this run: 2^term_count (from syntax_space).
  std::uint16_t region_count;

  // Mask with the lowest region_count bits set; used to safely bound operations
  // to the active region space.
  region_mask all_regions;
  std::uint8_t active_words;

  // For each base term t, mask of regions where t holds (and where ¬t holds,
  // cached for convenience). Complement literals in statements derive from
  // these masks rather than being separate "terms".
  std::array<region_mask, MAX_TERMS> regions_where_term_true;
  std::array<region_mask, MAX_TERMS> regions_where_term_false;

  // Per statement_id (aligned with syntax_space statement indexing), precompute
  // the mask updates:
  // - A/E statements forbid certain regions (those regions must be empty).
  std::vector<region_mask> forbidden_regions_by_statement;

  // I/O statements require at least one element to exist in some allowed
  // region(s).
  // Representation here is a mask; search-state logic decides how constraints
  // are accumulated/checked.
  std::vector<region_mask> required_nonempty_by_statement;
};

region_mask build_all_region_mask(std::uint16_t region_count);
void build_term_region_masks(const syntax_space& syn_space,
                             semantic_space& sem_space);
void build_forbidden_by_statement(const syntax_space& syn_space,
                                  semantic_space& sem_space);
void build_nonempty_by_statement(const syntax_space& syn_space,
                                 semantic_space& sem_space);
const region_mask& true_mask_for(const term_literal lit,
                                 const semantic_space& s);
const region_mask& false_mask_for(const term_literal literal,
                                  const semantic_space& sem_space);

semantic_space build_semantic_space(const syntax_space& syn_space);
}  // namespace conclusion_explorer