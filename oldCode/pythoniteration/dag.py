
from dataclasses import dataclass, field
from typing import TypeAlias
from ConclusionExplorer import conclusions, expansion_filters, memo, state_transitions
from ConclusionExplorer.node import Node
from ConclusionExplorer.semantics import SemanticSpace
from ConclusionExplorer.syntax import SyntaxSpace
from ConclusionExplorer.types import State, Statement

@dataclass(frozen=True)
class TransitionResult:
    child_state: State
    pair_status_mask: int
    used_base_terms_mask: int
    empty_base_terms_mask: int
    universal_polarity_mask: int

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
    rejected_syntactic = 0
    rejected_semantic_inconsistent = 0 #state becomes none
    rejected_semantic_emptycap = 0 # reached max empty terms
    rejected_memo = 0 
    no_change = 0
    inconsistent_state = 0

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
            child_recipe_sig = node.recipe_signature + (i,)
            transition_result = self.add_transition(node, statement, child_recipe_sig)
            self.attempted_transitions += 1
            if transition_result is None:
                continue
            child_allowed_regions_mask, child_existence_constraints_mask = transition_result.child_state
            child_node = Node(
                            allowed_regions_mask=child_allowed_regions_mask,
                            existence_constraints_masks=child_existence_constraints_mask, 
                            depth=node.depth + 1,
                            last_index=i,
                            pair_status_mask=transition_result.pair_status_mask,
                            used_base_terms_mask=transition_result.used_base_terms_mask,
                            empty_base_terms_mask=transition_result.empty_base_terms_mask,
                            recipe_signature=child_recipe_sig,
                            universal_polarity_mask=transition_result.universal_polarity_mask,
                            )
            self.accepted_transitions += 1
            yield child_node

    def populate_all_conclusions(self):
        for dag_node in self.nodes.values():
            if dag_node.conclusions is None:
                dag_node.conclusions = conclusions.compute_conclusions(dag_node.state, self.syntax_space, self.semantic_space)


    def add_transition(self, source_node: Node, statement: Statement, child_recipe_signature: tuple[int,...]
                    ) -> TransitionResult | None:
        source_state = (source_node.allowed_regions_mask, source_node.existence_constraints_masks)
        source_depth = source_node.depth
        child_syntactic_masks = expansion_filters.check_syntactic(self.memo.term_count, source_node, statement)
        if child_syntactic_masks is None:  
            self.rejected_syntactic += 1
            return None
        self.get_or_create_DAG_node(source_state, source_depth)
        
        child_state, changed = state_transitions.apply_statement(self.semantic_space, source_state, statement, "expand")
        if child_state is None:
            self.inconsistent_state += 1
            return None
        if not changed:
            self.no_change += 1
            return None

        child_empty_terms_mask = expansion_filters.post_semantic_checks(self.memo.term_count, self.semantic_space, 
                                                                        source_node.empty_base_terms_mask, child_state, 
                                                                        child_syntactic_masks[1], max(1, self.memo.term_count - 2))
        if child_empty_terms_mask is None:
            self.rejected_semantic_emptycap += 1
            return None

  
        child_depth = source_depth + 1
        accepted, canon_child_state = self.memo.accept(child_state, child_depth, child_recipe_signature)
        if accepted == memo.AcceptKind.REJECT:
            self.rejected_memo += 1
            return None
      
        if accepted == memo.AcceptKind.TIE:
          return None
        
        transition_result = TransitionResult(
                                            child_state=canon_child_state,
                                            pair_status_mask=child_syntactic_masks[0],
                                            used_base_terms_mask=child_syntactic_masks[1],
                                            universal_polarity_mask=child_syntactic_masks[2],
                                            empty_base_terms_mask=child_empty_terms_mask                                    
                                            )
        
        self.get_or_create_DAG_node(canon_child_state, child_depth)
        
        new_edge = Edge(source_state=source_state, 
                        destination_state=canon_child_state,
                        statement= statement
                        )
        
        if new_edge not in self.nodes[source_state].out_edges:
            self.edge_count += 1
            self.nodes[source_state].out_edges.add(new_edge)
            self.nodes[canon_child_state].in_edges.add(new_edge)
        return transition_result


