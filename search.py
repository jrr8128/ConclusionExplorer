# BFS search for premises, node expansion, pruning 
from dataclasses import dataclass

from ConclusionExplorer.semantics import SemanticSpace
from ConclusionExplorer.syntax import SyntaxSpace

@dataclass
class Node:
    premises: tuple[int, ...]
    current_running_mask: int
    parent_node: 'Node' | None = None
    validConclusions: set[int]

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
