# BFS Engine <-> Expansion Logic
# expand (state, depth) -> iterable[(action, child_state, child_depth_delta)]
#   Note: child_depth_delta is usually +1, but don't want BFS to infer 
#   Note: same (state, depth) -> same children in same order

# BFS <-> Memo 
#   Memo answers if child should be enqueued
#   memo.accept(state, depth) -> (accepted, canonical_state)

import collections
from dataclasses import dataclass
from ConclusionExplorer import expansion_policy, memo
from ConclusionExplorer.node import Node


def search_tree(root: Node):
    search_queue = collections.deque()
    search_queue.append(root)
    while(search_queue): 
        current_node = search_queue.popleft()
        child_nodes : list[Node] = expansion_policy.expand(current_node)
        for child_node in child_nodes:
            accepted, canonical_state = memo.accept((child_node.allowed_regions_mask, child_node.existence_constraints_masks), child_node.depth)
            if(accepted):
                new_node = Node(canonical_state[0],
                                canonical_state[1],
                                child_node.depth,
                                child_node.last_index)
                search_queue.append(new_node)

        


