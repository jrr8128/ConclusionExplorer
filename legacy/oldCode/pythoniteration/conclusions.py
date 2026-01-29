





from ConclusionExplorer import state_transitions
from ConclusionExplorer.semantics import SemanticSpace
from ConclusionExplorer.syntax import SyntaxSpace
from ConclusionExplorer.types import State, Statement

# A = 0 | ~A = 3 = O
# E = 1 | ~E = 2 = I
# I = 2 | ~I = 1 = E
# O = 3 | ~O = 0 = A

def negate_statement(statement: Statement) -> Statement:
    form_index, s_term, p_term = statement
    negative_form_index = (3,2,1,0)[form_index]
    negated_statement = (negative_form_index, s_term, p_term)
    return negated_statement

def compute_conclusions(state: State, syntax_space: SyntaxSpace, semantic_space: SemanticSpace) -> frozenset[Statement]:
    conclusions = []
    for statement in syntax_space.list_of_statements:
        new_state, _ = state_transitions.apply_statement(semantic_space, state, negate_statement(statement), "entailment")
        if new_state is None:
            conclusions.append(statement)
    return frozenset(conclusions)
