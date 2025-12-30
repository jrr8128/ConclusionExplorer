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


from ConclusionExplorer.types import CanonicalState, State
recipe_bucket = dict
# For each canonical semantic state:
seen_depth : dict[tuple[int, tuple[int,...]], int] = {} # store smallest number of premises used to reach it
recipes : dict[tuple[int, tuple[int,...]], recipe_bucket] = {} # store first recipe found (base), every other recipe maps to same state


def canonicalize_state(state: State) -> CanonicalState:
    canoncalized_constraints = tuple(sorted(set(state[1])))
    return (state[0], canoncalized_constraints)

def _should_expand(canonical_state: CanonicalState, depth: int) -> bool:
    if canonical_state not in seen_depth:
        return True
    if depth < seen_depth[canonical_state]:
        return True
    return False

def _mark_expanded(canonical_state: CanonicalState, depth: int):
    if canonical_state not in seen_depth or depth < seen_depth[canonical_state]:
        seen_depth[canonical_state] = depth

def _attach_recipe(canonical_state: CanonicalState, recipe_signature):
    if canonical_state not in recipes:
        recipes[canonical_state] = {
            "base": recipe_signature,
            "variants": list[recipe_signature]
        }
    else:
        recipes[canonical_state]["variants"].append(recipe_signature)

def accept(state: State, depth: int) ->tuple[bool, CanonicalState | None]:
    """
    Returns (accepted, canonical_state); canonical_state is non-None iff accepted.
    If True, the state is already recorded at best depth.
    Canonicalizes state = (allowed_regions_mask, existence_constraints_masks).
    """
    canonical_state = canonicalize_state(state)
    if(_should_expand(canonical_state, depth)):
        _mark_expanded(canonical_state, depth)
        return (True, canonical_state)
    return (False, None)