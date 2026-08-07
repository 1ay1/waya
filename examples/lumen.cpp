/// examples/lumen.cpp — LUMEN: a command palette, the "wait, C++ did that?"
/// demo. Press Cmd/Ctrl-K anywhere to summon a Raycast-style launcher: type to
/// fuzzy-filter a keyed list of commands (results reorder with a FLIP glide),
/// arrow to move the selection, Enter to run, Esc to close. The chosen command
/// updates a little live "workspace" beneath it. Global shortcut, live input,
/// keyed animation, and a modal layer -- each one line.
///
///   waya run lumen            # then open the printed URL and hit Cmd/Ctrl-K

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>
#include <waya/ui.hpp>

#include <algorithm>
#include <string>
#include <variant>
#include <vector>

using namespace waya::surface;
using namespace waya::surface::color;
using namespace waya::surface::literals;
using namespace waya::ui;

struct Lumen {
    struct Command { std::string id, glyph, name, group; std::uint32_t hue; };

    static const std::vector<Command>& all() {
        static const std::vector<Command> cs = {
            { "new",     "\u2795", "New document",       "Create",  0x22d3ee },
            { "folder",  "▢", "New folder",         "Create",  0x22d3ee },
            { "invite",  "\u2709", "Invite teammate",    "Team",    0xa78bfa },
            { "theme",   "\u25D0", "Toggle dark mode",   "View",    0xa78bfa },
            { "search",  "\u2315", "Search everything",  "View",    0x60a5fa },
            { "deploy",  "\u2191", "Deploy to production","Ship",    0x34d399 },
            { "rollback","\u21BA", "Roll back release",  "Ship",    0xf59e0b },
            { "billing", "\u25B0", "Open billing",       "Account", 0xf472b6 },
            { "keys",    "\u2318", "Keyboard shortcuts", "Help",    0x94a3b8 },
            { "logout",  "\u23FB", "Sign out",           "Account", 0xf87171 },
        };
        return cs;
    }

    struct Model {
        bool        open = false;
        std::string query;
        int         sel = 0;
        std::string last = "deploy";     // last-run command id
    };

    struct Open {}; struct Close {}; struct Query {}; struct Move { int d; };
    struct Run {}; struct Pick { std::string id; };
    using Msg = std::variant<Open, Close, Query, Move, Run, Pick>;

    static Model init() { return {}; }

    // filtered command list for the current query (case-insensitive substring)
    static std::vector<Command> filtered(const std::string& q) {
        std::string lc; for (char c : q) lc += (char)std::tolower((unsigned char)c);
        std::vector<Command> out;
        for (auto& c : all()) {
            std::string name; for (char ch : c.name) name += (char)std::tolower((unsigned char)ch);
            if (lc.empty() || name.find(lc) != std::string::npos) out.push_back(c);
        }
        return out;
    }

    static Model update(Model m, Msg msg, std::string value) {
        std::visit(overload{
            [&](Open)  { m.open = true; m.query.clear(); m.sel = 0; },
            [&](Close) { m.open = false; },
            [&](Query) { m.query = value; m.sel = 0; },
            [&](Move mv) {
                int n = (int)filtered(m.query).size();
                if (n > 0) m.sel = ((m.sel + mv.d) % n + n) % n;
            },
            [&](Run) {
                auto f = filtered(m.query);
                if (!f.empty() && m.sel < (int)f.size()) { m.last = f[m.sel].id; m.open = false; }
            },
            [&](Pick p) { m.last = p.id; m.open = false; },
        }, msg);
        return m;
    }

    static const Command* by_id(const std::string& id) {
        for (auto& c : all()) if (c.id == id) return &c;
        return nullptr;
    }

    static NodeRef result_row(const Command& c, bool active) {
        auto n = row(
            box(text(c.glyph) | font(16)) | square(34) | center | round(9)
                | gradient(c.hue, 0x0b1020, 145) | ring(c.hue, active ? 1 : 0),
            col(text(c.name) | fg(ink) | font(15) | semibold,
                text(c.group) | fg(faint) | font(12)) | gap(1),
            box() | grow(),
            when(active, [] { return text("\u21B5") | fg(muted) | font(14); })
        ) | gap(12) | items_center | pad_x(12) | pad_y(10) | round(11)
          | pointer | tap(Pick{ c.id })
          | key(c.id) | animated();
        if (active) n = n | tint(0xffffff, 0.08f);
        return n | transition("background-color .12s ease");
    }

