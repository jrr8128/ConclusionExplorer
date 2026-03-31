#include "iddfs_search.hpp"

#include <atomic>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <limits>
#include <unordered_map>

namespace conclusion_explorer {

static volatile std::sig_atomic_t g_stop = 0;
static void on_sigint(int) { g_stop = 1; }
bool stop_requested() noexcept { return g_stop != 0; }
void install_sigint_handler() noexcept { std::signal(SIGINT, on_sigint); }

uint16_t covered;
std::vector<uint8_t> has_solution;
uint64_t solutions_before_limit;
uint8_t plateau;

struct premise_bitset_key_hash {
  std::uint16_t class_words = 0;

  size_t operator()(const premise_bitset_key& k) const noexcept {
    size_t h = 0;
    for (std::uint16_t i = 0; i < class_words; ++i) {
      const std::uint64_t x = k[i];
      h ^= static_cast<size_t>(x) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    }
    return h;
  }
};

struct premise_bitset_key_eq {
  std::uint16_t class_words = 0;

  bool operator()(const premise_bitset_key& a,
                  const premise_bitset_key& b) const noexcept {
    for (std::uint16_t i = 0; i < class_words; ++i) {
      if (a[i] != b[i]) return false;
    }
    return true;
  }
};

struct iddfs_frame {
  semantic_state state;
  uint16_t next_cid;
  uint8_t depth_left;
  size_t path_size_before;
};

using premise_seen_map =
    std::unordered_map<premise_bitset_key, std::int16_t,
                       premise_bitset_key_hash, premise_bitset_key_eq>;

struct iddfs_ctx {
  // inputs
  const semantic_space& sem_space;
  const syntax_space& syn_space;
  const prune_rules& rules;
  memo& memo_table;
  collector& output;
  profiler& prof;

  // constants
  std::uint16_t class_count = 0;
  std::uint16_t class_words = 0;
  std::uint8_t max_depth = 0;

  // per-run mutable
  class_bitset present_bits{};
  std::vector<iddfs_frame> frame_stack;
  premise_path& path;
  premise_seen_map premise_seen;

