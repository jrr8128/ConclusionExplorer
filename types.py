from typing import TypeAlias

AllowedRegionsMask : TypeAlias = int
ExistenceConstraintsMask : TypeAlias = int
ExistenceConstraints : TypeAlias = tuple[ExistenceConstraintsMask,...]

FormIndex = int # (0..3 for AEIO)
TermIndex = int # index from term table
Term = tuple[TermIndex, int] # term can be either the term (int = 0) or its complement (int = 1)
Statement: TypeAlias = tuple[FormIndex, Term, Term]
State: TypeAlias = tuple[AllowedRegionsMask, ExistenceConstraints]

Depth = int
ConclusionKey = frozenset[Statement]
Recipe = tuple[Statement,...]
Recipes = dict[ConclusionKey, set[Recipe]]
UsedBaseTermsMask = int
EmptyTermsMask = int
PairStatusMask = int
SeenDepthKey = tuple[State,PairStatusMask,UsedBaseTermsMask]
SeenDepth = dict[SeenDepthKey, Depth]
RegionRemapsByUsedBaseTermsMask = dict[UsedBaseTermsMask, list[list[int]]]

