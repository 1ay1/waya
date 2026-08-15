#pragma once
/// \file ui/markdown.hpp
/// markdown(src) — render a useful subset of Markdown to a NODE TREE.
///
/// `markup()` injects raw HTML — fine for content you author, dangerous for
/// anything user-supplied. Chat messages, comments, README previews, LLM output:
/// you want Markdown rendered, but SAFE. `markdown()` parses to waya nodes
/// (text/box/link/…), never raw HTML — so there is no injection surface at all,
/// and the result styles + diffs like any other subtree.
///
///   markdown(m.comment_body) | max_w(680)
///
/// Supported (the 90% that appears in real prose):
///   • headings          # .. ######
///   • paragraphs        blank-line separated
///   • bold / italic     **b** *i* (and `code` spans)
///   • links             [text](url)   (url sanitised, javascript: stripped)
///   • bullet lists      - / * items
///   • ordered lists     1. items
///   • blockquotes       > line
///   • fenced code       ```lang \n … \n ```
///   • horizontal rule   ---
///
/// It's line-oriented and dependency-free — not a full CommonMark engine, but
/// the reliable core, and every span goes through waya's escaping so a `<script>`
/// in the source renders as literal text, never executes.

#include "../surface/node.hpp"
#include "components.hpp"

#include <string>
#include <vector>

