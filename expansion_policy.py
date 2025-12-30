# expand(parent: Node) -> Iterable[CandidateNode]
from __future__ import annotations
from dataclasses import dataclass
from typing import Iterable, Optional

from ConclusionExplorer import state_transitions
from ConclusionExplorer.node import Node
from ConclusionExplorer.semantics import SemanticSpace
from ConclusionExplorer.syntax import SyntaxSpace
from ConclusionExplorer.types import Term


@dataclass(frozen=True)
class ExpansionPolicy:
    syntax_space : SyntaxSpace
    semantic_space : SemanticSpace
    

    def expand(self, parent: Node) -> Iterable[Node]: # Returns candidate nodes that are not necessarily canonical
        
        for i in range(parent.last_index+1, len(self.syntax_space.list_of_statements)):
            statement = self.syntax_space.list_of_statements[i]

            state = (parent.allowed_regions_mask, parent.existence_constraints_masks)
            new_state, changed = state_transitions.apply_statement(self.semantic_space, state, statement, "expand")
                

            if not changed: # redundant
                continue

            #NOTE: Because the apply statement returned false for all cases of inconsistent *and* redundant, 
            # this check is not necessary
            #if new_state is None: # inconsistent
            #    continue

            new_child = Node(allowed_regions_mask= new_state[0],
                            existence_constraints_masks= new_state[1],
                            depth= parent.depth + 1,
                            last_index= i)
            yield new_child
            continue
