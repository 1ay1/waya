#pragma once
/// \file ui/code_view.hpp
/// code_view() — syntax-highlighted code as a SAFE node tree.
///
/// `code_block` (patterns.hpp) shows monospace text with no highlighting;
/// markdown's fenced code is the same. Real code reads far better coloured \u2014
/// but you can't do that with `markup()` on user/LLM-supplied code without an
/// injection risk. `code_view()` tokenizes the source and emits COLOURED text
/// nodes (never raw HTML), so a `<script>` in the code renders as literal,
/// coloured text and the whole thing diffs like any subtree.
///
///   code_view(source, "cpp") | max_w(720)
///
/// The lexer is deliberately small and language-agnostic-ish: it recognises
/// line/block comments, string + char literals, numbers, and a keyword set you
/// can pass in (a C-family default is built in). It's a HIGHLIGHTER, not a
/// parser \u2014 the point is legibility, safe and dependency-free, not a compiler.

#include "../surface/node.hpp"
#include "components.hpp"

#include <string>
#include <unordered_set>
#include <vector>

namespace waya::ui {

using namespace waya::surface;

namespace code_detail {
// A palette (dark-theme friendly). Distinct hues per token class.
inline constexpr std::uint32_t c_plain   = 0xd6deeb;
inline constexpr std::uint32_t c_keyword = 0xc792ea;   // purple
inline constexpr std::uint32_t c_string  = 0xc3e88d;   // green
inline constexpr std::uint32_t c_number  = 0xf78c6c;   // orange
inline constexpr std::uint32_t c_comment = 0x637777;   // muted
inline constexpr std::uint32_t c_punct   = 0x89ddff;   // cyan

inline const std::unordered_set<std::string>& c_family_keywords(){
    static const std::unordered_set<std::string> kw = {
        // control + declarations common across C/C++/JS/TS/Rust/Go/Java/…
        "if","else","for","while","do","switch","case","default","break","continue",
        "return","goto","try","catch","finally","throw","yield","await","async",
        "class","struct","enum","union","interface","trait","impl","namespace","module",
        "public","private","protected","static","const","constexpr","final","virtual",
        "override","abstract","inline","extern","export","import","package","use","using",
        "let","var","const","fn","func","function","def","void","auto","new","delete",
        "template","typename","typedef","type","match","where","move","mut","ref",
        "true","false","null","nullptr","none","undefined","this","self","super",
        "int","float","double","bool","char","string","str","long","short","unsigned",
        "signed","size_t","uint","usize","isize","i32","i64","u32","u64","f32","f64",
        "in","of","as","is","not","and","or","typeof","instanceof","sizeof","operator",
    };
    return kw;
}

inline bool is_ident_start(char c){ return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'||c=='$'; }
inline bool is_ident(char c){ return is_ident_start(c)||(c>='0'&&c<='9'); }
inline bool is_digit(char c){ return c>='0'&&c<='9'; }

/// A coloured run of source text.
struct Token { std::string text; std::uint32_t color; };

/// Tokenise `src` for highlighting. Handles // and /* */ and # comments,
/// "..." '...' `...` strings (with escapes), numbers, keywords, punctuation.
inline std::vector<Token> lex(const std::string& src,
                              const std::unordered_set<std::string>& kw){
    std::vector<Token> out;
    std::size_t i = 0, n = src.size();
    auto emit = [&](std::size_t a, std::size_t b, std::uint32_t c){ if(b>a) out.push_back({ src.substr(a, b-a), c }); };
    while (i < n){
        char c = src[i];
        // line comment: // or #
        if ((c=='/' && i+1<n && src[i+1]=='/') || c=='#'){
            std::size_t a=i; while(i<n && src[i]!='\n') ++i; emit(a,i,c_comment); continue;
        }
        // block comment /* … */
        if (c=='/' && i+1<n && src[i+1]=='*'){
            std::size_t a=i; i+=2; while(i+1<n && !(src[i]=='*'&&src[i+1]=='/')) ++i; if(i+1<n) i+=2; else i=n; emit(a,i,c_comment); continue;
        }
        // string / char / template literals
        if (c=='"' || c=='\'' || c=='`'){
            char q=c; std::size_t a=i; ++i;
            while(i<n){ if(src[i]=='\\'){ i+=2; continue; } if(src[i]==q){ ++i; break; } ++i; }
            emit(a,i,c_string); continue;
        }
        // number
        if (is_digit(c) || (c=='.' && i+1<n && is_digit(src[i+1]))){
            std::size_t a=i; while(i<n && (is_ident(src[i])||src[i]=='.'||src[i]=='x'||src[i]=='X')) ++i; emit(a,i,c_number); continue;
        }
        // identifier / keyword
        if (is_ident_start(c)){
            std::size_t a=i; while(i<n && is_ident(src[i])) ++i;
            std::string word = src.substr(a, i-a);
            emit(a,i, kw.count(word) ? c_keyword : c_plain); continue;
        }
        // punctuation run (operators/brackets) vs whitespace/plain
        if (c=='{'||c=='}'||c=='('||c==')'||c=='['||c==']'||c==';'||c==','||c=='.'||
            c=='+'||c=='-'||c=='*'||c=='/'||c=='='||c=='<'||c=='>'||c=='&'||c=='|'||
            c=='!'||c=='?'||c==':'||c=='%'||c=='^'||c=='~'){
            emit(i,i+1,c_punct); ++i; continue;
        }
        // plain (whitespace and anything else) \u2014 batch a run
        std::size_t a=i; while(i<n && src[i]!='/' && src[i]!='#' && src[i]!='"' && src[i]!='\'' && src[i]!='`'
                               && !is_ident_start(src[i]) && !is_digit(src[i])
                               && std::string("{}()[];,.+-*/=<>&|!?:%^~").find(src[i])==std::string::npos) ++i;
        if(i==a) ++i;  // guard: always advance
        emit(a,i,c_plain);
    }
    return out;
}
} // namespace code_detail

/// `code_view(source, lang)` — syntax-highlighted code, safe (no raw HTML).
/// `lang` is advisory (the highlighter is C-family by default). Pass your own
/// keyword set to specialise. Whitespace + newlines are preserved (`pre`).
inline NodeRef code_view(const std::string& source, std::string lang = "",
                         const std::unordered_set<std::string>* keywords = nullptr){
    (void)lang;
    auto toks = code_detail::lex(source, keywords ? *keywords : code_detail::c_family_keywords());
    std::vector<NodeRef> spans;
    spans.reserve(toks.size());
    for (auto& t : toks)
        spans.push_back(text(t.text) | fg(t.color) | pre);
    // one wrapping <code>-ish block: monospace, scrollable, framed.
    auto box_ = box(); box_->kids = std::move(spans); box_->style.wrap = Wrap::nowrap;
    finalize(*box_);
    return box_ | mono | pre | pad(14) | round(8)
        | detail::raw_css("background", "var(--wa-bg, #0b1020)")
        | detail::raw_css("border", "1px solid var(--wa-line, rgba(255,255,255,.10))")
        | detail::raw_css("font-size", "13px")
        | detail::raw_css("line-height", "1.55")
        | detail::raw_css("overflow-x", "auto")
        | detail::raw_css("white-space", "pre");
}

} // namespace waya::ui
