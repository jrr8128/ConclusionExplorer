import cProfile
from collections import Counter, defaultdict
import itertools
import pstats
from ConclusionExplorer import bfs_engine, conclusions, memo, semantics, state_transitions, syntax
from ConclusionExplorer.dag import DAG
from ConclusionExplorer.node import Node
import os
import json
import hashlib

from ConclusionExplorer.types import State, Statement

FORM = {0: "A", 1: "E", 2: "I", 3: "O"}
TERM_NAMES = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"  # term_count<=26
FORM_ENG = {0: "All {S} are {P}", 1: "No {S} are {P}", 2: "Some {S} are {P}", 3: "Some {S} are not {P}"}

_concl_key_cache: dict = {}
def conclusion_group_key(stmt, semantic_space, memo_instance):
    k = _concl_key_cache.get(stmt)
    if k is not None:
        return k
    root = (semantic_space.all_regions_mask, tuple())
    st, _ = state_transitions.apply_statement(semantic_space, root, stmt, "expand")
    assert st is not None
    k = memo_instance.canonicalize_state(memo._normalize_state(st))
    _concl_key_cache[stmt] = k
    return k

def term_eng(term):
    base, is_comp = term
    name = TERM_NAMES[base]
    return f"non-{name}" if is_comp else name

def stmt_eng(statement):
    form, s, p = statement
    return FORM_ENG[form].format(S=term_eng(s), P=term_eng(p))

def statement_to_json(statement):
    form, s, p = statement
    return [form, [s[0], s[1]], [p[0], p[1]]]

def signature_id(recipe_signature: tuple[int, ...]) -> str:
    b = json.dumps(recipe_signature, separators=(",", ":")).encode("utf-8")
    return hashlib.sha1(b).hexdigest()

def build_stmt_index(syntax_space):
    return {stmt: i for i, stmt in enumerate(syntax_space.list_of_statements)}

def to_mask(stmts, stmt_to_idx) -> int:
    m = 0
    for s in stmts:
        m |= 1 << stmt_to_idx[s]
    return m

def precompute_singleton_conclusion_masks(syntax_space, semantic_space, stmt_to_idx) -> list[int]:
    root = (semantic_space.all_regions_mask, tuple())
    masks = [0] * len(syntax_space.list_of_statements)

    for i, premise in enumerate(syntax_space.list_of_statements):
        st, _ = state_transitions.apply_statement(semantic_space, root, premise, "expand")
        if st is None:
            continue
        c = conclusions.compute_conclusions(st, syntax_space, semantic_space)
        masks[i] = to_mask(c, stmt_to_idx)

    return masks

def base_terms_in_stmt(stmt) -> set[int]:
    _, s, p = stmt
    return {s[0], p[0]}

def base_terms_in_recipe(recipe_signature, syntax_space) -> set[int]:
    used = set()
    for i in recipe_signature:
        form, s, p = syntax_space.list_of_statements[i]
        used.add(s[0]); used.add(p[0])
    return used


