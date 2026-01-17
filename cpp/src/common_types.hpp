#pragma once
#include <compare>
#include <cstdint>
#include <limits>
#include <vector>

namespace conclusion_explorer {
// A: All S are P, E: No S are P, I: Some S are P, O: Some S are not P
enum class form : std::uint8_t { A = 0, E = 1, I = 2, O = 3 };

// Mapping each letter to a value (0 -> A, 1 -> B...etc) (current planned max
// term count = 8) Then if is_complement = True then we take the complement of
// the term example 0,True -> ~A
struct term_literal {
  std::uint8_t term;
  bool is_complement;
};

// example  A, (A,0), (B,0) -> All A are B
struct statement {
  form f;
  term_literal subject;
  term_literal predicate;
};

// Strong typedef over uint16_t to prevent mixing IDs (statement vs premise vs
// class). Defines == and <=> so IDs work as keys in std::map/set and compare
// like the underlying integer. Default-initializes to invalid sentinel (max
// uint16_t = 65535) to catch “unset id” bugs in debug/logging.
struct statement_id {
  std::uint16_t id = std::numeric_limits<std::uint16_t>::max();
  friend constexpr bool operator==(statement_id,
                                   statement_id) noexcept = default;
  friend constexpr std::strong_ordering operator<=>(
      statement_id, statement_id) noexcept = default;
};

struct premise_id {
  std::uint16_t id = std::numeric_limits<std::uint16_t>::max();
  friend constexpr bool operator==(premise_id, premise_id) noexcept = default;
  friend constexpr std::strong_ordering operator<=>(
      premise_id, premise_id) noexcept = default;
};

struct class_id {
  std::uint16_t id = std::numeric_limits<std::uint16_t>::max();
  friend constexpr bool operator==(class_id, class_id) noexcept = default;
  friend constexpr std::strong_ordering operator<=>(
      class_id, class_id) noexcept = default;
};

// Need to be able to reference all statements
using all_statements = std::vector<statement>;

// But only search through tailored id's (avoiding redundant/degenerate/etc
// statements)
using search_statement_ids = std::vector<statement_id>;

// Some statements generated will be logically equivalent to others,
// put these into buckets/equivalence classes
using equiv_class_id_by_statement_id = std::vector<class_id>;

// Each bucket will have an ID associated with it that acts as the
// representative statement
using representative_id = statement_id;

}  // namespace conclusion_explorer