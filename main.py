import itertools
import time


formText ={
    'A': 'All {S} are {P}',
    'E': 'No {S} are {P}',
    'I': 'Some {S} are {P}',
    'O': 'Some {S} are not {P}'
}

def generate_primitive_patterns(termCount):
    patterns = []
    for i in range(1, 2**termCount): # Start from 1 to avoid '0000'
        bits = []
        for bit in range(termCount):
            if i & (1 << bit):
                bits.append('1')
            else:
                bits.append('0')
        patterns.append(''.join(bits))
    return patterns


#Helper function to check if a term is present in a given pattern
# Pattern is a 4 bit string representing presence(1)/absence(0) of terms A,B,C,D
# Term is an integer 0-3 representing A-D
def term_is_present(pattern, term):
    if not (0 <= term <= 3):
        raise ValueError("Term must be an integer between 0 and 3 inclusive.")
    if not isinstance(pattern, str) or  any(c not in '01' for c in pattern):
        raise ValueError("Pattern must be a string of '0's and '1's.")
    
    return pattern[term] == '1'

def check_for_all(model, subject, predicate):
    # Check if for all patterns in the model, if the subject term is present,
    # then the predicate term must also be present.
    # If there exists a pattern where subject is present and predicate is absent, return False.
    for pattern in model:
        if term_is_present(pattern, subject) and not term_is_present(pattern, predicate):
            return False
    return True

def check_for_none(model, subject, predicate):
    # Check if for all patterns in the model, if the subject term is present,
    # then the predicate term must be absent.
    # If there exists a pattern where subject is present and predicate is also present, return False.
    for pattern in model:
        if term_is_present(pattern, subject) and term_is_present(pattern, predicate):
            return False
    return True

def check_for_some(model, subject, predicate):
    # Check if there exists at least one pattern in the model where both
    # the subject term and predicate term are present.
    for pattern in model:
        if term_is_present(pattern, subject) and term_is_present(pattern, predicate):
            return True
    return False

def check_for_some_not(model, subject, predicate):
    # Check if there exists at least one pattern in the model where
    # the subject term is present and the predicate term is absent.
    for pattern in model:
        if term_is_present(pattern, subject) and not term_is_present(pattern, predicate):
            return True
    return False

def evaluate_statement(model, form, subject, predicate):
    if form == 'A': #A - All S are P (Universal Affirmative)
        return check_for_all(model, subject, predicate)
    elif form == 'E': #E - No S are P (Universal Negative)
        return check_for_none(model, subject, predicate)
    elif form == 'I': #I - Some S are P (Particular Affirmative)
        return check_for_some(model, subject, predicate)
    elif form == 'O': #O - Some S are not P (Particular Negative)
        return check_for_some_not(model, subject, predicate)
    else:
        raise ValueError("Invalid form. Must be one of 'A', 'E', 'I', 'O'.")
    
def model_satisfies_premises(model, premises):
    for premise in premises:
        form, subject, predicate = premise
        if not evaluate_statement(model, form, subject, predicate):
            return False
    return True

# Generates the power set of input pattern set
def generate_models(patterns):
    allModels = []
    for subsetSize in range(1, len(patterns)+1):
        for combination in itertools.combinations(patterns, subsetSize):
            allModels.append(combination)
    return allModels


def generate_all_statements(termCount):
    allStatements = []
    for subject in range(termCount):
        for predicate in range(termCount):
            if subject != predicate:
                for form in ['A', 'E', 'I', 'O']:
                    allStatements.append( (form, subject, predicate) )
    return allStatements

def find_conclusions(truthTable, statementIndex, premises, statements):
    premiseSet = set(premises)
    conclusions = []
    seen = set()
    for statement in statements:
        if statement in premiseSet:
            continue
        if statement in seen:
            continue
        seen.add(statement)
        if entails(truthTable, statementIndex, premises, statement):
            conclusions.append(statement)
    return conclusions

def model_satisfies_premises(model, premises):
    for premise in premises:
        form, subject, predicate = premise
        if not evaluate_statement(model, form, subject, predicate):
            return False
    return True

# Check if the conclusion is entailed by the premises across all models
# Entailment means that in every model where all premises are true, the conclusion is also true
# If a model that satisfies the premises but not the conclusion is found, return False
def entails(truthTable, statementIndex, premises, conclusion):
    premiseCols = [statementIndex[premise] for premise in premises]
    conclusionCol = statementIndex[conclusion]
    for mIndex in range(len(truthTable)):  # Iterate over models
        row = truthTable[mIndex]
        premisesTrue = True
        for pCol in premiseCols:
            if not row[pCol]:
                premisesTrue = False
                break
        if not premisesTrue:
                continue # This model does not satisfy all premises, skip
        if not row[conclusionCol]:
            return False  # Found a model where premises are true but conclusion is false (countermodel)
    return True

