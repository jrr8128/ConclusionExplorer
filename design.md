# DESIGN.md — Game of Logic (AEIO) Generator (C++)

This document specifies the **search/generation program**: semantic search, pruning, memoization, and persistence of discovered recipes. The presentation program is out of scope, except for the **data contract** the search must provide.

---

## 0. Overview

### 0.1 Inspiration

Inspired by Lewis Carroll’s:

* **Symbolic Logic**
* **The Game of Logic**

These works use categorical propositions (AEIO forms) about sets (e.g., “All A are B”, “Some A are not B”) and derive valid conclusions.

Tiny AEIO example:

* Premises:

  * `A(A,B)` — All A are B
  * `I(A,C)` — Some A are C
* Conclusion:

  * `I(B,C)` — Some B are C

### 0.2 Goals

* Enumerate **minimal core premise sets** (“recipes”) over AEIO statements.
* A recipe is valid iff:

  * **Coverage:** all `T` base terms appear at least once (in either polarity) in the premise set.
  * **Unique interesting conclusion:** exactly one conclusion is “interesting” (project-defined filter).
  * **Necessity:** every premise is required to entail that interesting conclusion.
* Prefer **target-directed generation** for a fixed conclusion `c` using two exact checks:

  1. exclude a branch if `state ∧ c` is inconsistent
  2. accept a candidate if `state ∧ ¬c` is inconsistent
* Persist results so term counts `T=2..T_MAX` can be generated once and then queried.

### 0.3 Methodology and tractability

The search space grows combinatorially with:

* term count `T` (statement universe expands)
* premise count `k` (combinations of statements)

<details>
<summary><strong>How the baseline statement count is derived</strong></summary>

Let `T` be the number of base terms.

* Each base term has two literals: `X` and `¬X`, so literals `L = 2T`.
* Statements are built over literal pairs `(S,P)`.
* Reflexive pairs are rejected: `S == P` is not generated.

Count by form:

* `A(S,P)` and `O(S,P)` are **ordered**.

  * Count each: `L * (L - 1)`.
* `E(S,P)` and `I(S,P)` are **unordered** (symmetric):

  * `E(S,P) == E(P,S)` and `I(S,P) == I(P,S)`.
  * Count each: `C(L,2) = L*(L-1)/2`.

Total after these generation prunes:

`N = 2 * L*(L-1) + 2 * C(L,2) = 3L(L-1) = 6T(2T-1)`.

Note: additional generation-time bans (project “unfun” rules) reduce `N` further.

</details>

Concrete sizes (baseline `N`, before extra bans):

* `T=2`: `N=36` → all premise sets `2^36 ≈ 6.87e10`
* `T=3`: `N=90` → all premise sets `2^90 ≈ 1.24e27`
* `T=4`: `N=168` → all premise sets `2^168 ≈ 3.74e50`

Even with a hard cap like `k ≤ 3T`, the combinations are enormous without early pruning and memoization.

This design relies on:

* **heavy pruning** before expensive semantic work
* **target-directed classification** (two inconsistency checks)
* **active-term projection** (reuse work computed in smaller active-term spaces)

### 0.4 Coding SOP

* Naming: **snake_case** for everything (types, functions, variables, files).
* Constants/macros: **SCREAMING_SNAKE_CASE**.
* Avoid abbreviations; add any adopted abbreviations to Appendix A.
* Keep responsibilities narrow; split modules if a file/class does more than one job.
* Bundle knobs into config structs to keep call sites stable.

---

## 1. Terminology

* **base term:** A, B, C… (index `0..T-1`)
* **complement:** ¬A (polarity of a base term)
* **term literal:** `(term_index, is_complement)`
* **active term:** base term that appears (either polarity) in any premise in the set
* **premise graph:** vertices = base terms; edge between the two base terms used by a premise

---

## 2. Core data layouts

### 2.1 Statements

```cpp
// Four categorical forms.
enum class form : uint8_t { A, E, I, O };

// A base term in either positive or complemented form.
struct term_literal {
  uint8_t term;          // base term index [0..T-1]
  bool is_complement;    // false: X, true: ¬X
};

// An AEIO statement over two literals.
struct statement {
  form f;
  term_literal s;            // subject literal
  term_literal p;            // predicate literal
};

// For T<=8 the generated statement space is small; uint16_t is ample.
using statement_id = uint16_t; // index into syntax_space::statements
using premise_id = statement_id;
```

