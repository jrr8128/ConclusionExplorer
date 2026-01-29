#include "puzzle_collector.hpp"

#include <filesystem>
#include <sstream>

#include "canonicalizer.hpp"
#include "profiler.hpp"

namespace fs = std::filesystem;

namespace conclusion_explorer {

static char term_char(std::uint8_t t) { return static_cast<char>('A' + t); }

static std::string literal_name(const term_literal lit) {
  // Keep it simple and consistent for scanning debug logs.
  // Examples: "A", "¬A"
  std::string s;
  if (lit.is_complement) s += "¬";
  s += term_char(lit.term);
  return s;
}

static const char* form_word(form f) {
  switch (f) {
    case form::A:
      return "All";
    case form::E:
      return "No";
    case form::I:
      return "Some";
    case form::O:
      return "Some";
  }
  return "?";
}

static const char* copula(form f) {
  switch (f) {
    case form::A:
      return "are";
    case form::E:
      return "are";
    case form::I:
      return "are";
    case form::O:
      return "are not";
  }
  return "?";
}

static std::string statement_english(const statement& s) {
  // Examples:
  // E: "No A are B"
  // I: "Some A are B"
  // (You’re searching E/I reps; still handle all 4 for completeness.)
  std::ostringstream oss;
  oss << form_word(s.f) << " " << literal_name(s.subject) << " " << copula(s.f)
      << " " << literal_name(s.predicate);
  return oss.str();
}

collector::collector(collector_config cfg) : cfg_(std::move(cfg)) {}

void collector::begin_term_run(std::uint8_t term_count,
                               const syntax_space& syn_space) {
  current_term_count_ = term_count;
  current_depth_limit_ = 0;
  next_solution_id_ = 1;

  output_by_k_.clear();
  debug_by_k_.clear();

  // Ensure directories exist.
  fs::create_directories(fs::path(cfg_.recipes_root) / "output" /
                         (std::to_string(term_count) + "-terms"));
  if (cfg_.enable_debug) {
    fs::create_directories(fs::path(cfg_.recipes_root) / "debug" /
                           (std::to_string(term_count) + "-terms"));
  }
  fs::create_directories(fs::path(cfg_.recipes_root) / "meta" /
                         (std::to_string(term_count) + "-terms"));

  write_meta_(term_count, syn_space);
}

void collector::set_depth_limit(std::uint8_t depth_limit) {
  current_depth_limit_ = depth_limit;
}

collector::file_sink& collector::get_output_sink_(std::uint8_t term_count,
                                                  std::uint16_t k) {
  auto it = output_by_k_.find(k);
  if (it != output_by_k_.end()) return it->second;

  file_sink sink{};
  sink.flush_bytes = cfg_.flush_bytes;

  fs::path p = fs::path(cfg_.recipes_root) / "output" /
               (std::to_string(term_count) + "-terms");

  // You said “per premise count file”. Optional depth tagging:
  // "k-premises.txt" or "k-premises.d<limit>.txt"
  std::string name = std::to_string(k) + "-premises";
  if (current_depth_limit_) name += ".d" + std::to_string(current_depth_limit_);
  name += ".txt";

  p /= name;

  sink.out.open(p, std::ios::out | std::ios::binary);
  auto [ins_it, _] = output_by_k_.emplace(k, std::move(sink));
  return ins_it->second;
}

collector::file_sink& collector::get_debug_sink_(std::uint8_t term_count,
                                                 std::uint16_t k) {
  auto it = debug_by_k_.find(k);
  if (it != debug_by_k_.end()) return it->second;

  file_sink sink{};
  sink.flush_bytes = cfg_.flush_bytes;

  fs::path p = fs::path(cfg_.recipes_root) / "debug" /
               (std::to_string(term_count) + "-terms");

  std::string name = std::to_string(k) + "-premises";
  if (current_depth_limit_) name += ".d" + std::to_string(current_depth_limit_);
  name += ".txt";

  p /= name;

  sink.out.open(p, std::ios::out | std::ios::binary);
  auto [ins_it, _] = debug_by_k_.emplace(k, std::move(sink));
  return ins_it->second;
}

void collector::append_(file_sink& sink, const std::string& s) {
  sink.buffer += s;
  if (sink.buffer.size() >= sink.flush_bytes) {
    sink.out.write(sink.buffer.data(),
                   static_cast<std::streamsize>(sink.buffer.size()));
    sink.buffer.clear();
  }
}

void collector::add_solution(cid_list_key& tmp_key, std::uint8_t term_count,
                             class_id conclusion_cid, const premise_path& path,
                             const semantic_state& state,
                             const semantic_space& sem_space,
                             const syntax_space& syn_space,
                             std::vector<std::uint16_t>& tmp_ids,
                             profiler& prof) {
  const std::uint16_t k = static_cast<std::uint16_t>(path.size());
  if (k == 0) {
    return;
  }

  canon_recipe_key(tmp_key, conclusion_cid, path, syn_space, tmp_ids);
  auto [iter, inserted] = iso_count_by_rep_.emplace(tmp_key, 1);
  if (!inserted) {
    ++iter->second;
    return;
  }

  prof.solutions_accepted.fetch_add(1, std::memory_order_relaxed);
  // Compact output line:
  // conc | cid cid cid
  {
    file_sink& out = get_output_sink_(term_count, k);
    std::ostringstream line;
    line << conclusion_cid.id << " |";
    for (class_id cid : path) line << " " << cid.id;
    line << "\n";
    append_(out, line.str());
  }

  if (!cfg_.enable_debug) return;

  // Debug block:
  // line 1: cids... -> conc
  // premises numbered
  // final line: C: english
  {
    file_sink& dbg = get_debug_sink_(term_count, k);
    const std::uint64_t solution_id = next_solution_id_++;
    std::ostringstream block;
    block << "#" << solution_id << ": ";
    for (class_id cid : path) block << " " << cid.id;
    block << " -> " << conclusion_cid.id;
    block << "\n";

    for (std::size_t i = 0; i < path.size(); ++i) {
      const class_id cid = path[i];
      const statement& s = syn_space.rep_statement_by_class_id[cid.id];
      block << (i + 1) << ". " << statement_english(s) << "\n";
    }

    const statement& conc_s =
        syn_space.rep_statement_by_class_id[conclusion_cid.id];
    block << "C: " << statement_english(conc_s) << "\n\n";

    append_(dbg, block.str());
  }
}

void collector::write_meta_(std::uint8_t term_count,
                            const syntax_space& syn_space) {
  fs::path meta_dir = fs::path(cfg_.recipes_root) / "meta" /
                      (std::to_string(term_count) + "-terms");
  fs::path meta_file = meta_dir / "meta.txt";

  std::ofstream out(meta_file, std::ios::out | std::ios::binary);

  const std::uint16_t class_count =
      static_cast<std::uint16_t>(syn_space.class_count());
  const std::uint16_t stmt_count =
      static_cast<std::uint16_t>(syn_space.all_statements_by_id.size());

  // Build class -> members list.
  std::vector<std::vector<statement_id>> members(class_count);
  members.reserve(class_count);
  for (std::uint16_t sid = 0; sid < stmt_count; ++sid) {
    const class_id cid = syn_space.class_id_by_statement_id[sid];
    members[cid.id].push_back(statement_id{sid});
  }

  out << "term_count " << static_cast<int>(term_count) << "\n";
  out << "class_count " << class_count << "\n";
  out << "statement_count " << stmt_count << "\n\n";

  out << "[class_members]\n";
  for (std::uint16_t cid = 0; cid < class_count; ++cid) {
    out << cid << " :";
    for (statement_id sid : members[cid]) out << " " << sid.id;
    out << "\n";
  }
  out << "\n";

  out << "[statements]\n";
  // statement_id : f s_term s_comp p_term p_comp
  for (std::uint16_t sid = 0; sid < stmt_count; ++sid) {
    const statement& s = syn_space.all_statements_by_id[sid];
    out << sid << " : " << static_cast<int>(s.f) << " "
        << static_cast<int>(s.subject.term) << " "
        << static_cast<int>(s.subject.is_complement) << " "
        << static_cast<int>(s.predicate.term) << " "
        << static_cast<int>(s.predicate.is_complement) << "\n";
  }
  out << "\n";
}

void collector::finalize() {
  for (auto& [k, sink] : output_by_k_) {
    if (!sink.buffer.empty()) {
      sink.out.write(sink.buffer.data(),
                     static_cast<std::streamsize>(sink.buffer.size()));
      sink.buffer.clear();
    }
    sink.out.close();
  }
  for (auto& [k, sink] : debug_by_k_) {
    if (!sink.buffer.empty()) {
      sink.out.write(sink.buffer.data(),
                     static_cast<std::streamsize>(sink.buffer.size()));
      sink.buffer.clear();
    }
    sink.out.close();
  }
}

void collector::flush() {
  for (auto& [k, sink] : output_by_k_) {
    if (!sink.buffer.empty()) {
      sink.out.write(sink.buffer.data(),
                     static_cast<std::streamsize>(sink.buffer.size()));
      sink.buffer.clear();
    }
    sink.out.flush();
  }
  for (auto& [k, sink] : debug_by_k_) {
    if (!sink.buffer.empty()) {
      sink.out.write(sink.buffer.data(),
                     static_cast<std::streamsize>(sink.buffer.size()));
      sink.buffer.clear();
    }
    sink.out.flush();
  }
}

}  // namespace conclusion_explorer
