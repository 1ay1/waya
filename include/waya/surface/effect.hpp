#pragma once
/// \file effect.hpp
/// Effects and subscriptions for the surface runtime — maya's `Cmd`/`Sub`,
/// transposed onto the browser. This is what turns a pure `update` into a real
/// app: `update` returns a `Cmd` *describing* side effects, and the runtime
/// interprets them (timers, navigation, async fetch) and feeds the results back
/// as messages. `subscribe` declares standing event sources (intervals, so
/// live/real-time UIs work) as a function of the model.
///
///   static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
///       if (msg == Tick)  return { m.advance(), Cmd<Msg>::after(1000ms, Tick) };  // clock
///       if (msg == Load)  return { m, Cmd<Msg>::fetch("/data.json", Loaded) };    // async
///       if (msg == Go)    return { m, Cmd<Msg>::navigate("/next") };              // route
///       return { m, Cmd<Msg>::none() };
///   }
///
///   static Sub<Msg> subscribe(const Model& m) {
///       return m.live ? Sub<Msg>::every(1000ms, Tick) : Sub<Msg>::none();
///   }
///
/// The type-theoretic insight (maya's words): a pure function cannot perform
/// I/O, but it CAN return a *description* of I/O to perform later. That keeps
/// `update` pure — same inputs, same outputs, testable with `==`, no server.
///
/// A Program may return just `Model` (no effects) OR `(Model, Cmd)` — the
/// runtime supports both, so simple apps stay simple.
///
/// Surface `Msg` is whatever the app chooses (typically `int`, an int enum, or
/// a small variant); it rides the WebSocket as an integer tap id. `Cmd`/`Sub`
/// carry `Msg` values and the runtime converts them at the wire boundary.

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace waya::surface {

using namespace std::chrono_literals;

/// A message-with-value the runtime delivers to `update`: `msg` is the app's
/// message as an integer tap id; `value` is an optional string payload (input
/// value, fetch body, route path, broadcast payload). `topic` is set only for a
/// broadcast delivery, so the owner loop can find the matching on_topic handler.
struct Deliver { int msg; std::string value; std::string topic; };

// ── overload helper (same trick maya uses for std::visit) ───────────────────
template <class... Ts> struct overload : Ts... { using Ts::operator()...; };
template <class... Ts> overload(Ts...) -> overload<Ts...>;

// ============================================================================
// Cmd<Msg> — a description of side effects to perform.
// ============================================================================
// A sum type: exactly one alternative. The runtime pattern-matches and performs
// the effect; user code never performs an effect directly — it returns a Cmd.
// Alternatives split into substrate-neutral (none/quit/batch/after/emit/task)
// and web effects (navigate/push_url/fetch).
template <typename Msg>
class Cmd {
public:
    // ── Alternatives (each describes one effect) ────────────────────────────
    struct None {};
    struct Quit {};
    struct Batch { std::vector<Cmd> cmds; };
    /// Deliver `msg` after `delay` (clocks, debounce, retry backoff).
    struct After { std::chrono::milliseconds delay; Msg msg; };
    /// Deliver `msg` immediately, next tick (chain reducers without a view pass
    /// in between — self-messaging).
    struct Emit  { Msg msg; };
    /// Run `work` off the render path; whatever Msg it produces is delivered.
    struct Task  { std::function<Msg()> work; };
    // ── Web effects ─────────────────────────────────────────────────────────
    /// Client-side navigation: change the URL and route to it. `replace` uses
    /// history.replaceState instead of pushState (no back-button entry).
    struct Navigate { std::string url; bool replace = false; };
    /// history.pushState the URL without triggering a route (deep-link sync).
    struct PushUrl  { std::string url; };
    /// GET `url`; the response body becomes a Msg via `on_done`.
    struct Fetch    { std::string url; std::function<Msg(std::string)> on_done; };
    /// Publish `payload` to everyone subscribed to `topic` (incl. self). The
    /// sender doesn't know or care who's listening — each receiver's
    /// `Sub::on_topic(topic, fn)` turns the payload into ITS own Msg. This is
    /// how independent sessions share state (chat, presence, collaborative UIs).
    struct Broadcast { std::string topic; std::string payload; };

    using Alt = std::variant<None, Quit, Batch, After, Emit, Task,
                             Navigate, PushUrl, Fetch, Broadcast>;

    Cmd() : alt_(std::make_shared<Alt>(None{})) {}
    explicit Cmd(Alt a) : alt_(std::make_shared<Alt>(std::move(a))) {}

    // ── Factories (read like maya) ──────────────────────────────────────────
    static Cmd none() { return Cmd(Alt{None{}}); }
    static Cmd quit() { return Cmd(Alt{Quit{}}); }
    static Cmd after(std::chrono::milliseconds d, Msg m) { return Cmd(Alt{After{d, std::move(m)}}); }
    static Cmd after(long ms, Msg m) { return after(std::chrono::milliseconds{ms}, std::move(m)); }
    static Cmd emit(Msg m) { return Cmd(Alt{Emit{std::move(m)}}); }
    static Cmd task(std::function<Msg()> w) { return Cmd(Alt{Task{std::move(w)}}); }
    static Cmd navigate(std::string url, bool replace = false) {
        return Cmd(Alt{Navigate{std::move(url), replace}});
    }
    static Cmd push_url(std::string url) { return Cmd(Alt{PushUrl{std::move(url)}}); }
    static Cmd fetch(std::string url, std::function<Msg(std::string)> on_done) {
        return Cmd(Alt{Fetch{std::move(url), std::move(on_done)}});
    }
    /// Publish to a topic. Every session subscribed via Sub::on_topic(topic,..)
    /// — this one included — receives `payload` and maps it to its own Msg.
    static Cmd broadcast(std::string topic, std::string payload = {}) {
        return Cmd(Alt{Broadcast{std::move(topic), std::move(payload)}});
    }

    /// Combine multiple Cmds. Flattens nested batches and strips Nones — so a
    /// `batch` of one real effect is that effect, and a `batch` of nothing is
    /// `none`. Same normalizing behavior as maya.
    static Cmd batch(std::vector<Cmd> cs) {
        std::vector<Cmd> flat;
        flat.reserve(cs.size());
        for (auto& c : cs) {
            if (std::holds_alternative<None>(*c.alt_)) continue;
            if (auto* b = std::get_if<Batch>(c.alt_.get()))
                for (auto& inner : b->cmds) flat.push_back(std::move(inner));
            else flat.push_back(std::move(c));
        }
        if (flat.empty()) return none();
        if (flat.size() == 1) return std::move(flat[0]);
        return Cmd(Alt{Batch{std::move(flat)}});
    }
    template <typename... Cs>
        requires (sizeof...(Cs) > 1 && (std::same_as<std::remove_cvref_t<Cs>, Cmd> && ...))
    static Cmd batch(Cs&&... cs) {
        std::vector<Cmd> v; v.reserve(sizeof...(Cs));
        (v.push_back(std::forward<Cs>(cs)), ...);
        return batch(std::move(v));
    }

    [[nodiscard]] const Alt& alt() const { return *alt_; }
    [[nodiscard]] bool is_none() const noexcept { return std::holds_alternative<None>(*alt_); }
    [[nodiscard]] bool is_quit() const noexcept { return std::holds_alternative<Quit>(*alt_); }

    // ── Functor map — embed a child's Cmd into a parent's Msg type ───────────
    template <std::invocable<Msg> F>
    [[nodiscard]] auto map(F&& f) const -> Cmd<std::invoke_result_t<F, Msg>> {
        using B = std::invoke_result_t<F, Msg>;
        return std::visit(overload{
            [](const None&) -> Cmd<B> { return Cmd<B>::none(); },
            [](const Quit&) -> Cmd<B> { return Cmd<B>::quit(); },
            [&](const After& a) -> Cmd<B> { return Cmd<B>::after(a.delay, f(a.msg)); },
            [&](const Emit&  e) -> Cmd<B> { return Cmd<B>::emit(f(e.msg)); },
            [](const Navigate& n) -> Cmd<B> { return Cmd<B>::navigate(n.url, n.replace); },
            [](const PushUrl&  p) -> Cmd<B> { return Cmd<B>::push_url(p.url); },
            [](const Broadcast& b) -> Cmd<B> { return Cmd<B>::broadcast(b.topic, b.payload); },
            [&](const Batch& b) -> Cmd<B> {
                std::vector<Cmd<B>> mapped; mapped.reserve(b.cmds.size());
                for (auto& c : b.cmds) mapped.push_back(c.map(f));
                return Cmd<B>::batch(std::move(mapped));
            },
            [&](const Task& t) -> Cmd<B> {
                return Cmd<B>::task([w = t.work, mapper = std::forward<F>(f)]() -> B {
                    return mapper(w());
                });
            },
            [&](const Fetch& ft) -> Cmd<B> {
                return Cmd<B>::fetch(ft.url,
                    [on = ft.on_done, mapper = std::forward<F>(f)](std::string body) -> B {
                        return mapper(on(std::move(body)));
                    });
            },
        }, *alt_);
    }

    /// Equality — makes `update` testable. Value-carrying alternatives compare
    /// their values; those that carry callables compare only the discriminant
    /// (functions aren't comparable). Msg is compared when it's comparable.
    bool operator==(const Cmd& o) const {
        if (alt_->index() != o.alt_->index()) return false;
        return std::visit([&](const auto& a) -> bool {
            using A = std::decay_t<decltype(a)>;
            const auto& b = std::get<A>(*o.alt_);
            if constexpr (std::is_same_v<A, None> || std::is_same_v<A, Quit>) return true;
            else if constexpr (std::is_same_v<A, After>) {
                if (a.delay != b.delay) return false;
                if constexpr (std::equality_comparable<Msg>) return a.msg == b.msg; else return true;
            }
            else if constexpr (std::is_same_v<A, Emit>) {
                if constexpr (std::equality_comparable<Msg>) return a.msg == b.msg; else return true;
            }
            else if constexpr (std::is_same_v<A, Navigate>) return a.url == b.url && a.replace == b.replace;
            else if constexpr (std::is_same_v<A, PushUrl>)  return a.url == b.url;
            else if constexpr (std::is_same_v<A, Broadcast>) return a.topic == b.topic && a.payload == b.payload;
            else if constexpr (std::is_same_v<A, Batch>) {
                if (a.cmds.size() != b.cmds.size()) return false;
                for (std::size_t i = 0; i < a.cmds.size(); ++i)
                    if (!(a.cmds[i] == b.cmds[i])) return false;
                return true;
            }
            else return true;  // callables (Task/Fetch): discriminant match is enough
        }, *alt_);
    }

private:
    std::shared_ptr<Alt> alt_;   // shared_ptr keeps Cmd cheaply copyable
};

// ============================================================================
// Sub<Msg> — declarative event subscriptions (the input side).
// ============================================================================
//   Cmd<Msg>  effects going OUT  (timers, navigation, fetch)
//   Sub<Msg>  events coming IN   (interval ticks, route changes)
// Both are data. The runtime diffs Subs between frames, starting/stopping
// listeners as the model changes — the foundation for clocks and live polling.
template <typename Msg>
class Sub {
public:
    struct None {};
    struct Batch { std::vector<Sub> subs; };
    /// Emit `msg` every `interval` (animations, polling, live data).
    struct Every { std::chrono::milliseconds interval; Msg msg; };
    /// Map the current URL path into a Msg on every route change.
    struct OnRoute { std::function<Msg(std::string)> route; };
    /// Join a pub/sub `topic`; each broadcast `payload` becomes a Msg via `on`.
    /// The runtime registers/unregisters this session as the subscription set
    /// changes, so joining or leaving a room is just a model-driven Sub.
    struct OnTopic { std::string topic; std::function<Msg(std::string)> on; };

    using Alt = std::variant<None, Batch, Every, OnRoute, OnTopic>;

    Sub() : alt_(std::make_shared<Alt>(None{})) {}
    explicit Sub(Alt a) : alt_(std::make_shared<Alt>(std::move(a))) {}

    static Sub none() { return Sub{}; }
    static Sub every(std::chrono::milliseconds interval, Msg msg) { return Sub(Alt{Every{interval, std::move(msg)}}); }
    static Sub every(long ms, Msg msg) { return every(std::chrono::milliseconds{ms}, std::move(msg)); }
    static Sub on_route(std::function<Msg(std::string)> f) { return Sub(Alt{OnRoute{std::move(f)}}); }
    static Sub on_topic(std::string topic, std::function<Msg(std::string)> on) {
        return Sub(Alt{OnTopic{std::move(topic), std::move(on)}});
    }

    static Sub batch(std::vector<Sub> subs) {
        std::vector<Sub> flat;
        flat.reserve(subs.size());
        for (auto& s : subs) {
            if (std::holds_alternative<None>(*s.alt_)) continue;
            if (auto* b = std::get_if<Batch>(s.alt_.get()))
                for (auto& inner : b->subs) flat.push_back(std::move(inner));
            else flat.push_back(std::move(s));
        }
        if (flat.empty()) return none();
        if (flat.size() == 1) return std::move(flat[0]);
        return Sub(Alt{Batch{std::move(flat)}});
    }
    template <typename... Ss>
        requires (sizeof...(Ss) > 1 && (std::same_as<std::remove_cvref_t<Ss>, Sub> && ...))
    static Sub batch(Ss&&... ss) {
        std::vector<Sub> v; v.reserve(sizeof...(Ss));
        (v.push_back(std::forward<Ss>(ss)), ...);
        return batch(std::move(v));
    }

    [[nodiscard]] const Alt& alt() const { return *alt_; }
    [[nodiscard]] bool is_none() const noexcept { return std::holds_alternative<None>(*alt_); }

    /// Collect every interval timer this Sub declares (flattening batches). The
    /// runtime reconciles these against the ones already running each frame.
    [[nodiscard]] std::vector<Every> timers() const {
        std::vector<Every> out; collect_timers(out); return out;
    }
    /// The route handler, if any (last one wins in a batch).
    [[nodiscard]] const OnRoute* route() const {
        const OnRoute* r = nullptr; collect_route(r); return r;
    }
    /// Every topic subscription this Sub declares (flattening batches). The
    /// runtime reconciles these against the topics the session is joined to.
    [[nodiscard]] std::vector<const OnTopic*> topics() const {
        std::vector<const OnTopic*> out; collect_topics(out); return out;
    }

    template <std::invocable<Msg> F>
    [[nodiscard]] auto map(F&& f) const -> Sub<std::invoke_result_t<F, Msg>> {
        using B = std::invoke_result_t<F, Msg>;
        return std::visit(overload{
            [](const None&) -> Sub<B> { return Sub<B>::none(); },
            [&](const Every& e) -> Sub<B> { return Sub<B>::every(e.interval, f(e.msg)); },
            [&](const OnRoute& r) -> Sub<B> {
                return Sub<B>::on_route([rt = r.route, mapper = std::forward<F>(f)](std::string p) -> B {
                    return mapper(rt(std::move(p)));
                });
            },
            [&](const OnTopic& t) -> Sub<B> {
                return Sub<B>::on_topic(t.topic, [on = t.on, mapper = std::forward<F>(f)](std::string p) -> B {
                    return mapper(on(std::move(p)));
                });
            },
            [&](const Batch& b) -> Sub<B> {
                std::vector<Sub<B>> mapped; mapped.reserve(b.subs.size());
                for (auto& s : b.subs) mapped.push_back(s.map(f));
                return Sub<B>::batch(std::move(mapped));
            },
        }, *alt_);
    }

private:
    void collect_timers(std::vector<Every>& out) const {
        std::visit(overload{
            [&](const Every& e) { out.push_back(e); },
            [&](const Batch& b) { for (auto& s : b.subs) s.collect_timers(out); },
            [](const auto&) {},
        }, *alt_);
    }
    void collect_route(const OnRoute*& r) const {
        std::visit(overload{
            [&](const OnRoute& o) { r = &o; },
            [&](const Batch& b) { for (auto& s : b.subs) s.collect_route(r); },
            [](const auto&) {},
        }, *alt_);
    }
    void collect_topics(std::vector<const OnTopic*>& out) const {
        std::visit(overload{
            [&](const OnTopic& o) { out.push_back(&o); },
            [&](const Batch& b) { for (auto& s : b.subs) s.collect_topics(out); },
            [](const auto&) {},
        }, *alt_);
    }
    std::shared_ptr<Alt> alt_;
};

} // namespace waya::surface
