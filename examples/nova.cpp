/// examples/nova.cpp — NOVA: a real issue tracker, in pure C++. No HTML, no CSS,
/// no JavaScript, no client bundle. Yet it does everything a modern web app
/// does, and does it well:
///
///   • DRAG a card between columns — it glides (keyed move-diffing), and the
///     drop lands it exactly where you released it.
///   • LIVE MULTIPLAYER — open two tabs: create, move, edit, or delete in one
///     and it appears in the other instantly (pub/sub broadcast).
///   • COMMAND PALETTE — press ⌘K / Ctrl-K anywhere: fuzzy-filter actions,
///     arrow-key through them, Enter to run.
///   • INLINE CREATE — type in a column's composer and press Enter to add a card.
///   • DETAIL DRAWER — click a card: a slide-in panel to edit its title, change
///     priority/assignee, or delete it.
///   • LIVE SEARCH + priority filter across the whole board.
///   • UNDO the last change (⌘Z). Keyboard-first, optimistic, polished.
///
/// The whole app is a pure `update` over a plain `Model`. Everything you see is
/// a function of that model; every interaction is a typed message. That is the
/// entire program — the framework streams the minimal delta to the browser.
///
///   waya run nova            # then open the printed URL in TWO tabs

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>
#include <waya/ui.hpp>

#include <algorithm>
#include <string>
#include <variant>
#include <vector>

using namespace waya::surface;
using namespace waya::surface::literals;
using namespace waya::ui;

struct Nova {
    // ── palette ──────────────────────────────────────────────────────────────
    static constexpr std::uint32_t bg      = 0x0a0c14;
    static constexpr std::uint32_t panel   = 0x0f131d;
    static constexpr std::uint32_t card_c  = 0x151b28;
    static constexpr std::uint32_t card_hi = 0x1a2130;
    static constexpr std::uint32_t line    = 0x232c3d;
    static constexpr std::uint32_t ink     = 0xeef2f9;
    static constexpr std::uint32_t body_c  = 0x93a1ba;
    static constexpr std::uint32_t faint   = 0x5b6a82;
    static constexpr std::uint32_t brand   = 0x7c8cff;
    static constexpr std::uint32_t brand2  = 0x22d3ee;

    // priority → (label, colour)
    static std::uint32_t prio_color(int p){ return p>=3 ? 0xff6b6b : p==2 ? 0xffb454 : p==1 ? 0x6d9cff : 0x5b6a82; }
    static const char*   prio_label(int p){ return p>=3 ? "Urgent" : p==2 ? "High" : p==1 ? "Medium" : "Low"; }

    // ── data ─────────────────────────────────────────────────────────────────
    enum Col { Backlog = 0, Todo = 1, Doing = 2, Done = 3 };
    static const char* col_name(int c){ return c==Backlog?"Backlog":c==Todo?"Todo":c==Doing?"In Progress":"Done"; }
    static constexpr int NCOL = 4;

    struct Card { int id; std::string title; int col; int prio; std::string who; };

    struct Model {
        std::vector<Card> cards = seed();
        int next_id = 100;
        // ui state
        std::string query;                 // live search
        int prio_filter = -1;              // -1 = all
        int open_card = -1;                // detail drawer target (-1 = closed)
        bool palette = false;             // command palette open
        std::string palette_q;             // palette filter
        int palette_sel = 0;               // highlighted palette row
        std::vector<std::string> drafts = std::vector<std::string>(NCOL);  // per-column composer text
        std::vector<Card> undo_stack;      // last-deleted cards (simple undo)
        std::string flash;                 // transient status (a11y live region)
        std::string me = pick_name();      // this session's identity
    };

