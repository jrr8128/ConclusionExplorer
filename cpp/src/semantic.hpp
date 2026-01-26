#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "common_types.hpp"

namespace conclusion_explorer {

struct syntax_space;

constexpr int MAX_TERMS = 8;
constexpr int MAX_REGIONS = 1 << MAX_TERMS;
constexpr int MASK_WORDS = (MAX_REGIONS + 63) / 64;

using word_t = std::uint64_t;

struct region_mask {
  std::array<word_t, MASK_WORDS> w{};
  bool operator==(const region_mask&) const = default;
};

enum class constraint_kind : uint8_t { require = 0, forbid = 1 };

struct semantic_space {
  std::uint16_t region_count;
  region_mask all_regions;
  std::uint8_t active_words;

  std::array<region_mask, MAX_TERMS> regions_where_term_true;
  std::array<region_mask, MAX_TERMS> regions_where_term_false;

  std::vector<region_mask> forbid_mask_by_class_id;
  std::vector<int16_t> req_index_by_class_id;
  std::vector<constraint_kind> kind_by_class_id;
  std::vector<region_mask> req_mask_by_req_index;
  uint8_t req_words;
};

bool is_empty(const region_mask&, uint8_t);

semantic_space build_semantic_space(const syntax_space&);
}  // namespace conclusion_explorer