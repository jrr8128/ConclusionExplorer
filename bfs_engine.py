# BFS Engine <-> Expansion Logic
# expand (state, depth) -> iterable[(action, child_state, child_depth_delta)]
#   Note: child_depth_delta is usually +1, but don't want BFS to infer 
#   Note: same (state, depth) -> same children in same order

# BFS <-> Memo 
#   Memo answers if child should be enqueued
#   memo.accept(state, depth) -> (accepted, canonical_state)

import collections
from dataclasses import dataclass
import time
from ConclusionExplorer import dag, expansion_policy, memo
from ConclusionExplorer.node import Node

def search_tree(root: Node, dag_instance: dag.DAG):
    popped_count = 0
    enqueued_count = 0
    start_time = time.monotonic()
    last_print_time = start_time - 2

    search_queue = collections.deque()
    search_queue.append(root)
    while(search_queue): 
        current_node = search_queue.popleft()
        popped_count += 1
        for child_node in dag_instance.expand(current_node):
                search_queue.append(child_node)
                enqueued_count += 1
        now = time.monotonic()
        if (now - last_print_time >= 2.0):
            last_print_time = now
            print(f"Current Runtime: {now - start_time:.4f}")
            print(f"Popped: {popped_count}")
            print(f"In Queue: {len(search_queue)}")
            dag_node_count = len(dag_instance.nodes)
            print(f"DAG Nodes: {dag_node_count}")
            print(f"Attempted Transitions: {dag_instance.attempted_transitions}")
            print(f"Accepted Transitions: {dag_instance.accepted_transitions}")
            print(f"DAG Edges: {dag_instance.edge_count}")

    print("-----------Totals-----------\n")
    print(f"Runtime: {now - start_time:.4f}")
    print(f"Popped: {popped_count}")
    print(f"In Queue: {len(search_queue)}")
    dag_node_count = len(dag_instance.nodes)
    print(f"DAG Nodes: {dag_node_count}")
    print(f"Attempted Transitions: {dag_instance.attempted_transitions}")
    print(f"Accepted Transitions: {dag_instance.accepted_transitions}")
    print(f"DAG Edges: {dag_instance.edge_count}")

        


