#include "prune_rules.hpp"

#include <assert.h>

#include <algorithm>
#include <optional>

#include "profiler.hpp"
#include "semantic_state.hpp"
#include "syntax.hpp"

namespace conclusion_explorer {


static inline bool test_and_set(class_bitset& bits, uint16_t cid){
  uint64_t& word = bits[cid >> 6];
  const uint64_t mask = 1ull << (cid & 63);
  const bool was_set = (word & mask) != 0;
  word |= mask;
  return was_set;
}



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



static void or_region_mask(region_mask& dst, const region_mask& src, const semantic_space& sem_space)
{
  for(uint8_t word = 0; word < sem_space.active_words; word++){
    dst.w[word] |= src.w[word];
  }
}

static void or_req_bits(std::array<std::uint64_t, MAX_REQ_WORDS>& dst, const std::array<std::uint64_t, MAX_REQ_WORDS>& src, const semantic_space& sem_space)
{
  for(uint8_t word = 0; word < sem_space.req_words; word++){
    dst[word] |= src[word];
  }
}

static void build_premise_aggregate(const premise_path& path, const semantic_space& sem_space, premise_aggregate& aggregate){
  const size_t n = path.size();
  aggregate.empty_prefix.resize(n+1);
  aggregate.empty_suffix.resize(n+1);
  aggregate.req_prefix.resize(n+1);
  aggregate.req_suffix.resize(n+1);

  aggregate.empty_prefix[0] = region_mask{};
  aggregate.req_prefix[0] = {};
  aggregate.empty_suffix[n] = region_mask{};
  aggregate.req_suffix[n] = {};

  //prefix
  for(size_t index = 0; index < n; index++){
    aggregate.empty_prefix[index+1] = aggregate.empty_prefix[index];
    or_region_mask(aggregate.empty_prefix[index+1], sem_space.forbid_mask_by_class_id[path[index].id], sem_space);

    aggregate.req_prefix[index+1] = aggregate.req_prefix[index];
    const int16_t req_id = sem_space.req_index_by_class_id[path[index].id];
    if(req_id >= 0)
    {
      const uint16_t word = static_cast<uint16_t>(req_id) >> 6;
      const uint16_t bit = static_cast<uint16_t>(req_id) & 63;
      aggregate.req_prefix[index+1][word] |= (1ull << bit);
    }
   }

   //suffix
   for(size_t index = n; index-- > 0;)
   {
    aggregate.empty_suffix[index] = aggregate.empty_suffix[index+1];
    or_region_mask(aggregate.empty_suffix[index], sem_space.forbid_mask_by_class_id[path[index].id], sem_space);

    aggregate.req_suffix[index] = aggregate.req_suffix[index+1];
    const int16_t req_id = sem_space.req_index_by_class_id[path[index].id];
    if(req_id >= 0)
    {
      const uint16_t word = static_cast<uint16_t>(req_id)>> 6;
      const uint16_t bit = static_cast<uint16_t>(req_id) & 63;
      aggregate.req_suffix[index][word] |= (1ull << bit);
    }
   }
}

static bool requires_all_premises(const leaf_ctx& ctx, const premise_aggregate& aggregate, class_id conc)
{
  const size_t n = ctx.path.size();
  for(size_t index =0; index < n; index++){
    semantic_state state{};
    state.empty = aggregate.empty_prefix[index];
    or_region_mask(state.empty, aggregate.empty_suffix[index+1], ctx.sem_space);
    state.req_bits = aggregate.req_prefix[index];
    or_req_bits(state.req_bits, aggregate.req_suffix[index+1], ctx.sem_space);

    if(entails(state, conc, ctx.sem_space, ctx.prof))
    {
      return false;
    }
  }
  return true;
}


static inline bool is_present(const class_bitset& bits, uint16_t cid) {
  return (bits[cid >> 6] & (1ull << (cid & 63))) != 0;
}

static region_mask compute_allowed(const semantic_state& state, const semantic_space& sem_space){
  region_mask allowed{};
  for(uint8_t word = 0; word < sem_space.active_words; word++)
  {
    allowed.w[word] = (~state.empty.w[word]) & sem_space.all_regions.w[word];
  }
  return allowed;
}

static bool entails_forbid_from_allowed(const region_mask& allowed, const region_mask& forb_mask, const semantic_space& sem_space){
  for(uint8_t word = 0; word < sem_space.active_words; word++){
    if((allowed.w[word] & forb_mask.w[word]) != 0)
    {
      return false;
    }
  }
  return true;
}


static void collect_entailed_req_conclusions(
    const semantic_state& state, const region_mask& allowed,
    const semantic_space& sem_space,
    std::unordered_map<region_mask, std::vector<class_id>, region_mask_hash>&
        cache,
    std::vector<class_id>& out) {
  out.clear();
  class_bitset entailed_bits{};

  for (uint16_t word_index = 0; word_index < sem_space.req_words; ++word_index) {
    for (uint64_t bits = state.req_bits[word_index]; bits; bits &= (bits - 1)) {
      const uint16_t req_index =
          static_cast<uint16_t>(word_index * 64 + __builtin_ctzll(bits));
      if (req_index >= sem_space.req_mask_by_req_index.size()) {
        continue;
      }

      const region_mask& req_mask =
          sem_space.req_mask_by_req_index[req_index];

      region_mask s{};
      for (uint8_t w = 0; w < sem_space.active_words; ++w) {
        s.w[w] = allowed.w[w] & req_mask.w[w];
      }
      if (region_is_empty(s, sem_space.active_words)) {
        continue;
      }

      auto it = cache.find(s);
      if (it == cache.end()) {
        std::vector<class_id> list;
        list.reserve(sem_space.req_conc_cids.size());
        for (class_id cid : sem_space.req_conc_cids) {
          const int16_t conc_req_id = sem_space.req_index_by_class_id[cid.id];
          const region_mask& conc_mask =
              sem_space.req_mask_by_req_index[static_cast<size_t>(conc_req_id)];

          bool subset_ok = true;
          for (uint8_t w = 0; w < sem_space.active_words; ++w) {
            if ((s.w[w] & ~conc_mask.w[w]) != 0) {
              subset_ok = false;
              break;
            }
          }
          if (subset_ok) {
            list.push_back(cid);
          }
        }
        it = cache.emplace(s, std::move(list)).first;
      }

      for (class_id cid : it->second) {
        if (!test_and_set(entailed_bits, cid.id)) {
          out.push_back(cid);
        }
      }
    }
  }
}



std::optional<class_id> prune_rules::unique_interesting_conclusion(
    const semantic_state& state, const leaf_ctx& ctx) const {
  int candidate_count = 0;
  class_id unique_conc{};

  const region_mask allowed = compute_allowed(state, ctx.sem_space);

  bool have_aggregate = false;
  auto ensure_aggregate = [&]() -> const premise_aggregate& {
    if(!have_aggregate){
      build_premise_aggregate(ctx.path, ctx.sem_space, aggregate_scratch);
      have_aggregate = true;
    }
    return aggregate_scratch;
  };

  entailed_req_cids_scratch.clear();
  collect_entailed_req_conclusions(state, allowed, ctx.sem_space,
                                   req_superset_cache, entailed_req_cids_scratch);


  for (class_id cid : ctx.sem_space.forbid_cids) {
    if (is_present(ctx.present_bits, cid.id)) {
      ctx.prof.leaf_prune_present.fetch_add(1, std::memory_order_relaxed);
      continue;
    }

    if(!entails_forbid_from_allowed(allowed, ctx.sem_space.forbid_mask_by_class_id[cid.id], ctx.sem_space)){
      continue;
    }

    if (!requires_all_premises(ctx, ensure_aggregate(), cid)) {
      ctx.prof.leaf_prune_requires_all_failed.fetch_add(1, std::memory_order_relaxed);
      continue;
    }
    unique_conc = cid;
    if (++candidate_count > 1) {
      ctx.prof.leaf_prune_too_many_conclusions.fetch_add(1, std::memory_order_relaxed);
      return std::nullopt;
    }
  }

  for (class_id cid : entailed_req_cids_scratch)
  {
    if (is_present(ctx.present_bits, cid.id)) {
      ctx.prof.leaf_prune_present.fetch_add(1, std::memory_order_relaxed);
      continue;
    }

    if(!requires_all_premises(ctx, ensure_aggregate(), cid)){
      ctx.prof.leaf_prune_requires_all_failed.fetch_add(1, std::memory_order_relaxed);
      continue;
    }

    unique_conc = cid;
    if(++candidate_count > 1){
      ctx.prof.leaf_prune_too_many_conclusions.fetch_add(1, std::memory_order_relaxed);
      return std::nullopt;
    }
  }
  return (candidate_count == 1) ? std::optional<class_id>{unique_conc}
                                : std::nullopt;
}

bool prune_rules::is_banned_with_path(class_id cid, const premise_path& path) const {
  // TODO : Implement this if any new bans found
  return false;
}

bool prune_rules::should_expand(const semantic_state& state,
                                uint16_t next_min_id, uint8_t depth_left, const semantic_space& sem_space) const {
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