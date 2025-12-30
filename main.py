from collections import Counter
import itertools
from ConclusionExplorer import bfs_engine, memo, semantics, syntax
from ConclusionExplorer.dag import DAG
from ConclusionExplorer.node import Node
import os


term_count = 4

syntax_space = syntax.SyntaxSpace.build_syntax_space(term_count)
semantic_space = semantics.SemanticSpace.build_semantic_space(term_count)

root_regions_mask = semantic_space.all_regions_mask
root_node = Node(
                allowed_regions_mask=root_regions_mask,
                existence_constraints_masks=tuple(),
                depth=0,
                last_index=-1
                )

memo_instance = memo.Memo()
dag_instance = DAG(
        nodes={},
        syntax_space=syntax_space,
        semantic_space=semantic_space,
        memo=memo_instance
        )

dag_instance.get_or_create_DAG_node((root_node.allowed_regions_mask, 
                            root_node.existence_constraints_masks), 
                            root_node.depth
                            )

print(f"Term Count {term_count}")
print(f"# Statements: {len(syntax_space.list_of_statements)}")
print(f"Region Mask: {semantic_space.all_regions_mask}")

form_counts = Counter()
pair_counts = Counter()

for stmt in syntax_space.list_of_statements:
    form, s, p = stmt  # form index, (base, is_comp), (base, is_comp)

    form_counts[form] += 1

    s_base, s_comp = s
    p_base, p_comp = p
    pair_counts[(s_comp, p_comp)] += 1  # (0/1, 0/1)

print("Total statements:", len(syntax_space.list_of_statements))
print("By form (A,E,I,O as 0,1,2,3):")
for k in sorted(form_counts):
    print(f"  form {k}: {form_counts[k]}")

print("By (S is complemented, P is complemented):")
for k in sorted(pair_counts):
    print(f"  {k}: {pair_counts[k]}")

search = bfs_engine.search_tree(root_node, dag_instance)

dag_instance.populate_all_conclusions()

