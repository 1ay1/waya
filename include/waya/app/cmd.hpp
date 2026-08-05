#pragma once
/// \file cmd.hpp
/// `Cmd<Msg>` — side effects as data. Ported from maya's core/cmd.hpp.
///
/// The type-theoretic insight (maya's words): a pure function cannot perform
/// I/O, but it CAN return a *description* of I/O to perform later. `update`
/// returns `(Model, Cmd<Msg>)`; the runtime interprets the Cmd. That keeps
/// `update` pure — same inputs, same outputs, testable with `==`, no server.
///
///   auto update(Model m, Msg msg) -> std::pair<Model, Cmd<Msg>> {
///       return std::visit(overload{
///           [&](Inc)  { return std::pair{Model{m.n+1}, Cmd<Msg>::none()}; },
///           [&](Save) { return std::pair{m, Cmd<Msg>::broadcast("room", ...)}; },
///       }, msg);
///   }
///
/// The alternatives split into substrate-neutral (none/quit/batch/after/task)
/// and web effects (navigate/push_state/broadcast/fetch). The runtime pattern-
/// matches and performs them; user code never performs an effect directly.

#include <chrono>
#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace waya {

template <typename Msg>
class Cmd {
public:
    // ── Alternatives (each describes one effect) ────────────────────────────
    struct None {};
    struct Quit {};
    struct Batch  { std::vector<Cmd> cmds; };
    /// Deliver `msg` after `delay` (timers, debounce).
    struct After  { std::chrono::milliseconds delay; Msg msg; };
    /// Run `work` off the render path; its result becomes a Msg.
    struct Task   { std::function<Msg()> work; };
    // ── Web effects ─────────────────────────────────────────────────────────
    struct Navigate  { std::string url; bool replace = false; }; ///< client-side nav
    struct PushState { std::string url; };                        ///< history.pushState
    struct SetCookie { std::string name, value; };
    struct Broadcast { std::string topic; std::function<void()> emit; };
    struct Fetch     { std::string url; std::function<Msg(std::string)> on_done; };

    using Alt = std::variant<None, Quit, Batch, After, Task,
                             Navigate, PushState, SetCookie, Broadcast, Fetch>;

    Cmd() : alt_(std::make_shared<Alt>(None{})) {}
    explicit Cmd(Alt a) : alt_(std::make_shared<Alt>(std::move(a))) {}

    // ── Factories (read like maya) ──────────────────────────────────────────
    static Cmd none()  { return Cmd(Alt{None{}}); }
    static Cmd quit()  { return Cmd(Alt{Quit{}}); }
    static Cmd batch(std::vector<Cmd> cs) { return Cmd(Alt{Batch{std::move(cs)}}); }
    static Cmd after(std::chrono::milliseconds d, Msg m) {
        return Cmd(Alt{After{d, std::move(m)}});
    }
    static Cmd task(std::function<Msg()> w) { return Cmd(Alt{Task{std::move(w)}}); }
    static Cmd navigate(std::string url, bool replace = false) {
        return Cmd(Alt{Navigate{std::move(url), replace}});
    }
    static Cmd push_state(std::string url) { return Cmd(Alt{PushState{std::move(url)}}); }
    static Cmd set_cookie(std::string n, std::string v) {
        return Cmd(Alt{SetCookie{std::move(n), std::move(v)}});
    }
    static Cmd fetch(std::string url, std::function<Msg(std::string)> on_done) {
        return Cmd(Alt{Fetch{std::move(url), std::move(on_done)}});
    }

    [[nodiscard]] const Alt& alt() const { return *alt_; }

    /// Equality — makes `update` testable. Only the discriminant is compared
    /// for alternatives that carry callables (functions aren't comparable);
    /// value-carrying alternatives compare their values.
    bool operator==(const Cmd& o) const {
        if (alt_->index() != o.alt_->index()) return false;
        return std::visit([&](const auto& a) -> bool {
            using A = std::decay_t<decltype(a)>;
            const auto& b = std::get<A>(*o.alt_);
            if constexpr (std::is_same_v<A, None> || std::is_same_v<A, Quit>)
                return true;
            else if constexpr (std::is_same_v<A, After>) {
                if (a.delay != b.delay) return false;
                if constexpr (std::equality_comparable<Msg>)
                    return a.msg == b.msg;
                else
                    return true;   // Msg not comparable: discriminant is enough
            }
            else if constexpr (std::is_same_v<A, Navigate>)
                return a.url == b.url && a.replace == b.replace;
            else if constexpr (std::is_same_v<A, PushState>)
                return a.url == b.url;
            else if constexpr (std::is_same_v<A, SetCookie>)
                return a.name == b.name && a.value == b.value;
            else
                return true;  // callables/batch: discriminant match is enough
        }, *alt_);
    }

private:
    std::shared_ptr<Alt> alt_;   // shared_ptr keeps Cmd cheaply copyable
};

} // namespace waya
