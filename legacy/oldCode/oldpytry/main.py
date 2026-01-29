from precompute import build_precomputation
from dfs import run_dfs, SearchConfig, DFSState
from filters import analyze_premises, RecipeConfig

def make_callback(precompute, recipe_store, recipe_config):
    calls = {"count": 0}
    def handle_state(state: DFSState) -> None:
        calls["count"] += 1
        premises = state.premises
    # TODO: Analyze premises need precomp, premises list, global recipe registry
        analyze_premises(
            precomp = precompute, 
            premises = state.premises,
            recipe_store = recipe_store,
            recipe_config = recipe_config,
        )
    handle_state.calls = calls
    return handle_state

def main():
    print("Building precomputation...")
    precomputation = build_precomputation(term_count=3)

    search_config = SearchConfig(
        min_premises=2,
        max_premises=3,
        max_premise_terms=4,
        require_all_terms_in_premises=True,
    )


    recipe_config = RecipeConfig(
        max_decoy_terms=4,
        require_single_conclusion=False,
        min_conclusions=0,
        max_conclusions=None,
        require_irreducible_premises=False,
        require_all_terms_in_premises=False,
        min_active_terms_in_conclusions=0,
    )
    
    recipe_store: dict = {}
    
    callback = make_callback(precomputation, recipe_store, recipe_config)
    run_dfs(
        precomp=precomputation, 
        config=search_config, 
        callback=callback,
    )

    print("analyze premise calls:", callback.calls["count"])
    print(f"Found {len(recipe_store)} recipes:")

    max_to_show = 10
    for i, (key, recipe) in enumerate(recipe_store.items()):
        if i >= max_to_show:
            break
        print(f"Recipe {i+1}:", {key})
        print("Premises:")
        for (form, subject, predicate) in recipe['premises']:
            print(f"  {form} {precomputation.term_names[subject]} {precomputation.term_names[predicate]}")
        print("Conclusions:")
        for (form, subject, predicate) in recipe['conclusions']:
            print(f"  {form} {precomputation.term_names[subject]} {precomputation.term_names[predicate]}")
        
    with open("recipes.txt", "w", encoding="utf-8") as f:
        f.write(f"Total recipes: {len(recipe_store)}\n\n")
        for i, (key, recipe) in enumerate(recipe_store.items(), start=1):
            f.write(f"Recipe #{i}, key={key}\n")
            f.write("Premises:\n")
            for (form, s, p) in recipe["premises"]:
                f.write(f"  {form} {precomputation.term_names[s]} {precomputation.term_names[p]}\n")
            f.write("Conclusions:\n")
            for (form, s, p) in recipe["conclusions"]:
                f.write(f"  {form} {precomputation.term_names[s]} {precomputation.term_names[p]}\n")
            f.write("\n")

if __name__ == "__main__":
    main()