    static NodeRef palette(const Model& m) {
        auto results = filtered(m.query);
        std::vector<NodeRef> rows;
        for (int i = 0; i < (int)results.size(); ++i)
            rows.push_back(result_row(results[i], i == m.sel));

        auto search = row(
            text("\u2315") | font(18) | fg(muted),
            input(m.query)
                | placeholder("Type a command or search\u2026")
                | autofocus()
                | on_input([](std::string) { return Query{}; })
                | on_key("ArrowDown", Move{ +1 })
                | on_key("ArrowUp",   Move{ -1 })
                | on_enter(Run{})
                | on_escape(Close{})
                | grow()
                | detail::raw_css("background", "transparent")
                | detail::raw_css("border", "0")
                | detail::raw_css("outline", "0")
                | fg(ink) | font(17),
            text("esc") | fg(faint) | font(12) | semibold
                | pad_x(7) | pad_y(3) | round(6) | tint(0xffffff, 0.08f)
                | pointer | tap(Close{})
        ) | gap(12) | items_center | pad_x(18) | pad_y(16)
          | detail::raw_css("border-bottom", "1px solid rgba(255,255,255,.08)");

        auto body = results.empty()
            ? (col(text("No commands match \u201C" + m.query + "\u201D") | fg(muted) | font(14))
                | center | pad(40))
            : (col_(std::move(rows)) | gap(3) | pad(10)
                | detail::raw_css("max-height", "50vh")
                | detail::raw_css("overflow-y", "auto"));

        auto footer = row(
            text(std::to_string(results.size()) + " results") | fg(faint) | font(12),
            box() | grow(),
            row(text("\u2191\u2193") | fg(faint) | font(12), text("navigate") | fg(faint) | font(12))
                | gap(6),
            row(text("\u21B5") | fg(faint) | font(12), text("run") | fg(faint) | font(12)) | gap(6)
        ) | gap(16) | items_center | pad_x(18) | pad_y(12)
          | detail::raw_css("border-top", "1px solid rgba(255,255,255,.08)");

        auto panel = col(search, body, footer)
            | w_full | max_w(560) | round(18) | clip_content
            | detail::raw_css("background",
                "linear-gradient(180deg, rgba(24,26,40,.96), rgba(14,16,28,.96))")
            | hairline(0xffffff, 0.12f) | elevation(5) | stop() | pop_in(220);

        return modal(m.open, panel) | tap(Close{});
    }

    static NodeRef workspace(const Model& m) {
        const Command* c = by_id(m.last);
        auto kbd = [](std::string k) {
            return text(std::move(k)) | fg(ink) | font(13) | semibold
                 | pad_x(9) | pad_y(5) | round(7)
                 | frost(10) | hairline(0xffffff, 0.16f);
        };
        return col(
            text("Last action") | fg(faint) | font(12) | uppercase | tracking_em(0.14f),
            row(box(text(c ? c->glyph : "\u2726") | font(26))
                    | square(56) | center | round(15)
                    | gradient(c ? c->hue : 0x6366f1, 0x0b1020, 145)
                    | glow(c ? c->hue : 0x6366f1, 22),
                col(text(c ? c->name : "Nothing yet") | fg(ink) | font(22) | weight(Weight::black),
                    text(c ? c->group : "run a command") | fg(muted) | font(14)) | gap(2)
            ) | gap(16) | items_center,
            row(text("Press") | fg(muted) | font(14),
                kbd(cmd_key()), kbd("K"),
                text("to open the palette") | fg(muted) | font(14)) | gap(8) | items_center
        ) | gap(18) | pad(28) | round(20) | w_full | max_w(440)
          | frost(16) | hairline(0xffffff, 0.10f) | elevation(3);
    }

    static std::string cmd_key() { return "\u2318"; }  // shown as Cmd; Ctrl also works

    static NodeRef view(const Model& m) {
        auto hero = col(
            row(box() | circle(9) | bg(0x34d399) | breathe(),
                text("LUMEN") | fg(muted) | font(13) | semibold | tracking_em(0.28f))
                | gap(10) | items_center,
            text("The command palette") | font_fluid(30, 56) | weight(Weight::black)
                | aurora_text(0x22d3ee, 0xa78bfa, 0xf472b6) | text_center | leading(1.05f),
            text("Everything a keystroke away. Press \u2318K or \u2303K \u2014 or click below \u2014 "
                 "to summon a fuzzy launcher with keyed, animated results.")
                | fg(muted) | font(16) | leading(1.7f) | max_w(520) | text_center,
            row(text("\u2315") | font(16) | fg(ink),
                text("Open command palette") | fg(ink) | semibold,
                text(cmd_key() + "K") | fg(muted) | font(13) | semibold
                    | pad_x(8) | pad_y(3) | round(7) | tint(0xffffff, 0.10f))
                | gap(12) | items_center | pad_x(20) | pad_y(14) | round(13)
                | frost(14) | hairline(0xffffff, 0.16f)
                | interactive() | hover_glow(0x8b5cf6, 30) | tap(Open{})
        ) | gap(22) | items_center;

        return col(hero, workspace(m), palette(m))
             | gap(36) | items_center | justify_center | pad(32)
             | min_h(100_vh) | max_w(900) | center_x
             // the GLOBAL shortcut lives on the always-mounted root
             | on_shortcut("mod+k", Open{})
             | aurora(0x1a1040, 0x0b1020, 0x0a1f2e, 28)
             | as_main;
    }

    static Meta meta(const Model&) {
        Meta mt; mt.title = "waya \u00b7 Lumen";
        mt.description = "A Cmd-K command palette \u2014 global shortcut, live search, keyed animated results.";
        return mt;
    }
};

int main() {
    return live<Lumen>({ .port = 8080, .title = "waya \u00b7 lumen" });
}