namespace waya::ui {

using namespace waya::surface;

namespace md_detail {

/// Parse inline spans (**bold**, *italic*, `code`, [text](url)) into styled
/// text nodes. Everything is real text() — escaped by the backend — so no HTML
/// can be injected. `url`s go through the same safe-url path as href().
inline std::vector<NodeRef> inlines(const std::string& s){
    std::vector<NodeRef> out;
    std::string buf;
    auto flush = [&]{ if(!buf.empty()){ out.push_back(text(buf) | detail::raw_css("white-space","pre-wrap")); buf.clear(); } };
    for (std::size_t i = 0; i < s.size(); ){
        char c = s[i];
        // links: [text](url)
        if (c == '['){
            auto close = s.find(']', i);
            if (close != std::string::npos && close+1 < s.size() && s[close+1] == '('){
                auto paren = s.find(')', close+2);
                if (paren != std::string::npos){
                    flush();
                    std::string label = s.substr(i+1, close-i-1);
                    std::string url   = s.substr(close+2, paren-close-2);
                    out.push_back(link_to(label, url) | detail::raw_css("text-decoration","underline"));
                    i = paren + 1; continue;
                }
            }
        }
        // bold: **text**
        if (c == '*' && i+1 < s.size() && s[i+1] == '*'){
            auto end = s.find("**", i+2);
            if (end != std::string::npos){
                flush();
                out.push_back(text(s.substr(i+2, end-i-2)) | semibold);
                i = end + 2; continue;
            }
        }
        // italic: *text*
        if (c == '*'){
            auto end = s.find('*', i+1);
            if (end != std::string::npos){
                flush();
                out.push_back(text(s.substr(i+1, end-i-1)) | italic);
                i = end + 1; continue;
            }
        }
        // inline code: `text`
        if (c == '`'){
            auto end = s.find('`', i+1);
            if (end != std::string::npos){
                flush();
                out.push_back(text(s.substr(i+1, end-i-1)) | mono
                    | detail::raw_css("background","var(--wa-raised, rgba(255,255,255,.08))")
                    | pad_x(5) | round(4) | detail::raw_css("font-size",".9em"));
                i = end + 1; continue;
            }
        }
        buf += c; ++i;
    }
    flush();
    return out;
}

inline std::string trim(const std::string& s){
    std::size_t a = s.find_first_not_of(" \t\r");
    if (a == std::string::npos) return {};
    std::size_t b = s.find_last_not_of(" \t\r");
    return s.substr(a, b-a+1);
}
inline std::vector<std::string> split_lines(const std::string& s){
    std::vector<std::string> out; std::string cur;
    for (char c : s){ if (c=='\n'){ out.push_back(cur); cur.clear(); } else cur += c; }
    out.push_back(cur);
    return out;
}

} // namespace md_detail

/// `markdown(source)` — render Markdown to a safe node tree (no raw HTML).
inline NodeRef markdown(const std::string& source){
    using namespace md_detail;
    auto lines = split_lines(source);
    std::vector<NodeRef> blocks;

    auto para_row = [](std::vector<NodeRef> spans){
        auto p = box(); p->kids = std::move(spans); p->style.flow = Flow::row;
        p->style.wrap = Wrap::wrap; finalize(*p);
        return p | detail::raw_css("line-height","1.6");
    };

    for (std::size_t i = 0; i < lines.size(); ){
        std::string line = lines[i];
        std::string t = trim(line);

        // blank line: separator
        if (t.empty()){ ++i; continue; }

        // fenced code block
        if (t.rfind("```", 0) == 0){
            std::string code; ++i;
            while (i < lines.size() && trim(lines[i]).rfind("```", 0) != 0){ code += lines[i]; code += '\n'; ++i; }
            if (i < lines.size()) ++i;   // closing fence
            if (!code.empty() && code.back()=='\n') code.pop_back();
            blocks.push_back(text(code) | mono | pre_wrap | w_full | pad(12) | round(8)
                | detail::raw_css("background","var(--wa-bg, #0b1020)")
                | detail::raw_css("border","1px solid var(--wa-line, rgba(255,255,255,.10))")
                | text_size(13) | detail::raw_css("overflow-x","auto"));
            continue;
        }

        // horizontal rule
        if (t == "---" || t == "***" || t == "___"){
            blocks.push_back(box() | h(1) | w_full | pad_y(0)
                | detail::raw_css("background","var(--wa-line, rgba(255,255,255,.12))") | detail::raw_css("margin","8px 0"));
            ++i; continue;
        }

        // heading
        if (t[0] == '#'){
            int level = 0; while (level < (int)t.size() && t[level]=='#') ++level;
            if (level <= 6 && level < (int)t.size() && t[level]==' '){
                std::string htext = trim(t.substr(level));
                float sz = 30 - (level-1)*3.0f;
                // a heading is single-line: render its spans as a row but push
                // the font-size onto each SPAN (font-size only emits on text/
                // input nodes, not the wrapping box).
                auto spans = inlines(htext);
                for (auto& sp : spans) sp = sp | font(sz) | semibold;
                blocks.push_back(para_row(std::move(spans)) | detail::raw_css("margin-top","8px"));
                ++i; continue;
            }
        }

        // blockquote
        if (t[0] == '>'){
            std::vector<NodeRef> qlines;
            while (i < lines.size() && !trim(lines[i]).empty() && trim(lines[i])[0]=='>'){
                qlines.push_back(para_row(inlines(trim(trim(lines[i]).substr(1)))));
                ++i;
            }
            auto q = box(); q->kids = std::move(qlines); q->style.flow = Flow::col;
            q->style.gap = {4,Unit::px}; finalize(*q);
            blocks.push_back(q | pad_x(14) | fg_muted
                | detail::raw_css("border-left","3px solid var(--wa-line, rgba(255,255,255,.25))"));
            continue;
        }

        // unordered list
        if ((t[0]=='-' || t[0]=='*') && t.size()>1 && t[1]==' '){
            std::vector<NodeRef> items;
            while (i < lines.size()){
                std::string lt = trim(lines[i]);
                if (!((lt.size()>1) && (lt[0]=='-'||lt[0]=='*') && lt[1]==' ')) break;
                items.push_back(row(text("\xe2\x80\xa2") | fg_muted, para_row(inlines(trim(lt.substr(2))))) | gap(8) | items_start);
                ++i;
            }
            auto ul = box(); ul->kids = std::move(items); ul->style.flow = Flow::col;
            ul->style.gap = {4,Unit::px}; finalize(*ul);
            blocks.push_back(ul | pad_x(4));
            continue;
        }

        // ordered list (1. 2. …)
        if (t.size()>2 && t[0]>='0' && t[0]<='9'){
            std::size_t dot = t.find('.');
            if (dot != std::string::npos && dot+1 < t.size() && t[dot+1]==' '){
                std::vector<NodeRef> items; int num = 1;
                while (i < lines.size()){
                    std::string lt = trim(lines[i]);
                    auto d = lt.find('.');
                    if (!(lt.size()>2 && lt[0]>='0' && lt[0]<='9' && d!=std::string::npos && d+1<lt.size() && lt[d+1]==' ')) break;
                    items.push_back(row(text(std::to_string(num++) + ".") | fg_muted, para_row(inlines(trim(lt.substr(d+2))))) | gap(8) | items_start);
                    ++i;
                }
                auto ol = box(); ol->kids = std::move(items); ol->style.flow = Flow::col;
                ol->style.gap = {4,Unit::px}; finalize(*ol);
                blocks.push_back(ol | pad_x(4));
                continue;
            }
        }

        // paragraph: gather consecutive non-blank, non-special lines
        std::string para;
        while (i < lines.size()){
            std::string lt = trim(lines[i]);
            if (lt.empty() || lt[0]=='#' || lt[0]=='>' || lt.rfind("```",0)==0
                || ((lt[0]=='-'||lt[0]=='*') && lt.size()>1 && lt[1]==' ')) break;
            if (!para.empty()) para += ' ';
            para += lt;
            ++i;
        }
        if (!para.empty()) blocks.push_back(para_row(inlines(para)));
    }

    auto root = box(); root->kids = std::move(blocks); root->style.flow = Flow::col;
    root->style.gap = {12,Unit::px}; finalize(*root);
    return root | fg_text;
}

} // namespace waya::ui
