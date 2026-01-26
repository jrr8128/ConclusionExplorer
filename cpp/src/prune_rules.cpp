#include "prune_rules.hpp"

#include <assert.h>

#include <algorithm>
#include <optional>

#include "puzzle_collector.hpp"
#include "semantic_state.hpp"
#include "syntax.hpp"

namespace conclusion_explorer {

prune_rules::prune_rules(uint8_t term_count, const syntax_space& syn_space) {
  goal_mask = static_cast<uint8_t>((1u << term_count) - 1u);
  std::vector<uint8_t> base_mask(syn_space.class_count(), 0);
  for (uint16_t cid = 0; cid < syn_space.class_count(); cid++) {
    const statement& stmt = syn_space.rep_statement_by_class_id[cid];
    base_mask[cid] = static_cast<uint8_t>((1u << stmt.subject.term) |
                                          (1u << stmt.predicate.term));
  }

  suffix_union_mask.assign(syn_space.class_count() + 1, 0);
  for (int index = static_cast<int>(syn_space.class_count()) - 1; index >= 0;
       --index) {
    suffix_union_mask[index] = suffix_union_mask[index + 1] | base_mask[index];
  }
}

static bool entails_without_one(const premise_path& path, size_t skip_i,
                                class_id conc, const semantic_space& s) {
  semantic_state st{};
  for (size_t i = 0; i < path.size(); ++i)
    if (i != skip_i &&
        apply_premise(st, path[i], s) == apply_result::inconsistent)
      return false;
  return entails(st, conc, s);
}

static bool subsumed_by_any_premise(const premise_path& path, class_id conc,
                                    const semantic_space& s) {
  for (class_id p : path) {
    semantic_state st{};
    if (apply_premise(st, p, s) != apply_result::inconsistent &&
        entails(st, conc, s))
      return true;
  }
  return false;
}

static bool requires_all_premises(const premise_path& path, class_id conc,
                                  const semantic_space& s) {
  for (size_t i = 0; i < path.size(); i++) {
    if (entails_without_one(path, i, conc, s)) {
      return false;
    }
  }
  return true;
}

std::optional<class_id> prune_rules::unique_interesting_conclusion(
    const semantic_state& state, const premise_path& path,
    const semantic_space& sem_space) const {
  int candidate_count = 0;
  class_id unique_conc{};
  for (class_id cid{0}; cid.id < sem_space.kind_by_class_id.size(); cid.id++) {
    if (!entails(state, cid, sem_space)) {
      continue;
    }
    if (std::binary_search(path.begin(), path.end(), cid)) {
      continue;
    }
    if (subsumed_by_any_premise(path, cid, sem_space)) {
      continue;
    }
    if (!requires_all_premises(path, cid, sem_space)) {
      continue;
    }
    unique_conc = cid;
    if (++candidate_count > 1) {
      return std::nullopt;
    }
  }
  return (candidate_count == 1) ? std::optional<class_id>{unique_conc}
                                : std::nullopt;
}

bool prune_rules::should_expand(const semantic_state& state,
                                uint16_t next_min_id, uint8_t depth_left,
                                const semantic_space& sem_space) const {
  if (depth_left == 0) {
    return false;
  }

  if (is_inconsistent(state, sem_space)) {
    return false;
  }
  assert(next_min_id <= suffix_union_mask.size() - 1);
  const uint8_t missing_term =
      static_cast<uint8_t>(goal_mask & ~state.base_terms_mask);
  if ((missing_term & suffix_union_mask[next_min_id]) != missing_term) {
    return false;
  }
  return true;
}

}  // namespace conclusion_explorer