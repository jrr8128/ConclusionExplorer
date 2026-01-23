/*
Builds the AEIO syntax space for a fixed term_count.

Inputs:
- term_count in [3,8]. (2-term space is trivial; >8 is currently disallowed.)

Outputs populated in syntax_space:
-all_statements: global catalog; statement_id is an index. Used for
printing/debug, storing puzzles, and later translating a statement_id back into
(form, S, P) for entailment/presentation.

- class_id_by_statement_id: maps each statement to its equivalence class. Used
to (a) group logically-equivalent statements, (b) avoid duplicating work in
search by operating on class reps, and (c) later pick alternate surface forms in
the presentation layer.

- rep_id_by_class_id: canonical representative per equivalence class
(first-seen). Used as the search list (one per class) so the search iterates a
minimal set of statement IDs while still being able to recover all equivalent
variants via class_id_by_statement_id

Notes:
- Generation prunes remove reflexives and degenerate same-term pairs, and dedupe
E/I symmetric swaps.
- Equivalence classes use A≡E(S,¬P), O≡I(S,¬P), and E/I symmetry.
*/
#include "syntax.hpp"

#include <cassert>
#include <unordered_map>
#include <utility>

namespace conclusion_explorer {

// Hash functor for using `statement` as a key in std::unordered_map.
// unordered_map is a hash table: it uses hash(key) to choose a bucket, then
// uses == to confirm an exact match. For custom key types like `statement`, we
// must provide a hashing function.
//
// Why `operator()`?
// - In C++, a "functor" is an object that can be called like a function.
// - unordered_map expects a type `H` where `H{}(key)` returns std::size_t.
//   This call syntax is implemented by defining `operator()`.
//
// Why `const` on operator()?
// - The hasher object is typically stored as a const member inside
// unordered_map.
// - Marking it const means hashing does not mutate the hasher state.
//
// Why `noexcept`?
// - Hashing must not throw. Marking noexcept communicates that and can enable
//   small optimizations in the library.
//
// Why this rolling hash pattern (h = h*B + field)?
// - Cheap, deterministic way to combine multiple small fields into one
// std::size_t.
// - The multiply mixes earlier fields so order matters (subject vs predicate,
// etc.).
// - Collisions are acceptable: unordered_map will resolve them by checking
// `==`.
//
// Why base 37?
// - Any small odd constant works; choice isn’t tied to map size here (key space
// is tiny).
struct statement_hash {
  std::size_t operator()(const statement& x) const noexcept {
    // statement has fields form f, term_literal subject,predicate
    // Start from the form (A/E/I/O), then mix in subject and predicate fields.
    std::size_t h = static_cast<std::size_t>(x.f);
    h = h * 37 + x.subject.term;
    h = h * 37 + static_cast<std::size_t>(x.subject.is_complement);
    h = h * 37 + x.predicate.term;
    h = h * 37 + static_cast<std::size_t>(x.predicate.is_complement);
    return h;
  }
};

// Returns the canonical representative used as the *equivalence-class key* for
// `stmt`.
//
// Goal: map all logically-equivalent surface forms to a single, stable key so
// it can be assigned a class_id once and reuse it.
//
// Equivalence rules encoded here:
// - A(S,P) ≡ E(S, ¬P)   (rewrite A into E with complemented predicate)
// - O(S,P) ≡ I(S, ¬P)   (rewrite O into I with complemented predicate)
// - E and I are symmetric in their arguments: E(S,P) == E(P,S), I(S,P) ==
// I(P,S)
//
// Canonical form produced:
// - Always ends in form E or I (never A or O)
// - For E/I, (subject,predicate) are ordered so subject <= predicate under the
//   term_literal ordering (term index, then complement flag).
//
// Note: the returned statement is used only as a hash/map key; it is not
// necessarily one of the generated `all_statements`.
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

  // By previous if's Form is guaranteed to be E/I, symmetry -> sort literals
  if (canon_stmt.predicate < canon_stmt.subject) {
    std::swap(canon_stmt.subject, canon_stmt.predicate);
  }
  return canon_stmt;
}

// Look up (or create) the dense equivalence-class id for a canonicalized
// statement.
//
// Inputs:
// - canon: canonical equivalence-key (E/I-only + ordered literals)
// - class_id_by_canon: build-time map from canonical key -> class_id
// - syn_space.rep_id_by_class_id: used for allocating the next class_id (size
// == #classes)
// - sid: statement_id of the just-generated statement (candidate
// representative)
//
// Behavior:
// - If canon is new: assign class_id = current rep_id_by_class_id.size(), store
// mapping,
//   and record sid as the representative for that class.
// - If canon exists: reuse the existing class_id.
// - Returns the class_id for canon.
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

// Enumerate the full AEIO syntax space for the given term_count and populate:
// - all_statements: stable catalog (statement_id == index)
// - class_id_by_statement_id: statement_id -> equivalence class id
// - rep_id_by_class_id: one representative statement_id per equivalence class
//
// Design choices:
// - Generation order (form, S, ¬S, P, ¬P) defines stable statement_id ordering.
// - Prune statements that are useless/trivial for puzzles (same base term).
// - Also prune E/I symmetric duplicates during generation (only keep canonical
// S,P order).
// - Equivalence classes are assigned by hashing canonicalized statements (A/O
// rewritten to E/I,
//   plus E/I symmetry), so logically-equivalent surface forms share one
//   class_id.
static void build_statements(syntax_space& syn_space) {
  std::uint8_t term_count = syn_space.term_count;

  // Build-time map: canonical equivalence-key -> dense class_id.
  // Map not stored in syntax_space because it is only needed during
  // construction.
  std::unordered_map<statement, class_id, statement_hash> class_id_by_canon;

  // Iterate all AEIO forms in a fixed order for stable statement_id assignment.
  for (form form_type : {form::A, form::E, form::I, form::O}) {
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
            // Prune degenerate same-base-term pairs:
            // - reflexives: (S,S) in any form
            // - "self-complement" pairs: (S,¬S) (also trivial/undesirable for
            // puzzles)
            if (subject_literal.term == predicate_literal.term) {
              continue;
            }

            // E and I are symmetric in their arguments; keep only one of
            // (S,P)/(P,S). Enforce a canonical ordering on the literals and
            // skip the "second" occurrence.
            if ((form_type == form::E || form_type == form::I) &&
                (predicate_literal < subject_literal)) {
              continue;
            }

            statement stmt{form_type, subject_literal, predicate_literal};
            syn_space.all_statements.push_back(stmt);

            statement canon_statement = canonical_equiv_statement(stmt);

            // statement_id of the just-appended statement (index into
            // all_statements).
            const statement_id sid{static_cast<std::uint16_t>(
                syn_space.all_statements.size() - 1)};

            // Dense equivalence class assignment; first-seen statement becomes
            // the representative. Dense class ids: class ids are
            // 0..(num_classes-1) with no gaps.
            const class_id cid = get_or_create_class_id(
                canon_statement, class_id_by_canon, syn_space, sid);

            // Keep output vectors aligned: index i in class_id_by_statement_id
            // corresponds to statement_id i in all_statements.
            syn_space.class_id_by_statement_id.push_back(cid);
          }
        }
      }
    }
  }
}

syntax_space build_syntax_space(const std::uint8_t term_count) {
  // DEBUG
  assert(term_count > 2 && term_count <= 8);
  syntax_space syn_space{};
  syn_space.term_count = term_count;
  build_statements(syn_space);

  return syn_space;
}
}  // namespace conclusion_explorer