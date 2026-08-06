/// examples/chat.cpp — a real multi-client app: live chat over broadcast.
///
///   cmake --build build -j && ./build/chat     # open TWO tabs on :8080
///
/// Open it in two browser tabs and type — each message appears in BOTH,
/// instantly. That is the whole point of broadcast: independent sessions (each
/// with its own Model, its own render loop) share state through a topic, with
/// zero shared memory and not a line of locking in the app.
///
///   • Cmd::broadcast("room", line)  — publish a message to everyone
///   • Sub::on_topic("room", ...)    — every session joins the room and turns
///                                     each broadcast payload into a Msg
///   • keyed message list            — new lines append without re-rendering old
///
/// The model is per-session (each tab has its own draft + name); the CHAT LOG is
/// shared: a broadcast lands in every session's log via on_topic.

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>

#include <string>
#include <vector>

using namespace waya::surface;

struct Chat {
    struct Line { long id; std::string who, text; };
    struct Model {
        std::string name = "anon";
        std::string draft;
        std::vector<Line> log;    // shared history, filled by broadcasts
        long next_id = 1;
    };
    using Msg = int;
    enum { SetName, SetDraft, Send, Recv };

    static Model init() { return {}; }

    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg, std::string value) {
        switch (msg) {
            case SetName:  m.name  = value; break;
            case SetDraft: m.draft = value; break;
            case Send: {
                if (m.draft.empty()) break;
                // Broadcast "who\ntext" to the room; DON'T touch our own log —
                // the broadcast comes back to us via on_topic (Recv), so every
                // session, sender included, appends exactly once. Single source
                // of truth: the topic.
                std::string line = m.name + "\n" + m.draft;
                m.draft.clear();
                return { m, Cmd<Msg>::broadcast("room", line) };
            }
            case Recv: {
                // A broadcast arrived: value is "who\ntext".
                auto nl = value.find('\n');
                std::string who  = nl == std::string::npos ? "?" : value.substr(0, nl);
                std::string text = nl == std::string::npos ? value : value.substr(nl + 1);
                m.log.push_back({m.next_id++, std::move(who), std::move(text)});
                if (m.log.size() > 200) m.log.erase(m.log.begin());  // cap history
                break;
            }
        }
        return { m, Cmd<Msg>::none() };
    }

    // Every session joins the "room" topic; each broadcast payload becomes Recv.
    static Sub<Msg> subscribe(const Model&) {
        return Sub<Msg>::on_topic("room", [](std::string){ return Recv; });
    }

    static NodeRef view(const Model& m) {
        std::vector<NodeRef> rows;
        for (auto& l : m.log) {
            bool mine = (l.who == m.name);
            rows.push_back(
                row(
                    text(l.who) | fg(mine ? 0x818cf8 : 0x38bdf8) | font(13) | semibold
                               | css("min-width", "5rem"),
                    text(l.text) | fg(0xe2e8f0) | font(15)
                ) | key(std::to_string(l.id))
                  | gap(12) | pad_x(14) | pad_y(8) | round(10)
                  | bg(mine ? 0x1e293b : 0x0b1220) | border(1, 0x1f2937)
            );
        }
        auto logbox = box(); logbox->kids = std::move(rows);
        logbox->style.flow = Flow::col; finalize(*logbox);
        // The log fills the leftover height and scrolls internally — so the
        // composer row stays pinned to the bottom, on desktop AND on a phone.
        logbox = logbox | gap(8) | scroll_fill();

        auto field = [](std::string v, int msg, std::string ph, bool grow_) {
            auto n = input(std::move(v)) | placeholder(std::move(ph)) | on_input(msg)
                 | fg(0xe2e8f0) | bg(0x0b1220) | pad_x(14) | pad_y(10)
                 | round(10) | border(1, 0x1f2937) | font(15);
            return grow_ ? (n | grow(1)) : n;   // the message field fills the row
        };

        // The chat CARD — a readable column that fills the height it's given and
        // caps its width for legibility. No fixed pixel size: it adapts.
        auto card = col(
            row(
                text("waya chat") | fg(0xe2e8f0) | font(24) | weight(Weight::black),
                text("open a second tab → messages sync live") | fg(0x64748b) | font(13)
            ) | gap(14) | wrap | css("align-items", "baseline"),

            logbox,   // grows to fill the middle (flex:1)

            row(
                field(m.name,  SetName,  "name",  false) | css("width", "8rem"),
                field(m.draft, SetDraft, "message…", true),
                button("send") | fg(0xffffff) | bg(0x6366f1) | font(15) | semibold
                    | pad_x(20) | pad_y(10) | round(10) | tap(Send)
                    | transition() | on(Hover, opacity(0.85f))
            ) | gap(10) | wrap | css("align-items", "stretch")

        ) | grow(1) | gap(16) | pad(24) | round(20) | bg(0x111827)
          | shadow() | border(1, 0x1f2937)
          | css("min-height", "0");   // bound the card so its log scroller works

        // A fixed-viewport chat: the shell is bounded to the screen, the log
        // (scroll_fill) takes the middle and scrolls, the composer stays pinned.
        // Works identically on desktop and a phone — nothing scrolls off-screen.
        return app_shell(color::bg0, centered(42, card));
    }
};

int main() {
    static_assert(SurfaceProgram<Chat>);
    return live<Chat>({.port = 8080});
}