def export_from_memo(term_count, memo, syntax_space, semantic_space, out_root: str):
    stmt_to_idx = build_stmt_index(syntax_space)
    single_concl_masks = precompute_singleton_conclusion_masks(syntax_space, semantic_space, stmt_to_idx)

    buckets = defaultdict(list)

    for canonical_state, bucket in memo.recipes.items():
        if bucket.get("conclusions") is None:
            bucket["conclusions"] = conclusions.compute_conclusions(
                canonical_state, syntax_space, semantic_space
            )

        full_concl_mask = to_mask(bucket["conclusions"], stmt_to_idx)

        for recipe_signature in bucket["variants"]:
            P = len(recipe_signature)
            if P < 2:
                continue
            single_union = 0
            for idx in recipe_signature:
                single_union |= single_concl_masks[idx]

            multi_mask = full_concl_mask & ~single_union
            if multi_mask == 0:
                continue  # no multi-premise conclusions worth exporting
            
            multi_stmts = [s for s in bucket["conclusions"] if (multi_mask >> stmt_to_idx[s]) & 1]
            prem_base = base_terms_in_recipe(recipe_signature, syntax_space)

            multi_stmts = [
                s for s in multi_stmts
                if base_terms_in_stmt(s).issubset(prem_base)
            ]
            if not multi_stmts:
                continue

            conclusion_groups = defaultdict(list)
            for stmt in multi_stmts:
                key = conclusion_group_key(stmt, semantic_space, memo)
                conclusion_groups[key].append(stmt)

            conclusions_groups = []
            for alts in conclusion_groups.values():
                alts = sorted(alts)
                conclusions_groups.append({
                    "canon": statement_to_json(alts[0]),
                    "canon_str": stmt_eng(alts[0]),
                    "alts": [statement_to_json(s) for s in alts[1:]],
                    "alts_str": [stmt_eng(s) for s in alts[1:]],
                })

            C = len(conclusions_groups)

            premise_stmts = [syntax_space.list_of_statements[i] for i in recipe_signature]
            premises_arr = [statement_to_json(s) for s in premise_stmts]
            premises_str = [stmt_eng(s) for s in premise_stmts]
            
            pid = signature_id(recipe_signature)

            if C > 0:
                folder = os.path.join(out_root, f"terms_{term_count}", "with_conclusions",
                                      f"premises_{P}", f"conclusions_{C}")
            else:
                folder = os.path.join(out_root, f"terms_{term_count}", "no_conclusions",
                                      f"premises_{P}")

            buckets[folder].append({
                "meta": {"id": pid, "recipe_signature": list(recipe_signature)},
                "premises": json.dumps(premises_arr, separators=(",", ":")),
                "premises_str": premises_str,
                "conclusions_groups": json.dumps(conclusions_groups, separators=(",", ":")),
                "conclusions_groups_str": [{"canon": g["canon_str"], "alts": g["alts_str"]} for g in conclusions_groups],
            })


    for folder, items in buckets.items():
        os.makedirs(folder, exist_ok=True)
        out_path = os.path.join(folder, "puzzles.json")
        with open(out_path, "w", encoding="utf-8") as f:
            json.dump(items, f, indent=1)

def main():

    term_count = 3

    syntax_space = syntax.SyntaxSpace.build_syntax_space(term_count)
    semantic_space = semantics.SemanticSpace.build_semantic_space(term_count)


    memo_instance = memo.Memo(term_count=term_count, semantic_space=semantic_space)
    #memo_instance.seen_depth_normalized.clear()
    #memo_instance.recipes.clear()
    dag_instance = DAG(
            nodes={},
            syntax_space=syntax_space,
            semantic_space=semantic_space,
            memo=memo_instance
            )

    root_state = (semantic_space.all_regions_mask, tuple())
    print(len(memo_instance.seen_depth_normalized))
    accepted, canonical_root_state = memo_instance.accept(root_state, depth=0, recipe_signature=())
    assert accepted != memo.AcceptKind.REJECT

    root_node = Node(
                    allowed_regions_mask=canonical_root_state[0],
                    existence_constraints_masks=canonical_root_state[1],
                    depth=0,
                    last_index=-1,
                    used_base_terms_mask=0,
                    pair_status_mask=0,
                    empty_base_terms_mask=0,
                    universal_polarity_mask=0,
                    recipe_signature=()
                    )



    dag_instance.get_or_create_DAG_node(canonical_root_state, 0)

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

    print("Initiating Search")
    search = bfs_engine.search_tree(root_node, dag_instance)
    print("Search Complete...")
    print("Exporting to json")
    export_from_memo(term_count, memo_instance, syntax_space, semantic_space, out_root="puzzle_exports")
    print("Export complete")





if __name__ == "__main__":
    profiler = cProfile.Profile()
    profiler.enable()
    main()
    profiler.disable()

    stats = pstats.Stats(profiler).strip_dirs().sort_stats("cumtime")
    stats.print_stats(20)
