


from typing import Optional
from ConclusionExplorer.semantics import SemanticSpace
from ConclusionExplorer.types import State, Statement, Term


def _term_to_mask(semantic_space: SemanticSpace, term: Term) -> int:
    (index, is_complement) = term
    base_term = semantic_space.regions_where_term_is_true[index]
    if is_complement:
        return base_term ^ semantic_space.all_regions_mask
    else:
        return base_term 

def _apply_AE(semantic_space: SemanticSpace, form_index, s_mask, p_mask, allowed_regions_mask) -> Optional[int]:
    if form_index == 0:
        forbidden_regions = semantic_space.apply_all_s_are_p(s_mask, p_mask, semantic_space.all_regions_mask)
    elif form_index == 1:
        forbidden_regions = semantic_space.apply_no_s_are_p(s_mask, p_mask)
    new_allowed_regions = allowed_regions_mask & (semantic_space.all_regions_mask ^ forbidden_regions)

    if new_allowed_regions == 0: # inconsistent
        return None
    return new_allowed_regions

def _apply_IO(semantic_space: SemanticSpace, form_index, s_mask, p_mask, allowed_regions_mask, existence_constraints_masks) -> Optional[tuple[int,...]]:
    if form_index == 2:
        required_regions = semantic_space.apply_some_s_are_p(s_mask, p_mask)
    elif form_index == 3:
        required_regions = semantic_space.apply_some_s_are_not_p(s_mask, p_mask, semantic_space.all_regions_mask)
    if (required_regions & allowed_regions_mask) == 0: # inconsistent
        return None
    new_constraints_masks = existence_constraints_masks
    if required_regions not in existence_constraints_masks: # redundant
        new_constraints_masks += (required_regions,) # append to current existence constraints and return
    return new_constraints_masks

def apply_statement(semantic_space: SemanticSpace, state: State, statement: Statement, mode: str) -> tuple[State | None, bool]:
    assert mode in ("expand", "entailment")
    form_index, s_term, p_term = statement
    s_mask = _term_to_mask(semantic_space, s_term)
    p_mask = _term_to_mask(semantic_space, p_term)

    allowed_regions_mask, existence_constraints_masks = state
    changed = False
    if form_index in (0,1):
        new_allowed_regions = _apply_AE(semantic_space, form_index, s_mask, p_mask, allowed_regions_mask)
        if new_allowed_regions is None:
            return None, False
        elif new_allowed_regions == allowed_regions_mask: # redundant
            if mode == "expand":
                return state, False
        else:
            allowed_regions_mask = new_allowed_regions
            changed = True
        
    if form_index in (2,3):
        new_existence_constraints = _apply_IO(semantic_space, form_index, s_mask, p_mask, allowed_regions_mask, existence_constraints_masks)
        if new_existence_constraints is None:
            return None, False
        elif new_existence_constraints == existence_constraints_masks:
            if mode == "expand":
                return state, False
        else:
            existence_constraints_masks = new_existence_constraints
            changed = True

    for constraint in existence_constraints_masks:
        if(constraint & allowed_regions_mask) == 0:
            return None, False
    
    new_state = (allowed_regions_mask, existence_constraints_masks)
    return new_state, changed