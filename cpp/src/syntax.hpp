#pragma once
#include <vector>

#include "common_types.hpp"

namespace conclusion_explorer {

struct syntax_space {
  std::uint8_t term_count;

  // Global catalog; statement_id is the index into this vector.
  all_statements all_statements;

  // Per-statement mapping to an equivalence class
  // (used to group/logically rewrite AEIO forms).
  equiv_class_id_by_statement_id class_id_by_statement_id;

  // Representative search list stores statement_ids so search can iterate one
  // statement per equivalence class (avoids redundant AEIO rewrites)
  // while still referring back to a concrete (form,S,P) stored in
  // all_statements.
  std::vector<representative_id> rep_id_by_class_id;
};

syntax_space build_syntax_space(std::uint8_t term_count);
}  // namespace conclusion_explorer