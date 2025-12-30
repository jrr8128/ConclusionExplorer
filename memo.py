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
from ConclusionExplorer.types import State


@staticmethod
def canonicalize_state(state: State) -> State:
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
    # For each canonical semantic state:
    seen_depth : dict[tuple[int, tuple[int,...]], int] = field(default_factory=dict) # store smallest number of premises used to reach it
    recipes : dict[tuple[int, tuple[int,...]], dict] = field(default_factory=dict) # store first recipe found (base), every other recipe maps to same state

    def _should_expand(self, canonical_state: State, depth: int) -> bool:
        if canonical_state not in self.seen_depth:
            return True
        if depth < self.seen_depth[canonical_state]:
            return True
        return False

    def _mark_expanded(self, canonical_state: State, depth: int):
        if canonical_state not in self.seen_depth or depth < self.seen_depth[canonical_state]:
            self.seen_depth[canonical_state] = depth

    def _attach_recipe(self, canonical_state: State, recipe_signature):
        if canonical_state not in self.recipes:
            self.recipes[canonical_state] = {
                "base": recipe_signature,
                "variants": list[recipe_signature]
            }
        else:
            self.recipes[canonical_state]["variants"].append(recipe_signature)

    def accept(self, state: State, depth: int) ->tuple[bool, State | None]:
        """
        Returns (accepted, canonical_state); canonical_state is non-None iff accepted.
        If True, the state is already recorded at best depth.
        Canonicalizes state = (allowed_regions_mask, existence_constraints_masks).
        """
        canonical_state = canonicalize_state(state)
        if(self._should_expand(canonical_state, depth)):
            self._mark_expanded(canonical_state, depth)
            return (True, canonical_state)
        return (False, None)