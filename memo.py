# Module Responsibilities
#   Canonical semantic state identiy
#   Best-depth memoization
#   Recipe aggregation

# State Shape
#   allowed_regions_mask : int
#   existence_constraint_masks : tuple[int] (already canonical)
#   #Two states are equivalent if their canonical (allowed_regions_mask, existence_constraint_masks) match bitwise

# API
#   key(state) -> key
#   should_expand(key, depth) -> bool (best depth only)
#   mark_expanded(key, depth) -> None
#   attach_recipe(key, recipe_signature) -> None


from dataclasses import dataclass, field
from ConclusionExplorer.types import Recipes, RegionRemapsByUsedBaseTermsMask, State
from itertools import permutations
import time

def _region_remap_permutation(term_count: int, permutation: tuple[int,...]) -> list[int]:
    region_count = 1 << term_count
    remap = [0] * region_count
    for old_region in range(region_count):
        new_region = 0
        for new_bit_pos in range(term_count):
            bit = (old_region >> permutation[new_bit_pos]) & 1
            new_region |= (bit << new_bit_pos)
        remap[old_region] = new_region
    return remap

def _get_all_region_remaps(term_count: int) -> list[list[int]]:
    return [_region_remap_permutation(term_count, perm) for perm in permutations(range(term_count))]

def _permute_mask(region_mask: int, bit_remap: list[int]) -> int:
    permuted_mask = 0
    remaining_bits = region_mask

    while remaining_bits:
        lowest_set_bit = remaining_bits & -remaining_bits
        old_region_index = lowest_set_bit.bit_length() - 1
        permuted_mask |= bit_remap[old_region_index]
        remaining_bits ^= lowest_set_bit
    return permuted_mask

def _normalize_state(state: State) -> State:
    allowed_regions_mask, constraints = state
   
    normalized = []
    for constraint in set(constraints):
        constraint = constraint & allowed_regions_mask
        if constraint != 0:
            normalized.append(constraint)
    
    kept_constraints = []
    for constraint1 in normalized:
        redundant = False
        for constraint2 in normalized:
            if constraint2 != constraint1 and (constraint2 & constraint1) == constraint2:
                redundant = True
                break
        if not redundant:
            kept_constraints.append(constraint1)

    
    
    canonical_constraints = tuple(sorted(set(kept_constraints)))
    return (allowed_regions_mask, canonical_constraints)