    static std::vector<Card> seed(){
        return {
            { 1, "Design the onboarding flow",          Todo,    2, "Ada" },
            { 2, "Ship the C++ diff streamer",          Doing,   3, "Lin" },
            { 3, "Write the wire-protocol docs",        Backlog, 1, "Ada" },
            { 4, "Fix the drag-and-drop drop target",   Done,    2, "You" },
            { 5, "Add live multiplayer presence",       Doing,   2, "Kai" },
            { 6, "Audit the JSON parser",               Done,    1, "Lin" },
            { 7, "Command palette fuzzy match",         Backlog, 1, "Kai" },
            { 8, "Polish the empty states",             Todo,    0, "Ada" },
        };
    }
    static std::string pick_name(){
        static const char* names[] = { "You", "Sky", "Rey", "Max", "Ivy", "Jo" };
        static int i = 0; return names[(i++) % 6];
    }

    // ── messages ─────────────────────────────────────────────────────────────
    struct Search   { std::string v; };
    struct FilterP  { int p; };
    struct Drop     { std::string payload; };     // "cardId:colId"
    struct Draft    { int col; std::string v; };
    struct Add      { int col; };
    struct Open     { int id; };
    struct Close    {};
    struct EditTitle{ std::string v; };
    struct SetPrio  { int p; };
    struct Del      { int id; };
    struct Undo     {};
    struct PalOpen  {}; struct PalClose {}; struct PalQ { std::string v; };
    struct PalMove  { int d; }; struct PalRun {};
    struct Recv     { std::string payload; };     // broadcast in
    using Msg = std::variant<Search, FilterP, Drop, Draft, Add, Open, Close,
                             EditTitle, SetPrio, Del, Undo,
                             PalOpen, PalClose, PalQ, PalMove, PalRun, Recv>;

    static Model init(){ return {}; }

    // ── helpers ──────────────────────────────────────────────────────────────
    static Card* find(Model& m, int id){ for(auto& c : m.cards) if(c.id==id) return &c; return nullptr; }
    static const Card* find(const Model& m, int id){ for(auto& c : m.cards) if(c.id==id) return &c; return nullptr; }
    static std::string lower(std::string s){ for(char& c:s) c=(char)std::tolower((unsigned char)c); return s; }
    static bool icontains(const std::string& hay, const std::string& n){
        if(n.empty()) return true;
        return lower(hay).find(lower(n))!=std::string::npos; }

    // serialize the whole board for a broadcast (simple + robust: full-state sync)
    static std::string encode(const Model& m){
        std::string s = std::to_string(m.next_id);
        for(auto& c : m.cards){
            s += "\x1e" + std::to_string(c.id) + "\x1f" + std::to_string(c.col) + "\x1f"
               + std::to_string(c.prio) + "\x1f" + c.who + "\x1f" + c.title;
        }
        return s;
    }
    static void decode_into(Model& m, const std::string& s){
        std::vector<std::string> recs;
        std::size_t p=0; while(true){ auto n=s.find('\x1e',p); recs.push_back(s.substr(p, n==std::string::npos?n:n-p)); if(n==std::string::npos) break; p=n+1; }
        if(recs.empty()) return;
        m.next_id = std::atoi(recs[0].c_str());
        std::vector<Card> nc;
        for(std::size_t i=1;i<recs.size();++i){
            auto& r = recs[i]; std::vector<std::string> f; std::size_t q=0;
            while(true){ auto n=r.find('\x1f',q); f.push_back(r.substr(q, n==std::string::npos?n:n-q)); if(n==std::string::npos) break; q=n+1; }
            if(f.size()>=5) nc.push_back({ std::atoi(f[0].c_str()), f[4], std::atoi(f[1].c_str()), std::atoi(f[2].c_str()), f[3] });
        }
        m.cards = std::move(nc);
    }
    static std::pair<Model,Cmd<Msg>> sync(Model m, std::string flash={}){
        m.flash = std::move(flash);
        return { m, Cmd<Msg>::broadcast("nova", encode(m)) };
    }

