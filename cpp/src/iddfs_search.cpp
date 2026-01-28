#include "iddfs_search.hpp"

#include <iostream>

#include "puzzle_collector.hpp"

namespace conclusion_explorer {

uint16_t covered;
std::vector<uint8_t> has_solution;
uint64_t solutions_before_limit;
uint8_t plateau;

struct premise_bitset_key_hash {
  size_t operator()(const premise_bitset_key& k) const noexcept {
    size_t h = 0;
    for (std::uint64_t x : k) {
      h ^= static_cast<size_t>(x) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    }
    return h;
  }
};

struct iddfs_frame {
  semantic_state state;
  uint16_t next_cid;
  uint8_t depth_left;
  size_t path_size_before;
};

void profiler::print_snapshot() const {
  const auto limit = current_limit.load(std::memory_order_relaxed);
  const auto stack = current_stack_size.load(std::memory_order_relaxed);

  const auto ps = premise_seen_size.load(std::memory_order_relaxed);
  const auto pbkt = premise_seen_buckets.load(std::memory_order_relaxed);

  const auto nodes = nodes_considered.load(std::memory_order_relaxed);

  const auto leaf_re = leaf_reached.load(std::memory_order_relaxed);
  const auto leaf_cov = leaf_missing_coverage.load(std::memory_order_relaxed);
  const auto leaf_uniq = leaf_non_unique.load(std::memory_order_relaxed);
  const auto leaf_bt = leaf_taste_banned.load(std::memory_order_relaxed);

  const auto r = redunant.load(std::memory_order_relaxed);
  const auto s = subsumed.load(std::memory_order_relaxed);
  const auto p = partial.load(std::memory_order_relaxed);
  const auto m = toomanyconc.load(std::memory_order_relaxed);

  const auto sol = solutions_emitted.load(std::memory_order_relaxed);

  const auto pr_conf = conflicts_pruned.load(std::memory_order_relaxed);
  const auto pr_inc = inconsistent_pruned.load(std::memory_order_relaxed);
  const auto pr_memo = memo_pruned.load(std::memory_order_relaxed);
  const auto pr_se = should_expand_pruned.load(std::memory_order_relaxed);

  std::cout << "t=" << format_elapsed_ms(elapsed_ms()) << "  d=" << int(limit)
            << "  stack=" << stack << "  nodes=" << nodes
            << "  premise_seen=" << ps << " (bkt=" << pbkt << ")\n"
            << "leaf:   reached=" << leaf_re << "  miss_cov=" << leaf_cov
            << "  nonuniq=" << leaf_uniq << "  taste=" << leaf_bt
            << "  sol=" << sol << "\n"
            << "conc:   redundant=" << r << "  subsumed=" << s
            << "  partial=" << p << "  >1=" << m << "\n"
            << "prune:  conf=" << pr_conf << "  inc=" << pr_inc
            << "  memo=" << pr_memo << "  se=" << pr_se << "\n";
}

static void set_present(class_bitset& bits, uint16_t cid) {
  bits[cid >> 6] |= +(1ull << (cid & 63));
}

static void clear_present(class_bitset& bits) {
  for (size_t index = 0; index < bits.size(); index++) {
    bits[index] = 0;
  }
}

static void clear_present(class_bitset& bits, uint16_t cid) {
  bits[cid >> 6] &= ~(1ull << (cid & 63));
}

static bool overlaps_conflicts(const class_bitset& conflicts,
                               const class_bitset& present,
                               uint16_t class_words) {
  for (uint16_t word_index = 0; word_index < class_words; word_index++) {
    if ((conflicts[word_index] & present[word_index]) != 0) {
      return true;
    }
  }
  return false;
}

static void pop_to(premise_path& path, class_bitset& present_bits, size_t sz) {
  while (path.size() > sz) {
    const class_id cid = path.back();
    clear_present(present_bits, cid.id);
    path.pop_back();
  }
}

static bool handle_leaf(cid_list_key& tmp_out, const prune_rules& rules,
                        const semantic_space& sem_space,
                        const syntax_space& syn_space,
                        const class_bitset& present_bits, collector& output,
                        const semantic_state& state, const premise_path& path,
                        profiler& prof, std::vector<std::uint16_t>& tmp_ids) {
  if ((state.base_terms_mask & rules.goal_mask) != rules.goal_mask) {
    prof.leaf_missing_coverage.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  prof.unique_conclusion_scans.fetch_add(1, std::memory_order_relaxed);
  std::optional<class_id> conc = rules.unique_interesting_conclusion(
      state, path, present_bits, sem_space, prof);
  if (!conc) {
    prof.leaf_non_unique.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  if (rules.is_banned_with_path(*conc, path)) {
    prof.leaf_taste_banned.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  if (!has_solution[conc->id]) {
    has_solution[conc->id] = 1;
    covered++;
  }
  output.add_solution(tmp_out, syn_space.term_count, *conc, path, state,
                      sem_space, syn_space, tmp_ids);
  return true;
}

static bool try_descend(const prune_rules& rules, memo& memo_table,
                        const syntax_space& syn_space,
                        const semantic_space& sem_space,
                        const class_bitset& present_bits, uint16_t class_words,
                        const iddfs_frame& frame, class_id cid,
                        semantic_state& child_state, uint16_t& next_min_id,
                        uint8_t& next_depth_left, profiler& prof) {
  if (overlaps_conflicts(rules.conflict_bits_by_cid[cid.id], present_bits,
                         class_words)) {
    prof.conflicts_pruned.fetch_add(1, std::memory_order_relaxed);
    return false;
  }

  child_state = frame.state;
  next_min_id = static_cast<uint16_t>(cid.id + 1);
  next_depth_left = static_cast<uint8_t>(frame.depth_left - 1);
  prof.apply_calls.fetch_add(1, std::memory_order_relaxed);
  if (apply_premise(child_state, cid, sem_space) ==
      apply_result::inconsistent) {
    memo_table.record_dead(child_state, syn_space, sem_space);
    prof.inconsistent_pruned.fetch_add(1, std::memory_order_relaxed);
    return false;
  }

  child_state.base_terms_mask |= rules.base_terms_mask_by_cid[cid.id];
  if (memo_table.is_dead(child_state, syn_space, sem_space)) {
    prof.memo_pruned.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  return true;
}

void run_iddfs(const semantic_space& sem_space, const syntax_space& syn_space,
               const prune_rules& rules, memo& memo_table, semantic_state root,
               premise_path& path, uint8_t max_depth, collector& output,
               profiler& prof) {
  const uint16_t class_count = static_cast<uint16_t>(syn_space.class_count());
  const uint16_t class_words = static_cast<uint16_t>((class_count + 63) / 64);
  uint8_t start_limit = std::max<uint8_t>(2, syn_space.term_count - 1);

  class_bitset present_bits{};
  std::vector<iddfs_frame> frame_stack;
  size_t max_stack_depth_this_limit;
  has_solution.assign(class_count, 0);
  covered = 0;
  solutions_before_limit = 0;
  plateau = 0;

  std::unordered_map<premise_bitset_key, std::int16_t, premise_bitset_key_hash>
      premise_seen;
  premise_seen.max_load_factor(0.7f);
  premise_seen.reserve(1000000);

  cid_list_key tmp_key{};
  std::vector<std::uint16_t> tmp_ids;
  tmp_ids.reserve(static_cast<size_t>(max_depth) + 1);
  tmp_key.ids.reserve(static_cast<size_t>(max_depth) + 1);
  premise_bitset_key pkey{};
  premise_bitset_key tmp_pkey{};

  for (uint8_t limit = start_limit; limit <= max_depth; ++limit) {
    prof.current_limit.store(limit, std::memory_order_relaxed);
    prof.limit_epoch.fetch_add(1, std::memory_order_relaxed);
    output.set_depth_limit(limit);
    max_stack_depth_this_limit = 0;
    solutions_before_limit =
        prof.solutions_emitted.load(std::memory_order_relaxed);

    frame_stack.clear();
    path.clear();
    clear_present(present_bits);

    frame_stack.push_back(iddfs_frame{root, 0, limit, 0});

    while (!frame_stack.empty()) {
      prof.on_stack_depth(frame_stack.size());
      if (frame_stack.size() > max_stack_depth_this_limit) {
        max_stack_depth_this_limit = frame_stack.size();
      }
      iddfs_frame& curr_frame = frame_stack.back();
      assert(curr_frame.depth_left > 0);
      assert(curr_frame.next_cid <= class_count);
      assert(frame_stack.size() >= 1);

      if (curr_frame.next_cid >= class_count || curr_frame.depth_left == 0) {
        pop_to(path, present_bits, curr_frame.path_size_before);
        frame_stack.pop_back();
        continue;
      }

      const class_id cid{curr_frame.next_cid++};
      prof.nodes_considered.fetch_add(1, std::memory_order_relaxed);
      prof.nodes_by_limit[limit] += 1;

      semantic_state child_state{};
      uint16_t next_min_id = 0;
      uint8_t next_depth_left = 0;

      if (!try_descend(rules, memo_table, syn_space, sem_space, present_bits,
                       class_words, curr_frame, cid, child_state, next_min_id,
                       next_depth_left, prof)) {
        continue;
      }

      const size_t parent_path_size = path.size();
      path.push_back(cid);
      set_present(present_bits, cid.id);

      canon_premise_set_bitset_key(pkey, path, syn_space, class_words,
                                   tmp_pkey);

      auto iter = premise_seen.find(pkey);
      if (iter != premise_seen.end() &&
          next_depth_left <= static_cast<std::uint8_t>(iter->second)) {
        prof.memo_pruned.fetch_add(1, std::memory_order_relaxed);
        clear_present(present_bits, cid.id);
        path.pop_back();
        continue;
      }

      auto [ins_it, inserted] = premise_seen.emplace(
          pkey, static_cast<std::int16_t>(next_depth_left));
      if (!inserted &&
          next_depth_left > static_cast<std::uint8_t>(ins_it->second)) {
        ins_it->second = static_cast<std::int16_t>(next_depth_left);
      }

      if (curr_frame.depth_left == 1) {
        prof.leaf_reached.fetch_add(1, std::memory_order_relaxed);
        prof.leaves_by_limit[limit] += 1;
        const std::size_t k = path.size();
        if (k < prof.leaves_by_k.size()) {
          prof.leaves_by_k[k] += 1;
        }
        if (handle_leaf(tmp_key, rules, sem_space, syn_space, present_bits,
                        output, child_state, path, prof, tmp_ids)) {
          prof.solutions_emitted.fetch_add(1, std::memory_order_relaxed);
          prof.solutions_by_limit[limit] += 1;
          if (k < prof.solutions_by_k.size()) {
            prof.solutions_by_k[k] += 1;
          }
        }

        clear_present(present_bits, cid.id);
        path.pop_back();
        continue;
      }

      if (!rules.should_expand(child_state, next_min_id, next_depth_left,
                               sem_space)) {
        prof.should_expand_pruned.fetch_add(1, std::memory_order_relaxed);
        clear_present(present_bits, cid.id);
        path.pop_back();
        continue;
      }
      frame_stack.push_back(iddfs_frame{child_state, next_min_id,
                                        next_depth_left, parent_path_size});
    }
    prof.premise_seen_size.store(premise_seen.size(),
                                 std::memory_order_relaxed);
    prof.premise_seen_buckets.store(premise_seen.bucket_count(),
                                    std::memory_order_relaxed);
    prof.print_snapshot();
    output.flush();
    uint64_t new_solutions = prof.solutions_emitted - solutions_before_limit;
    if ((prof.solutions_emitted >= 1000 || new_solutions == 0) &&
        limit > start_limit) {
      std::cout << "No new solutions, exiting." << std::endl;
      break;
    }
    if (max_stack_depth_this_limit < limit) {
      std::cout << "Exiting: max_stack_depth: "
                << std::to_string(max_stack_depth_this_limit)
                << " < limit: " << std::to_string(limit) << std::endl;
      break;
    }
  }
}
}  // namespace conclusion_explorer
   // namespace conclusion_explorer