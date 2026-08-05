#pragma once
/// \file sub.hpp
/// `Sub<Msg>` — declarative event sources, reconciled each frame (maya's Sub).
///
/// `subscribe(model)` returns the set of event sources the app currently wants:
/// timers, pub/sub topics, and (later) DB change streams. The runtime diffs the
/// returned Sub against the previous one and starts/stops sources to match — so
/// a subscription that disappears from the returned value is torn down
/// automatically, which is how you avoid the leak every hand-rolled socket app
/// eventually grows.

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace waya {

template <typename Msg>
class Sub {
public:
    struct None {};
    /// Fire `make()` -> Msg every `interval`.
    struct Every { std::chrono::milliseconds interval; std::function<Msg()> make; std::string id; };
    /// Subscribe to a pub/sub topic; each payload becomes a Msg.
    struct Topic { std::string topic; std::function<Msg(std::string)> on_msg; };

    using Source = std::variant<Every, Topic>;

    Sub() = default;

    // ── Factories ───────────────────────────────────────────────────────────
    static Sub none() { return Sub{}; }

    static Sub every(std::chrono::milliseconds interval, std::function<Msg()> make,
                     std::string id = "") {
        Sub s; s.sources_.push_back(Every{interval, std::move(make), std::move(id)});
        return s;
    }
    static Sub topic(std::string t, std::function<Msg(std::string)> on_msg) {
        Sub s; s.sources_.push_back(Topic{std::move(t), std::move(on_msg)});
        return s;
    }
    /// Combine several subscriptions.
    static Sub batch(std::vector<Sub> subs) {
        Sub s;
        for (auto& sub : subs)
            for (auto& src : sub.sources_) s.sources_.push_back(std::move(src));
        return s;
    }

    [[nodiscard]] const std::vector<Source>& sources() const { return sources_; }

private:
    std::vector<Source> sources_;
};

} // namespace waya
