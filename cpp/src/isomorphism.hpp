#pragma once

namespace conclusion_explorer {
struct syntax_space;
struct semantic_space;
void precompute_build_perms(syntax_space& syn_space);
void precompute_build_permuted_cid(syntax_space& syn_space);
void precompute_build_permuted_req_index(const syntax_space&, semantic_space&);
void precompute_build_permuted_region_index(const syntax_space&,
                                            semantic_space&);
void precompute_build_permuted_word_mask(syntax_space& syn_space);

}  // namespace conclusion_explorer