def collect_terms(statements):
    termSet = set()
    for form, subject, predicate in statements:
        termSet.add(subject)
        termSet.add(predicate)
    return termSet

def compute_term_activity(premises, conclusions):
    premiseTerms = collect_terms(premises)
    conclusionTerms = collect_terms(conclusions)

    activeTerms = conclusionTerms
    decoyTerms = premiseTerms - conclusionTerms

    return {
        'premiseTerms': premiseTerms,
        'conclusionTerms': conclusionTerms,
        'activeTerms': activeTerms,
        'decoyTerms': decoyTerms
    }


def statement_to_string(statement):
    form, subject, predicate = statement
    subjectName = termNames[subject]
    predicateName = termNames[predicate]
    template = formText.get(form)
    if template is None:
        return f"Unknown form {form} {subjectName} {predicateName}"
    return template.format(S=subjectName, P=predicateName)
        
def print_results(premises, conclusions, termActivity, isValid):
    print("Premises:")
    for premise in premises:
        print(" - " + statement_to_string(premise))
    if conclusions:
        print(f"\nFound {len(conclusions)} conclusions:")
        for conclusion in conclusions:
            print(" - " + statement_to_string(conclusion))
    else:
        print(" No conclusions found.")

    print("\nTerm Activity Analysis:")
    if len(termActivity['premiseTerms']) == len(termActivity['conclusionTerms']):
        print(" All terms in premises are active in conclusions.")
    print("There are [", len(termActivity['activeTerms']), "] active terms: ", [termNames[t] for t in termActivity['activeTerms']])
    if termActivity['decoyTerms']:
        print("Decoy Terms:", [termNames[t] for t in termActivity['decoyTerms']])
    else:
        print(" No decoy terms found.")

    print("\nRecipe: " + ("Valid" if isValid else "Invalid"))
    if not isValid:
        count = 0
        if len(premises) > config['maxPremises']:
            count += 1
            print(f"{count}: # Premises exceeded: {len(premises)} > {config['maxPremises']}")
        if len(termActivity['decoyTerms']) > config['maxDecoyTerms']:
            count += 1
            print(f"{count}: # Decoy Terms exceeded: {len(termActivity['decoyTerms'])} > {config['maxDecoyTerms']}")
        if config['requireSingleConclusion'] and len(conclusions) != 1:
            count += 1
            print(f"{count}: Single conclusion required, found: {len(conclusions)}")
        if not premises_have_model(truthTable, statementIndex, premises):
            count += 1
            print(f"{count}: Premises have no model (are contradictory).")


def analyze_premises(truthTable, statementIndex, premises, statements, config):
    hasModel = premises_have_model(truthTable, statementIndex, premises)
    # Find all entailed AEIO statements
    conclusions = find_conclusions(truthTable, statementIndex, premises, statements)

    # Compute term activity
    termActivity = compute_term_activity(premises, conclusions)
    premiseTerms = termActivity['premiseTerms']
    conclusionTerms = termActivity['conclusionTerms']
    activeTerms = termActivity['activeTerms']
    decoyTerms = termActivity['decoyTerms']

    # Filter results based on config
    tooManyPremises = len(premises) > config['maxPremises']
    tooManyDecoys = len(decoyTerms) > config['maxDecoyTerms']

    if config['requireSingleConclusion']:
        singleConclusion = (len(conclusions) == 1)
    
    isValidRecipe = hasModel and (not tooManyPremises) and (not tooManyDecoys) and singleConclusion
    
    recipe = {
        'premises': premises,
        'conclusions': conclusions,
        'termActivity': {
            'premiseTerms': premiseTerms,
            'conclusionTerms': conclusionTerms,
            'activeTerms': activeTerms,
            'decoyTerms': decoyTerms
        },
        'isValid': isValidRecipe
    }


    return recipe

def generate_premise_sets(allStatements, maxPremises):
    premiseSets = []
    for size in range(1, maxPremises + 1):
        for premiseCombination in itertools.combinations(allStatements, size):
            premiseSets.append(list(premiseCombination))
    print(f"DEBUG: Generated {len(premiseSets)} premise sets up to size {maxPremises}.")
    return premiseSets