### 2.2 Semantic state

A `semantic_state` encodes which Venn regions are still possible under the current premise set.

Region indexing and bit ordering (convention):

* Each region corresponds to a length-`T` truth assignment over base terms.
* Region index `r` is the integer whose bit `i` is `1` iff base term `i` is positive in that region, and `0` iff it is complemented.

  * Example (`T=2`):

    * `r=0b00` = `¬A ∧ ¬B`
    * `r=0b01` = `A ∧ ¬B`
    * `r=0b10` = `¬A ∧ B`
    * `r=0b11` = `A ∧ B`
* `allowed_regions` is a bitset over region indices.

  * Bit 0 (LSB) corresponds to region index `0`.
  * Only the low `2^T` bits are meaningful.

Small example (`T=2`): regions are `A∧B`, `A∧¬B`, `¬A∧B`, `¬A∧¬B`.

* Premise `A(A,B)` (All A are B) forbids region `A∧¬B`.
* If the allowed region set becomes empty (or violates existence constraints), the premise set is inconsistent.

Design constraint:

* `T_MAX = 8`.
* Regions = `2^T ≤ 256` bits → stored as a fixed 4×`uint64_t` bitset.

```cpp
using uint64 = uint64_t;

static constexpr uint8_t T_MAX = 8;
static constexpr uint16_t MAX_REGION_BITS = 1u << T_MAX;     // 256
static constexpr uint8_t  MAX_REGION_WORDS = MAX_REGION_BITS / 64; // 4

// Bitset over the 2^T regions. Only the low (1<<T) bits are meaningful.
struct region_mask {
  std::array<uint64, MAX_REGION_WORDS> words;
};

// Existence constraints induced by I/O statements.
// Layout: bitset(s) over regions; exact encoding defined by semantics.
struct constraint_mask {
  std::array<uint64, MAX_REGION_WORDS> words;
};

struct semantic_state {
  region_mask allowed_regions;
  std::vector<constraint_mask> existence_constraints;
};

// Hashable canonical bytes for memo keys (must encode only meaningful bits for the current T/k).
struct state_key {
  // e.g., packed words of allowed_regions plus a canonicalized encoding of existence_constraints.
};
```

### 2.3 Canonicalization witness

Canonicalization identifies semantic states up to renaming of terms.

Example: if only `{A,B,C}` are active inside a `T=4` run, then the subproblem is equivalent to a `T=3` run after renaming terms.

Canonicalization produces:

* a canonical `state_key` (used for dedupe/memo)
* a **witness permutation** that records how canonical term slots map back to the run’s term indices (used for consistent rendering/export)

```cpp
struct permutation {
  // Number of active terms (k <= T_MAX).
  uint8_t k;

  // Maps canonical term slot -> original base-term index.
  // Only first k entries are used.
  std::array<uint8_t, T_MAX> canon_to_orig;
};

struct canonicalized_state {
  uint8_t active_k;  // = popcount(active_terms_mask)
  state_key key;
  permutation witness;
};
```

### 2.4 Node-local tracking (for early prunes)

These structures support cheap pruning and bookkeeping without touching `semantic_state`.

```cpp
struct dsu_state {
  // Union-find over base terms (T<=8). Used for a depth-cap feasibility prune.
  std::array<uint8_t, T_MAX> parent;
  std::array<uint8_t, T_MAX> rank;

  // Connected components among active terms.
  // If this cannot reach 1 within remaining depth, the branch is pruned.
  uint8_t components_among_active;
};

struct pair_tracker {
  // Tracks seen premise patterns on (subject_literal, predicate_term/literal) to ban
  // specific "unfun" combinations before semantic work.
  // The exact mask layout is owned by prune_rules.
};

struct node_info {
  // Coverage tracking.
  uint64 active_terms_mask;

  // Polarity-touch tracking.
  uint64 seen_positive_terms_mask;
  uint64 seen_negative_terms_mask;

  // Pattern bans (pair/form/polarity constraints).
  pair_tracker pairs;

  // Connectedness feasibility under the depth cap.
  dsu_state dsu;

  // Order-invariant premise set identity.
  // Stored on the DFS stack for leaf validation and recipe export.
  uint16_t premise_count;
  std::vector<premise_id> premise_ids_sorted;
};
```

