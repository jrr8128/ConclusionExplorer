

from dataclasses import dataclass


@dataclass
class Node:
    allowed_regions_mask: int
    existence_constraints_masks: tuple[int,...] # canonicalized after memo accepts
    depth: int # representative of # of premises
    last_index: int # index of last statement applied, expansion only considers statements with index > last_index
    used_base_terms_mask: int # for checking which base terms have been used
    pair_status_mask: int # for checking which pairs of terms have been used (a pair of terms should ideally only appear once in a list of premises)
    empty_base_terms_mask: int # checking if any term is empty (looking for puzzles where if there is an empty term then there will only be conclusions for empty terms)