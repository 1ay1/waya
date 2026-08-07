/// examples/lumen.cpp — LUMEN: a command palette, the "wait, C++ did that?"
/// demo. Press Cmd/Ctrl-K to summon a launcher: type to filter a keyed list
/// (results reorder with a glide), arrow to move, Enter to run, Esc to close.
/// Global shortcut, live input, keyed animation, and a modal layer -- each one
/// line of waya.
///
///   waya run lumen            # then open the printed URL and hit Cmd/Ctrl-K

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>
#include <waya/ui.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <variant>
#include <vector>

using namespace waya::surface;
using namespace waya::surface::literals;
using namespace waya::ui;

struct Lumen {
    static constexpr std::uint32_t ink    = 0xeef2f8;
    static constexpr std::uint32_t body_c = 0x8b98af;
    static constexpr std::uint32_t faint  = 0x5b6478;
    static constexpr std::uint32_t brand  = 0x6d7cff;

    struct Command { std::string id, ico, name, group; std::uint32_t hue; };

    static const std::vector<Command>& all() {
        static const std::vector<Command> cs = {
            { "new",      "plus",     "New document",        "Create",  0x6d7cff },
            { "search",   "search",   "Search everything",   "Navigate",0x00d4ff },
            { "invite",   "user",     "Invite teammate",     "Team",    0xa78bfa },
            { "mail",     "mail",     "Compose message",     "Team",    0xa78bfa },
            { "settings", "settings", "Open settings",       "System",  0x8b98af },
            { "deploy",   "arrow-right","Deploy to production","Ship",   0x2ee6a6 },
            { "star",     "star",     "Star this repository","Ship",    0xf0b429 },
            { "docs",     "external", "Open documentation",  "Help",    0x00d4ff },
            { "trash",    "trash",    "Delete workspace",    "Danger",  0xff6b81 },
        };
        return cs;
    }

    struct Model { bool open = false; std::string query; int sel = 0; std::string last = "deploy"; };

    struct Open {}; struct Close {}; struct Query {}; struct Move { int d; }; struct Run {}; struct Pick { std::string id; };
    using Msg = std::variant<Open, Close, Query, Move, Run, Pick>;

    static Model init() { return {}; }

    static std::vector<Command> filtered(const std::string& q) {
        std::string lc; for (char c : q) lc += (char)std::tolower((unsigned char)c);
        std::vector<Command> out;
        for (auto& c : all()) {
            std::string nm; for (char ch : c.name) nm += (char)std::tolower((unsigned char)ch);
            if (lc.empty() || nm.find(lc) != std::string::npos) out.push_back(c);
        }
        return out;
    }

    static Model update(Model m, Msg msg, std::string value) {
        std::visit(overload{
            [&](Open)  { m.open = true; m.query.clear(); m.sel = 0; },
            [&](Close) { m.open = false; },
            [&](Query) { m.query = value; m.sel = 0; },
            [&](Move mv){ int n=(int)filtered(m.query).size(); if(n) m.sel=((m.sel+mv.d)%n+n)%n; },
            [&](Run)   { auto f=filtered(m.query); if(!f.empty()&&m.sel<(int)f.size()){ m.last=f[m.sel].id; m.open=false; } },
            [&](Pick p){ m.last = p.id; m.open = false; },
        }, msg);
        return m;
    }

    static const Command* by_id(const std::string& id) {
        for (auto& c : all()) if (c.id == id) return &c; return nullptr;
    }
    static Mod border(std::uint32_t c) { return detail::raw_css("border","1px solid "+detail::hexstr(c)); }

    static NodeRef result_row(const Command& c, bool active) {
        auto n = row(
            box(icon(c.ico, 16) | fg(c.hue)) | square(34) | center | round(8)
                | detail::raw_css("background", "rgba(255,255,255,.04)") | border(0x232c3d),
            col(text(c.name) | fg(ink) | font(15) | weight(Weight::semibold),
                text(c.group) | fg(faint) | font(12)) | gap(1),
            box() | grow(),
            active ? (row(text("Enter") | fg(body_c) | font(11) | weight(Weight::semibold)
                        | pad_x(7) | pad_y(3) | round(5) | detail::raw_css("background","rgba(255,255,255,.06)"))
                     ) : (box() | w(1))
        ) | gap(12) | items_center | pad_x(12) | pad_y(9) | round(10)
          | pointer | tap(Pick{ c.id }) | key(c.id) | animated()
          | transition("background-color .12s ease");
        if (active) n = n | detail::raw_css("background", "rgba(109,124,255,.14)");
        else n = n | on(Hover, detail::raw_css("background", "rgba(255,255,255,.04)"));
        return n;
    }

