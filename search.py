# BFS search for premises, node expansion, pruning 
from dataclasses import dataclass

from semantics import SemanticSpace
from syntax import SyntaxSpace


# Node dataclass needs to hold all the premises that are in its current running mask
# Needs to be able to reference parent node for backtracking
# Needs to hold valid conclusions, resultant from the current running mask
@dataclass
class Node:
    premises: tuple[int, ...]
    current_running_mask: int
    parent_node: 'Node' | None = None
    validConclusions: set[int]

# The search tree will be a BFS tree, starting from the root node with no premises
# It will generate both the syntax space and the semanti space for the given term count
# The syntax space contains all possible premises
# The semantic space contains all possible types and their masks
# The semantic space uses theory to reduce the amount of types to consider 

@dataclass
class SearchTree:
    term_count: int
    root: Node
    syntax_space: SyntaxSpace
    semantic_space: SemanticSpace
    nodes: tuple[Node, ...]

def build_initial_search_tree(term_count: int) -> SearchTree:
    syntax_space = SyntaxSpace.build_syntax_space(term_count)
    semantic_space = SemanticSpace.build_semantic_space(term_count)
    root_node = Node(
        premises=(),
        current_running_mask=semantic_space.universe_mask,
        parent_node=None,
        validConclusions=set()
    )
    return SearchTree(
        term_count=term_count,
        root=root_node,
        syntax_space=syntax_space,
        semantic_space=semantic_space,
        nodes=(root_node,)
    )
