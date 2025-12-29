# AEIO forms, terms, canonicalization
from __future__ import annotations
from dataclasses import dataclass


form_char_to_index = {
    'A': 0, #All S are P
    'E': 1, #No S are P
    'I': 2, #Some S are P
    'O': 3  #Some S are not P
}

form_index_to_char ={
    0: 'A', #All S are P
    1: 'E', #No S are P
    2: 'I', #Some S are P
    3: 'O'  #Some S are not P
}

statement = tuple[int, tuple(int,int), tuple(int,int)] #(form_index, (subject_term_index, is_positive), (predicate_term_index, is_positive)

@dataclass(frozen=True)
class SyntaxSpace:
    term_count: int
    term_index_to_label: tuple[str, ...] # [term_index] -> term label "A","B","C",...
    term_label_to_index: dict[str, int] # term label "A","B","C",... -> term_index

    list_of_statements: tuple[statement, ...]
    map_statement_to_index: dict[statement, int]

    # Generate term labels and mappings for given term count
    # For example: term_count = 3 -> ('A','B','C'), {'A':0,'B':1,'C':2}
    # Returns two structures:
    # 1. tuple of term labels indexed by term index i.e. for term_count = 3 -> ('A','B','C')
    # 2. dict mapping term labels to term indices i.e. for term_count = 3 -> {'A':0,'B':1,'C':2}
    @staticmethod
    def build_terms(term_count: int) -> tuple[tuple[str, ...], dict[str, int]]:
        terms: list[str] = []
        term_label_to_index: dict[str, int] = {}
        for term_index in range(term_count):
            current_term_label = chr(ord('A') + term_index)
            term_label_to_index[current_term_label] = term_index
            terms += (current_term_label, )
        term_index_to_label = tuple(terms)
        return term_index_to_label, term_label_to_index

    # Generate all possible AEIO statements for given term count skipping reflexive statements
    # For example: term_count = 2 -> ((0,(0,0),(1,0)),(0,(0,1),(1,0)),(0,(0,0),(1,1)),(0,(0,1),(1,1)),...
    # (0,0,1) = All A are B, (0,1,0) = All B are A, (1,0,1) = No A are B, etc.
    # Returns two structures:
    # 1. tuple of all possible statements (form_index, subject_term_index, predicate_term_index) 
    # 2. dict mapping each statement to its index in the tuple ie. {(0,(0,0),(1,0)):0,(0,(0,1),(1,0)):1,...}
    @staticmethod
    def generate_all_statements(term_count: int) ->tuple[tuple[statement, ...], dict[statement, int]]:
        statements = []
        statement_map = {}
        statement_index = 0
        for form_index in range(4): # A,E,I,O
            for subject_index in range(term_count): # 0 -> 'A', 1 -> 'B', ...
                for subject_is_positive in (0,1):
                    for predicate_index in range(term_count): # 0 -> 'A', 1 -> 'B', ...
                        for predicate_is_positive in range(0,1):
                            if (subject_index, subject_is_positive) == (predicate_index, predicate_is_positive): 
                                continue # skip statements like All S are S (reflexive statements are trivial and assumed)
                            if(form_index in (1,2)) and (subject_index, subject_is_positive) > (predicate_index, predicate_is_positive):
                                continue # skip E and I statements not in canonical order to avoid duplicates (No B are A is same as No A are B)
                            statement = (form_index, (subject_index, subject_is_positive), (predicate_index, predicate_is_positive))
                            statements.append(statement)
                            statement_map[statement] = statement_index
                            statement_index += 1
        return tuple(statements), statement_map
    
    @staticmethod
    def build_syntax_space(term_count: int) -> "SyntaxSpace":
        term_index_to_label: tuple[str, ...] = ()
        term_label_to_index: dict[str, int] = {}

        term_index_to_label, term_label_to_index = SyntaxSpace.build_terms(term_count)
        list_of_statements, map_statement_to_index = SyntaxSpace.generate_all_statements(term_count)

        return SyntaxSpace(
            term_count=term_count,
            term_index_to_label=term_index_to_label,
            term_label_to_index=term_label_to_index,
            list_of_statements=list_of_statements,
            map_statement_to_index=map_statement_to_index
        )