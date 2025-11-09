import itertools


#Terms
A = 0
B = 1
C = 2
D = 3


primitivePatterns = [
     #'0000', #0 - in none of the terms (not used in models as it represents an empty universe / vacuous truth)
    '0001', #1 - A only
    '0010', #2 - B only
    '0011', #3 - A and B
    '0100', #4 - C only
    '0101', #5 - A and C
    '0110', #6 - B and C
    '0111', #7 - A and B and C
    '1000', #8 - D only
    '1001', #9 - A and D
    '1010', #10 - B and D
    '1011', #11 - A and B and D
    '1100', #12 - C and D
    '1101', #13 - A and C and D
    '1110', #14 - B and C and D
    '1111'  #15 - A and B and C and D
]

#Helper function to check if a term is present in a given pattern
# Pattern is a 4 bit string representing presence(1)/absence(0) of terms A,B,C,D
# Term is an integer 0-3 representing A-D
def term_is_present(pattern, term):
    if not (0 <= term <= 3):
        raise ValueError("Term must be an integer between 0 and 3 inclusive.")
    if not isinstance(pattern, str) or len(pattern) != 4 or any(c not in '01' for c in pattern):
        raise ValueError("Pattern must be a 4-character string of '0's and '1's.")
    
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