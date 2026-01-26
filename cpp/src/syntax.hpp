#pragma once
#include <vector>

#include "common_types.hpp"

namespace conclusion_explorer {

struct syntax_space {
  std::uint8_t term_count;

  all_statements all_statements;

  equiv_class_id_by_statement_id class_id_by_statement_id;

  std::vector<representative_id> rep_id_by_class_id;
  std::vector<statement> rep_statement_by_class_id;

  statement_id rep_statement_id(class_id cid) const {
    assert(cid.id != 65535 && cid.id < rep_id_by_class_id.size());
    return rep_id_by_class_id[cid.id];
  }
  class_id equiv_class_of(statement_id sid) const {
    assert(sid.id != 65535 && sid.id < class_id_by_statement_id.size());
    return class_id_by_statement_id[sid.id];
  }

  size_t class_count() const { return rep_id_by_class_id.size(); }
};

syntax_space build_syntax_space(std::uint8_t term_count);
}  // namespace conclusion_explorer