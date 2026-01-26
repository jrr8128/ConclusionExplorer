
#include "syntax.hpp"

#include <cassert>
#include <unordered_map>
#include <utility>

namespace conclusion_explorer {

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

static statement canonical_equiv_statement(const statement& stmt) {
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
  auto [it, inserted] = class_id_by_canon.emplace(
      canon, class_id{static_cast<std::uint16_t>(
                 syn_space.rep_id_by_class_id.size())});
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

            statement canon_statement = canonical_equiv_statement(stmt);

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

syntax_space build_syntax_space(const std::uint8_t term_count) {
  // DEBUG
  assert(term_count > 2 && term_count <= 8);
  syntax_space syn_space{};
  syn_space.term_count = term_count;
  build_statements(syn_space);
  build_rep_statement_by_class_id(syn_space);

  // DEBUG
  assert(syn_space.class_id_by_statement_id.size() ==
         syn_space.all_statements.size());
  assert(syn_space.rep_id_by_class_id.size() < 65535);
  assert(syn_space.rep_id_by_class_id.size() ==
         syn_space.rep_statement_by_class_id.size());

  return syn_space;
}
}  // namespace conclusion_explorer