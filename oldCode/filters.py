#analyze_premises(...) and all closure-based and config-based filters you already have.
from precompute import LogicPrecomputation
from dataclasses import dataclass
from typing import Optional, Any, Dict, Hashable, Tuple, List


Recipe = Dict[str, Any] 
RecipeKey = Hashable # (premise_count, frozenset(canonical_conclusions))
Statement = Tuple[str, int, int] # (form, subject_index, predicate_index)

@dataclass
class RecipeConfig:
    max_decoy_terms: int = 0 # terms in premises but not in conclusions

    #Conclusion filters
    require_single_conclusion: bool = False
    require_universal_conclusions: bool = False
    min_conclusions: int = 0
    max_conclusions: Optional[int] = None

    # Premise filters
    require_irreducible_premises: bool = True

    # Term activity filters
    min_active_terms_in_conclusions: int = 0
    require_all_terms_in_premises: bool = False

    # Symmetry / Canonicalization
    canonicalize_EI_conclusions: bool = True

def analyze_premises(
        precomp: LogicPrecomputation,
        premises: Tuple[int, ...],
        recipe_store: Dict[RecipeKey, Recipe],
        recipe_config,
) -> None:
    # Translate indices -> statement triples (form, subject, predicate)
    premise_statements = [precomp.statements[i] for i in premises]

    # Compute all entails AEIO conclusions (raw, not normalized)
    raw_conclusions = find_conclusions(
        precomp.truth_table,
        precomp.statement_index,
        premise_statements,
        precomp.statements
    )

    #Normalize conclusions for puzzle use per config
    novel_conclusions = normalize_conclusions_for_puzzle(
        premise_statements,
        raw_conclusions,
    )

    # Compute term activity and decoys
    term_activity = compute_term_activity(
        premise_statements,
        novel_conclusions,
    )
    premise_terms = term_activity['premise_terms']
    conclusion_terms = term_activity['conclusion_terms']
    decoy_terms = term_activity['decoy_terms']

    # Apply filters from recipe_config
    conclusion_count = len(novel_conclusions)
    if conclusion_count < recipe_config.min_conclusions:
        return
    if recipe_config.max_conclusions is not None and conclusion_count > recipe_config.max_conclusions:
        return
    if recipe_config.require_single_conclusion and conclusion_count != 1:
        return
    if len(decoy_terms) > recipe_config.max_decoy_terms:
        return
    
    # Term activity filters
    if recipe_config.require_all_terms_in_premises:
        if not premise_terms.issubset(conclusion_terms):
            return
    if len(conclusion_terms) < recipe_config.min_active_terms_in_conclusions:
        return
    
    # Symmetry filter
    if recipe_config.canonicalize_EI_conclusions:
        norm_conclusions = canonicalize_EI_conclusions(novel_conclusions)
    
    # Closure-based redundancy check
    if recipe_config.require_irreducible_premises:
        if has_redundant_premises(
            precomp.truth_table,
            precomp.statement_index,
            premise_statements,
            precomp.statements,
        ):
            return
        
    # boring premise/conclusion data
    only_isolated_I = has_only_isolated_I_conclusions(premise_statements, novel_conclusions)
    ae_conflict = has_AE_polarity_conflict_on_pair(premise_statements)
    universal_particular_overlap = has_universal_particular_overlap(premise_statements)

    if only_isolated_I or ae_conflict or universal_particular_overlap:
        return
    
    # Diagnostics flags
    diagnostics = {
        'onlyIsolateI': has_only_isolated_I_conclusions(premise_statements, novel_conclusions),
        'AEConflict': has_AE_polarity_conflict_on_pair(premise_statements),
        'universalParticularOverlap': has_universal_particular_overlap(premise_statements),
        'redundantPremises': False,
    }

    closure = closure_key(
        precomp.truth_table,
        precomp.statement_index,
        premise_statements,
        precomp.statements,
    )
    key: RecipeKey = (len(premises), closure)

    if key in recipe_store:
        return
    
    recipe_store[key] = {
        'premiseIndices': premises,
        'premises': premise_statements,
        'conclusions': norm_conclusions,
        'term_activity': term_activity,
        'diagnostics': diagnostics,
    }

def entails(
        truth_table: List[List[bool]],
        statement_index: Dict[Statement, int],
        premises: List[Statement],
        conclusion: Statement,
)-> bool:
    premise_cols = [statement_index[p] for p in premises]
    conclusion_col = statement_index[conclusion]

    for row in truth_table: # each row = one model
        # Check if this model satisfies all premises
        premises_true = True
        for col in premise_cols:
            if not row[col]:
                premises_true = False
                break
            if not premises_true: #This model does not satisfy all premises
                continue

            # Model satisfies all premises
            if not row[conclusion_col]:
                return False # Found a model that satisfies premises but not conclusion
    
    return True # No Countermodel found

def find_conclusions(
        truth_table: List[List[bool]],
        statement_index: Dict[Statement, int],
        premises: List[Statement],
        all_statements: List[Statement],
) -> List[Statement]:
    premise_set = set(premises)
    conclusions: List[Statement] = []
    seen: set[Statement] = set()

    for statement in all_statements:
        if statement in premise_set:
            continue # Skip premises
        if statement in seen:
            continue # Skip already found conclusions
        seen.add(statement)
        if entails(truth_table, statement_index, premises, statement):
            conclusions.append(statement)
    return conclusions

def collect_terms(statements: List[Statement]) -> set[int]:
    term_set: set[int] = set()
    for form, subject, predicate in statements:
        term_set.add(subject)
        term_set.add(predicate)
    return term_set

