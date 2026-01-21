#pragma once
#include "common_types.hpp"
#include <vector>

namespace conclusion_explorer {
    struct syntax_space{
        std::uint8_t term_count;
        all_statements all_statements;
        search_statement_ids search_ids;
        equiv_class_id_by_statement_id class_id_by_statement_id;
        std::vector<representative_id> rep_id_by_class_id;
    };
    syntax_space build_syntax_space(std::uint8_t term_count);
}