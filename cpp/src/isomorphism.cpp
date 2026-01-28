#include "isomorphism.hpp"

#include <algorithm>
#include <cassert>
#include <unordered_map>

#include "common_types.hpp"
#include "semantic.hpp"
#include "syntax.hpp"

namespace conclusion_explorer {

void precompute_build_perms(syntax_space& syn_space) {
  const std::uint8_t term_count = syn_space.term_count;

  std::vector<std::uint8_t> v;
  v.reserve(term_count);
  for (std::uint8_t index = 0; index < term_count; ++index) {
    v.push_back(index);
  }

  syn_space.perms.clear();
  do {
    term_perm tp{};
    for (std::uint8_t index = 0; index < term_count; ++index) {
      tp.p[index] = v[index];
    }
    syn_space.perms.push_back(tp);
  } while (std::next_permutation(v.begin(), v.end()));
}

static std::unordered_map<statement, class_id, statement_hash> build_cid_of_rep(
    const syntax_space& syn_space) {
  const std::uint16_t class_count =
      static_cast<std::uint16_t>(syn_space.class_count());

  std::unordered_map<statement, class_id, statement_hash> cid_of;
  cid_of.reserve(class_count);
  for (std::uint16_t cid = 0; cid < class_count; ++cid) {
    cid_of.emplace(syn_space.rep_statement_by_class_id[cid], class_id{cid});
  }
  return cid_of;
}

static term_literal perm_lit(const term_perm& perm, term_literal lit) {
  lit.term = perm.p[lit.term];
  return lit;
}

static statement perm_stmt(const term_perm& perm, statement stmt,
                           const syntax_space& syn_space) {
  stmt.subject = perm_lit(perm, stmt.subject);
  stmt.predicate = perm_lit(perm, stmt.predicate);
  return syn_space.canonical_equiv_statement(stmt);
}

void precompute_build_permuted_cid(syntax_space& syn_space) {
  const std::uint16_t class_count =
      static_cast<std::uint16_t>(syn_space.class_count());

  const std::unordered_map<statement, class_id, statement_hash> cid_of =
      build_cid_of_rep(syn_space);

  syn_space.permuted_cid_by_perm_and_cid.assign(
      static_cast<std::size_t>(syn_space.perms.size()) *
          static_cast<std::size_t>(class_count),
      class_id{0});

  for (std::uint16_t perm_index = 0; perm_index < syn_space.perms.size();
       ++perm_index) {
    const term_perm& perm = syn_space.perms[perm_index];

    for (std::uint16_t cid = 0; cid < class_count; ++cid) {
      const statement& rep_stmt = syn_space.rep_statement_by_class_id[cid];
      const statement mapped = perm_stmt(perm, rep_stmt, syn_space);

      const std::unordered_map<statement, class_id,
                               statement_hash>::const_iterator iter =
          cid_of.find(mapped);

      assert(iter != cid_of.end());
      syn_space.permuted_cid_by_perm_and_cid
          [static_cast<std::size_t>(perm_index) * class_count + cid] =
          iter->second;
    }
  }
}

static std::uint16_t permute_region_index(std::uint16_t old_region,
                                          const term_perm& perm,
                                          std::uint8_t term_count) {
  std::uint16_t output = 0;
  for (std::uint8_t term = 0; term < term_count; ++term) {
    if ((old_region >> term) & 1u) {
      output |= static_cast<std::uint16_t>(1u << perm.p[term]);
    }
  }
  return output;
}

void precompute_build_permuted_region_index(const syntax_space& syn_space,
                                            semantic_space& sem_space) {
  const std::size_t perms_size = syn_space.perms.size();
  const std::uint16_t region_count = sem_space.region_count;

  sem_space.permuted_region_index_by_perm_and_region.resize(perms_size *
                                                            region_count);

  for (std::size_t perm_i = 0; perm_i < perms_size; ++perm_i) {
    const term_perm& perm = syn_space.perms[perm_i];
    for (std::uint16_t region = 0; region < region_count; ++region) {
      const std::uint16_t region2 =
          permute_region_index(region, perm, syn_space.term_count);
      assert(region2 < region_count);
      sem_space.permuted_region_index_by_perm_and_region[perm_i * region_count +
                                                         region] = region2;
    }
  }
}

void precompute_build_permuted_req_index(const syntax_space& syn_space,
                                         semantic_space& sem_space) {
  const std::size_t perm_size = syn_space.perms.size();
  const std::uint16_t req_count = sem_space.req_count;

  sem_space.permuted_req_index_by_perm_and_req.assign(
      perm_size * req_count, std::numeric_limits<std::uint16_t>::max());

  if (req_count == 0) {
    return;
  }

  // Build req -> cid (representative) mapping
  std::vector<class_id> cid_by_req(req_count, class_id{65535});
  for (class_id cid{0}; cid.id < syn_space.class_count(); ++cid.id) {
    const int16_t req = sem_space.req_index_by_class_id[cid.id];
    if (req < 0) {
      continue;
    }

    const auto r = static_cast<std::uint16_t>(req);
    assert(r < req_count);
    assert(cid_by_req[r].id == 65535);  // must be unique
    cid_by_req[r] = cid;
  }

  for (std::size_t perm_i = 0; perm_i < perm_size; ++perm_i) {
    for (std::uint16_t req = 0; req < req_count; ++req) {
      const class_id cid = cid_by_req[req];
      assert(cid.id != 65535);

      const class_id cid2 = syn_space.permuted_cid(perm_i, cid);
      const int16_t req2 = sem_space.req_index_by_class_id[cid2.id];
      assert(req2 >= 0);

      sem_space.permuted_req_index_by_perm_and_req[perm_i * req_count + req] =
          static_cast<std::uint16_t>(req2);
    }
  }

  for (std::size_t i = 0;
       i < sem_space.permuted_req_index_by_perm_and_req.size(); ++i) {
    assert(sem_space.permuted_req_index_by_perm_and_req[i] !=
           std::numeric_limits<std::uint16_t>::max());
  }
}

void precompute_build_permuted_word_mask(syntax_space& syn_space) {
  const std::size_t class_count = syn_space.class_count();
  syn_space.permuted_word_mask.resize(syn_space.perms.size() * class_count);

  for (std::size_t perm_i = 0; perm_i < syn_space.perms.size(); ++perm_i) {
    for (std::size_t cid = 0; cid < class_count; ++cid) {
      const std::uint16_t mapped =
          syn_space
              .permuted_cid(perm_i, class_id{static_cast<std::uint16_t>(cid)})
              .id;

      syn_space.permuted_word_mask[perm_i * class_count + cid] = word_mask{
          static_cast<std::uint8_t>(mapped >> 6), (1ull << (mapped & 63))};
    }
  }
}

}  // namespace conclusion_explorer