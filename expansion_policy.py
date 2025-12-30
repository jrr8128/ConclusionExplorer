# expand(parent: Node) -> Iterable[CandidateNode]
from __future__ import annotations
from dataclasses import dataclass
from typing import Iterable, Optional

from ConclusionExplorer.node import Node
from ConclusionExplorer.semantics import SemanticSpace
from ConclusionExplorer.syntax import SyntaxSpace


@dataclass(frozen=True)
class ExpansionPolicy:
    syntax_space : SyntaxSpace
    semantic_space : SemanticSpace
    
    def _apply_AE(self, form_index, s_mask, p_mask, allowed_regions_mask) -> Optional[int]:
        if form_index == 0:
            forbidden_regions = self.semantic_space.apply_all_s_are_p(s_mask, p_mask, self.semantic_space.all_regions_mask)
        elif form_index == 1:
            forbidden_regions = self.semantic_space.apply_no_s_are_p(s_mask, p_mask)
        new_allowed_regions = allowed_regions_mask & ~forbidden_regions

        if new_allowed_regions == 0: # inconsistent
            return None
        if new_allowed_regions == allowed_regions_mask: # redundant
            return None
        return new_allowed_regions

    def _apply_IO(self, form_index, s_mask, p_mask, allowed_regions_mask, existence_constraints_masks) -> Optional[tuple[int,...]]:
        if form_index == 2:
            required_regions = self.semantic_space.apply_some_s_are_p(s_mask, p_mask)
        elif form_index == 3:
            required_regions = self.semantic_space.apply_some_s_are_not_p(s_mask, p_mask, self.semantic_space.all_regions_mask)
        if (required_regions & allowed_regions_mask) == 0: # inconsistent
            return None
        if required_regions in existence_constraints_masks: # redundant
            return None
        new_constraints_masks = existence_constraints_masks + (required_regions,) # append to current existence constraints and return
        return new_constraints_masks

    def expand(self, parent: Node) -> Iterable[Node]: # Returns candidate nodes that are not necessarily canonical
        
        for i in range(parent.last_index+1, len(self.syntax_space.list_of_statements)):
            statement = self.syntax_space.list_of_statements[i]
            form_index, s_index, p_index = statement

            s_mask = self.semantic_space.regions_where_term_is_true[s_index]
            p_mask = self.semantic_space.regions_where_term_is_true[p_index]

            child_allowed_regions = parent.allowed_regions_mask
            child_constraints = parent.existence_constraints_masks

            if form_index in (0,1):
                child_allowed_regions = self.__apply_AE(form_index, s_mask, p_mask, child_allowed_regions)
                if child_allowed_regions is None:
                    continue
                
            elif form_index in (2,3):
                child_constraints = self.__apply_IO(form_index, s_mask, p_mask, child_allowed_regions, child_constraints)
                if child_constraints is None:
                    continue

            for constraint in child_constraints:
                if(constraint & child_allowed_regions) == 0:
                    break
            else:
                new_child = Node(allowed_regions_mask= child_allowed_regions,
                                existence_constraints_masks= child_constraints,
                                depth= parent.depth + 1,
                                last_index= i)
                yield new_child
                continue
