

from ConclusionExplorer.node import Node
from ConclusionExplorer.semantics import SemanticSpace
from ConclusionExplorer.types import EmptyTermsMask, State, Statement, Term, UsedBaseTermsMask


def is_trivial_existence(statement: Statement) -> bool:
    form, s, p = statement
    s_base, s_comp = s
    p_base, p_comp = p

    # O(X, ~X) or O(~X, X)
    if form == 3 and s_base == p_base and s_comp != p_comp:
        return True

    # I(X, X) or I(~X, ~X)
    if form == 2 and s_base == p_base and s_comp == p_comp:
        return True

    return False

def prune_unfun(stmt: Statement) -> bool:
    form, s, p = stmt
    sb, sc = s
    pb, pc = p
    same_base = (sb == pb)
    opp_comp = (sc != pc)

    if same_base and not opp_comp:  # X vs X
        return form in (0, 1, 2, 3) and form != 2 
    if same_base and opp_comp:      # X vs ~X
        return True                 # prune all four forms
    return False


def term_id(term: Term, term_count: int) -> int:
    return term[0] + term[1] * term_count

def pair_id(statement: Statement, term_count: int) -> int:
    s_id = term_id(statement[1], term_count)
    p_id = term_id(statement[2], term_count)
    return s_id * (2 * term_count) + p_id

def check_syntactic(term_count: int, node: Node, statement: Statement) -> tuple[int,int,int] | None:
    if is_trivial_existence(statement):
      return None
    
    if prune_unfun(statement):
        return None
    
    form, s, p = statement
    s_base, s_comp = s
    p_base, p_comp = p

    # Forbid A(S,P) if A(S,~P) already present (same S variant, same P base)
    child_universal_polarity_mask = node.universal_polarity_mask
    if form == 0:  # A
        s_id = s_base * 2 + s_comp              # 0..2N-1
        bit = ((s_id * term_count + p_base) * 2 + p_comp)
        opp_bit = ((s_id * term_count + p_base) * 2 + (1 - p_comp))
        if (node.universal_polarity_mask >> opp_bit) & 1:
            return None
        child_universal_polarity_mask |= (1 << bit)

    # Existing pair uniqueness rule
    pid = pair_id(statement, term_count)
    shift = 2 * pid
    status = (node.pair_status_mask >> shift) & 0b11
    if status != 0:
        return None
    else:
        child_pair_status_mask = node.pair_status_mask | (0b01 << shift)
        child_used_base_terms_mask = node.used_base_terms_mask | (1 << statement[1][0]) | (1 << statement[2][0])
        return (child_pair_status_mask, child_used_base_terms_mask, child_universal_polarity_mask)

def post_semantic_checks(term_count: int, semantic_space: SemanticSpace, source_empty_mask: EmptyTermsMask, child_state: State, child_used_base_terms_mask: UsedBaseTermsMask, cap_K: int) -> EmptyTermsMask | None:
    new_empty_mask = source_empty_mask
    for base_term in range(term_count):
        if not (child_used_base_terms_mask >> base_term) & 1:
            continue
        if (source_empty_mask >> base_term) & 1:
            continue
        if (child_state[0] & semantic_space.regions_where_term_is_true[base_term]) == 0:
            new_empty_mask |= (1 << base_term)
        
    if new_empty_mask.bit_count() > cap_K:
        return None
    else:
        return new_empty_mask