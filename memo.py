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

recipe_bucket = dict
# For each canonical semantic state:
seen_depth : dict[tuple[int, tuple[int,...]], int] = {} # store smallest number of premises used to reach it
recipes : dict[tuple[int, tuple[int,...]], recipe_bucket] = {} # store first recipe found (base), every other recipe maps to same state
 

def get_key(allowed_regions_mask : int, existence_constraint_masks : tuple[int,...]) -> tuple[int, tuple[int]]:
    return (allowed_regions_mask, existence_constraint_masks)

def should_expand(key, depth) -> bool:
    if key not in seen_depth:
        return True
    if depth < seen_depth[key]:
        return True
    return False

def mark_expanded(key, depth):
    if key not in seen_depth or depth < seen_depth[key]:
        seen_depth[key] = depth

def attach_recipe(key, recipe_signature):
    if key not in recipes:
        recipes[key] = {
            "base": recipe_signature,
            "variants": []
        }
    else:
        recipes[key]["variants"].append(recipe_signature)

