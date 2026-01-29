#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "common_types.hpp"

namespace conclusion_explorer {

struct syntax_space;

using word_t = std::uint64_t;

struct region_mask {
  std::array<word_t, MASK_WORDS> w{};
  bool operator==(const region_mask&) const = default;
};

enum class constraint_kind : std::uint8_t { require = 0, forbid = 1 };

struct semantic_space {
  std::uint16_t region_count;
  region_mask all_regions;
  std::uint8_t active_words;

  std::array<region_mask, MAX_TERMS> regions_where_term_true;
  std::array<region_mask, MAX_TERMS> regions_where_term_false;

  std::vector<region_mask> forbid_mask_by_class_id;
  std::vector<std::int16_t> req_index_by_class_id;
  std::vector<constraint_kind> kind_by_class_id;

  std::vector<class_id> forbid_cids;
  std::vector<class_id> req_conc_cids;
  
  std::vector<region_mask> req_mask_by_req_index;
  std::uint8_t req_words;
  std::uint16_t req_count;

  std::vector<std::uint16_t> permuted_req_index_by_perm_and_req;
  std::vector<std::uint16_t> permuted_region_index_by_perm_and_region;
  std::uint16_t permuted_req(std::size_t perm_i, std::uint16_t req) const;
  std::uint16_t permuted_region(std::size_t perm_i, std::uint16_t region) const;

};

bool region_is_empty(const region_mask&, std::uint8_t active_words);

semantic_space build_semantic_space(const syntax_space&);
}  // namespace conclusion_explorer