    static NodeRef palette(const Model& m) {
        auto results = filtered(m.query);
        std::vector<NodeRef> rows;
        for (int i = 0; i < (int)results.size(); ++i) rows.push_back(result_row(results[i], i == m.sel));

        auto search = row(
            icon("search", 18) | fg(faint),
            input(m.query) | placeholder("Type a command or search\u2026") | autofocus()
                | on_input([](std::string){ return Query{}; })
                | on_key("ArrowDown", Move{ +1 }) | on_key("ArrowUp", Move{ -1 })
                | on_enter(Run{}) | on_escape(Close{}) | grow()
                | detail::raw_css("background","transparent") | detail::raw_css("border","0")
                | detail::raw_css("outline","0") | fg(ink) | font(16),
            text("ESC") | fg(faint) | font(11) | weight(Weight::bold)
                | pad_x(7) | pad_y(3) | round(5) | detail::raw_css("background","rgba(255,255,255,.06)")
                | pointer | tap(Close{})
        ) | gap(12) | items_center | pad_x(16) | pad_y(15)
          | detail::raw_css("border-bottom","1px solid rgba(255,255,255,.07)");

        auto body = results.empty()
            ? (col(icon("search", 26) | fg(faint),
                   text("No results for \u201C" + m.query + "\u201D") | fg(body_c) | font(14))
                | gap(12) | center | pad(44))
            : (col_(std::move(rows)) | gap(2) | pad(8)
                | detail::raw_css("max-height","46vh") | detail::raw_css("overflow-y","auto"));

        auto foot = row(
            text(std::to_string(results.size()) + " commands") | fg(faint) | font(12),
            box() | grow(),
            row(text("\u2191\u2193 navigate") | fg(faint) | font(12),
                text("\u21B5 run") | fg(faint) | font(12)) | gap(16)
        ) | items_center | pad_x(16) | pad_y(11)
          | detail::raw_css("border-top","1px solid rgba(255,255,255,.07)");

        auto panel = col(search, body, foot)
            | w_full | max_w(560) | round(16) | detail::raw_css("overflow","hidden")
            | detail::raw_css("background","#12151f") | border(0x262f42)
            | detail::raw_css("box-shadow","0 24px 60px -12px rgba(0,0,0,.7)")
            | stop() | pop_in(200);

        return modal(m.open, panel) | tap(Close{});
    }

    static NodeRef view(const Model& m) {
        const Command* c = by_id(m.last);
        auto kbd = [](std::string k) {
            return text(std::move(k)) | fg(ink) | font(13) | weight(Weight::bold)
                 | pad_x(9) | pad_y(5) | round(7)
                 | detail::raw_css("background","rgba(255,255,255,.05)") | border(0x262f42);
        };

        auto trigger = row(
            icon("search", 17) | fg(body_c),
            text("Search or jump to\u2026") | fg(body_c) | font(15),
            box() | grow(),
            row(kbd("\u2318"), kbd("K")) | gap(4)
        ) | gap(12) | items_center | pad_x(16) | pad_y(13) | round(12) | w(360)
          | detail::raw_css("background","rgba(255,255,255,.03)") | border(0x232c3d)
          | pointer | on(Hover, detail::raw_css("border-color","#3a4560"))
          | tap(Open{}) | transition("border-color .15s ease");

        auto last_card = col(
            text("LAST ACTION") | fg(faint) | font(11) | weight(Weight::bold) | tracking_em(0.14f),
            row(box(icon(c ? c->ico : "star", 22) | fg(c ? c->hue : brand))
                    | square(52) | center | round(13)
                    | detail::raw_css("background","rgba(255,255,255,.04)") | border(0x262f42),
                col(text(c ? c->name : "Nothing yet") | fg(ink) | font(18) | weight(Weight::bold),
                    text(c ? c->group : "run a command") | fg(body_c) | font(14)) | gap(2)
            ) | gap(16) | items_center
        ) | gap(16) | pad(24) | round(16) | w(360)
          | detail::raw_css("background","#0f1420") | border(0x1c2434);

        auto hero = col(
            row(box() | circle(7) | detail::raw_css("background","#2ee6a6") | breathe(),
                text("COMMAND PALETTE") | fg(body_c) | font(12) | weight(Weight::bold) | tracking_em(0.22f))
                | gap(9) | items_center,
            text("Everything, one keystroke away")
                | fg(ink) | font_fluid(30, 52) | weight(Weight::black) | text_center
                | detail::raw_css("letter-spacing","-0.03em") | detail::raw_css("line-height","1.05"),
            text("Press \u2318K or \u2303K \u2014 or click below \u2014 to open a fuzzy launcher "
                 "with keyed, animated results.")
                | fg(body_c) | font(16) | leading(1.7f) | max_w(500) | text_center,
            trigger, last_card
        ) | gap(24) | items_center;

        return col(hero, palette(m))
             | gap(0) | items_center | justify_center | pad(32)
             | min_h(100_vh) | max_w(900) | center_x
             | on_shortcut("mod+k", Open{})
             | detail::raw_css("background",
                 "radial-gradient(900px 500px at 50% 0%, rgba(109,124,255,.14), transparent 60%), #0a0d16")
             | as_main;
    }

    static Meta meta(const Model&) {
        Meta mt; mt.title = "Lumen \u00b7 command palette";
        mt.description = "A Cmd-K command palette \u2014 global shortcut, live search, keyed animated results.";
        return mt;
    }
};

int main() { return live<Lumen>({ .port = 8080, .page_bg = 0x0a0d16, .title = "Lumen" }); }
