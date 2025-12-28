import search
import os


term_count = 3
limit_depth = False
max_depth = 18

search_tree = search.build_initial_search_tree(term_count)
term_index_to_label = search_tree.syntax_space.term_index_to_label
nodes = search.search(search_tree, limit_depth, max_depth)

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

groups = {"conclusions": {}, "no_conclusion": {}}

for node in nodes:
    k = len(node.premises)
    bucket = "conclusions" if node.validConclusions else "no_conclusion"
    if k not in groups[bucket]:
        groups[bucket][k] = []
    groups[bucket][k].append(node)

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