---

## 3. Modules

### 3.1 `syntax_space`

Responsibilities:

* enumerate all AEIO statements for a given `T`
* assign `statement_id` indices (stable within the generated `syntax_space`)
* apply statement-generation prunes (symmetry/reflexive/degenerate)
* define a **canonical representative** per statement-equivalence class for internal dedupe

```cpp
struct syntax_space {
  uint8_t term_count;
  std::vector<statement> statements;

  const statement& get_statement(statement_id id) const;

  // Equivalence metadata (aligned by statement_id).
  // equiv_class_id groups logically-equivalent statements under allowed rewrites.
  std::vector<uint32_t> equiv_class_id_by_statement_id;

  // Internal dedupe: maps each statement_id to a chosen canonical representative id.
  // If used, search iterates/stores representative IDs only.
  std::vector<statement_id> equiv_rep_id;
};

syntax_space build_syntax_space(uint8_t term_count);
```

````

### 3.2 `semantics`
Responsibilities:
- apply a premise to a semantic state
- decide inconsistency of `state ∧ stmt` efficiently using bit operations

Plain meaning of `state ∧ stmt`:
- `state` represents the set of Venn regions still possible.
- applying `stmt` intersects `state` with the constraint imposed by `stmt`.
- inconsistency occurs when:
  - the allowed region mask becomes empty, or
  - existence constraints cannot be satisfied by the remaining regions.

```cpp
struct apply_result {
  semantic_state new_state;

  // True if the premise eliminates at least one previously-allowed region or tightens constraints.
  // Used to reject premises that add no information.
  bool changed;
};

// Returns nullopt if applying the premise makes the state inconsistent.
std::optional<apply_result> apply_premise(
  const syntax_space&, const semantic_state&, premise_id);

// Convenience check: is (state ∧ stmt) inconsistent?
bool inconsistent_with(
  const syntax_space&, const semantic_state&, statement_id);
````

### 3.3 `canonicalizer`

Responsibilities:

* compute a canonical representative of a semantic state under permutations of **active terms**
* return a witness permutation for export/rendering

Why canonicalization exists:

* Many different premise sets yield semantically identical states after renaming terms.
* Canonicalization allows memo keys to collapse these cases.

Why canonicalization is expected to be expensive:

* it compares/tries permutations of active terms to find the canonical representative.
* it was a hotspot in the Python implementation.

Active-term projection:

* Only active terms (those mentioned so far) participate in the canonicalization.
* This reduces permutation cost and enables cross-k reuse.

```cpp
// Produces a canonical key for memoization and a witness mapping for export.
canonicalized_state canonicalize_state(
  const syntax_space&, const semantic_state&, uint64 active_terms_mask);

// Applies a permutation to a statement so conclusions/premises remain meaningful under renaming.
// (If recipe storage uses canonical statement IDs, both premises and target are stored in the same naming.)
statement_id permute_statement_id(
  const syntax_space&, statement_id, const permutation&);
```

### 3.4 `memo_store`

Responsibilities:

* store results of expensive computations for reuse
* enable search-time skipping of repeated work
* reuse work across term-count runs via active-term projection

Memoization roles:

1. avoid recomputation of the same test on the same canonical state
2. enable search-time branch skipping using previously saved results
3. cross-k reuse: if only `k` terms are active inside a larger `T` run, reuse `k`-space results

Key idea:

* a hash lookup in `memo_store` is far cheaper than re-running canonicalization + semantic checks.

```cpp
// Per-target status of a canonical state.
enum class target_status : uint8_t {
  // not yet tested for this target
  unknown,
  // state ∧ target is inconsistent (target cannot be true from this branch)
  excluded,
  // state ∧ ¬target is inconsistent (target is entailed by this state)
  entails,
};

struct memo_store {
  // Saved answers for "is (state ∧ stmt) inconsistent?".
  // Used in target classification and in validation checks.
  bool get_inconsistent(uint8_t k, const state_key&, statement_id, bool* out) const;
  void put_inconsistent(uint8_t k, const state_key&, statement_id, bool value);

