
from dataclasses import dataclass
from typing import TypeAlias
from ConclusionExplorer import conclusions, memo, state_transitions
from ConclusionExplorer.semantics import SemanticSpace
from ConclusionExplorer.syntax import SyntaxSpace
from ConclusionExplorer.types import State, Statement

@dataclass(frozen=True)
class Edge:
    source_state : State
    destination_state : State
    statement: Statement

@dataclass
class DAGNode:
    state : State
    depth_min : int
    conclusions : frozenset[Statement] | None
    in_edges : set[Edge]
    out_edges : set[Edge]

DAG = dict[State, DAGNode]

def get_or_create_DAG_node(dag: DAG, state: State, depth: int) -> DAGNode:
    if state not in dag:
        dagNode = DAGNode(
            state=state,
            depth_min=depth,
            conclusions=None,
            in_edges=set(),
            out_edges=set()
        )
        dag[state] = dagNode
    else:
        dagNode = dag[state]
        dagNode.depth_min = min(dagNode.depth_min, depth)
    return dagNode

def populate_all_conclusions(dag: DAG, syntax_space: SyntaxSpace, semantic_space: SemanticSpace):
    for dag_node in dag.values():
        if dag_node.conclusions is None:
            dag_node.conclusions = conclusions.compute_conclusions(dag_node.state, syntax_space, semantic_space)


def add_transition(dag: DAG, source_state: State, statement: Statement, source_depth: int, 
                   syntax_space: SyntaxSpace, semantic_space: SemanticSpace
                   ) -> tuple[State | None, bool]:
    canon_source_state = memo.canonicalize_state(source_state)
    source_DAG_node = get_or_create_DAG_node(dag, canon_source_state, source_depth)
    
    child_state, changed = state_transitions.apply_statement(semantic_space, canon_source_state, statement, "expand")
    if child_state is None:
        return (None, False)

    
    child_depth = source_depth + 1
    accepted, canon_child_state = memo.accept(child_state, child_depth)
    if not accepted:
        return (None, False)
    destination_DAG_node = get_or_create_DAG_node(dag, canon_child_state, child_depth)
    
    new_edge = Edge(source_state=canon_source_state, 
                    destination_state=canon_child_state,
                    statement= statement
                    )
    if new_edge in dag[canon_source_state].out_edges:
        return (canon_child_state, False)
    else:
        dag[canon_source_state].out_edges.add(new_edge)
        dag[canon_child_state].in_edges.add(new_edge)
        return (canon_child_state, True)




