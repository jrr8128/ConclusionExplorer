#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "common_types.hpp"
#include "semantic.hpp"  // or "semantic.hpp" (whatever your file is)
#include "semantic_state.hpp"
#include "syntax.hpp"

namespace conclusion_explorer {

using premise_path = std::vector<class_id>;
struct cid_list_key_hash {
  size_t operator()(const cid_list_key& k) const noexcept {
    size_t h = 0;
    for (std::uint16_t x : k.ids) {
      h ^= static_cast<size_t>(x) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    }
    return h;
  }
};

struct collector_config {
  std::string recipes_root = "recipes";
  bool enable_debug = true;

  std::size_t flush_bytes = 1u << 20;  // batching in 1 MiB increments
};

class collector {
 public:
  explicit collector(collector_config cfg);

  void begin_term_run(std::uint8_t term_count, const syntax_space& syn_space);

  void set_depth_limit(std::uint8_t depth_limit);

  void add_solution(cid_list_key&, std::uint8_t term_count,
                    class_id conclusion_cid, const premise_path& path,
                    const semantic_state& state,
                    const semantic_space& sem_space,
                    const syntax_space& syn_space, std::vector<std::uint16_t>&,
                    profiler&);

  std::unordered_map<cid_list_key, std::uint32_t, cid_list_key_hash>
      iso_count_by_rep_;

  // Flush all buffers and close files.
  void finalize();
  void flush();

 private:
  struct file_sink {
    std::ofstream out;
    std::string buffer;
    std::size_t flush_bytes = 0;
  };

  collector_config cfg_;
  std::uint8_t current_term_count_ = 0;
  std::uint8_t current_depth_limit_ = 0;
  std::uint64_t next_solution_id_ = 1;

  // keyed by premise_count (path.size()).
  std::unordered_map<std::uint16_t, file_sink> output_by_k_;
  std::unordered_map<std::uint16_t, file_sink> debug_by_k_;

  void write_meta_(std::uint8_t term_count, const syntax_space& syn_space);

  file_sink& get_output_sink_(std::uint8_t term_count, std::uint16_t k);
  file_sink& get_debug_sink_(std::uint8_t term_count, std::uint16_t k);

  void append_(file_sink& sink, const std::string& s);
};

}  // namespace conclusion_explorer
