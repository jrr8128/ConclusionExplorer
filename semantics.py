# Closure computation, bitmask logic for AEIO statements
from dataclasses import dataclass

@dataclass(frozen=True)
class SemanticSpace:
    term_count: int
    types_count: int
    type_patterns: tuple[int, ...]
    term_to_type_mask: tuple[int, ...]


def build_semantic_space(term_count: int) -> SemanticSpace:
    # 2^term_count; ven diagram regions
    types_count = 1 << term_count 
    # patterns: [0][1]...[2^term_count -1] (bits over terms)
    type_patterns = [k for k in range(types_count)]  
    # (bits over types) for each term [0,...,0] with length term_count
    term_to_type_mask = [0] * term_count  
    # For each term, build bitmask over types where term is present
    # ex. for 2 terms A,B: A = 1, B = 2
    # types(regions): 0:00 (not present), 1:01(in A only), 2:10(in B only), 3:11(in A and B)
    # so term A is 01 and term B is 10 (binary), compare to each type (region)
    # term A = 01 & 00 = region 0 -> 0 (not present)
    # term A = 01 & 01 = region 1 -> 1 (present
    # term A = 01 & 10 = region 2 -> 0 (not present)
    # term A = 01 & 11 = region 3 -> 1 (present)
    # so term A is present in regions (types) 1 and 3 -> mask = 101 (binary) or 5 (decimal)
    for term_index in range(term_count):
        mask = 0
        for type_index in range(types_count):
            # Check if the bit at position type_index when moved right by term_index is 1
            # ex. type_index = 5 (101 binary), term_index = 0 -> (101 >> 0) = 1 & 1 = 1 (present)
            # ex. type_index = 5 (101 binary), term_index = 1 -> (101 >> 1) = 10 & 1 = 0 (not present)
            # Then if it's present, we bitwise or the mask with (1 << type_index) to set that bit
            # ex for term_index = 0, type_index = 0 -> mask |= (1 << 0) -> mask = 1 (binary)
            # ex.for term_index = 0, type_index = 2 -> 
            # mask (=1 from previous) |= (1 << 2) (=100) -> mask = 101 (binary)
            if (type_patterns[type_index] >> term_index) & 1:
                mask |= (1 << type_index)
        term_to_type_mask[term_index] = mask


    return SemanticSpace(
        term_count=term_count,
        types_count=types_count,
        type_patterns=tuple(patterns),
        term_to_type_mask=tuple(type_mask),
    )