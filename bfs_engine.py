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
from ConclusionExplorer import dag, memo
from ConclusionExplorer.node import Node

def search_tree(root: Node, dag_instance: dag.DAG):
    memo = dag_instance.memo
    popped_count = 0
    enqueued_count = 0
    start_time = time.perf_counter()
    last_print_time = start_time - 2
    max_depth_seen = 0
    depth_hist = collections.defaultdict(int)

    search_queue = collections.deque()
    search_queue.append(root)
    while(search_queue): 
        current_node = search_queue.popleft()
        current_depth = current_node.depth
        if current_depth > max_depth_seen:
             max_depth_seen = current_depth
        depth_hist[current_depth] += 1

        popped_count += 1
        for child_node in dag_instance.expand(current_node):
                search_queue.append(child_node)
                enqueued_count += 1
        now = time.perf_counter()
        if (now - last_print_time >= 2.0):
            last_print_time = now
            print(f"-------------------------------------------")
            print(f"Current Runtime: {now - start_time:.4f}")
            print(f"Popped: {popped_count}")
            print(f"In Queue: {len(search_queue)}")
            dag_node_count = len(dag_instance.nodes)
            print(f"DAG Nodes: {dag_node_count}")
            #print(f"Attempted Transitions: {dag_instance.attempted_transitions}")
            #print(f"Accepted Transitions: {dag_instance.accepted_transitions}")
            #print(f"DAG Edges: {dag_instance.edge_count}")
            print(f"Cannonicalize_state Calls: {memo.canonicalize_calls}")
            print(f"Canonicalize cache hits: {memo.canonicalized_cache_hits}")
            print(f"Canonicalize cache misses: {memo.canonicalize_cache_misses}")
            print(f"Canonicalize cumtime: {memo.canonicalize_seconds:.4f}")
            #print(f"Canonical avg time: {1000 * memo.canonicalize_seconds / max(1, memo.canonicalize_calls):4f}")
            #print(f"Avg Permutations: {memo.permutation_trials / memo.canonicalize_calls:.4f}")
            #print(f"Max Depth Seen: {max_depth_seen}")
            #print(f"Depth histogram: ", {d: depth_hist[d] for d in range(max_depth_seen + 1)})
            #print(f"Distinct Seen Depths: {len(memo.seen_depth)}")
            print(f"Accept calls: {memo.accept_calls}")
            print(f"Normalized precheck reject: {memo.normalized_precheck_rejects}")
            print(f"Accepted calls: {memo.accepted_calls}")
            #print(f"Syntactic Prunes: {dag_instance.rejected_syntactic}")
            #print(f"Semantic Prunes: {dag_instance.rejected_semantic_emptycap}")
            #print(f"Memo Prunes: {dag_instance.rejected_memo}")
            #print(f"No change prunes: {dag_instance.no_change}")
            #print(f"Inconsistent prunes: {dag_instance.inconsistent_state}")

    print("-----------Totals-----------\n")
    print(f"For term count: {memo.term_count}")
    print(f"Runtime: {now - start_time:.4f}")
    print(f"Popped: {popped_count}")
    print(f"In Queue: {len(search_queue)}")
    dag_node_count = len(dag_instance.nodes)
    print(f"DAG Nodes: {dag_node_count}")
    print(f"Attempted Transitions: {dag_instance.attempted_transitions}")
    print(f"Accepted Transitions: {dag_instance.accepted_transitions}")
    print(f"DAG Edges: {dag_instance.edge_count}")
    print(f"Cannonicalize_state Calls: {memo.canonicalize_calls}")
    print(f"Canonicalize cache hits: {memo.canonicalized_cache_hits}")
    print(f"Canonicalize cache misses: {memo.canonicalize_cache_misses}")
    print(f"Canonicalize cumtime: {memo.canonicalize_seconds:.4f}")
    print(f"Canonical avg time: {1000 * memo.canonicalize_seconds / max(1, memo.canonicalize_calls):4f}")
    print(f"Avg Permutations: {memo.permutation_trials / memo.canonicalize_calls:.2f}")
    print(f"Max Depth Seen: {max_depth_seen}")
    print(f"Depth histogram: {depth_hist}")
    print(f"Distinct Seen Depths: {len(memo.seen_depth)}")
    print(f"Accept calls: {memo.accept_calls}")
    print(f"Normalized precheck reject: {memo.normalized_precheck_rejects}")
    print(f"Accepted calls: {memo.accepted_calls}")
    print(f"Syntactic Prunes: {dag_instance.rejected_syntactic}")
    print(f"Semantic Prunes: {dag_instance.rejected_semantic_emptycap}")
    print(f"Memo Prunes: {dag_instance.rejected_memo}")
    print(f"No change prunes: {dag_instance.no_change}")
    print(f"Inconsistent prunes: {dag_instance.inconsistent_state}")