    // ── palette actions ──────────────────────────────────────────────────────
    struct Action { std::string label; std::string hint; Msg (*make)(); };
    static std::vector<Action> actions(){
        return {
            { "New card in Backlog", "Backlog", []{ return Msg{Add{Backlog}}; } },
            { "New card in Todo",    "Todo",    []{ return Msg{Add{Todo}}; } },
            { "New card in Progress","Doing",   []{ return Msg{Add{Doing}}; } },
            { "Clear all filters",   "reset",   []{ return Msg{FilterP{-1}}; } },
            { "Show only Urgent",    "P: urgent",[]{ return Msg{FilterP{3}}; } },
            { "Show only High",      "P: high", []{ return Msg{FilterP{2}}; } },
            { "Undo last change",    "\u2318Z",  []{ return Msg{Undo{}}; } },
            { "Close",               "esc",     []{ return Msg{PalClose{}}; } },
        };
    }
    static std::vector<Action> pal_filtered(const Model& m){
        std::vector<Action> out;
        for(auto& a : actions()) if(icontains(a.label + " " + a.hint, m.palette_q)) out.push_back(a);
        return out;
    }

    // ── update ───────────────────────────────────────────────────────────────
    static std::pair<Model,Cmd<Msg>> update(Model m, Msg msg){
        return std::visit(overload{
            [&](const Search& s) -> std::pair<Model,Cmd<Msg>> { m.query = s.v; return { m, Cmd<Msg>::none() }; },
            [&](const FilterP& f) -> std::pair<Model,Cmd<Msg>> { m.prio_filter = f.p; m.palette=false; return { m, Cmd<Msg>::none() }; },

            // DROP: payload is "<cardId>:<colId>". Move the card, keep its order
            // stable within the target column (append), and broadcast.
            [&](const Drop& d) -> std::pair<Model,Cmd<Msg>> {
                auto colon = d.payload.rfind(':');
                if(colon==std::string::npos) return { m, Cmd<Msg>::none() };
                int id  = std::atoi(d.payload.substr(0,colon).c_str());
                int col = std::atoi(d.payload.substr(colon+1).c_str());
                if(auto* c = find(m,id)){ if(c->col==col) return { m, Cmd<Msg>::none() }; c->col = col; }
                return sync(std::move(m));
            },

            [&](const Draft& d) -> std::pair<Model,Cmd<Msg>> {
                if(d.col>=0 && d.col<NCOL) m.drafts[d.col] = d.v;
                return { m, Cmd<Msg>::none() }; },
            [&](const Add& a) -> std::pair<Model,Cmd<Msg>> {
                std::string t = a.col<NCOL ? trim(m.drafts[a.col]) : "";
                if(t.empty()) t = "New issue";
                m.cards.push_back({ m.next_id++, t, a.col, 1, m.me });
                if(a.col<NCOL) m.drafts[a.col].clear();
                m.palette = false;
                return sync(std::move(m), "Card added");
            },

            [&](const Open& o) -> std::pair<Model,Cmd<Msg>> { m.open_card = o.id; return { m, Cmd<Msg>::none() }; },
            [&](Close) -> std::pair<Model,Cmd<Msg>> { m.open_card = -1; return { m, Cmd<Msg>::none() }; },
            [&](const EditTitle& e) -> std::pair<Model,Cmd<Msg>> {
                if(auto* c = find(m, m.open_card)) c->title = e.v;
                return sync(std::move(m)); },
            [&](const SetPrio& s) -> std::pair<Model,Cmd<Msg>> {
                if(auto* c = find(m, m.open_card)) c->prio = s.p;
                return sync(std::move(m)); },
            [&](const Del& d) -> std::pair<Model,Cmd<Msg>> {
                if(auto* c = find(m,d.id)) m.undo_stack.push_back(*c);
                std::erase_if(m.cards, [&](const Card& c){ return c.id==d.id; });
                if(m.open_card==d.id) m.open_card = -1;
                return sync(std::move(m), "Card deleted \u00b7 \u2318Z to undo");
            },
            [&](Undo) -> std::pair<Model,Cmd<Msg>> {
                if(m.undo_stack.empty()) return { m, Cmd<Msg>::none() };
                m.cards.push_back(m.undo_stack.back()); m.undo_stack.pop_back();
                m.palette = false;
                return sync(std::move(m), "Undone");
            },

            // command palette
            [&](PalOpen) -> std::pair<Model,Cmd<Msg>> { m.palette=true; m.palette_q.clear(); m.palette_sel=0; return { m, Cmd<Msg>::none() }; },
            [&](PalClose) -> std::pair<Model,Cmd<Msg>> { m.palette=false; return { m, Cmd<Msg>::none() }; },
            [&](const PalQ& q) -> std::pair<Model,Cmd<Msg>> { m.palette_q=q.v; m.palette_sel=0; return { m, Cmd<Msg>::none() }; },
            [&](const PalMove& mv) -> std::pair<Model,Cmd<Msg>> {
                int n = (int)pal_filtered(m).size(); if(n==0) return { m, Cmd<Msg>::none() };
                m.palette_sel = ((m.palette_sel + mv.d) % n + n) % n; return { m, Cmd<Msg>::none() }; },
            [&](PalRun) -> std::pair<Model,Cmd<Msg>> {
                auto f = pal_filtered(m); if(m.palette_sel<0 || m.palette_sel>=(int)f.size()) return { m, Cmd<Msg>::none() };
                Msg next = f[m.palette_sel].make(); m.palette=false;
                return update(std::move(m), next); },

            // broadcast in: adopt the sender's board state (full-state sync)
            [&](const Recv& r) -> std::pair<Model,Cmd<Msg>> { decode_into(m, r.payload); return { m, Cmd<Msg>::none() }; },
        }, msg);
    }

