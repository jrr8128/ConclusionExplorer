#include "semantic.hpp"

#include <assert.h>

#include "isomorphism.hpp"
#include "syntax.hpp"

namespace conclusion_explorer {

bool region_is_empty(const region_mask& mask, uint8_t active_words) {
  for (uint8_t index = 0; index < active_words; index++) {
    if (mask.w[index] != 0) {
      return false;
    }
  }
  return true;
}

static region_mask build_all_region_mask(const std::uint16_t region_count) {
  region_mask regions{};

  int full_words = region_count / 64;
  int tail_bits = region_count % 64;

  assert(full_words < MASK_WORDS ||
         (full_words == MASK_WORDS && tail_bits == 0));
  for (int i = 0; i < full_words; ++i) {
    regions.w[i] = ~0ULL;
  }
  if (tail_bits != 0) {
    regions.w[full_words] = (1ULL << tail_bits) - 1ULL;
  }
  return regions;
}

static void build_term_region_masks(const syntax_space& syn_space,
                                    semantic_space& sem_space) {
  for (std::uint8_t term_ind = 0; term_ind < syn_space.term_count; term_ind++) {
    region_mask& true_mask = sem_space.regions_where_term_true[term_ind];

    for (int i = 0; i < MASK_WORDS; i++) {
      true_mask.w[i] = 0;
    }

    for (std::uint16_t region_ind = 0; region_ind < sem_space.region_count;
         region_ind++) {
      if (((region_ind >> static_cast<unsigned>(term_ind)) & 1u) != 0) {
        std::uint16_t word = region_ind >> 6;  // region_ind / 64
        int bit = region_ind & 63;             // region_ind % 64
        // DEBUG
        assert(word < sem_space.active_words);
        true_mask.w[word] |= (1ULL << bit);
      }
    }
  }

  for (std::uint8_t term_ind = 0; term_ind < syn_space.term_count; term_ind++) {
    region_mask& false_mask = sem_space.regions_where_term_false[term_ind];
    region_mask& true_mask = sem_space.regions_where_term_true[term_ind];
    for (int active_ind = 0; active_ind < MASK_WORDS; active_ind++) {
      false_mask.w[active_ind] =
          (active_ind < sem_space.active_words)
              ? (sem_space.all_regions.w[active_ind] & ~true_mask.w[active_ind])
              : 0;
    }
  }
}

static const region_mask& true_mask_for(const term_literal literal,
                                        const semantic_space& sem_space) {
  return literal.is_complement
             ? sem_space.regions_where_term_false[literal.term]
             : sem_space.regions_where_term_true[literal.term];
}

static void build_forbid_by_class_id(const syntax_space& syn_space,
                                     semantic_space& sem_space) {
  sem_space.forbid_mask_by_class_id.assign(syn_space.class_count(),
                                           region_mask{});
  for (class_id cid{0}; cid.id < sem_space.forbid_mask_by_class_id.size();
       cid.id++) {
    const statement& stmt = syn_space.rep_statement_by_class_id[cid.id];
    if (stmt.f != form::E) {
      continue;
    }
    const region_mask& subject_mask = true_mask_for(stmt.subject, sem_space);
    const region_mask& predicate_mask =
        true_mask_for(stmt.predicate, sem_space);
    for (uint8_t word = 0; word < sem_space.active_words; word++) {
      sem_space.forbid_mask_by_class_id[cid.id].w[word] =
          subject_mask.w[word] & predicate_mask.w[word];
    }
  }
}

static void build_req_index_by_class_id(const syntax_space& syn_space,
                                        semantic_space& sem_space) {
  sem_space.req_index_by_class_id.assign(syn_space.class_count(), -1);
  sem_space.req_mask_by_req_index.clear();
  sem_space.req_mask_by_req_index.reserve(syn_space.class_count());
  std::uint16_t next = 0;
  for (class_id cid{0}; cid.id < sem_space.req_index_by_class_id.size();
       cid.id++) {
    const statement& stmt = syn_space.rep_statement_by_class_id[cid.id];
    if (stmt.f != form::I) {
      continue;
    }
    region_mask mask{};
    const region_mask& subject_mask = true_mask_for(stmt.subject, sem_space);
    const region_mask& predicate_mask =
        true_mask_for(stmt.predicate, sem_space);
    for (uint8_t word = 0; word < sem_space.active_words; word++) {
      mask.w[word] = subject_mask.w[word] & predicate_mask.w[word];
    }
    if (!region_is_empty(mask, sem_space.active_words)) {
      sem_space.req_index_by_class_id[cid.id] = next++;
      sem_space.req_mask_by_req_index.push_back(mask);
    }
  }
  sem_space.req_count = static_cast<std::uint16_t>(next);
  sem_space.req_words = static_cast<uint8_t>((next + 63) / 64);
}

static void build_constraint_kind_by_cid(const syntax_space& syn_space,
                                         semantic_space& sem_space) {
  sem_space.kind_by_class_id.resize(syn_space.class_count());
  for (size_t cid = 0; cid < syn_space.class_count(); cid++) {
    const form f = syn_space.rep_statement_by_class_id[cid].f;
    assert(f == form::E || f == form::I);
    sem_space.kind_by_class_id[cid] =
        (f == form::E) ? constraint_kind::forbid : constraint_kind::require;
  }
}

static void build_constraint_partitions(const syntax_space& syn_space, semantic_space& sem_space)
{
  sem_space.forbid_cids.clear();
  sem_space.req_conc_cids.clear();
  sem_space.forbid_cids.reserve(syn_space.class_count());
  sem_space.req_conc_cids.reserve(syn_space.class_count());

  for (class_id cid{0}; cid.id < syn_space.class_count(); cid.id++)
  {
    if(sem_space.kind_by_class_id[cid.id] == constraint_kind::forbid)
    {
      sem_space.forbid_cids.push_back(cid);
      continue;
    }
    if(sem_space.req_index_by_class_id[cid.id] >= 0)
    {
      sem_space.req_conc_cids.push_back(cid);
    }
  }
}

semantic_space build_semantic_space(const syntax_space& syn_space) {
  // DEBUG
  assert(syn_space.term_count <= MAX_TERMS);
  semantic_space sem_space{};
  sem_space.region_count =
      static_cast<std::uint16_t>(1u << syn_space.term_count);
  sem_space.all_regions = build_all_region_mask(sem_space.region_count);
  sem_space.active_words = (sem_space.region_count + 63) / 64;

  build_term_region_masks(syn_space, sem_space);
  build_forbid_by_class_id(syn_space, sem_space);
  build_req_index_by_class_id(syn_space, sem_space);
  build_constraint_kind_by_cid(syn_space, sem_space);
  build_constraint_partitions(syn_space, sem_space);

  precompute_build_permuted_region_index(syn_space, sem_space);
  precompute_build_permuted_req_index(syn_space, sem_space);

  // DEBUG
  assert(sem_space.forbid_mask_by_class_id.size() == syn_space.class_count());
  assert(sem_space.req_index_by_class_id.size() == syn_space.class_count());
  assert(sem_space.req_mask_by_req_index.size() <= syn_space.class_count());
  assert(!sem_space.permuted_region_index_by_perm_and_region.empty());
  assert(!sem_space.permuted_req_index_by_perm_and_req.empty());

  return sem_space;
}
}  // namespace conclusion_explorer