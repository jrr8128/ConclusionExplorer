#include "prune_rules.hpp"

#include <assert.h>

#include <algorithm>
#include <optional>

#include "profiler.hpp"
#include "puzzle_collector.hpp"
#include "semantic_state.hpp"
#include "syntax.hpp"

namespace conclusion_explorer {

static statement norm_ei(statement stmt) {
  if (stmt.predicate < stmt.subject) {
    std::swap(stmt.subject, stmt.predicate);
  }
  return stmt;
}

static void set_conflict(class_bitset& a_bits, uint16_t b_cid) {
  a_bits[b_cid >> 6] |= (1ull << (b_cid & 63));
}

static void add_conflict_if_found(
    uint16_t a_cid, const statement& stmt,
    const std::unordered_map<statement, class_id, statement_hash>& cid_of,
    std::vector<class_bitset>& conflict_bits_by_cid) {
  std::unordered_map<statement, class_id, statement_hash>::const_iterator iter =
      cid_of.find(stmt);
  if (iter == cid_of.end()) {
    return;
  }
  const uint16_t b_cid = iter->second.id;
  set_conflict(conflict_bits_by_cid[a_cid], b_cid);
  set_conflict(conflict_bits_by_cid[b_cid], a_cid);
}

prune_rules::prune_rules(uint8_t term_count, const syntax_space& syn_space,
                         const semantic_space& sem_space) {
  const uint16_t class_count = static_cast<uint16_t>(syn_space.class_count());
  goal_mask = static_cast<uint8_t>((1u << term_count) - 1u);
  base_terms_mask_by_cid.resize(class_count, 0);
  for (uint16_t cid = 0; cid < class_count; cid++) {
    const statement& stmt = syn_space.rep_statement_by_class_id[cid];
    base_terms_mask_by_cid[cid] = static_cast<uint8_t>(
        (1u << stmt.subject.term) | (1u << stmt.predicate.term));
  }

  suffix_union_mask.assign(class_count + 1, 0);
  for (int index = static_cast<int>(class_count) - 1; index >= 0; --index) {
    suffix_union_mask[index] =
        suffix_union_mask[index + 1] | base_terms_mask_by_cid[index];
  }

  conflict_bits_by_cid.assign(class_count, class_bitset{});

  std::unordered_map<statement, class_id, statement_hash> cid_of;
  cid_of.reserve(class_count);
  for (uint16_t cid = 0; cid < class_count; cid++) {
    cid_of.emplace(syn_space.rep_statement_by_class_id[cid], class_id{cid});
  }
  for (uint16_t cid = 0; cid < class_count; cid++) {
    const statement& stmt = syn_space.rep_statement_by_class_id[cid];
    statement pred = stmt;

    pred.f = (stmt.f == form::E) ? form::I : form::E;
    add_conflict_if_found(cid, norm_ei(pred), cid_of, conflict_bits_by_cid);

    pred = stmt;
    pred.predicate.is_complement = !pred.predicate.is_complement;
    add_conflict_if_found(cid, norm_ei(pred), cid_of, conflict_bits_by_cid);

    pred = stmt;
    pred.f = (stmt.f == form::E) ? form::I : form::E;
    pred.predicate.is_complement = !pred.predicate.is_complement;
    add_conflict_if_found(cid, norm_ei(pred), cid_of, conflict_bits_by_cid);

    // E(A,B) vs E(~A,B) (your “empty B / looks contradictory” taste-ban)
    pred = stmt;
    pred.subject.is_complement = !pred.subject.is_complement;
    add_conflict_if_found(cid, norm_ei(pred), cid_of, conflict_bits_by_cid);

    pred = stmt;
    pred.subject.is_complement = !pred.subject.is_complement;
    pred.predicate.is_complement = !pred.predicate.is_complement;
    add_conflict_if_found(cid, norm_ei(pred), cid_of, conflict_bits_by_cid);
  }
}

static bool entails_without_one(const premise_path& path, size_t skip_i,
                                class_id conc, const semantic_space& s,
                                profiler& prof) {
  semantic_state st{};
  for (size_t i = 0; i < path.size(); ++i)
    if (i != skip_i &&
        apply_premise(st, path[i], s) == apply_result::inconsistent)
      return false;
  return entails(st, conc, s, prof);
}

static bool subsumed_by_any_premise(const premise_path& path, class_id conc,
                                    const semantic_space& s, profiler& prof) {
  for (class_id p : path) {
    semantic_state st{};
    if (apply_premise(st, p, s) != apply_result::inconsistent &&
        entails(st, conc, s, prof))
      return true;
  }
  return false;
}

static bool requires_all_premises(const premise_path& path, class_id conc,
                                  const semantic_space& s, profiler& prof) {
  for (size_t i = 0; i < path.size(); i++) {
    if (entails_without_one(path, i, conc, s, prof)) {
      return false;
    }
  }
  return true;
}

static inline bool is_present(const class_bitset& bits, uint16_t cid) {
  return (bits[cid >> 6] & (1ull << (cid & 63))) != 0;
}

std::optional<class_id> prune_rules::unique_interesting_conclusion(
    const semantic_state& state, const premise_path& path,
    const class_bitset& present_bits, const semantic_space& sem_space,
    profiler& prof) const {
  int candidate_count = 0;
  class_id unique_conc{};
  for (class_id cid{0}; cid.id < sem_space.kind_by_class_id.size(); cid.id++) {
    if (is_present(present_bits, cid.id)) {
      prof.redunant.fetch_add(1, std::memory_order_relaxed);
      continue;
    }

    if (!entails(state, cid, sem_space, prof)) {
      continue;
    }

    if (subsumed_by_any_premise(path, cid, sem_space, prof)) {
      prof.subsumed.fetch_add(1, std::memory_order_relaxed);
      continue;
    }
    if (!requires_all_premises(path, cid, sem_space, prof)) {
      prof.partial.fetch_add(1, std::memory_order_relaxed);
      continue;
    }
    unique_conc = cid;
    if (++candidate_count > 1) {
      prof.toomanyconc.fetch_add(1, std::memory_order_relaxed);
      return std::nullopt;
    }
  }
  return (candidate_count == 1) ? std::optional<class_id>{unique_conc}
                                : std::nullopt;
}

bool prune_rules::is_banned_with_path(class_id cid,
                                      const premise_path& path) const {
  // TODO : Implement this if any new bans found
  return false;
}

bool prune_rules::should_expand(const semantic_state& state,
                                uint16_t next_min_id, uint8_t depth_left,
                                const semantic_space& sem_space) const {
  if (depth_left == 0) {
    return false;
  }

  assert(next_min_id <= suffix_union_mask.size() - 1);
  const uint8_t missing_term =
      static_cast<uint8_t>(goal_mask & ~state.base_terms_mask);
  if ((missing_term & suffix_union_mask[next_min_id]) != missing_term) {
    return false;
  }

  if (is_inconsistent(state, sem_space)) {
    return false;
  }

  return true;
}

}  // namespace conclusion_explorer