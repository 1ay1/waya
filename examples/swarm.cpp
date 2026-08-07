/// examples/swarm.cpp — SWARM: a real-time multiplayer star-pop game. Stars
/// drift across a glowing field on a physics tick; tap one before it escapes
/// and it bursts into a keyed, FLIP-animated shard cloud. Every pop broadcasts
/// your score to a shared, live leaderboard — open two tabs (or send a friend
/// the link) and watch both boards update the instant either of you scores.
/// Combo streak, decaying "heat" glow, and a spawn cadence that quickens as
/// your combo grows. Built entirely from ticks + broadcast + keyed FLIP +
/// raw SVG paths — no client JS beyond what waya ships.
///
///   cmake --build build --target swarm && ./build/swarm   # open TWO tabs
///                                                          # on :8080

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>
#include <waya/ui.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

using namespace waya::surface;
using namespace waya::surface::color;
using namespace waya::ui;

struct Swarm {
    // ── world ────────────────────────────────────────────────────────────
    struct Star {
        int id; double x, y, vx, vy, born; std::uint32_t hue;
    };
    struct Shard {
        int id; double x, y, dx, dy; std::uint32_t hue; int life;
    };
    struct Board { std::string name; int score; };

    struct Model {
        std::vector<Star> stars;
        std::vector<Shard> shards;      // burst debris, culled after a few ticks
        int next_id = 1;
        double t = 0;
        int score = 0;
        int combo = 0;
        int best_combo = 0;
        int missed = 0;
        std::vector<Board> board;       // shared leaderboard (via broadcast)
        std::string me = "you";
    };

    struct Tick {};
    struct Pop { int id; };
    struct Spawn {};
    struct Recv { std::string payload; };   // "name|score"
    using Msg = std::variant<Tick, Pop, Spawn, Recv>;

    static constexpr double W = 640, H = 420;

    static std::uint32_t hue_at(int i) {
        static const std::uint32_t palette[] = {
            0x818cf8, 0x22d3ee, 0x34d399, 0xf472b6, 0xfbbf24, 0xa78bfa, 0x38bdf8, 0xfb7185
        };
        return palette[i % 8];
    }