def compute_term_activity(
        premises: List[Statement],
        conclusions: List[Statement],
) -> Dict[str, Any]:
    premise_terms = collect_terms(premises)
    conclusion_terms = collect_terms(conclusions)
    
    active_terms = conclusion_terms
    decoy_terms = premise_terms - conclusion_terms

    return {
        'premise_terms': premise_terms,
        'conclusion_terms': conclusion_terms,
        'active_terms': active_terms,
        'decoy_terms': decoy_terms,
    }

def canonical_statement(statement: Statement) -> Statement:
    form, subject, predicate = statement
    if form in ('E', 'I') and subject > predicate:
        return (form, predicate, subject)
    return statement

def _unordered_pairs_from_premises(premises: List[Statement]) -> set[Tuple[int, int]]:
    pairs: set[Tuple[int, int]] = set()
    for(_, subject, predicate) in premises:
        a, b = (subject, predicate) if subject <= predicate else (predicate, subject)
        pairs.add((a, b))
    return pairs

def normalize_conclusions_for_puzzle(
        premises: List[Statement],
        raw_conclusions: List[Statement],
) -> List[Statement]:
    normalized_set: set[Statement] = set()
    premise_pairs = _unordered_pairs_from_premises(premises)

    for statement in raw_conclusions:
        form, subject, predicate = statement
        if subject == predicate:
            continue

        # Unordered pair of this conclusion
        a,b = (subject, predicate) if subject <= predicate else (predicate, subject)

        # Drop if premises already directled relate this pair
        if (a,b) in premise_pairs:
            continue

        if form in ('E', 'I'):
            statement = canonical_statement(statement)
        normalized_set.add(statement)

    # Return as a list in a stable order (sorted by form, subject, predicate)         
    normalized_list = sorted(normalized_set, key=lambda s: (s[0], s[1], s[2])) #lambda is here because sorted needs a function
    return normalized_list

def closure_key(
        truth_table: List[List[bool]],
        statement_index: Dict[Statement, int],
        premises: List[Statement],
        all_statements: List[Statement],
) -> frozenset[Statement]:
    raw_conclusions = find_conclusions(
        truth_table,
        statement_index,
        premises,
        all_statements
    )
    
    # Canonicalize E/I
    conclusions = find_conclusions(
        truth_table,
        statement_index,
        premises,
        all_statements
        )
    canon = [canonical_statement(s) for s in conclusions]
    return frozenset(canon)

def _normalized_non_premise_closure(
        truth_table: List[List[bool]],
        statement_index: Dict[Statement, int],
        premises: List[Statement],
        all_statements: List[Statement],
        ) -> List[Statement]:

    raw_conclusions = find_conclusions(
        truth_table,
        statement_index,
        premises,
        all_statements
    )
    normalized_conclusions = normalize_conclusions_for_puzzle(
        premises,
        raw_conclusions,
    )
    return normalized_conclusions

def has_redundant_premises(
        truth_table: List[List[bool]],
        statement_index: Dict[Statement, int],
        premises: List[Statement],
        all_statements: List[Statement]
) -> bool:
    
    full_normalized_closure = _normalized_non_premise_closure(
        truth_table,
        statement_index,
        premises,
        all_statements,
    )

    full_normalized_set = set(full_normalized_closure)

    n = len(premises)
    for i in range(n):
        reduced_premises = premises[:i] + premises[i+1:]
        reduced_normalized_closure = _normalized_non_premise_closure(
            truth_table,
            statement_index,
            reduced_premises,
            all_statements,
        )
        reduced_normalized_set = set(reduced_normalized_closure)

        if reduced_normalized_set == full_normalized_set:
            return True # Found redundancy

    return False

def canonicalize_EI_conclusions(
        conclusions: List[Statement]
) -> List[Statement]:
    canon_set: set[Statement] = set()
    for statement in conclusions:
        form, _, _ = statement
        if form in ('E', 'I'):
            statement = canonical_statement(statement)
        canon_set.add(statement)
    canon_list = sorted(canon_set, key=lambda s: (s[0], s[1], s[2]))
    return canon_list

def is_isolated_pair(Subject:int, Predicate:int, premises: List[Statement]) -> bool:
    for form, subject, predicate in premises:
        if {subject, predicate} != {Subject, Predicate}:
            return False
        if form not in ('A', 'E'):
            return False
    return True

def has_only_isolated_I_conclusions(
        premises: List[Statement],
        conclusions: List[Statement],
) -> bool:
    if not conclusions:
        return False
    for form, subject, predicate in conclusions:
        if form == 'I':
            return False
        if not is_isolated_pair(subject, predicate, premises):
            return False
    return True

def has_AE_polarity_conflict_on_pair(premises: List[Statement]) -> bool:
    by_pair: Dict[Tuple[int,int], set[str]] = {}

    for form, subject, predicate in premises:
        key = (min(subject, predicate), max(subject, predicate))
        forms = by_pair.setdefault(key, set())
        if form in ('A', 'E'):
            forms.add(form)

    for forms in by_pair.values():
        if 'A' in forms and 'E' in forms:
            return True
    return False

def has_universal_particular_overlap(premises: List[Statement]) -> bool:
    by_pair: Dict[Tuple[int,int], set[str]] = {}

    for form, subject, predicate in premises:
        key = (subject, predicate)
        forms = by_pair.setdefault(key, set())
        forms.add(form)
    
    for forms in by_pair.values():
        if 'A' in forms and 'I' in forms:
            return True
        if 'E' in forms and 'O' in forms:
            return True
    return False
