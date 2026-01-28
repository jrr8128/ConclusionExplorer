
#include "syntax.hpp"

#include <algorithm>
#include <cassert>
#include <unordered_map>
#include <utility>

#include "isomorphism.hpp"

namespace conclusion_explorer {

statement syntax_space::canonical_equiv_statement(const statement& stmt) const {
  statement canon_stmt;

  if (stmt.f == form::A || stmt.f == form::O) {
    term_literal not_predicate{stmt.predicate.term,
                               !stmt.predicate.is_complement};
    if (stmt.f == form::A) {
      canon_stmt.f = form::E;
    } else {  // stmt.f == form::O
      canon_stmt.f = form::I;
    }
    canon_stmt.subject = stmt.subject;
    canon_stmt.predicate = not_predicate;
  } else if (stmt.f == form::E || stmt.f == form::I) {
    canon_stmt = stmt;
  } else {
    assert(false);
  }

  if (canon_stmt.predicate < canon_stmt.subject) {
    std::swap(canon_stmt.subject, canon_stmt.predicate);
  }
  return canon_stmt;
}

static class_id get_or_create_class_id(
    const statement& canon,
    std::unordered_map<statement, class_id, statement_hash>& class_id_by_canon,
    syntax_space& syn_space, statement_id sid) {
  const uint16_t next_id =
      static_cast<std::uint16_t>(syn_space.rep_id_by_class_id.size());
  auto [it, inserted] = class_id_by_canon.emplace(canon, class_id{next_id});
  if (inserted) syn_space.rep_id_by_class_id.push_back(sid);
  return it->second;
}

static void build_statements(syntax_space& syn_space) {
  std::uint8_t term_count = syn_space.term_count;

  std::unordered_map<statement, class_id, statement_hash> class_id_by_canon;

  for (form form_type : {form::E, form::I, form::A, form::O}) {
    for (std::uint8_t subject_index = 0; subject_index < term_count;
         subject_index++) {
      for (bool subject_is_complement : {false, true}) {
        for (std::uint8_t predicate_index = 0; predicate_index < term_count;
             predicate_index++) {
          for (bool predicate_is_complement : {false, true}) {
            const term_literal subject_literal{subject_index,
                                               subject_is_complement};
            const term_literal predicate_literal{predicate_index,
                                                 predicate_is_complement};

            if (subject_literal.term == predicate_literal.term) {
              continue;
            }

            if ((form_type == form::E || form_type == form::I) &&
                (predicate_literal < subject_literal)) {
              continue;
            }

            statement stmt{form_type, subject_literal, predicate_literal};
            syn_space.all_statements.push_back(stmt);

            statement canon_statement =
                syn_space.canonical_equiv_statement(stmt);

            const statement_id sid{static_cast<std::uint16_t>(
                syn_space.all_statements.size() - 1)};

            const class_id cid = get_or_create_class_id(
                canon_statement, class_id_by_canon, syn_space, sid);

            syn_space.class_id_by_statement_id.push_back(cid);
          }
        }
      }
    }
  }
}

static void build_rep_statement_by_class_id(syntax_space& syn_space) {
  syn_space.rep_statement_by_class_id.clear();
  syn_space.rep_statement_by_class_id.reserve(
      syn_space.rep_id_by_class_id.size());
  for (class_id cid{0}; cid.id < syn_space.rep_id_by_class_id.size();
       cid.id++) {
    syn_space.rep_statement_by_class_id.push_back(
        syn_space.all_statements[syn_space.rep_id_by_class_id[cid.id].id]);
  }
}

static void build_class_id_by_rep_stmt(syntax_space& syn_space) {
  syn_space.class_id_by_rep_stmt.clear();
  syn_space.class_id_by_rep_stmt.reserve(
      syn_space.rep_statement_by_class_id.size());
  for (class_id cid{0}; cid.id < syn_space.rep_statement_by_class_id.size();
       cid.id++) {
    syn_space.class_id_by_rep_stmt.emplace(
        syn_space.rep_statement_by_class_id[cid.id], cid);
  }
}

syntax_space build_syntax_space(const std::uint8_t term_count) {
  // DEBUG
  assert(term_count > 2 && term_count <= 8);
  syntax_space syn_space{};
  syn_space.term_count = term_count;
  build_statements(syn_space);
  build_rep_statement_by_class_id(syn_space);
  build_class_id_by_rep_stmt(syn_space);
  precompute_build_perms(syn_space);
  precompute_build_permuted_cid(syn_space);
  precompute_build_permuted_word_mask(syn_space);

  // DEBUG
  assert(syn_space.class_id_by_statement_id.size() ==
         syn_space.all_statements.size());
  assert(syn_space.rep_id_by_class_id.size() < 65535);
  assert(syn_space.rep_id_by_class_id.size() ==
         syn_space.rep_statement_by_class_id.size());

  return syn_space;
}
}  // namespace conclusion_explorer