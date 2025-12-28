# BFS search for premises, node expansion, pruning 
from __future__ import annotations
from dataclasses import dataclass

from semantics import SemanticSpace
from syntax import SyntaxSpace
import collections
import time



# Node dataclass needs to hold all the premises that are in its current running mask
# Needs to be able to reference parent node for backtracking
# Needs to hold valid conclusions, resultant from the current running mask
@dataclass
class Node:
    premises: tuple[int, ...]
    allowed_regions_mask: int
    existence_constraints_masks: tuple[int, ...]
    validConclusions: set[tuple[int,int,int]]
    last_statement_index: int
    parent_node: 'Node' | None = None

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
        allowed_regions_mask=semantic_space.all_regions_mask,
        existence_constraints_masks=(),
        validConclusions=set(),
        last_statement_index=-1,
        parent_node=None
    )
    return SearchTree(
        term_count=term_count,
        root=root_node,
        syntax_space=syntax_space,
        semantic_space=semantic_space,
        nodes=(root_node,)
    )

def update_allowed_regions_mask(semantic_space: SemanticSpace, current_regions_mask: int, statement: tuple[int,...]) -> int:
    S_mask = semantic_space.regions_where_term_is_true[statement[1]]
    P_mask = semantic_space.regions_where_term_is_true[statement[2]]
    if(statement[0] == 0):
        forbidden_regions = semantic_space.apply_all_s_are_p(S_mask, P_mask, semantic_space.all_regions_mask)
    if(statement[0] == 1):
        forbidden_regions = semantic_space.apply_no_s_are_p(S_mask, P_mask)
    return current_regions_mask & (semantic_space.all_regions_mask ^ forbidden_regions)

def create_existence_constraint(semantic_space: SemanticSpace, current_regions_mask: int, statement: tuple[int,...]) -> int:
    S_mask = semantic_space.regions_where_term_is_true[statement[1]]
    P_mask = semantic_space.regions_where_term_is_true[statement[2]]
    if(statement[0] == 2):
        required_regions = semantic_space.apply_some_s_are_p(S_mask, P_mask)
    if(statement[0] == 3):
        required_regions = semantic_space.apply_some_s_are_not_p(S_mask, P_mask, semantic_space.all_regions_mask)
    return required_regions

def create_negation_AE(semantic_space: SemanticSpace, current_regions_mask: int, statement: tuple[int,...]) -> int:
    S_mask = semantic_space.regions_where_term_is_true[statement[1]]
    P_mask = semantic_space.regions_where_term_is_true[statement[2]]
    if(statement[0] == 1):
        required_regions = semantic_space.apply_some_s_are_p(S_mask, P_mask)
    if(statement[0] == 0):
        required_regions = semantic_space.apply_some_s_are_not_p(S_mask, P_mask, semantic_space.all_regions_mask)
    return required_regions

def create_negation_IO(semantic_space: SemanticSpace, current_regions_mask: int, statement: tuple[int,...]) -> int:
    S_mask = semantic_space.regions_where_term_is_true[statement[1]]
    P_mask = semantic_space.regions_where_term_is_true[statement[2]]
    if(statement[0] == 3):
        forbidden_regions = semantic_space.apply_all_s_are_p(S_mask, P_mask, semantic_space.all_regions_mask)
    if(statement[0] == 2):
        forbidden_regions = semantic_space.apply_no_s_are_p(S_mask, P_mask)
    return current_regions_mask & (semantic_space.all_regions_mask ^ forbidden_regions)

def generate_valid_conclusions(search_tree: SearchTree, allowed_regions_mask, existence_constraints_masks, premises):
    validConclusions = set()
    not_allowed = set()
    for premise in premises:
        if premise[0] == 0:
            not_allowed.add((2, min(premise[1], premise[2]), max(premise[1], premise[2])))
        if premise[0] == 1:
            not_allowed.add((3, premise[1], premise[2]))
            not_allowed.add((3, premise[2], premise[1]))
    for statement in search_tree.syntax_space.list_of_statements:
        if statement in premises:
            continue
        if statement in not_allowed:
            continue
        satisfiable = True
        if(statement[0] in (0,1)):
            new_constraint = create_negation_AE(search_tree.semantic_space, allowed_regions_mask, statement)
            temp_existence_constraints = existence_constraints_masks + (new_constraint,)
            for constraint in temp_existence_constraints:
                if (constraint & allowed_regions_mask) == 0:
                    satisfiable = False
                    break
        
        if(statement[0] in (2,3)):
            new_allowed_regions_mask = create_negation_IO(search_tree.semantic_space, allowed_regions_mask, statement)
            for constraint in existence_constraints_masks:
                if (constraint & new_allowed_regions_mask) == 0:
                    satisfiable = False
                    break

        if not satisfiable:
            validConclusions.add(statement)
    return validConclusions


