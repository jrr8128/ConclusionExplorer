#Builds MODELS, STATEMENTS, TRUTH_TABLE
#Builds STMT_FORM, STMT_SUBJ, STMT_PRED, MODELS_TRUE_MASK, constants

from __future__ import annotations

import itertools
from dataclasses import dataclass
from typing import Dict, List, Tuple


Pattern = int # bitmask representing primitive pattern, e.g. 0b101 -> 1st and 3rd term present
Statement = Tuple[str, int, int] # (form, subject_index, predicate_index)
Model = int # primitive pattern strings, "101", "011"

@dataclass
class LogicPrecomputation:
    term_count: int

    #Symbolic Data
    term_names: Dict[int, str] #0 -> "A", 1 -> "B", ...
    primitive_patterns: List[Pattern] # ["000", "001", "010", ..., "111"]
    models: List[Model] # All non-empty subsets of primitive patterns
    statements: List[Statement] # All AEIO statements over ordered distinct pairs
    statement_index: Dict[Statement, int] # mapes statement -> column index

    #Truth Info
    truth_table: List[List[bool]] # rows: models, cols: statements -> bool

    #Compact Semantic Structures for DFS
    models_true_mask: List[int] # for each statement, bitmask of models where true
    full_model_mask: int # bitmask with all models set to 1

    #Statement Metadata arrays for fast access in DFS
    statement_form: List[str] # [statement_index] -> form "A","E","I","O"
    statement_subject: List[int] # [statement_index] -> subject term index
    statement_predicate: List[int] # [statement_index] -> predicate term index


def generate_primitive_patterns(term_count: int) -> List[Pattern]:
    patterns: List[Pattern] = []

    # 1 to 2^term_count - 1, skipping 0 (empty pattern)
    for mask in range(1, 1 << term_count):
        patterns.append(mask)
    return patterns

def term_is_present(pattern: Pattern, term: int) -> bool:
    return (pattern & (1 << term)) != 0  #check if term bit is set

def check_for_all(model_mask: Model, patterns: List[Pattern], subject: int, predicate: int) -> bool:
    # A: For all individuals, if S then P. Or ALl S are P.
    n = len(patterns)
    for index in range(n):
        if not (model_mask & (1 << index)):
            continue  # pattern not in model
        pattern = patterns[index]
        if term_is_present(pattern, subject) and not term_is_present(pattern, predicate): # S present but P absent
            return False
    return True

def check_for_none(model_mask: Model, patterns: List[Pattern], subject: int, predicate: int) -> bool:
    # E: For no individuals, if S then P. Or No S are P.
    n = len(patterns)
    for index in range(n):
        if not (model_mask & (1 << index)):
            continue  # pattern not in model
        pattern = patterns[index]
        if term_is_present(pattern, subject) and term_is_present(pattern, predicate): # S and P both present
            return False
    return True

def check_for_some(model_mask: Model, patterns: List[Pattern], subject:int, predicate:int) -> bool:
    # I: There exists at least one individual such that if S then P. Or Some S are P.
    n = len(patterns)
    for index in range(n):
        if not (model_mask & (1 << index)):
            continue  # pattern not in model
        pattern = patterns[index]
        if term_is_present(pattern, subject) and term_is_present(pattern, predicate): # S and P both present
            return True
    return False

def check_for_some_not(model_mask: Model, patterns: List[Pattern], subject:int, predicate:int) -> bool:
    # O: There exists at least one individual such that if S then not P. Or Some S are not P.
    n = len(patterns)
    for index in range(n):
        if not (model_mask & (1 << index)):
            continue  # pattern not in model
        pattern = patterns[index]
        if term_is_present(pattern, subject) and not term_is_present(pattern, predicate): # S present but P absent
            return True
    return False

def evaluate_statement(model_mask: Model, patterns: List[Pattern], form: str, subject: int, predicate: int) -> bool:
    if form == "A":
        return check_for_all(model_mask, patterns, subject, predicate)
    elif form == "E":
        return check_for_none(model_mask, patterns, subject, predicate)
    elif form == "I":
        return check_for_some(model_mask, patterns, subject, predicate)
    elif form == "O":
        return check_for_some_not(model_mask, patterns, subject, predicate)
    else:
        raise ValueError(f"Unknown statement form: {form}")
    
def generate_models(patterns: List[Pattern]) -> List[Model]:
    all_models: List[Model] = []

    n = len(patterns)
    for model_mask in range(1, 1 << n): 
        all_models.append(model_mask)
    return all_models

def build_truth_table(models: List[Model], patterns: List[Pattern], statements: List[Statement]) -> List[List[bool]]:
    truth_table: List[List[bool]] = []

    for model_mask in models:
        row: List[bool] = []
        for (form, subject, predicate) in statements:
            value = evaluate_statement(model_mask, patterns, form, subject, predicate)
            row.append(value)
        truth_table.append(row)
    return truth_table

def generate_all_statements(term_count: int) -> List[Statement]:
    all_statements: List[Statement] = []
    for subject in range(term_count):
        for predicate in range(term_count):
            if subject == predicate:
                continue
            for form in ["A", "E", "I", "O"]:
                all_statements.append((form, subject, predicate))
    return all_statements

def build_models_true_mask(truth_table: List[List[bool]]) -> List[int]:
    if not truth_table:
        return []
    
    num_models = len(truth_table)
    num_statements = len(truth_table[0])

    masks: List[int] = [0] * num_statements
    for model_index, row in enumerate(truth_table):
        bit = 1 << model_index
        for statement_index, value in enumerate(row):
            if value:
                masks[statement_index] |= bit
    return masks

def extract_statement_metadata(statements: List[Statement]) -> Tuple[List[str], List[int], List[int]]:
    statement_form: List[str] = []
    statement_subject: List[int] = []
    statement_predicate: List[int] = []

    for (form, subject, predicate) in statements:
        statement_form.append(form)
        statement_subject.append(subject)
        statement_predicate.append(predicate)
    return statement_form, statement_subject, statement_predicate

def build_precomputation(term_count: int) -> LogicPrecomputation:
    
    #1. Term naming: 0 -> "A", 1 -> "B", ...
    term_names: Dict[int, str] = {i: chr(ord('A') + i) for i in range(term_count)}

    primitive_patterns = generate_primitive_patterns(term_count)

    models = generate_models(primitive_patterns)
    print(f"Generated {len(models)} models for {term_count} terms.")

    statements = generate_all_statements(term_count)
    statement_index: Dict[Statement, int] = {
        statement: index for index, statement in enumerate(statements)
    }
    print(f"Generated {len(statements)} statements for {term_count} terms.")
    truth_table = build_truth_table(models, primitive_patterns, statements)

    # For DFS: models where each statement is true
    models_true_mask = build_models_true_mask(truth_table)
    full_model_mask = (1 << len(models)) - 1 if models else 0 # all bits set to 1

    # Statement metadata arrays for fast access in DFS
    statement_form, statement_subject, statement_predicate = extract_statement_metadata(statements)

    # Pack into LogicPrecomputation
    return LogicPrecomputation(
        term_count=term_count,
        term_names=term_names,
        primitive_patterns=primitive_patterns,
        models=models,
        statements=statements,
        statement_index=statement_index,
        truth_table=truth_table,
        models_true_mask=models_true_mask,
        full_model_mask=full_model_mask,
        statement_form=statement_form,
        statement_subject=statement_subject,
        statement_predicate=statement_predicate
    )