  // Saved classification for a fixed target.
  bool get_target_status(uint8_t k, const state_key&, statement_id target, target_status* out) const;
  void put_target_status(uint8_t k, const state_key&, statement_id target, target_status value);

  // Transposition pruning: record the smallest premise count that reached this canonical state.
  bool get_best_depth(uint8_t k, const state_key&, uint16_t* out) const;
  void put_best_depth(uint8_t k, const state_key&, uint16_t depth);
};
```

Plain explanation: “transposition pruning”

* Different premise addition orders (and different premise sets) can reach the same canonical semantic state.
* If the same canonical state is reached again with **more** premises, that path cannot produce a **more minimal** recipe than the earlier one.
* Therefore the later visit is pruned.

### 3.5 `prune_rules`

Responsibilities:

* centralize prune logic behind named checks (for readability and profiling)
* provide a small number of “gateway” methods used by search
* map each gateway to the detailed prune list (Section 4)

```cpp
struct prune_config {
  uint8_t term_count;
  uint16_t max_premises;
  uint8_t coverage_slack; // allow a small delay before full coverage is required
};

struct prune_rules {
  prune_config cfg;

  // Called during syntax_space generation.
  bool reject_generated_statement(const statement&) const;

  // Called during search after updating cheap node_info but before semantic work.
  bool reject_before_expensive_work(const node_info&, premise_id next) const;

  // Called after semantic work (state update) but before canonicalization/memo accept.
  bool reject_after_state_update(const node_info&, const semantic_state&) const;
};
```

### 3.6 `search`

Responsibilities:

* orchestrate target-directed iterative deepening
* manage the DFS stack of nodes
* call prunes, semantics, canonicalizer, and memo_store

`search_node` is an instantiable type in its own file.

```cpp
struct search_config {
  uint8_t term_count;
  uint16_t max_premises;
  uint8_t coverage_slack;
};

struct search_node {
  semantic_state state;
  node_info info;

  // Combination enumeration: only consider premises with id >= next_min_premise_id.
  // Child nodes set next_min_premise_id = last_added_id + 1.
  statement_id next_min_premise_id;

  // Additional stack-only fields can be added here (e.g., iterator index over candidate premises).
};

// Emits recipe records via callback to keep search independent of persistence.
using recipe_callback = std::function<void(const search_node& leaf)>;

// Callback for accepted AEIO recipes.
void enumerate_target_recipes(
  const syntax_space&, const search_config&, memo_store&, statement_id target,
  recipe_callback on_recipe);