    static Model init() { return {}; }

    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
        return std::visit(overload{
            [&](Tick) -> std::pair<Model, Cmd<Msg>> {
                m.t += 0.033;
                // drift every star; cull ones that escaped off-field (a miss)
                std::vector<Star> alive;
                for (auto& s : m.stars) {
                    Star n = s;
                    n.x += n.vx; n.y += n.vy;
                    bool bounced = false;
                    if (n.x < 24 || n.x > W - 24) { n.vx = -n.vx; bounced = true; }
                    if (n.y < 24 || n.y > H - 24) { n.vy = -n.vy; bounced = true; }
                    (void)bounced;
                    // a star that's lived too long fades out uncaught (a miss)
                    if (m.t - n.born > 9.0) { m.missed++; m.combo = 0; continue; }
                    alive.push_back(n);
                }
                m.stars = std::move(alive);
                // age + cull shard debris
                std::vector<Shard> shards;
                for (auto& sh : m.shards) {
                    Shard n = sh;
                    n.x += n.dx; n.y += n.dy; n.dx *= 0.90; n.dy *= 0.90; n.life--;
                    if (n.life > 0) shards.push_back(n);
                }
                m.shards = std::move(shards);
                return { m, Cmd<Msg>::none() };
            },
            [&](Spawn) -> std::pair<Model, Cmd<Msg>> {
                if (m.stars.size() >= 6) return { m, Cmd<Msg>::none() };
                double a = std::fmod(m.t * 137.5, 6.28318);
                double speed = 0.7 + 0.25 * (m.next_id % 5);
                Star s{ m.next_id++,
                        60 + std::fmod(m.t * 911.0, W - 120),
                        60 + std::fmod(m.t * 613.0, H - 120),
                        speed * std::cos(a), speed * std::sin(a),
                        m.t, hue_at(m.next_id) };
                m.stars.push_back(s);
                return { m, Cmd<Msg>::none() };
            },
            [&](Pop p) -> std::pair<Model, Cmd<Msg>> {
                auto it = std::find_if(m.stars.begin(), m.stars.end(),
                                        [&](const Star& s){ return s.id == p.id; });
                if (it == m.stars.end()) return { m, Cmd<Msg>::none() };
                Star hit = *it;
                m.stars.erase(it);
                m.combo++;
                m.best_combo = std::max(m.best_combo, m.combo);
                int gained = 10 + m.combo * 2;
                m.score += gained;
                // spray shards from the hit point
                for (int i = 0; i < 8; ++i) {
                    double a = (double)i / 8 * 6.28318 + m.t;
                    m.shards.push_back({ m.next_id++, hit.x, hit.y,
                                          2.6 * std::cos(a), 2.6 * std::sin(a),
                                          hit.hue, 14 });
                }
                // publish to the shared leaderboard — every open tab hears this
                std::string payload = m.me + "|" + std::to_string(m.score);
                return { m, Cmd<Msg>::broadcast("swarm-board", payload) };
            },
            [&](const Recv& r) -> std::pair<Model, Cmd<Msg>> {
                auto bar = r.payload.find('|');
                if (bar == std::string::npos) return { m, Cmd<Msg>::none() };
                std::string name = r.payload.substr(0, bar);
                int score = std::atoi(r.payload.c_str() + bar + 1);
                bool found = false;
                for (auto& b : m.board) {
                    if (b.name == name) { b.score = std::max(b.score, score); found = true; break; }
                }
                if (!found) m.board.push_back({ name, score });
                std::sort(m.board.begin(), m.board.end(),
                          [](const Board& a, const Board& b){ return a.score > b.score; });
                if (m.board.size() > 6) m.board.resize(6);
                return { m, Cmd<Msg>::none() };
            },
        }, msg);
    }

    static Sub<Msg> subscribe(const Model&) {
        return Sub<Msg>::batch({
            Sub<Msg>::every(33, Tick{}),
            Sub<Msg>::every(650, Spawn{}),
            Sub<Msg>::on_topic("swarm-board", [](std::string payload){ return Recv{ std::move(payload) }; }),
        });
    }

    // ── view ─────────────────────────────────────────────────────────────
    // A star is an absolutely-positioned round box (NOT a thin SVG stroke), so
    // the whole disc is a generous tap target and moving it is just a top/left
    // change. tap_pop() gives instant client-side press feedback (no round-trip
    // lag), and the position is expressed in % of the field so it stays correct
    // as the responsive field scales. Keyed for FLIP-free identity across ticks.
    static NodeRef star_node(const Star& s, double t) {
        double pulse = 1.0 + 0.18 * std::sin(t * 6 + s.id);
        float size = (float)(30 * pulse);                 // visual disc diameter
        // the visible disc: radial highlight + colour + glow
        auto disc = box()
            | w(px(size)) | h(px(size)) | round(999)
            | radial(0xffffff, 32, 28, s.hue, 85)
            | drop_shadow(s.hue, 18, 0.7f);
        // an invisible padded hit-target around it so near-misses still count
        return box(disc)
            | at_pct((float)(s.y / H * 100.0), (float)(s.x / W * 100.0))
            | pad(10)                                     // fat clickable margin
            | key("star" + std::to_string(s.id))
            | pointer | tap(Pop{ s.id }) | tap_pop()
            | on(Hover, brightness(125));
    }

    static NodeRef shard_node(const Shard& s) {
        float o = (float)s.life / 14.0f;
        return box(box() | w(6) | h(6) | round(999) | bg(s.hue))
            | at_pct((float)(s.y / H * 100.0), (float)(s.x / W * 100.0))
            | opacity(o);
    }

    static NodeRef board_row(const Board& b, int rank, const std::string& me) {
        bool mine = b.name == me;
        return row(
            text(rank == 0 ? "\xF0\x9F\x91\x91" : ("#" + std::to_string(rank + 1)))
                | fg(rank == 0 ? warn : muted) | font(13) | mono | w(rem(2.2f)),
            text(b.name) | fg(mine ? brand2 : ink) | font(14) | (mine ? semibold : Mod{sty([](Style&){})}),
            push(),
            text(std::to_string(b.score)) | fg(ink) | font(14) | bold | mono
        ) | gap(8) | center | pad_x(4);
    }

    static NodeRef view(const Model& m) {
        std::vector<NodeRef> layers;
        for (auto& s : m.shards) layers.push_back(shard_node(s));
        for (auto& s : m.stars) layers.push_back(star_node(s, m.t));

        auto field = box_(std::move(layers))
            | relative | w_full | h(px((float)H))
            | round(20) | clip
            | radial(0x1e293b, 30, 20, 0x05070d, 70)
            | border(1, line);
        field->style.flow = Flow::stack; finalize(*field);

        auto stat = [](std::string label, std::string val, std::uint32_t c) {
            return col(
                text(label) | fg(muted) | font(11) | tracking(1.2f),
                text(val) | fg(c) | font(26) | bold | mono
            ) | gap(2);
        };

        auto header = row(
            col(text("swarm") | fg(ink) | font(24) | bold,
                text("tap a star before it drifts away \xE2\x80\x94 open two tabs, the board is shared") | fg(muted) | font(13)) | gap(2),
            push(),
            stat("score", std::to_string(m.score), brand2),
            stat("combo", std::to_string(m.combo) + "x", m.combo > 3 ? good : ink),
            stat("missed", std::to_string(m.missed), m.missed > 0 ? bad : muted)
        ) | gap(28) | center | wrap;

        std::vector<NodeRef> rows;
        if (m.board.empty()) {
            rows.push_back(text("no scores yet \xE2\x80\x94 pop a star") | fg(faint) | font(13) | pad_y(12));
        } else {
            int rank = 0;
            for (auto& b : m.board) rows.push_back(board_row(b, rank++, m.me));
        }

        auto board_card = col(
            text("live leaderboard") | fg(muted) | font(12) | tracking(1.4f) | bold,
            col_(std::move(rows)) | gap(6)
        ) | gap(12) | pad(18) | round(16) | frost(10) | elevation(2) | min_w(rem(14));

        return col(
            header,
            row(field | grow(1), board_card) | gap(20) | wrap | items_start
        ) | gap(20) | pad(28) | max_w(1040) | center_x
          | h_screen | theme(midnight())
          | radial(brand, 80, -10, 0x05070d, 60);
    }
};

int main() {
    static_assert(SurfaceProgram<Swarm>);
    return live<Swarm>({ .port = 8080, .page_bg = 0x05070d, .title = "waya \xC2\xB7 swarm" });
}
