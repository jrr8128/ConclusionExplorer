#pragma once
#include <array>
#include <cassert>
#include <cstddef>
#include <unordered_map>
#include <vector>

#include "common_types.hpp"

namespace conclusion_explorer {
struct term_perm {
  std::array<std::uint8_t, MAX_TERMS> p{};
};

struct word_mask {
  std::uint8_t word = 0;
  std::uint64_t mask = 0;
};

struct syntax_space {
  std::vector<term_perm> perms;
  std::vector<class_id> permuted_cid_by_perm_and_cid;

  class_id permuted_cid(std::size_t perm_i, class_id cid) const {
    return permuted_cid_by_perm_and_cid[perm_i * class_count() + cid.id];
  }

  std::vector<word_mask> permuted_word_mask;

  std::uint8_t term_count;

  all_statements all_statements;
  equiv_class_id_by_statement_id class_id_by_statement_id;
  std::vector<representative_id> rep_id_by_class_id;
  std::vector<statement> rep_statement_by_class_id;
  std::unordered_map<statement, class_id, statement_hash> class_id_by_rep_stmt;

  statement_id rep_statement_id(class_id cid) const {
    assert(cid.id != 65535 && cid.id < rep_id_by_class_id.size());
    return rep_id_by_class_id[cid.id];
  }
  class_id equiv_class_of(statement_id sid) const {
    assert(sid.id != 65535 && sid.id < class_id_by_statement_id.size());
    return class_id_by_statement_id[sid.id];
  }
  statement canonical_equiv_statement(const statement& stmt) const;
  size_t class_count() const { return rep_id_by_class_id.size(); }

  word_mask perm_word_mask(std::size_t perm_i, class_id cid) const {
    const std::size_t class_count = rep_id_by_class_id.size();
    return permuted_word_mask[perm_i * class_count + cid.id];
  }
};

syntax_space build_syntax_space(std::uint8_t term_count);
}  // namespace conclusion_explorer