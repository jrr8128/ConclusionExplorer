#pragma once
#include <compare>
#include <cstdint>
#include <limits>
#include <vector>

namespace conclusion_explorer {
constexpr int MAX_TERMS = 8;
constexpr int MAX_REGIONS = 1 << MAX_TERMS;
constexpr int MASK_WORDS = (MAX_REGIONS + 63) / 64;

constexpr std::uint16_t MAX_REQS = 6 * MAX_TERMS * (MAX_TERMS - 1);
constexpr std::uint16_t MAX_REQ_WORDS = (MAX_REQS + 63) / 64;

constexpr std::uint16_t MAX_CLASSES =
    4 * MAX_TERMS * (MAX_TERMS - 1);                                // 224 ish
constexpr std::uint16_t MAX_CLASS_WORDS = (MAX_CLASSES + 63) / 64;  // 4

enum class form : std::uint8_t { A = 0, E = 1, I = 2, O = 3 };

struct term_literal {
  std::uint8_t term;
  bool is_complement;
  auto operator<=>(const term_literal&) const = default;
};

using term_index_t = std::uint8_t;
constexpr term_literal make_pos(term_index_t t) { return {t, false}; }
constexpr term_literal make_neg(term_index_t t) { return {t, true}; }

struct perm_id {
  std::uint16_t id = std::numeric_limits<std::uint16_t>::max();
  friend constexpr bool operator==(perm_id, perm_id) noexcept = default;
  friend constexpr std::strong_ordering operator<=>(perm_id,
                                                    perm_id) noexcept = default;
};

using term_count_t = std::uint8_t;

using premise_bitset_key = std::array<std::uint64_t, MAX_CLASS_WORDS>;

struct key_bytes {
  std::vector<std::uint64_t> words;  // lexicographic compare on words
  friend bool operator==(const key_bytes&, const key_bytes&) = default;
  friend std::strong_ordering operator<=>(const key_bytes&,
                                          const key_bytes&) = default;
};

struct cid_list_key {
  std::vector<std::uint16_t> ids;
  bool operator==(const cid_list_key&) const = default;
};

struct statement {
  form f;
  term_literal subject;
  term_literal predicate;
  friend bool operator==(const statement&, const statement&) = default;
};

struct statement_hash {
  std::size_t operator()(const statement& x) const noexcept {
    std::size_t h = static_cast<std::size_t>(x.f);
    h = h * 37 + x.subject.term;
    h = h * 37 + static_cast<std::size_t>(x.subject.is_complement);
    h = h * 37 + x.predicate.term;
    h = h * 37 + static_cast<std::size_t>(x.predicate.is_complement);
    return h;
  }
};

struct statement_id {
  std::uint16_t id = std::numeric_limits<std::uint16_t>::max();
  friend constexpr bool operator==(statement_id,
                                   statement_id) noexcept = default;
  friend constexpr std::strong_ordering operator<=>(
      statement_id, statement_id) noexcept = default;
};

struct class_id {
  std::uint16_t id = std::numeric_limits<std::uint16_t>::max();
  friend constexpr bool operator==(class_id, class_id) noexcept = default;
  friend constexpr std::strong_ordering operator<=>(
      class_id, class_id) noexcept = default;
};

using all_statements = std::vector<statement>;

using equiv_class_id_by_statement_id = std::vector<class_id>;

using representative_id = statement_id;

}  // namespace conclusion_explorer