  cid_list_key tmp_key{};
  std::vector<std::uint16_t> tmp_ids;
  premise_bitset_key pkey{};
  premise_bitset_key tmp_pkey{};
};

void profiler::print_snapshot() const {
  const auto limit = current_limit.load(std::memory_order_relaxed);
  const auto stack = current_stack_size.load(std::memory_order_relaxed);
  const auto ps = premise_seen_size.load(std::memory_order_relaxed);

  const auto nodes = candidates_considered.load(std::memory_order_relaxed);

  double snap_rate = last_rate;

  if (!rate_initialized) {
    rate_initialized = true;
    last_snapshot_ms = elapsed_ms();
    last_snapshot_nodes = nodes;
  } else {
    const auto now_ms = elapsed_ms();
    const auto delta_ms = now_ms - last_snapshot_ms;
    const auto delta_nodes = nodes - last_snapshot_nodes;
    last_snapshot_ms = now_ms;
    last_snapshot_nodes = nodes;

    rate_acc_ms += delta_ms;
    rate_acc_nodes += delta_nodes;

    if (rate_acc_ms >= rate_window_ms && rate_acc_ms > 0) {
      last_rate = (double(rate_acc_nodes) * 1000.0) / double(rate_acc_ms);
      snap_rate = last_rate;
      rate_acc_ms = 0;
      rate_acc_nodes = 0;

      if (min_rate == 0.0 || last_rate < min_rate) min_rate = last_rate;
      if (last_rate > max_rate) max_rate = last_rate;
    }
  }

  const auto desc = descended.load(std::memory_order_relaxed);
  const auto leaves = leaves_reached.load(std::memory_order_relaxed);
  const auto sol = solutions_emitted.load(std::memory_order_relaxed);
  const auto sol2 = solutions_accepted.load(std::memory_order_relaxed);

  const auto leaf_cov =
      leaf_prune_missing_coverage.load(std::memory_order_relaxed);
  const auto leaf_uniq =
      leaf_prune_no_unique_conclusion.load(std::memory_order_relaxed);
  const auto leaf_bt = leaf_prune_taste_banned.load(std::memory_order_relaxed);

  const auto conc_present = leaf_prune_present.load(std::memory_order_relaxed);
  const auto conc_reqall =
      leaf_prune_requires_all_failed.load(std::memory_order_relaxed);
  const auto conc_toomany =
      leaf_prune_too_many_conclusions.load(std::memory_order_relaxed);

  const auto pr_conf = prune_conflict.load(std::memory_order_relaxed);
  const auto pr_nc = prune_no_change.load(std::memory_order_relaxed);
  const auto pr_inc = prune_inconsistent.load(std::memory_order_relaxed);
  const auto pr_dead = prune_memo_dead.load(std::memory_order_relaxed);
  const auto pr_dom = prune_dominance.load(std::memory_order_relaxed);
  const auto pr_se = prune_should_expand.load(std::memory_order_relaxed);
  const auto pr_seen = prune_premise_seen.load(std::memory_order_relaxed);

  auto pct = [](std::uint64_t num, std::uint64_t den) -> double {
    return den ? (100.0 * double(num) / double(den)) : 0.0;
  };

  const auto elapsed = elapsed_ms();
  const double elapsed_s = elapsed > 0 ? (double(elapsed) / 1000.0) : 0.0;
  const double nodes_per_sec =
      elapsed_s > 0.0 ? (double(nodes) / elapsed_s) : 0.0;

  if (sol2 != last_accept_count) {
    last_accept_count = sol2;
    last_accept_snapshot_ms = elapsed;
  }
  const std::uint64_t since_accept_ms =
      (last_accept_snapshot_ms == 0) ? 0 : (elapsed - last_accept_snapshot_ms);
  const std::string since_accept = (last_accept_snapshot_ms == 0)
                                       ? "n/a"
                                       : format_elapsed_ms(since_accept_ms);

  const auto old_flags = std::cout.flags();
  const auto old_prec = std::cout.precision();
  std::cout << std::fixed << std::setprecision(1);

  std::cout << "t=" << format_elapsed_ms(elapsed) << "  d=" << int(limit)
            << "  stack=" << stack << "  nodes=" << nodes << "  nodes/s="
            << snap_rate
            //<< "  seen=" << ps
            << "  leaf=" << leaves << "  since_accept=" << since_accept << "\n"
            << "leaf:   reached=" << leaves << "  miss_cov=" << leaf_cov
            << "  nonuniq="
            << leaf_uniq
            //<< "  taste=" << leaf_bt
            << "  sol: emit=" << sol << "  accept=" << sol2 << "\n"
            << "conc:   present=" << conc_present << "  reqall=" << conc_reqall
            << "  >1=" << conc_toomany << "\n"
            << "prune:  conf=" << pr_conf << "  nochg=" << pr_nc
            << "  inc=" << pr_inc << "  dead=" << pr_dead << "  dom="
            << pr_dom
            //<< "  se=" << pr_se
            //<< "  seen=" << pr_seen
            << "\n"
            << "survival rates: "
            << "preleaf=" << pct_str(desc, nodes) << " cons | "
            << "leaf=" << pct_str(leaves, nodes) << " total, "
            << pct_str(leaves, desc) << " preleaf | "
            << "final=" << pct_str(sol, nodes) << " total, "
            << pct_str(sol, leaves) << " leaf | "
            << "recorded=" << pct_str(sol2, nodes) << " total, "
            << pct_str(sol2, sol) << " final\n";

  std::cout.flags(old_flags);
  std::cout.precision(old_prec);
}

static void set_present(class_bitset& bits, uint16_t cid) {
  bits[cid >> 6] |= (1ull << (cid & 63));
}

static void clear_present_all(class_bitset& bits, std::uint16_t class_words) {
  for (std::uint16_t i = 0; i < class_words; ++i) bits[i] = 0;
}

static void clear_present_one(class_bitset& bits, std::uint16_t cid) {
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

static void pop_to(iddfs_ctx& ctx, size_t sz) {
  while (ctx.path.size() > sz) {
    const class_id cid = ctx.path.back();
    clear_present_one(ctx.present_bits, cid.id);
    ctx.path.pop_back();
  }
}

static bool leaf_missing_coverage(iddfs_ctx& ctx, const semantic_state& state) {
  if ((state.base_terms_mask & ctx.rules.goal_mask) != ctx.rules.goal_mask) {
    ctx.prof.leaf_prune_missing_coverage.fetch_add(1,
                                                   std::memory_order_relaxed);
    return true;
  }
  return false;
}

static std::optional<class_id> leaf_unique_interesting_conclusion(
    iddfs_ctx& ctx, const semantic_state& state) {
  ctx.prof.leaf_unique_scans.fetch_add(1, std::memory_order_relaxed);
  leaf_ctx lctx{ctx.sem_space, ctx.present_bits, ctx.path, ctx.prof};
  return ctx.rules.unique_interesting_conclusion(state, lctx);
}

static bool leaf_taste_banned(iddfs_ctx& ctx, class_id conc) {
  if (ctx.rules.is_banned_with_path(conc, ctx.path)) {
    ctx.prof.leaf_prune_taste_banned.fetch_add(1, std::memory_order_relaxed);
    return true;
  }
  return false;
}

static void leaf_record_coverage(class_id conc) {
  if (!has_solution[conc.id]) {
    has_solution[conc.id] = 1;
    ++covered;
  }
}

static void leaf_emit_solution(iddfs_ctx& ctx, class_id conc,
                               const semantic_state& state) {
  ctx.output.add_solution(ctx.tmp_key, ctx.syn_space.term_count, conc, ctx.path,
                          state, ctx.sem_space, ctx.syn_space, ctx.tmp_ids,
                          ctx.prof);
}

static bool handle_leaf(iddfs_ctx& ctx, const semantic_state& state) {
  if (leaf_missing_coverage(ctx, state)) {
    return false;
  }

  const std::optional<class_id> conc =
      leaf_unique_interesting_conclusion(ctx, state);
  if (!conc) {
    ctx.prof.leaf_prune_no_unique_conclusion.fetch_add(
        1, std::memory_order_relaxed);
    return false;
  }

  // if (leaf_taste_banned(ctx, *conc)) {
  //   return false;
  // }

  leaf_record_coverage(*conc);
  leaf_emit_solution(ctx, *conc, state);
  return true;
}

struct descend_plan {
  semantic_state child;
  std::uint16_t next_min_id = 0;
  std::uint8_t next_depth_left = 0;
  canonical_key iso_key{};
};

// 1) cheap prune: conflicts vs present
static bool conflict_prune(iddfs_ctx& ctx, class_id cid) {
  if (overlaps_conflicts(ctx.rules.conflict_bits_by_cid[cid.id],
                         ctx.present_bits, ctx.class_words)) {
    ctx.prof.prune_conflict.fetch_add(1, std::memory_order_relaxed);
    return true;
  }
  return false;
}

// 2) prepare child state + derived params
static descend_plan init_descend_plan(const iddfs_frame& frame, class_id cid) {
  descend_plan p{};
  p.child = frame.state;
  p.next_min_id = static_cast<std::uint16_t>(cid.id + 1);
  p.next_depth_left = static_cast<std::uint8_t>(frame.depth_left - 1);
  return p;
}

// 3) apply premise (records dead on inconsistency)
static apply_result apply_premise_result(iddfs_ctx& ctx, class_id cid,
                                         descend_plan& p) {
  ctx.prof.apply_premise_calls.fetch_add(1, std::memory_order_relaxed);
  apply_result add_premise_result = apply_premise(p.child, cid, ctx.sem_space);
  if (add_premise_result == apply_result::inconsistent) {
    ctx.memo_table.record_dead(p.child, ctx.syn_space, ctx.sem_space);
    ctx.prof.prune_inconsistent.fetch_add(1, std::memory_order_relaxed);
    return apply_result::inconsistent;
  }
  if (add_premise_result == apply_result::no_change) {
    ctx.prof.prune_no_change.fetch_add(1, std::memory_order_relaxed);
    return apply_result::no_change;
  }
  return add_premise_result;
}

// 4) update base coverage, then state-memo prune
static bool memo_prune_dead(iddfs_ctx& ctx, class_id cid, descend_plan& p) {
  p.child.base_terms_mask |= ctx.rules.base_terms_mask_by_cid[cid.id];
  p.iso_key =
      ctx.memo_table.c.make_iso_key(p.child, ctx.syn_space, ctx.sem_space);
  if (ctx.memo_table.is_dead_key(p.iso_key)) {
    ctx.prof.prune_memo_dead.fetch_add(1, std::memory_order_relaxed);
    return true;
  }
  return false;
}

// 5) dominance prune
static bool dominance_prune(iddfs_ctx& ctx, const descend_plan& p) {
  if (ctx.memo_table.should_prune_dominance_key(p.iso_key, p.next_depth_left,
                                                ctx.sem_space)) {
    ctx.prof.prune_dominance.fetch_add(1, std::memory_order_relaxed);
    return true;
  }
  return false;
}

static bool try_descend(iddfs_ctx& ctx, const iddfs_frame& frame, class_id cid,
                        semantic_state& child_state, std::uint16_t& next_min_id,
                        std::uint8_t& next_depth_left,
                        canonical_key& out_iso_key) {
  if (conflict_prune(ctx, cid)) {
    return false;
  }
  descend_plan p = init_descend_plan(frame, cid);

  if (apply_premise_result(ctx, cid, p) != apply_result::changed) {
    return false;
  }
  if (memo_prune_dead(ctx, cid, p)) {
    return false;
  }
  if (dominance_prune(ctx, p)) {
    return false;
  }
  // commit outputs
  child_state = p.child;
  next_min_id = p.next_min_id;
  next_depth_left = p.next_depth_left;
  out_iso_key = p.iso_key;
  return true;
}

static void init_limit(iddfs_ctx& ctx, std::uint8_t limit,
                       const semantic_state& root) {
  ctx.prof.current_limit.store(limit, std::memory_order_relaxed);
  ctx.prof.limit_epoch.fetch_add(1, std::memory_order_relaxed);
  ctx.output.set_depth_limit(limit);

  ctx.frame_stack.clear();
  ctx.path.clear();
  clear_present_all(ctx.present_bits, ctx.class_words);

  ctx.frame_stack.push_back(iddfs_frame{root, 0, limit, 0});
}

static bool pop_if_done(iddfs_ctx& ctx) {
  iddfs_frame& f = ctx.frame_stack.back();
  if (f.next_cid >= ctx.class_count || f.depth_left == 0) {
    pop_to(ctx, f.path_size_before);
    ctx.frame_stack.pop_back();
    return true;
  }
  return false;
}

static class_id next_candidate(iddfs_ctx& ctx, std::uint8_t limit) {
  iddfs_frame& f = ctx.frame_stack.back();
  const class_id cid{f.next_cid++};

  ctx.prof.candidates_considered.fetch_add(1, std::memory_order_relaxed);
  ctx.prof.nodes_by_limit[limit] += 1;

  return cid;
}

struct descend_out {
  semantic_state child;
  std::uint16_t next_min_id = 0;
  std::uint8_t next_depth_left = 0;
  canonical_key iso_key{};
};

static bool try_make_child(iddfs_ctx& ctx, const iddfs_frame& parent,
                           class_id cid, descend_out& out) {
  return try_descend(ctx, parent, cid, out.child, out.next_min_id,
                     out.next_depth_left, out.iso_key);
}

static std::size_t push_premise(iddfs_ctx& ctx, class_id cid) {
  const std::size_t parent_path_size = ctx.path.size();
  ctx.path.push_back(cid);
  set_present(ctx.present_bits, cid.id);
  return parent_path_size;
}

static void undo_premise(iddfs_ctx& ctx, class_id cid) {
  clear_present_one(ctx.present_bits, cid.id);
  ctx.path.pop_back();
}

static void handle_leaf_node(iddfs_ctx& ctx, std::uint8_t limit, class_id cid,
                             const semantic_state& child) {
  ctx.prof.leaves_reached.fetch_add(1, std::memory_order_relaxed);
  ctx.prof.leaves_by_limit[limit] += 1;

  const std::size_t k = ctx.path.size();
  if (k < ctx.prof.leaves_by_k.size()) ctx.prof.leaves_by_k[k] += 1;

  if (handle_leaf(ctx, child)) {
    ctx.prof.solutions_emitted.fetch_add(1, std::memory_order_relaxed);
    ctx.prof.solutions_by_limit[limit] += 1;
    if (k < ctx.prof.solutions_by_k.size()) ctx.prof.solutions_by_k[k] += 1;
  }

  undo_premise(ctx, cid);
}

static bool should_expand_or_undo(iddfs_ctx& ctx, class_id cid,
                                  const semantic_state& child,
                                  std::uint16_t next_min_id,
                                  std::uint8_t next_depth_left) {
  if (!ctx.rules.should_expand(child, next_min_id, next_depth_left,
                               ctx.sem_space)) {
    ctx.prof.prune_should_expand.fetch_add(1, std::memory_order_relaxed);
    undo_premise(ctx, cid);
    return false;
  }
  return true;
}

static bool premise_seen_or_undo(iddfs_ctx& ctx, class_id cid,
                                 std::uint8_t next_depth_left) {
  canon_premise_set_bitset_key(ctx.pkey, ctx.path, ctx.syn_space,
                               ctx.class_words, ctx.tmp_pkey);

  if (auto it = ctx.premise_seen.find(ctx.pkey); it != ctx.premise_seen.end()) {
    if (next_depth_left <= static_cast<std::uint8_t>(it->second)) {
      ctx.prof.prune_premise_seen.fetch_add(1, std::memory_order_relaxed);
      undo_premise(ctx, cid);
      return false;
    }
  }

  auto [ins_it, inserted] = ctx.premise_seen.emplace(
      ctx.pkey, static_cast<std::int16_t>(next_depth_left));
  if (!inserted &&
      next_depth_left > static_cast<std::uint8_t>(ins_it->second)) {
    ins_it->second = static_cast<std::int16_t>(next_depth_left);
  }

  return true;
}

static void push_child_frame(iddfs_ctx& ctx, const descend_out& d,
                             std::uint16_t next_min_id,
                             std::uint8_t next_depth_left,
                             std::size_t parent_path_size) {
  ctx.memo_table.record_dominance(d.child, next_depth_left, ctx.syn_space,
                                  ctx.sem_space);
  ctx.frame_stack.push_back(
      iddfs_frame{d.child, next_min_id, next_depth_left, parent_path_size});
}

static void step(iddfs_ctx& ctx, std::uint8_t limit,
                 std::size_t& max_stack_depth_this_limit) {
  ctx.prof.on_stack_depth(ctx.frame_stack.size());
  max_stack_depth_this_limit =
      std::max(max_stack_depth_this_limit, ctx.frame_stack.size());

  if (pop_if_done(ctx)) return;

  iddfs_frame& curr = ctx.frame_stack.back();
  const class_id cid = next_candidate(ctx, limit);
  ;

  descend_out d{};
  if (!try_make_child(ctx, curr, cid, d)) return;
  ctx.prof.descended.fetch_add(1, std::memory_order_relaxed);

  const std::size_t parent_path_size = push_premise(ctx, cid);

  if (curr.depth_left == 1) {
    handle_leaf_node(ctx, limit, cid, d.child);
    return;
  }

  // if (!should_expand_or_undo(ctx, cid, d.child, d.next_min_id,
  //                            d.next_depth_left)) {
  //   return;
  // }

  // if (!premise_seen_or_undo(ctx, cid, d.next_depth_left)) {
  //   return;
  // }
  push_child_frame(ctx, d, d.next_min_id, d.next_depth_left, parent_path_size);
}

static bool finish_limit(iddfs_ctx& ctx, std::uint8_t limit,
                         std::size_t max_stack_depth_this_limit,
                         std::uint64_t solutions_before_limit,
                         std::uint8_t start_limit) {
  //  ctx.prof.premise_seen_size.store(ctx.premise_seen.size(),
  //                                    std::memory_order_relaxed);
  //  ctx.prof.premise_seen_buckets.store(ctx.premise_seen.bucket_count(),
  //                                       std::memory_order_relaxed);

  std::cerr << "Finished Depth: " << std::to_string(limit) << "\n";

  const std::uint64_t new_solutions =
      ctx.prof.solutions_emitted.load(std::memory_order_relaxed) -
      solutions_before_limit;

  if ((ctx.prof.solutions_emitted.load(std::memory_order_relaxed) >= 1000 ||
       new_solutions == 0) &&
      limit > start_limit) {
    return false;
  }

  if (max_stack_depth_this_limit < limit) {
    return false;
  }

  return true;
}

void run_iddfs(const semantic_space& sem_space, const syntax_space& syn_space,
               const prune_rules& rules, memo& memo_table, semantic_state root,
               premise_path& path, std::uint8_t max_depth, collector& output,
               profiler& prof) {
  iddfs_ctx ctx{sem_space,
                syn_space,
                rules,
                memo_table,
                output,
                prof,
                static_cast<std::uint16_t>(syn_space.class_count()),
                static_cast<std::uint16_t>((syn_space.class_count() + 63) / 64),
                max_depth,
                {},
                {},
                path,
                premise_seen_map{},
                {},
                {},
                {},
                {}};

  has_solution.assign(ctx.class_count, 0);
  covered = 0;
  plateau = 0;

  // ctx.premise_seen =
  //     premise_seen_map(1, premise_bitset_key_hash{ctx.class_words},
  //                      premise_bitset_key_eq{ctx.class_words});
  // ctx.premise_seen.max_load_factor(0.7f);
  // ctx.premise_seen.rehash(1'000'000);

  ctx.tmp_ids.reserve(static_cast<std::size_t>(max_depth) + 1);
  ctx.tmp_key.ids.reserve(static_cast<std::size_t>(max_depth) + 1);

  // term count -1 is min premises to have a connected premise set
  const std::uint8_t start_limit =
      std::max<std::uint8_t>(2, ctx.syn_space.term_count - 1);

  for (std::uint8_t limit = start_limit; limit <= max_depth; ++limit) {
    const std::uint64_t solutions_before =
        ctx.prof.solutions_emitted.load(std::memory_order_relaxed);

    init_limit(ctx, limit, root);
    if (stop_requested()) {
      finish_limit(ctx, limit, /*max_stack_depth*/ 0, solutions_before,
                   start_limit);
      return;
    }

    std::size_t max_stack_depth_this_limit = 0;
    while (!ctx.frame_stack.empty()) {
      if (stop_requested()) {
        // write snapshot + flush what you have so far
        finish_limit(ctx, limit, max_stack_depth_this_limit, solutions_before,
                     start_limit);
        return;
      }
      step(ctx, limit, max_stack_depth_this_limit);
    }
    if (!finish_limit(ctx, limit, max_stack_depth_this_limit, solutions_before,
                      start_limit)) {
      break;
    }
  }
}

}  // namespace conclusion_explorer