def premises_have_model(truthTable, statementIndex, premises):
    premiseCols = [statementIndex[premise] for premise in premises]
    for row in truthTable:
        if all(row[pCol] for pCol in premiseCols):
            return True
    return False

def canonical_statement(statement):
    form, subject, predicate = statement
    if form in ('E', 'I'):
        if subject > predicate:
            return (form, predicate, subject)
        else:
            return statement
    return statement


## Main Execution
config = {
    "maxPremises": 2,
    "maxDecoyTerms": 1,
    "requireSingleConclusion": True,
    "maxPremiseTerms": 4
}

# Precomputing which statements are true in which models for optimization
# Instead of having the entails function generate and compare models repeatedly
termcount = 2  # Number of terms (A, B, C, D)

primitivePatterns = generate_primitive_patterns(termcount)
termNames = {i: chr(ord('A') + i) for i in range(termcount)}  # Map 0->A, 1->B, etc.

startPrecompute = time.perf_counter()
allStatements = generate_all_statements(termcount) #Candidate conclusions that need to be checked
statementIndex = {statement: index for index, statement in enumerate(allStatements)}

models = list(generate_models(primitivePatterns))

truthTable = []
for model in models:
    row = []
    for statement in allStatements:
        form, subject, predicate = statement
        result = evaluate_statement(model, form, subject, predicate)
        row.append(result)
    truthTable.append(row)

endPrecompute = time.perf_counter()
precomputeDuration = endPrecompute - startPrecompute
print(f"Precomputation Time: {precomputeDuration:.4f} seconds")

#Truth Table Stats
print(f"Total Models Generated: {len(models)}")
print(f"Total Statements Evaluated: {len(allStatements)}")
print(f"Truth Table Size: {len(truthTable)} rows x {len(truthTable[0])} columns")


#Example Test Case
checkTestCase = False
if (checkTestCase):
    testPremises = [
        ('A', 0, 1),  # All A are B
        ('A', 1, 2),  # All B are C
        #('E', 1, 2)   # No B are C
    ]

    testConclusion = ('E', 0 , 2)  # No A are C
    print("Entails?", entails(truthTable, statementIndex, testPremises, testConclusion))

    recipe = analyze_premises(truthTable, statementIndex, testPremises, allStatements, config)
    print_results(recipe['premises'], recipe['conclusions'], recipe['termActivity'], recipe['isValid'])

startSize4Recipes = time.perf_counter()
generateAllRecipes = True
if (generateAllRecipes):
    validRecipes = []
    premiseSets = generate_premise_sets(allStatements, config['maxPremises'])
    validRecipeCount = 0
    sizePremiseSets = len(premiseSets)
    seenKeys = set()
    uniqueRecipes = []

    for premises in premiseSets:
        #print(f"Analyzing Premises Set #{premiseCount + 1} of {sizePremiseSets} premises...")
        # Skip premise sets that exceed max premise terms
        if len(collect_terms(premises)) > config['maxPremiseTerms']:
            continue

        recipe = analyze_premises(truthTable, statementIndex, premises, allStatements, config)
        if not recipe['isValid']:
            continue
        validRecipeCount += 1

        normConclusions = [canonical_statement(c) for c in recipe['conclusions']]
        premiseCount = len(recipe['premises'])
        closure = frozenset(normConclusions)

        recipeKey = (premiseCount, closure)
        if recipeKey in seenKeys:
            continue
        seenKeys.add(recipeKey)
        uniqueRecipes.append(recipe)
        

    print(f"\nTotal Recipes Analyzed: {sizePremiseSets}")
    if validRecipeCount > 0:
        print(f" Total Valid Recipes Found: {validRecipeCount}")
        print(f" Unique Valid Recipes Found: {len(uniqueRecipes)}")
        print(f" Invalid Recipes Discarded: {sizePremiseSets - validRecipeCount}")
    else:
        print(" No Valid Recipes Found.")


endSize4Recipes = time.perf_counter()
size4RecipesDuration = endSize4Recipes - startSize4Recipes
print(f"Size 4 Recipes Generation Time: {size4RecipesDuration:.4f} seconds")


print("\nFirst 5 Unique Valid Recipes:")
print("-"*30)
for i, recipe in enumerate(uniqueRecipes[:5]):
    print(f"Recipe #{i+1}:")
    print_results(recipe['premises'], recipe['conclusions'], recipe['termActivity'], recipe['isValid'])
    print("-"*30)