    static Sub<Msg> subscribe(const Model&){
        return Sub<Msg>::on_topic("nova", [](std::string p){ return Recv{ p }; });
    }

    static std::string trim(const std::string& s){
        auto a=s.find_first_not_of(" \t\r\n"); if(a==std::string::npos) return "";
        auto b=s.find_last_not_of(" \t\r\n"); return s.substr(a,b-a+1); }

    // ── view: a card ─────────────────────────────────────────────────────────
    static NodeRef card_view(const Card& c){
        std::uint32_t pc = prio_color(c.prio);
        auto chip = row(box() | w(6) | h(6) | round(999) | detail::raw_css("background", detail::hexstr(pc)),
                        text(prio_label(c.prio)) | fg(pc) | detail::raw_css("font-size","11px") | weight(Weight::semibold))
                    | gap(6) | center;
        auto avatar = box(text(c.who.substr(0,1)) | fg(0x0a0c14) | detail::raw_css("font-size","11px") | weight(Weight::bold))
                    | w(20) | h(20) | round(999) | detail::raw_css("background", detail::hexstr(0x9aa7ff))
                    | detail::raw_css("display","grid") | detail::raw_css("place-items","center");
        return col(
                   row(chip, box() | grow(), text("#" + std::to_string(c.id)) | fg(faint) | detail::raw_css("font-size","11px")) | center,
                   text(c.title) | fg(ink) | detail::raw_css("font-size","14px") | leading(1.4f)
                       | detail::raw_css("font-weight","500"),
                   row(avatar, box() | grow(), text(c.who) | fg(body_c) | detail::raw_css("font-size","12px")) | center
               ) | gap(10) | pad(14) | round(12)
                 | detail::raw_css("background", detail::hexstr(card_c))
                 | detail::raw_css("border","1px solid " + detail::hexstr(line))
                 | detail::raw_css("border-left","3px solid " + detail::hexstr(pc))
                 | detail::raw_css("cursor","grab")
                 | on(State::Hover, detail::raw_css("background", detail::hexstr(card_hi))
                                  | detail::raw_css("transform","translateY(-1px)"))
                 | transition("transform .12s ease, background .12s ease")
                 | draggable(std::to_string(c.id))
                 | key("card:" + std::to_string(c.id))
                 | tap(Open{ c.id });
    }

