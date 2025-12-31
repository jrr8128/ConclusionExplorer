

from ConclusionExplorer.node import Node
from ConclusionExplorer.semantics import SemanticSpace
from ConclusionExplorer.types import EmptyTermsMask, State, Statement, Term, UsedBaseTermsMask


def term_id(term: Term, term_count: int) -> int:
    return term[0] + term[1] * term_count

def pair_id(statement: Statement, term_count: int) -> int:
    s_id = term_id(statement[1], term_count)
    p_id = term_id(statement[2], term_count)
    return s_id * (2 * term_count) + p_id

def check_syntactic(term_count: int, node: Node, statement: Statement) -> tuple[int,int] | None:
    pid = pair_id(statement, term_count)
    shift = 2 * pid
    status = (node.pair_status_mask >> shift) & 0b11
    if status != 0:
        return None
    else:
        child_pair_status_mask = node.pair_status_mask | (0b01 << shift)
        child_used_base_terms_mask = node.used_base_terms_mask | (1 << statement[1][0]) | (1 << statement[2][0])
        return (child_pair_status_mask, child_used_base_terms_mask)

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