@dataclass
class Memo:
    term_count: int
    # For each canonical semantic state:
    seen_depth: dict[State, int] = field(default_factory=dict)
    seen_depth_normalized: dict[State, int] = field(default_factory=dict)
    seen_depth_allowed_only: dict[int,int] = field(default_factory=dict)
    recipes : Recipes = field(default_factory=dict) # store first recipe found (base), every other recipe maps to same state
    #region_remaps: list[list[int]] = field(init=False)
    region_remaps_by_active_subset: RegionRemapsByUsedBaseTermsMask = field(init=False)
    
    canonicalization_cache: dict[tuple[State, int], State] = field(default_factory=dict)

    canonicalize_calls: int = 0
    canonicalize_seconds: float = 0.0
    permutation_trials: int = 0
    accept_calls: int = 0
    accepted_calls: int = 0
    canonicalized_cache_hits: int = 0
    canonicalize_cache_misses: int = 0
    normalized_precheck_rejects: int = 0
    allowed_only_precheck_rejects: int = 0


    def __post_init__(self):
        self.region_remaps_by_active_subset = {}
        base_term_indices = tuple(range(self.term_count))
        for used_base_terms_mask in range(1 << self.term_count):
            active_term_indices = tuple(
                                        term_index
                                        for term_index in base_term_indices
                                        if (used_base_terms_mask >> term_index) & 1)
            
            remaps_for_subset: list[list[int]] =[]
            for permuted_active_terms in permutations(active_term_indices):
                term_permutation = list(base_term_indices)
                for position_in_active, new_term_index in enumerate(active_term_indices):
                    old_term_index = permuted_active_terms[position_in_active]
                    term_permutation[new_term_index] = old_term_index

                index_remap = _region_remap_permutation(self.term_count, tuple(term_permutation))
                bit_remap = [1 << new_index for new_index in index_remap]
                remaps_for_subset.append(bit_remap)
            
            self.region_remaps_by_active_subset[used_base_terms_mask] = remaps_for_subset

    def canonicalize_state(self, state: State, used_base_terms_mask: int) -> State:
        start_time = time.perf_counter()
        self.canonicalize_calls += 1

        normalized_allowed_regions_mask, normalized_constraints = state
        normalized_state: State = (normalized_allowed_regions_mask, normalized_constraints)
        cache_key = (normalized_state, used_base_terms_mask)
        cached = self.canonicalization_cache.get(cache_key)
        if cached is not None:
            self.canonicalized_cache_hits += 1
            return cached
        self.canonicalize_cache_misses += 1

        region_remaps = self.region_remaps_by_active_subset[used_base_terms_mask]
        self.permutation_trials += len(region_remaps)
        best_canonical_state: State | None = None

        for bit_remap in region_remaps:
            permuted_allowed_mask = _permute_mask(region_mask=normalized_allowed_regions_mask,
                                                bit_remap=bit_remap)
            if not normalized_constraints:
                candidate_constraints = ()
            else:
                clamped_nonzero_constraints: set[int] = set()
                
                for normalized_constraint_mask in normalized_constraints:
                    permuted_constraint_mask = _permute_mask(region_mask=normalized_constraint_mask,
                                                             bit_remap=bit_remap)
                    constraint_allowed_regions = permuted_constraint_mask & permuted_allowed_mask
                    if constraint_allowed_regions != 0:
                        clamped_nonzero_constraints.add(constraint_allowed_regions)
                candidate_constraints = tuple(sorted(clamped_nonzero_constraints))
            
            candidate_state = (permuted_allowed_mask, candidate_constraints)
            if best_canonical_state is None or candidate_state < best_canonical_state:
                best_canonical_state = candidate_state

        assert best_canonical_state is not None
        normalized_best_canonical_state = _normalize_state(best_canonical_state)
        self.canonicalize_seconds += (time.perf_counter() - start_time)
        self.canonicalization_cache[cache_key] = normalized_best_canonical_state
        return normalized_best_canonical_state

          

    def _should_expand(self, state: State, depth: int) -> bool:
        if state not in self.seen_depth:
            return True
        if depth < self.seen_depth[state]:
            return True
        return False

    def _mark_expanded(self, state: State, depth: int):
        if state not in self.seen_depth or depth < self.seen_depth[state]:
            self.seen_depth[state] = depth

    def _attach_recipe(self, canonical_state: State, recipe_signature):
        if canonical_state not in self.recipes:
            self.recipes[canonical_state] = {
                "base": recipe_signature,
                "variants": list[recipe_signature]
            }
        else:
            self.recipes[canonical_state]["variants"].append(recipe_signature)

    def accept(self, state: State, depth: int,  used_base_terms_mask: int) ->tuple[bool, State | None]:
        """
        Returns (accepted, canonical_state); canonical_state is non-None iff accepted.
        If True, the state is already recorded at best depth.
        Canonicalizes state = (allowed_regions_mask, existence_constraints_masks).
        """
        self.accept_calls += 1
        normalized_state = _normalize_state(state)
        previous_normalized_best_state = self.seen_depth_normalized.get(normalized_state)
        if previous_normalized_best_state is not None and depth >= previous_normalized_best_state:
            self.normalized_precheck_rejects += 1
            return (False, None)
        
        canonical_state = self.canonicalize_state(normalized_state, used_base_terms_mask)
        
        if(self._should_expand(canonical_state, depth)):
            self.accepted_calls += 1
            self._mark_expanded(canonical_state, depth)
            if previous_normalized_best_state is None or depth < previous_normalized_best_state:
                self.seen_depth_normalized[normalized_state] = depth
            return (True, canonical_state)
        return (False, None)