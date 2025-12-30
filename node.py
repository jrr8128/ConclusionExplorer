

from dataclasses import dataclass


@dataclass
class Node:
    allowed_regions_mask: int
    existence_constraints_masks: tuple[int,...] # canonicalized after memo accepts
    depth: int # representative of # of premises
    last_index: int # index of last statement applied, expansion only considers statements with index > last_index