    // ── view: a column ───────────────────────────────────────────────────────
    static NodeRef column_view(const Model& m, int ci){
        std::vector<NodeRef> cards;
        int total = 0;
        for(auto& c : m.cards){
            if(c.col!=ci) continue;
            ++total;
            if(!icontains(c.title + " " + c.who, m.query)) continue;
            if(m.prio_filter>=0 && c.prio!=m.prio_filter) continue;
            cards.push_back(card_view(c));
        }
        auto stack = box(); stack->kids = std::move(cards); stack->style.flow = Flow::col;
        stack->style.gap = 10_px; finalize(*stack);
        bool cards_empty = stack->kids.empty();
        // The card list fills the lane and scrolls internally (vscroll), so a tall
        // board shows full-height columns and long lists never push the composer
        // away — the framework's vscroll() layout helper does exactly this.
        stack = stack | vscroll() | detail::raw_css("padding-right","2px");
        // Empty column: a soft hint so the lane never looks broken.
        if(cards_empty){
            stack = box(
                col(text("\u2014") | fg(faint) | detail::raw_css("font-size","20px"),
                    text("Drop or add a card") | fg(faint) | detail::raw_css("font-size","12px"))
                | gap(6) | center | detail::raw_css("align-items","center"))
                | detail::raw_css("flex","1 1 auto") | detail::raw_css("min-height","0")
                | detail::raw_css("display","grid") | detail::raw_css("place-items","center")
                | detail::raw_css("border","1px dashed rgba(255,255,255,.05)") | round(12);
        }

        auto header = row(
            text(col_name(ci)) | fg(ink) | detail::raw_css("font-size","13px") | weight(Weight::bold)
                | detail::raw_css("letter-spacing",".03em") | detail::raw_css("text-transform","uppercase"),
            box(text(std::to_string(total)) | fg(body_c) | detail::raw_css("font-size","11px") | weight(Weight::semibold))
                | pad_x(7) | pad_y(1) | round(999) | detail::raw_css("background", detail::hexstr(line)),
            box() | grow()
        ) | gap(8) | center | pad_x(4) | pad_y(2)
          | detail::raw_css("flex","0 0 auto");

        // composer: type a title, Enter adds a card to THIS column
        auto composer = input(m.drafts[ci])
            | placeholder("+ Add a card\u2026")
            | on_input([ci](std::string v){ return Draft{ ci, v }; })
            | on_enter(Add{ ci })
            | detail::raw_css("font-size","13px") | fg(ink)
            | pad_x(12) | pad_y(9) | round(9) | w(pct_(100))
            | detail::raw_css("flex","0 0 auto")
            | detail::raw_css("background","rgba(255,255,255,.03)")
            | detail::raw_css("border","1px dashed " + detail::hexstr(line))
            | detail::raw_css("outline","none");

        // the whole column is a full-height lane + a drop target tagged with its id
        return col(header, stack, composer) | gap(12) | pad(12) | round(16) | flex_col
             | detail::raw_css("background", detail::hexstr(panel))
             | detail::raw_css("border","1px solid " + detail::hexstr(line))
             | detail::raw_css("height","100%")
             | drop_target(std::to_string(ci), [](std::string s){ return Drop{ s }; });
    }