def search(search_tree: SearchTree, constraint_depth: bool, max_depth: int):
    if(not constraint_depth):
        max_depth = float("inf")
    else:
        if(max_depth < 2):
            raise ValueError("Max_depth parameter must be 2 or more when constraint_depth bool is true.")
    last_print = time.time()
    print_every = 2.0
    
    valid_nodes = []
    search_queue = collections.deque()
    search_queue.append(search_tree.root)

    valid_node_count = 0
    pruned_node_count = 0
    while(search_queue):
        current_node = search_queue.popleft()
        if len(current_node.premises) >= max_depth:
            continue
        now = time.time()
        if now - last_print >= print_every:
            print(f"Current Depth: ", len(current_node.premises))
            print(f"Current Queue Size: ", len(search_queue))
            print(f"Valid node count: ", valid_node_count)
            print(f"Pruned nodes: ", pruned_node_count)
            last_print = now

        not_allowed = set()
        for premise in current_node.premises:
            if premise[0] == 0:
                not_allowed.add((2, min(premise[1], premise[2]), max(premise[1], premise[2])))
            if premise[0] == 1:
                not_allowed.add((3, premise[1], premise[2]))
                not_allowed.add((3, premise[2], premise[1]))

        for i, statement in enumerate(search_tree.syntax_space.list_of_statements):
            if i <= current_node.last_statement_index:
                continue
            if statement in current_node.premises:
                continue
            if statement in not_allowed:
                continue


            new_node_allowed_regions_mask = current_node.allowed_regions_mask
            new_node_existence_constraint_masks = current_node.existence_constraints_masks
            S_mask = search_tree.semantic_space.regions_where_term_is_true[statement[1]]
            P_mask = search_tree.semantic_space.regions_where_term_is_true[statement[2]]
            if (S_mask not in new_node_existence_constraint_masks):
                new_node_existence_constraint_masks = new_node_existence_constraint_masks + (S_mask,)
            if (P_mask not in new_node_existence_constraint_masks):
                new_node_existence_constraint_masks = new_node_existence_constraint_masks + (P_mask,)

            if(statement[0] in (0,1)):
                new_allowed_regions_mask = update_allowed_regions_mask(
                                                search_tree.semantic_space, 
                                                current_node.allowed_regions_mask,
                                                statement
                                                )
                new_node_allowed_regions_mask = new_allowed_regions_mask
            if(statement[0] in (2,3)):
                new_existence_constraint = create_existence_constraint(search_tree.semantic_space,
                                                                       new_node_allowed_regions_mask,
                                                                       statement
                                                                       )
                if(new_existence_constraint & new_node_allowed_regions_mask == 0):
                    pruned_node_count += 1
                    continue        
                else:
                    new_node_existence_constraint_masks = new_node_existence_constraint_masks + (new_existence_constraint,)

            isConsistent = True
            for existence_constraint in new_node_existence_constraint_masks:
                if(existence_constraint & new_node_allowed_regions_mask == 0):
                    isConsistent = False
                    break
            if not isConsistent:
                pruned_node_count += 1
                continue
            new_node_premises = current_node.premises + (statement,)
            new_node_conclusions = set()
            if (len(new_node_premises) >= 2):
                new_node_conclusions = generate_valid_conclusions(search_tree, new_node_allowed_regions_mask, new_node_existence_constraint_masks, new_node_premises)
            new_node = Node(
                premises=new_node_premises,
                allowed_regions_mask=new_node_allowed_regions_mask,
                existence_constraints_masks=new_node_existence_constraint_masks,
                validConclusions=new_node_conclusions,
                last_statement_index=i,
                parent_node=current_node
            )
            search_queue.append(new_node)
            valid_nodes.append(new_node)
            valid_node_count +=1
    
    print(f"Final valid node count: ", valid_node_count)
    print(f"Final Pruned nodes: ", pruned_node_count)
    return valid_nodes