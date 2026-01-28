#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "common_types.hpp"
#include "semantic.hpp"

namespace conclusion_explorer {
struct semantic_state;
struct syntax_space;

struct canonical_key {
  region_mask empty;
  std::array<uint64_t, MAX_REQ_WORDS> req_bits{};
  uint8_t base_terms_mask = 0;
  bool operator==(const canonical_key&) const = default;
};

struct canon {
  canonical_key make_iso_key(const semantic_state&, const syntax_space&,
                             const semantic_space&) const;
};

void canon_premise_set_key(cid_list_key&, const std::vector<class_id>& premises,
                           const syntax_space&,
                           std::vector<std::uint16_t>& tmp_ids);

void canon_recipe_key(cid_list_key& out, const class_id conclusion,
                      const std::vector<class_id>& premises,
                      const syntax_space&, std::vector<std::uint16_t>& tmp_ids);

void canon_premise_set_bitset_key(premise_bitset_key& out_best,
                                  const std::vector<class_id>& premises,
                                  const syntax_space&,
                                  std::uint16_t class_words,
                                  premise_bitset_key& tmp);
bool key_weaker_eq(const canonical_key& a, const canonical_key& b,
                   const semantic_space& sem);
}  // namespace conclusion_explorer