    // ── view: detail drawer ──────────────────────────────────────────────────
    static NodeRef drawer(const Model& m){
        const Card* c = find(m, m.open_card);
        if(!c) return box();   // closed → nothing
        auto prio_pill = [&](int p){
            bool on = c->prio==p; std::uint32_t pc = prio_color(p);
            return text(prio_label(p)) | fg(on?0x0a0c14:pc) | detail::raw_css("font-size","12px") | weight(Weight::semibold)
                 | pad_x(12) | pad_y(6) | round(999) | pointer
                 | detail::raw_css("background", on ? detail::hexstr(pc) : "rgba(255,255,255,.04)")
                 | detail::raw_css("border","1px solid " + detail::hexstr(pc))
                 | tap(SetPrio{ p });
        };
        auto panel_ = col(
            row(text("Issue #" + std::to_string(c->id)) | fg(faint) | detail::raw_css("font-size","12px") | mono,
                box() | grow(),
                text("\u2715") | fg(body_c) | detail::raw_css("font-size","16px") | pointer | tap(Close{})) | center,
            input(c->title)
                | on_input([](std::string v){ return EditTitle{ v }; })
                | detail::raw_css("font-size","20px") | fg(ink) | weight(Weight::bold)
                | detail::raw_css("background","transparent") | detail::raw_css("border","none")
                | detail::raw_css("outline","none") | pad_y(6) | w(pct_(100)),
            text("PRIORITY") | fg(faint) | detail::raw_css("font-size","11px") | weight(Weight::bold) | detail::raw_css("letter-spacing",".1em"),
            row(prio_pill(0), prio_pill(1), prio_pill(2), prio_pill(3)) | gap(8) | wrap,
            box() | h(1) | detail::raw_css("background", detail::hexstr(line)) | detail::raw_css("margin","6px 0"),
            row(text("Column") | fg(body_c) | detail::raw_css("font-size","13px"), box()|grow(),
                text(col_name(c->col)) | fg(ink) | detail::raw_css("font-size","13px") | weight(Weight::semibold)) | center,
            row(text("Assignee") | fg(body_c) | detail::raw_css("font-size","13px"), box()|grow(),
                text(c->who) | fg(ink) | detail::raw_css("font-size","13px") | weight(Weight::semibold)) | center,
            box() | grow(),
            text("Delete issue") | fg(0xff6b6b) | detail::raw_css("font-size","13px") | weight(Weight::semibold)
                | pad_x(14) | pad_y(10) | round(9) | pointer
                | detail::raw_css("background","rgba(255,107,107,.10)")
                | detail::raw_css("border","1px solid rgba(255,107,107,.25)")
                | detail::raw_css("text-align","center")
                | tap(Del{ c->id })
        ) | gap(14) | pad(24) | h(pct_(100)) | w(px(380)) | detail::raw_css("max-width","100vw") | stop()
          | detail::raw_css("background", detail::hexstr(0x0d111a))
          | detail::raw_css("border-left","1px solid " + detail::hexstr(line))
          | slide_in(200);

        // backdrop: click outside closes
        return box(row(box()|grow(), panel_))
             | detail::raw_css("position","fixed") | detail::raw_css("inset","0")
             | detail::raw_css("background","rgba(4,6,12,.55)")
             | detail::raw_css("z-index","60") | detail::raw_css("backdrop-filter","blur(2px)")
             | tap(Close{}) | fade_in(140);
    }

    // ── view: command palette ────────────────────────────────────────────────
    static NodeRef palette(const Model& m){
        if(!m.palette) return box();
        auto rows_ = pal_filtered(m);
        std::vector<NodeRef> items;
        for(std::size_t i=0;i<rows_.size();++i){
            bool sel = (int)i==m.palette_sel;
            items.push_back(
                row(text(rows_[i].label) | fg(sel?ink:body_c) | detail::raw_css("font-size","14px")
                        | (sel?weight(Weight::semibold):weight(Weight::normal)),
                    box()|grow(),
                    text(rows_[i].hint) | fg(faint) | detail::raw_css("font-size","12px") | mono)
                | center | pad_x(14) | pad_y(11) | round(9)
                | detail::raw_css("background", sel ? "rgba(124,140,255,.14)" : "transparent")
                | key("pal:" + std::to_string(i)));
        }
        auto list = box(); list->kids = std::move(items); list->style.flow=Flow::col; list->style.gap=2_px; finalize(*list);

        auto box_ = col(
            input(m.palette_q)
                | placeholder("Type a command\u2026")
                | autofocus()
                | on_input([](std::string v){ return PalQ{ v }; })
                | on_enter(PalRun{})
                | on_key("ArrowDown", PalMove{ +1 }) | on_key("ArrowUp", PalMove{ -1 })
                | on_key("Escape", PalClose{})
                | detail::raw_css("font-size","16px") | fg(ink)
                | pad(16) | w(pct_(100))
                | detail::raw_css("background","transparent")
                | detail::raw_css("border","none") | detail::raw_css("outline","none")
                | detail::raw_css("border-bottom","1px solid " + detail::hexstr(line)),
            (rows_.empty()
                ? (text("No matching commands") | fg(faint) | detail::raw_css("font-size","13px") | pad(20) | detail::raw_css("text-align","center"))
                : (list | pad(6)))
        ) | w(px(520)) | detail::raw_css("max-width","92vw") | round(16) | stop()
          | detail::raw_css("background", detail::hexstr(0x121724))
          | detail::raw_css("border","1px solid " + detail::hexstr(0x2c374d))
          | detail::raw_css("box-shadow","0 24px 80px rgba(0,0,0,.6)")
          | pop_in(160);

        return box(col(box()|h(px(96)), row(box()|grow(), box_, box()|grow())))
             | detail::raw_css("position","fixed") | detail::raw_css("inset","0")
             | detail::raw_css("background","rgba(4,6,12,.5)") | detail::raw_css("z-index","80")
             | tap(PalClose{}) | fade_in(120);
    }

