#include "syntax.hpp"
#include <cassert>

namespace conclusion_explorer{
    static std::vector<char> build_terms(std::uint8_t term_count){
        std::vector<char> terms;
        terms.reserve(term_count);

        for(std::uint8_t index = 0; index < term_count; index++){
            terms.push_back('A' + index);
        }
        return terms;
    }

    static all_statements build_statements(std::uint8_t term_count){
        all_statements all_stmts;
        std::vector<char> terms = build_terms(term_count);
        assert(terms[0] == 'A');

        return all_stmts;
    }

    syntax_space build_syntax_space(std::uint8_t term_count){
        assert(term_count <= 8);
        syntax_space syn_space{};
        syn_space.term_count = term_count;
        syn_space.all_statements = build_statements(term_count);

        return syn_space;

    }
}