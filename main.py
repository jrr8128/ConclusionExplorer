import itertools
import search
import os


term_count = 4
limit_depth = False
max_depth = 18

search_tree = search.build_initial_search_tree(term_count)
term_index_to_label = search_tree.syntax_space.term_index_to_label
nodes = search.search(search_tree, limit_depth, max_depth)

term_list = search_tree.syntax_space.term_index_to_label
permutations = list(itertools.permutations(range(term_count)))

format_text ={
    0: "All {S} are {P}",
    1: "No {S} are {P}",
    2: "Some {S} are {P}",
    3: "Some {S} are not {P}"
}

def format_statement(statement: tuple[int,int,int], term_index_to_label: tuple[str,...]) -> str:
    q, s, p = statement
    return format_text[q].format(
                            S=term_index_to_label[s],
                            P=term_index_to_label[p]
                            )

def compute_isomorphism(node, permutations):
    best = None

    for permutation in permutations:
        renamed_premises = set()
        renamed_conclusions = set()
        for premise in node.premises:
            if premise[0] in (1,2):
                renamed_premises.add((premise[0], min(permutation[premise[1]], permutation[premise[2]]), max(permutation[premise[1]], permutation[premise[2]])))
            else:
                renamed_premises.add((premise[0], permutation[premise[1]], permutation[premise[2]]))
        for conclusion in node.validConclusions:
            if conclusion[0] in (1,2):
                renamed_conclusions.add((conclusion[0], min(permutation[conclusion[1]], permutation[conclusion[2]]), max(permutation[conclusion[1]], permutation[conclusion[2]])))
            else:
                renamed_conclusions.add((conclusion[0], permutation[conclusion[1]], permutation[conclusion[2]]))
        sorted_premises = sorted(renamed_premises)
        sorted_conclusions = sorted(renamed_conclusions)
        candidate_iso = (tuple(sorted_premises), tuple(sorted_conclusions))
        if best is None:
            best = candidate_iso
        else:
            best = min(best, candidate_iso)
    return best


groups = {"conclusions": {}, "no_conclusion": {}}

added_isomorphisms = {}
skipped = 0
pre_counts = {}
for node in nodes:
    node_isomorphism = compute_isomorphism(node, permutations)
    k = len(node.premises)
    bucket = "conclusions" if node.validConclusions else "no_conclusion"
    if k not in groups[bucket]:
        groups[bucket][k] = []
    iso_key = (bucket,k)
    if iso_key not in pre_counts:
        pre_counts[iso_key] = 0
    pre_counts[iso_key] = pre_counts.get(iso_key, 0) + 1
    if iso_key not in added_isomorphisms:
        added_isomorphisms[iso_key] = set()
    if node_isomorphism in added_isomorphisms[iso_key]:
        skipped += 1
        continue
    else:
        added_isomorphisms[iso_key].add(node_isomorphism)
    groups[bucket][k].append(node)

print("Pre Iso #'s Per Bucket:")
for bucket in ("conclusions", "no_conclusion"):
    print(f"For Bucket: {bucket}")
    for (b, k), count in sorted(pre_counts.items()):
        if b == bucket:
            print(f"# of puzzles with {k} premises: {count}")

print(f"Number of Isomorphisms: ", sum(len(s) for s in added_isomorphisms.values()))
print(f"Number of skipped nodes: ", skipped)

for bucket in groups:
    for k in sorted(groups[bucket]):
        groups[bucket][k].sort(key=lambda n: len(n.validConclusions))



out_dir = os.path.join("output", f"terms_{term_count}")
os.makedirs(out_dir, exist_ok=True)
out_dir_with = os.path.join(out_dir, "conclusions")
os.makedirs(out_dir_with, exist_ok=True)
out_dir_none = os.path.join(out_dir, "no_conclusion")
os.makedirs(out_dir_none, exist_ok=True)

for bucket in groups:
    for k in sorted(groups[bucket]):
        if k < 2:
            continue

        group_nodes = groups[bucket][k]
        path = os.path.join(out_dir, bucket, f"premises_{k}.txt")
        with open(path, "w", encoding="utf-8") as f:
            puzzle_num = 1
            for node in group_nodes:
                f.write(f"Puzzle {puzzle_num}\n")
                f.write("Premises:\n")
                for prem in node.premises:
                    f.write(f"     - {format_statement(prem, term_index_to_label)}\n")
                
                f.write("Conclusions:\n")
                if bucket == "conclusions":
                    for conclusion in sorted(node.validConclusions):
                        f.write(f"     - {format_statement(conclusion, term_index_to_label)}\n")
                else:
                    f.write(f"     - No conclusion.\n")

                f.write("\n")
                puzzle_num += 1