    // ── view: top bar ────────────────────────────────────────────────────────
    static NodeRef topbar(const Model& m){
        auto search = input(m.query)
            | placeholder("Search issues\u2026")
            | on_input([](std::string v){ return Search{ v }; })
            | detail::raw_css("font-size","13px") | fg(ink)
            | pad_x(14) | pad_y(9) | round(10) | w(px(260)) | detail::raw_css("max-width","40vw")
            | detail::raw_css("background","rgba(255,255,255,.04)")
            | detail::raw_css("border","1px solid " + detail::hexstr(line))
            | detail::raw_css("outline","none");

        auto pfilter = [&](int p, const char* label){
            bool on = m.prio_filter==p;
            std::uint32_t pc = p<0 ? brand : prio_color(p);
            return text(label) | fg(on?0x0a0c14:body_c) | detail::raw_css("font-size","12px") | weight(Weight::semibold)
                 | pad_x(11) | pad_y(6) | round(999) | pointer
                 | detail::raw_css("background", on ? detail::hexstr(pc) : "rgba(255,255,255,.04)")
                 | detail::raw_css("border","1px solid " + detail::hexstr(on?pc:line))
                 | tap(FilterP{ p });
        };

        auto palette_btn = row(text("\u2318K") | fg(body_c) | mono | detail::raw_css("font-size","12px"),
                               text("Commands") | fg(body_c) | detail::raw_css("font-size","13px"))
            | gap(8) | center | pad_x(12) | pad_y(8) | round(10) | pointer
            | detail::raw_css("background","rgba(255,255,255,.04)")
            | detail::raw_css("border","1px solid " + detail::hexstr(line))
            | tap(PalOpen{});

        return row(
            row(box() | w(10) | h(10) | round(3) | detail::raw_css("background","linear-gradient(135deg,#7c8cff,#22d3ee)"),
                text("Nova") | fg(ink) | detail::raw_css("font-size","17px") | weight(Weight::black)
                    | detail::raw_css("letter-spacing","-.01em"),
                text("issue tracker \u00b7 pure C++") | fg(faint) | detail::raw_css("font-size","12px")) | gap(10) | center,
            box() | grow(),
            search,
            row(pfilter(-1,"All"), pfilter(3,"Urgent"), pfilter(2,"High")) | gap(6) | center,
            palette_btn,
            // presence: who's here (this session's identity)
            box(text(m.me.substr(0,1)) | fg(0x0a0c14) | detail::raw_css("font-size","12px") | weight(Weight::bold))
                | w(28) | h(28) | round(999) | detail::raw_css("background","linear-gradient(135deg,#7c8cff,#22d3ee)")
                | detail::raw_css("display","grid") | detail::raw_css("place-items","center")
                | title("You are " + m.me)
        ) | gap(14) | center | wrap | pad_x(22) | pad_y(14)
          | detail::raw_css("border-bottom","1px solid " + detail::hexstr(line))
          | detail::raw_css("background","rgba(10,12,20,.7)")
          | detail::raw_css("backdrop-filter","blur(12px)")
          | detail::raw_css("position","sticky") | detail::raw_css("top","0") | detail::raw_css("z-index","40")
          | as_header;
    }

