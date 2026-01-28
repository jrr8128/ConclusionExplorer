#include "canonicalizer.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>

#include "semantic.hpp"
#include "semantic_state.hpp"
#include "syntax.hpp"

namespace conclusion_explorer {

static void bitset_clear(premise_bitset_key& b) {
  for (std::size_t w = 0; w < b.size(); ++w) b[w] = 0;
}

static void bitset_set(premise_bitset_key& b, std::uint16_t cid) {
  b[cid >> 6] |= (1ull << (cid & 63));
}

static bool bitset_less(const premise_bitset_key& a,
                        const premise_bitset_key& b,
                        std::uint16_t class_words) {
  for (std::uint16_t i = 0; i < class_words; ++i) {
    if (a[i] != b[i]) return a[i] < b[i];
  }
  return false;
}

void canon_premise_set_bitset_key(premise_bitset_key& out_best,
                                  const std::vector<class_id>& premises,
                                  const syntax_space& syn,
                                  std::uint16_t class_words,
                                  premise_bitset_key& tmp) {
  bool have_best = false;
  bitset_clear(out_best);

  for (std::size_t perm_i = 0; perm_i < syn.perms.size(); ++perm_i) {
    bitset_clear(tmp);

    for (class_id cid : premises) {
      const word_mask wm = syn.perm_word_mask(perm_i, cid);
      tmp[wm.word] |= wm.mask;
    }

    if (!have_best || bitset_less(tmp, out_best, class_words)) {
      out_best = tmp;
      have_best = true;
    }
  }
}

static bool key_less(const canonical_key& a_key, const canonical_key& b_key,
                     const semantic_space& sem_space) {
  if (a_key.base_terms_mask != b_key.base_terms_mask)
    return a_key.base_terms_mask < b_key.base_terms_mask;

  for (std::uint8_t w = 0; w < sem_space.active_words; ++w) {
    if (a_key.empty.w[w] != b_key.empty.w[w])
      return a_key.empty.w[w] < b_key.empty.w[w];
  }
  for (std::uint8_t w = 0; w < sem_space.req_words; ++w) {
    if (a_key.req_bits[w] != b_key.req_bits[w])
      return a_key.req_bits[w] < b_key.req_bits[w];
  }
  return false;
}

static canonical_key pack_key(const semantic_state& state,
                              const semantic_space& sem_space) {
  canonical_key k{};
  for (std::uint8_t w = 0; w < sem_space.active_words; ++w)
    k.empty.w[w] = state.empty.w[w] & sem_space.all_regions.w[w];
  for (std::uint8_t w = 0; w < sem_space.req_words; ++w)
    k.req_bits[w] = state.req_bits[w];
  // zero tail (safety)
  const std::uint16_t req_count = sem_space.req_count;
  const std::uint16_t tail = req_count & 63;
  if (tail && sem_space.req_words) {
    k.req_bits[sem_space.req_words - 1] &= ((1ULL << tail) - 1ULL);
  }
  k.base_terms_mask = state.base_terms_mask;
  return k;
}

canonical_key canon::make_iso_key(const semantic_state& state,
                                  const syntax_space& syn_space,
                                  const semantic_space& sem_space) const {
  // start with identity perm as current best (assumes perms[0] is identity)
  canonical_key best = pack_key(state, sem_space);
  for (std::size_t perm_i = 1; perm_i < syn_space.perms.size(); ++perm_i) {
    const semantic_state pst =
        permuted_state(state, perm_i, syn_space, sem_space);
    const canonical_key k = pack_key(pst, sem_space);
    if (key_less(k, best, sem_space)) {
      best = k;
    }
  }
  return best;
}

static bool lex_less(const std::vector<std::uint16_t>& a,
                     const std::vector<std::uint16_t>& b) {
  return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
}

void canon_premise_set_key(cid_list_key& out,
                           const std::vector<class_id>& premises,
                           const syntax_space& syn_space,
                           std::vector<std::uint16_t>& tmp_ids) {
  out.ids.clear();
  bool have_best = false;
  for (std::size_t perm_i = 0; perm_i < syn_space.perms.size(); ++perm_i) {
    tmp_ids.clear();
    for (const class_id cid : premises) {
      const class_id mapped = syn_space.permuted_cid(perm_i, cid);
      tmp_ids.push_back(mapped.id);
    }
    std::sort(tmp_ids.begin(), tmp_ids.end());

    if (!have_best || lex_less(tmp_ids, out.ids)) {
      out.ids.swap(tmp_ids);
      have_best = true;
    }
  }
}

void canon_recipe_key(cid_list_key& out, const class_id conclusion,
                      const std::vector<class_id>& premises,
                      const syntax_space& syn_space,
                      std::vector<std::uint16_t>& tmp_ids) {
  out.ids.clear();
  bool have_best = false;

  for (std::size_t perm_i = 0; perm_i < syn_space.perms.size(); ++perm_i) {
    tmp_ids.clear();

    const class_id conc_mapped = syn_space.permuted_cid(perm_i, conclusion);
    tmp_ids.push_back(conc_mapped.id);

    const std::size_t premise_begin = tmp_ids.size();
    for (const class_id cid : premises) {
      const class_id mapped = syn_space.permuted_cid(perm_i, cid);
      tmp_ids.push_back(mapped.id);
    }
    std::sort(tmp_ids.begin() + static_cast<std::ptrdiff_t>(premise_begin),
              tmp_ids.end());

    if (!have_best || lex_less(tmp_ids, out.ids)) {
      out.ids.swap(tmp_ids);
      have_best = true;
    }
  }
}
}  // namespace conclusion_explorer