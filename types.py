from typing import TypeAlias

AllowedRegionsMask : TypeAlias = int
ExistenceConstraintsMask : TypeAlias = int
ExistenceConstraints : TypeAlias = tuple[ExistenceConstraintsMask,...]

FormIndex = int # (0..3 for AEIO)
TermIndex = int # index from term table
Term = tuple[TermIndex, int] # term can be either the term (int = 0) or its complement (int = 1)
Statement: TypeAlias = tuple[FormIndex, Term, Term]
State: TypeAlias = tuple[AllowedRegionsMask, ExistenceConstraints]
