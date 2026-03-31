# Conclusion Explorer

A C++ search engine for enumerating minimal AEIO premise sets that entail exactly one nontrivial conclusion.

---

## 1. What It Does

- Generates valid categorical logic recipes built from `AEIO` statements over a fixed set of terms.
- Searches for premise sets that `entail` exactly one conclusion selected by the project's interest criteria.
- Filters for minimality, so every premise in a reported recipe is necessary to derive that conclusion.
- Uses pruning and memoization to cut down the search space enough to make larger term counts tractable.
- Exports discovered recipes and summary statistics for later analysis or presentation.

`AEIO` : the four classical categorical statement forms A: All A are B, E: No A are B, I: Some A are B, O: Some A are not B.

`entail` : a conclusion follows logically from the premises.

---

## 2. Why This Project Exists

Conclusion Explorer grew out of a long-standing interest in logic, especially Lewis Carroll's *The Game of Logic* as an introduction to categorical reasoning. I originally wanted to bring his whimsical style of logic to a wider audience through a more accessible interactive format, but the project quickly became just as much about the puzzle-generation problem itself: how to systematically construct 'good' puzzles, represent the search space, and make generation practical. This project is the result of exploring that generation side in a form that could eventually support a playable application.

---

## 3. Current Status

- Project role: Conclusion Explorer is the search and generation component of the broader Game of Logic project.
- Related project phase: PuzzlePresentation is the current downstream phase focused on using the generated outputs.
- Active implementation: cpp/
- Legacy implementations and early experiments: legacy/
- Current scope: a command-line search and generation engine for categorical logic puzzles
- Current outputs: recipe files, per-run metadata, search statistics, and human-readable debug output
- Status: largely complete for its intended generation role

---

## 4. Repository Layout

- **cpp/src/** : active C++ implementation of search/generation engine
- **cpp/recipes/** : generated outputs, metadata, statistics, and debug files
- **cpp/CMakeLists.txt** : build configuration for the current implementation
- **design.md** : architecture notes, terminology, and algorithm/design details
- **legacy/** : older Python and C++ iterations kept for reference

## 5. Build
- Supported environment: Windows 11 with MSVC and CMake.
- Prerequisites:
  - CMake 3.20 or newer
  - Visual Studio Build Tools or Visual Studio with C++ support
  - A shell with the MSVC environment loaded, such as Developer PowerShell
- From the Conclusion Explorer directory:
```ps
cmake -S cpp -B cpp/build
cmake --build cpp/build --config Release
```
- The executable will be placed in the build output directory selected by your generator.

---

## 6. Run

```
.\cpp\build\Release\app.exe
.\cpp\build\Release\app.exe 4
.\cpp\build\Release\app.exe 5 --no-conclusion
.\cpp\build\Release\app.exe 6 --unique
```
- *term_count* : an optional positional argument in the range [3..8]
- if omitted, the program defaults to **3**
- Larger term counts increase search space significantly
- Arguments *--no-conclusion* and *--unique* (default) determine which subset of puzzles the program is searching for.

*Note: actual executable path may differ by generator. Ninja produces an executable path of cpp/build/app.exe.*

Output:

- *cpp/recipes/output/<term_count>-terms/* : contains accepted recipe files.
- *cpp/recipes/debug/<term_count>-terms/* : contains human-readable debug output.
- *cpp/recipes/meta/<term_count>-terms/meta.txt* : contains metadata for the run.
- *cpp/recipes/<term_count>-term-statistics.txt* : contains profiler and search statistics.

---

## 7. How It Works

- Generate the space of valid categorical statements and equivalence classes for the chosen term count.
- Build the semantic model used to test consistency and entailment.
- Explore candidate premise sets with an iterative deepening depth-first search.
- Prune branches early when they are redundant, inconsistent, or cannot lead to an acceptable result.
- Canonicalize and memoize states to avoid repeating equivalent work under term renaming or previously seen search states.
- Record accepted recipes and write recipe files, metadata, debug output, and run statistics.

---

## 8. Example
A 2 premise puzzle output for 3 terms:
- Premises:
  - No A are B
  - No ¬A are C

- Conclusion:
  - No B are C

- AEIO form:
  - E(A,B)
  - E(¬A,C)
  - C: E(B,C)

*Note: a recipe in this project is a minimal set of premises like this that yields a conclusion the search classifies as worth reporting.*

---

## 9. Design Notes

This **README.md** is intended as a high-level overview. For the deeper technical details, including terminology, internal data representations, search flow, pruning strategy, and canonicalization approach, see 'design.md'.
 
---

## 10. Limitations / Open Issues

- The search space grows combinatorially, and runs at 8+ terms are not yet fully tractable under the current approach.
- Build and usage documentation are currently Windows-first; the project has also been run on Linux, but the Linux setup is not yet documented.
- The broader downstream presentation phase is still in progress, so this repository currently reflects the generation engine rather than a finished end-user experience.
- Some implementation areas, especially parts of the search pipeline, would benefit from further decomposition to reduce file size and improve readability.
- Profiling and observability are currently handled with project-specific instrumentation rather than an external profiling toolchain.
- Internal documentation and targeted code comments are still being improved, particularly around non-obvious control flow and invariants.

---

## 11. Future Work

- Improve internal documentation so the codebase is easier to revisit and reason about later.
- Add targeted comments around core sections, search flow, and non-obvious implementation decisions.
- Continue polishing project documentation so the current implementation and design decisions are easier for other readers to follow.
- Make small readability improvements where they reduce friction, without changing the overall architecture or current role of this component.