#Defines a DFSState dataclass or simple class with:
    #premises, models_mask, term_mask, pair_mask
    #method extend(stmt_idx, config) -> DFSState|None

from dataclasses import dataclass
from typing import List, Tuple, Optional, Callable
from precompute import LogicPrecomputation


#DFSState
form_bit = {
    'A': 1 << 0,
    'E': 1 << 1,
    'I': 1 << 2,
    'O': 1 << 3,
}

@dataclass
class SearchConfig:
    min_premises: int
    max_premises: int

    max_premise_terms: Optional[int] = None
    require_all_terms_in_premises: bool = False

@dataclass
class DFSState:
    # Sorted tuple of statement indices representing premises
    premises: Tuple[int, ...]

    #Bitmask over models:
    # bit m = 1 iff mdoel m is still consistent with all premises
    models_mask: int

    # Bitmask over term indices:
    # bit t = 1 iff term t appears in some premise (as subject or predicate)
    term_mask: int

    # pair_mask[s][p] is 4-bit AEIO mask for oriented pair (s,p)
    # each entry uses form_bit bits
    pair_mask: List[List[int]]

   
def make_root_state(precomp: LogicPrecomputation) -> 'DFSState': # cls is like calling self but for classmethods
        # Create initial DFSState with no premises, all models, no terms, empty pair_mask
        term_count = precomp.term_count
        pair_mask = [[0] * term_count for _ in range(term_count)]

        return DFSState(
            premises=(),
            models_mask=precomp.full_model_mask,
            term_mask=0,
            pair_mask=pair_mask
        )

# Extend State
def extend_state(
          state: DFSState,
          precomp: LogicPrecomputation,
          config: SearchConfig,
          statement_index: int
        ) -> Optional[DFSState]:
    
    form = precomp.statement_form[statement_index]
    subject = precomp.statement_subject[statement_index]
    predicate = precomp.statement_predicate[statement_index]

    # TODO: structural filters (AE polarity, A+I / E+O conflicts, etc)
    mask_subject_predicate = state.pair_mask[subject][predicate]
    mask_predicate_subject = state.pair_mask[predicate][subject]
    new_bit = form_bit[form]

    # Structural filters
    if form == 'A' and (mask_subject_predicate & form_bit['I']): # Some S are P conflicts with All S are P - since All is stronger
        return None
    if form == 'I' and (mask_subject_predicate & form_bit['A']): # All S are P conflicts with Some S are P - since All is stronger
        return None
    if form == 'E' and (mask_subject_predicate & form_bit['O']): # Some S are not P conflicts with No S are P - since No is stronger
        return None
    if form == 'O' and (mask_subject_predicate & form_bit['E']): # No S are P conflicts with Some S are not P - since No is stronger
        return None
    
    combined = mask_subject_predicate | mask_predicate_subject | new_bit
    has_A = bool(combined & form_bit['A'])
    has_E = bool(combined & form_bit['E'])

    if has_A and has_E: # All S are P conflicts with No S are P
        return None 
    
    # TODO: Semantic filters (inconsistent or semantically redundant premises)
    statement_models_mask = precomp.models_true_mask[statement_index]
    new_models_mask = state.models_mask & statement_models_mask

    if new_models_mask == 0:
        return None  # No models remain consistent
    
    if new_models_mask == state.models_mask:
        return None  # Premise is semantically redundant, ie this premise is logically entailed by existing ones
    
    # TODO: Term mask update (too many distinct terms -> reject)
    new_term_mask = state.term_mask | (1 << subject) | (1 << predicate)
    new_term_count = new_term_mask.bit_count()

    if config.max_premise_terms is not None: # Check if max premise terms constraint is set and make sure we don't exceed it
        if new_term_count > config.max_premise_terms:
            return None
        
    if config.require_all_terms_in_premises:
        total_terms = precomp.term_count
        missing_terms = total_terms - new_term_count

        depth_after = len(state.premises) + 1 # +1 for the new premise being added
        remaining_slots = config.max_premises - depth_after # how many more premises can we add after this one?

        max_new_terms_possible = 2 * max(0, remaining_slots) # each new premise can add at most 2 new terms (subject and predicate)
        if missing_terms > max_new_terms_possible:
            return None  # Not enough remaining premises to cover all terms
        
    
    # TODO: Build children
    term_count = precomp.term_count
    new_pair_mask: List[List[int]] = [  # deep copy
        row[:] for row in state.pair_mask
    ]
    new_pair_mask[subject][predicate] |= new_bit  # updates (s,p) with form bit

    new_premises = state.premises + (statement_index,) # append statement_index to premises tuple  

    return DFSState(
        premises=new_premises,
        models_mask=new_models_mask,
        term_mask=new_term_mask,
        pair_mask=new_pair_mask
    )

# Run DFS
DFSCallback = Callable[[DFSState], None]

def run_dfs(
        precomp: LogicPrecomputation,
        config: SearchConfig,
        callback: DFSCallback
) -> None:
    root_state = make_root_state(precomp)

    _dfs_from(root_state, precomp, config, callback, start_statement_index=0)

def _dfs_from(
        state: DFSState,
        precomp: LogicPrecomputation,
        config: SearchConfig,
        callback: DFSCallback,
        start_statement_index: int
) -> None:
    num_statements = len(precomp.statements)

    for statement_index in range(start_statement_index, num_statements):
        # Try to extend the state with the current statement
        child = extend_state(state, precomp, config, statement_index)
        if child is None:
            continue  # Extension was not valid, try next statement

        depth = len(child.premises)

        
        if config.min_premises <= depth <= config.max_premises:
            # Valid premise set found, call the callback
            callback(child)

        # If we can still add more premises, recurse deeper
        if depth < config.max_premises:
            # Continue DFS deeper
            _dfs_from(child, precomp, config, callback, statement_index + 1)