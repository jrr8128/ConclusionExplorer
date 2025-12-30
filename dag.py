
from dataclasses import dataclass, field
from typing import TypeAlias
from ConclusionExplorer import conclusions, memo, state_transitions
from ConclusionExplorer.node import Node
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

@dataclass
class DAG:
    syntax_space: SyntaxSpace
    semantic_space: SemanticSpace
    memo: memo.Memo
    nodes: dict[State, DAGNode] = (field(default_factory=dict))
    attempted_transitions = 0
    accepted_transitions = 0
    edge_count = 0

    def get_or_create_DAG_node(self, state: State, depth: int) -> DAGNode:
        if state not in self.nodes:
            dagNode = DAGNode(
                state=state,
                depth_min=depth,
                conclusions=None,
                in_edges=set(),
                out_edges=set()
            )
            self.nodes[state] = dagNode
        else:
            dagNode = self.nodes[state]
            dagNode.depth_min = min(dagNode.depth_min, depth)
        return dagNode

    def expand(self, node: Node):
        for i in range(node.last_index + 1, len(self.syntax_space.list_of_statements)):
            statement = self.syntax_space.list_of_statements[i]
            new_state, changed = self.add_transition((node.allowed_regions_mask, node.existence_constraints_masks), statement, node.depth)
            self.attempted_transitions += 1
            if new_state is not None and changed:
                child_node = Node(new_state[0], new_state[1], node.depth + 1, i)
                self.accepted_transitions += 1
                yield child_node

    def populate_all_conclusions(self):
        for dag_node in self.nodes.values():
            if dag_node.conclusions is None:
                dag_node.conclusions = conclusions.compute_conclusions(dag_node.state, self.syntax_space, self.semantic_space)


    def add_transition(self, source_state: State, statement: Statement, source_depth: int
                    ) -> tuple[State | None, bool]:
        canon_source_state = memo.canonicalize_state(source_state)
        self.get_or_create_DAG_node(canon_source_state, source_depth)
        
        child_state, changed = state_transitions.apply_statement(self.semantic_space, canon_source_state, statement, "expand")
        if child_state is None:
            return (None, False)
        if not changed:
            return (None, False)

        
        child_depth = source_depth + 1
        accepted, canon_child_state = self.memo.accept(child_state, child_depth)
        if not accepted:
            return (None, False)
        self.get_or_create_DAG_node(canon_child_state, child_depth)
        
        new_edge = Edge(source_state=canon_source_state, 
                        destination_state=canon_child_state,
                        statement= statement
                        )
        if new_edge in self.nodes[canon_source_state].out_edges:
            return (canon_child_state, changed)
        else:
            self.edge_count += 1
            self.nodes[canon_source_state].out_edges.add(new_edge)
            self.nodes[canon_child_state].in_edges.add(new_edge)
            return (canon_child_state, changed)