    // ── view: the board toolbar (context row under the top bar) ────────────
    static NodeRef board_bar(const Model& m){
        int total = (int)m.cards.size();
        int done = 0; for(auto& c : m.cards) if(c.col==Done) ++done;
        return row(
            text("Sprint board") | fg(ink) | detail::raw_css("font-size","15px") | weight(Weight::bold),
            box(text(std::to_string(total) + " issues") | fg(body_c) | detail::raw_css("font-size","12px"))
                | pad_x(9) | pad_y(3) | round(999) | detail::raw_css("background","rgba(255,255,255,.04)"),
            box(text(std::to_string(done) + " done") | fg(0x34d399) | detail::raw_css("font-size","12px") | weight(Weight::semibold))
                | pad_x(9) | pad_y(3) | round(999) | detail::raw_css("background","rgba(52,211,153,.10)"),
            box() | grow(),
            text("+ New issue") | fg(0x0a0c14) | detail::raw_css("font-size","13px") | weight(Weight::semibold)
                | pad_x(14) | pad_y(8) | round(9) | pointer
                | detail::raw_css("background","linear-gradient(135deg,#7c8cff,#22d3ee)")
                | tap(Add{ Backlog })
        ) | gap(10) | center | wrap | pad_x(24) | pad_y(14) | detail::raw_css("flex","0 0 auto");
    }

    // ── view ─────────────────────────────────────────────────────────────────
    static NodeRef view(const Model& m){
        // A responsive board that FILLS the remaining viewport height: the root
        // is a full-height flex column, the board grows into the leftover space,
        // and each column is a full-height lane whose card list scrolls inside.
        // Columns scroll horizontally inside the board on narrow screens.
        std::vector<NodeRef> cols;
        for(int c=0;c<NCOL;++c) cols.push_back(column_view(m, c));
        auto board = board_(px(300), std::move(cols))
            | pad_x(24) | detail::raw_css("padding-bottom","24px")
            | grows | detail::raw_css("min-height","0")
            | detail::raw_css("align-items","stretch");   // stretch lanes to full height

        // global keyboard: ⌘K opens palette, ⌘Z undoes, from anywhere
        auto keys = box()
            | on_shortcut("mod+k", PalOpen{})
            | on_shortcut("mod+z", Undo{});

        auto flashbar = m.flash.empty() ? box()
            : (text(m.flash) | fg(0x0a0c14) | detail::raw_css("font-size","13px") | weight(Weight::semibold)
                 | pad_x(16) | pad_y(9) | round(999)
                 | detail::raw_css("background","linear-gradient(135deg,#7c8cff,#22d3ee)")
                 | detail::raw_css("position","fixed") | detail::raw_css("bottom","20px") | detail::raw_css("left","50%")
                 | detail::raw_css("transform","translateX(-50%)") | detail::raw_css("z-index","50")
                 | detail::raw_css("box-shadow","0 10px 30px rgba(0,0,0,.5)")
                 | live_region(false) | slide_in(200));

        return viewport(
            topbar(m),
            board_bar(m),
            board,
            keys,
            drawer(m),
            palette(m),
            flashbar
        ) | detail::raw_css("max-width","100%")
          | detail::raw_css("background",
                "radial-gradient(1200px 560px at 12% -8%, rgba(124,140,255,.10), transparent 60%),"
                "radial-gradient(900px 500px at 92% 4%, rgba(34,211,238,.06), transparent 55%), " + std::string(detail::hexstr(bg)))
          | as_main;
    }

    static Meta meta(const Model&){
        Meta mt;
        mt.title = "Nova \u00b7 a real issue tracker in pure C++";
        mt.description = "Drag-and-drop kanban with live multiplayer, a command palette, "
                         "inline editing, search, and undo \u2014 no HTML, CSS, or JavaScript. "
                         "Built entirely with waya.";
        return mt;
    }
};

int main(){ return live<Nova>({ .port = 8080, .page_bg = 0x0a0c14, .title = "Nova" }); }