```

---

## 4. Pruning strategy

<details>
<summary><strong>Show / hide pruning details</strong></summary>

Prunes are grouped by when they fire. The intent is to reject as early as possible.

### 4.1 Syntax-space generation prunes

#### P1: Reflexive statements

Reject statements where subject literal == predicate literal.

* Reject: `A(X,X)`, `E(X,X)`, `I(X,X)`, `O(X,X)`

#### P2: Symmetry canonicalization for E and I

Because `E(S,P)==E(P,S)` and `I(S,P)==I(P,S)`, only one ordering is generated.

* Keep `E(A,B)`, reject `E(B,A)`

#### P3: Degenerate complement self-pairs

Reject known-degenerate self-complement patterns at generation time.

* Reject: `I(X,¬X)` (impossible)
* Reject: `E(X,¬X)` (tautology)
* Reject: `O(X,¬X)` and `O(¬X,X)` (project “unfun”)

### 4.2 In-search prunes (before expensive work)

#### P4: Repeat premise

Reject adding a premise already in the set.

(If canonical-order premise enumeration is used, repeats can only occur via equivalence-class representatives; handle via representative IDs or an explicit "seen premise" bitset.)
Reject adding a premise already in the set.

#### P5: Pair-level pattern bans (`pair_tracker`)

Reject known “unfun” or degenerate combinations.
Examples:

* if `A(S,P)` already present, reject `A(S,¬P)`
* if `A(S,P)` already present, reject `O(S,P)`
* if `E(S,P)` already present, reject `I(S,P)`

#### P6: Coverage pacing

Coverage is required for saved recipes, but coverage is enforced early to reduce combinatorics.

Let:

* `m = popcount(active_terms_mask)`
* `k = premise_count`
* `r = max_premises - k`
* `min_cover_depth = ceil(T/2)` (each premise can introduce at most 2 new terms)
* `coverage_deadline = min_cover_depth + coverage_slack`

Prunes:

* Impossible: if `m + 2*r < T`, prune branch
* Deadline: if `k >= coverage_deadline` and `m < T`, prune branch

#### P7: Connectedness pacing (DSU)

Purpose:

* Under a premise cap, some branches cannot become connected in time.
* DSU prevents exploring branches that are guaranteed to fail connectedness before reaching a leaf.

Let:

* `cc = components_among_active`
* `r = max_premises - k`

Prune:

* if `cc - 1 > r`, prune branch (not enough remaining premises to connect all components)

### 4.3 In-search prunes (after state update)

#### P8: No new information

If adding a premise does not eliminate any regions and does not tighten existence constraints, it adds no information and is rejected.

#### P9: Inconsistency

If applying a premise forces the allowed region set empty or violates existence constraints, the branch is rejected.

### 4.4 Leaf acceptance checks

#### P10: Full coverage

Require `popcount(active_terms_mask) == T`.

#### P11: Target entailment

Target is entailed iff adding `¬target` makes the state inconsistent.

#### P12: Necessity

For each premise `p` in the set, removing `p` must break entailment of the target.

#### P13: Unique interesting conclusion

Compute the set of “interesting” entailed conclusions; require it equals `{target}`.

Addendum: opportunistic compound candidates

* Some leaf nodes may entail multiple conclusions that are jointly renderable as a single compound sentence in the presentation layer.
* These nodes are still rejected by P13 for the AEIO-only recipe list, but can be recorded in a separate “compound candidate” bucket.

Notes:

* The second component needed for a compound may or may not be “interesting” under the current predicate.
* For compound detection, use a dedicated whitelist that operates on **entailed conclusions** (not just “interesting” ones), to avoid coupling compound discovery to the “interesting” filter.

Supported compound kinds (derived from sets of AEIO conclusions; no language/semantics extension):

* `all_same_subject_conjoin_predicates`: `{ A(S,P), A(S,Q) }` rendered as “All S are P and Q.”

  * Condition: both are entailed; `P != Q`.
  * Storage: component statement IDs `{A(S,P), A(S,Q)}`.

Implementation hook:

* During leaf validation, if P13 fails, run `detect_compound_candidate(entailed_statements)`.
* If a compound candidate is detected, emit it via a separate callback and store it separately from accepted recipes.

</details>

---

## 5. Search algorithm (iterative deepening, target-directed)

### 5.1 Canonicalization collapse and “multiple parents”

Canonicalization collapses distinct premise histories into the same `(active_k, state_key)`.

* The explored quotient space is a DAG.
* The implementation does not build/store explicit edges.

Recipe reconstruction:

* Recipes are reconstructed from the current DFS stack path when a leaf passes acceptance checks.

### 5.2 Per-target classification

Classification for fixed target `c` and current semantic state `S`:

* **excluded** if `(S ∧ c)` is inconsistent
* **entailed** if `(S ∧ ¬c)` is inconsistent

Saved results:

* `(active_k, state_key, c) -> target_status` is stored so the same classification is not recomputed.

### 5.3 Iterative deepening

Iterative deepening searches by increasing maximum premise count:

* For `depth_limit = 0..max_premises`:

  * run depth-limited DFS

Why:

* DFS uses memory proportional to depth, not number of discovered states.
* early valid recipes are minimal in premise count by construction.

Transposition pruning using `best_depth_seen`:

* If a canonical state is reached previously with fewer premises, reaching it again with more premises cannot yield a more minimal recipe; the later visit is pruned.

Leaf handling:

* When a node classifies as **entailed** for the target, leaf acceptance checks (Section 4.4) are run.
* If P13 fails, a compound-candidate detector will run (P13 addendum) and emit a separate record.

### 5.4 Canonical-order premise enumeration (combination generation)

Premise order does not matter, so the search enumerates each premise set exactly once by enforcing an increasing `statement_id` rule.

Rule:

* Each node carries `next_min_premise_id`.
* Only premises `id >= next_min_premise_id` are considered for expansion.
* When premise `id` is added, the child sets `next_min_premise_id = id + 1`.

Effects:

* eliminates k! permutations of the same premise set
* keeps `premise_ids_sorted` trivially sorted (push-back only)
* reduces the number of nodes that reach canonicalization/memoization

Completeness:

* For any set of premises, there is exactly one increasing-ID sequence that generates it.

Implementation note:

* Prunes may still restrict the candidate range further (e.g., coverage-feasibility), but the increasing-ID rule is always enforced.

### 5.5 Coverage-biased expansion order (completeness-safe)

Before full coverage is reached, prioritize adding premises that mention at least one inactive base term.

Definition:

* Let `adds_new_term(next)` be true if `premise_term_mask[next]` contains any bit not in `active_terms_mask`.

Policy:

* This is an **ordering**, not a prune.
* For a node, consider candidates in two passes:

  1. all `next` with `adds_new_term(next)`
  2. then all remaining `next`

Completeness:

* No candidate is skipped solely due to coverage bias; the search still enumerates all combinations under the same depth cap.

Operational note:

* If a strict coverage deadline prune is enabled (P6 “deadline”), it can remove combinations where the first term-introducing premise appears late in ID order. For completeness, disable the deadline prune and keep only the feasibility prune.

---

## 6. Correctness definitions and metadata

### 6.1 “Interesting” conclusion predicate

An “interesting conclusion” is defined relative to a leaf premise set `P` and a candidate conclusion `c`.

A candidate `c` is interesting iff all of the following hold:

* **Entailed:** `P ⊨ c`.
* **Not a single-premise restatement:** for every premise `p ∈ P`, `p` is not logically equivalent to `c` (under the project’s statement-equivalence relation).
* **Necessity (all premises):** for every premise `p ∈ P`, `(P \ {p}) ⊭ c`.

A recipe is accepted only if the set of interesting conclusions is exactly `{target}`.

If the set contains multiple non-equivalent interesting conclusions:

* the recipe is rejected from the AEIO-only recipe list
* a compound-candidate detector will run (Section 4.4 P13 addendum)

### 6.2 Negation vs equivalence rewrite

Two distinct operations are required:

**Logical negation** (used by target classification):

* `¬A(S,P) ≡ O(S,P)`
* `¬E(S,P) ≡ I(S,P)`
* `¬I(S,P) ≡ E(S,P)`
* `¬O(S,P) ≡ A(S,P)`

This is an involution: `negate(negate(x)) == x`.

**Equivalence rewrite** (used for equivalence classes / presentation):

* `A(S,P) ≡ E(S,¬P)`
* `O(S,P) ≡ I(S,¬P)`
* `E(S,P) ≡ E(P,S)`
* `I(S,P) ≡ I(P,S)`

These are *not* negations; they preserve truth value.

### 6.3 Statement equivalence classes

A statement equivalence class groups statement IDs that are logically equivalent under the project’s allowed rewrites.

Primary uses:

* avoid treating a conclusion as “new” if it is equivalent to a premise (restatement check)
* presentation-layer acceptance of alternate phrasings
* internal dedupe by iterating only a canonical representative `equiv_rep_id`

Non-goal:

* removing complemented literals from the language

  * complemented literals enable direct premises like “All non-A are B” and support equivalence rewrites cleanly

Implementation note:

* The syntax generator may still enumerate all statements.
* A separate `statement_metadata` table (or deterministic function) can map `statement_id -> equiv_class_id`.
* If internal dedupe is enabled:

  * choose one representative per equivalence class (deterministically)
  * iterate/store only representative IDs in search
  * store full statement IDs only for presentation/output as needed

### 6.4 `state_key` encoding contract

`state_key` must be:

* canonical and deterministic
* hashable and byte-comparable
* independent of allocation addresses / container order

Bit ordering dependency:

* `state_key` serialization must follow the region indexing and bit ordering convention defined in Section 2.2.
* The least-significant bit of `allowed_regions` corresponds to region index 0.

Minimum contents:

* `allowed_regions` words, masked to the low `(1<<active_k)` meaningful region bits
* `existence_constraints` encoded in a canonical order (sorted by byte value)

### 6.5 Existence constraints (I/O)

Existential statements add a requirement that at least one region remains possible.

Canonical treatment:

* `I(S,P)` requires region `(S ∧ P)` to be non-empty.
* `O(S,P)` is equivalent to `I(S,¬P)` and therefore requires region `(S ∧ ¬P)` to be non-empty.

Operationally:

* `existence_constraints` is a list of `constraint_mask` values.
* A state is inconsistent if any constraint mask has no overlap with `allowed_regions`.

### 6.6 Config knobs

Core run config:

* `term_count (T)`
* `max_premises`
* `coverage_slack`

Safety note on `coverage_slack`:

* The “impossible coverage” prune is sound: if `m + 2*r < T`, full coverage cannot be reached.
* Any additional deadline-based coverage prune is a heuristic knob; set `coverage_slack` high (or disable the deadline prune) to guarantee completeness.

### 6.7 Determinism

All runs must be deterministic given the same config:

* deterministic statement enumeration order
* deterministic premise iteration order
* canonicalizer tie-break rules must be deterministic
* stable hashing / ordering for `state_key` and constraint sorting

### 6.8 Minimal test plan

Unit-level (small T):

* `negate` involution: `negate(negate(x)) == x`
* equivalence rewrites preserve semantics (spot-check)
* `apply_premise` correctness on `T=2` with hand-checked cases
* canonicalizer witness consistency (applying witness to premises/conclusion preserves meaning)

Reference-check (bruteforce small T):

* For `T=2` (4 regions), brute force all region masks and compare entailment/inconsistency decisions against the bitset implementation.

---

## 7. Persistence (SQLite) — what and why

SQLite is used to support:

* one-time generation per `T`
* structured queries for later tooling
* optional resuming of long runs

### 7.1 Consumer contract (what downstream needs)

Downstream tooling needs:

* the **premise set identity** (order-invariant)
* the **target conclusion**
* a way to **render terms consistently** (avoid term-renaming mismatch)
* a way to accept **equivalent targets** (equivalence classes)

Therefore the generator must store:

* canonical `premise_id` list
* canonical `target_statement_id`
* witness permutation (canonical term slots → run term indices)
* equivalence class id for each “acceptable alternate” conclusion

Plain meaning: “witness permutation”

* Canonicalization renames terms to compare states.
* The witness records how to translate back to the term names used when exporting/printing.

### 7.2 Tables (minimal)

#### `recipe`

Stores one recipe row per accepted premise set.

* `recipe_id` (primary key: a unique row identifier)
* `term_count`
* `target_statement_id`
* `premise_count`
* `premise_id_list_bytes` (sorted canonical premise ids, packed)
* `premise_id_list_hash` (for fast dedupe / indexing)
* `witness_perm_bytes` (packed permutation for rendering)

Dedupe policy:

* Deduping is performed in-memory before insertion.
* A UNIQUE constraint on `(term_count, target_statement_id, premise_id_list_hash)` can be used as a safety net.

#### `recipe_conclusion`

Stores conclusion metadata for audit and downstream acceptance rules.

* `recipe_id` (FK)
* `statement_id`
* `kind` ∈ {`interesting`, `trivial`}
* `equiv_class_id`

Why:

* downstream can display the target and accept alternates in the same equivalence class.
* audit without recomputation.

#### `compound_candidate`

Stores “near-miss” leaves that fail P13 but match a whitelisted compound pattern.

* `compound_id` (primary key)
* `term_count`
* `compound_kind` (enum)
* `premise_count`
* `premise_id_list_bytes` (sorted canonical premise ids, packed)
* `premise_id_list_hash`
* `component_statement_id_list_bytes` (packed list of AEIO statement IDs that form the compound)
* `witness_perm_bytes`

Why:

* preserves optional “fun” compound conclusions without changing the AEIO search language.
* allows the presentation layer to require a single compound answer object while grading against component AEIO statements.

### 7.3 Memo persistence

Memo persistence is a performance/resume feature.

* If persisted, store in the same SQLite file under separate tables (clear separation by table name).
* If not persisted, keep memo in memory and only store recipes.

---

## Appendix B: Glossary

* **AEIO**: the four categorical statement forms
* **DSU**: disjoint set union (union-find)
