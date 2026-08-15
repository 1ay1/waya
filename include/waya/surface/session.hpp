#pragma once
/// \file session.hpp
/// Non-templated runtime classes for the Surface live runtime — the per-session
/// owner loop's Session, the broadcast Hub, the worker Pool, and the resumption
/// SessionStore. These build ONCE into the waya_runtime static lib (maya
/// convention: header declares, src/surface/session.cpp defines). The class
/// DEFINITIONS stay here because the templated live<App>/perform/handle in
/// surface/live.hpp use them by value/pointer; only the non-trivial method
/// BODIES move out-of-line into the .cpp. Meyers singletons and the templated
/// SessionStore::save/take stay inline.

#include <any>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "effect.hpp"   // waya::surface::Deliver (the queued (msg,value) unit)

namespace waya::surface {

namespace detail {

/// A live session: the single owner of one connection's model + render loop.
/// Background effects (timers, tasks, fetches, wire input) all funnel messages
/// into `queue`; the loop drains it, so the model is only ever touched by one
/// thread. `write` is serialized so paint frames and control frames from
/// different threads never interleave on the socket.
struct Session {
    int conn;
    std::mutex qm;
    std::condition_variable qcv;
    std::deque<Deliver> queue;         // pending (msg,value) to dispatch
    std::atomic<bool> alive{true};
    std::atomic<bool> quit{false};    // true only on an explicit Cmd::quit (not a drop)
    /// Display visibility (browser tab hidden/shown). While hidden, deltas are
    /// pointless — nothing will be seen — so the owner loop skips sending and
    /// resyncs with ONE full paint on return. Like a terminal on an unfocused
    /// tty: don't write to a screen nobody is watching.
    std::atomic<bool> visible{true};
    std::mutex wm;                     // serializes socket writes
    // Running interval subscriptions. Each carries the typed Msg to deliver on
    // tick, keyed for reconciliation by (interval_ms, Msg-token).
    struct Timer { long ms; std::uint64_t key; std::any msg; std::shared_ptr<std::atomic<bool>> run; };
    std::vector<Timer> timers;
    // Fingerprint of the subscriptions applied last frame. reconcile_subs is a
    // global-lock + allocation cost every message; when the model's declared
    // subs are byte-for-byte the same as last frame (the overwhelmingly common
    // case — a keystroke rarely changes what you subscribe to), we skip the
    // whole reconcile. 0 = "never reconciled", so the first frame always runs.
    std::uint64_t sub_fingerprint = 0;

    /// Push a WIRE message: a token (looked up in the msg registry) + value.
    void push_wire(int token, std::string value = {});
    /// Push an already-typed Msg produced by an effect (emit/after/task/fetch).
    void push_msg(std::any msg);
    /// Push a route change (value = path).
    void push_route(std::string path);
    /// Push a display self-report (value = "w|h|dark|tz") from an @env frame.
    void push_env(std::string report);
    /// Push a restored localStorage value on connect (value = "key|val").
    void push_storage(std::string kv);
    /// Push a "tab visible again" sync request (repaint if frames were skipped).
    void push_sync();
    /// Deliver a topic broadcast: the owner loop resolves the on_topic handler.
    void push_topic(std::string topic, std::string payload);
    std::optional<Deliver> pop();
    void stop() { alive = false; qcv.notify_all(); }
    /// Explicit application quit (Cmd::quit) — distinct from a dropped socket, so
    /// teardown knows NOT to retain the model for resumption.
    void quit_now() { quit = true; alive = false; qcv.notify_all(); }

    /// Unblock a reader parked in ::recv and wake the owner loop. Does NOT close
    /// the fd — the owner loop is the sole closer, after the reader has exited,
    /// so the fd number can never be recycled under a stale recv()/send().
    void shutdown_io();

    void send_binary(const std::string& frame);
    void send_text(const std::string& s);
private:
    bool write_all(const char* data, std::size_t len);   // all-or-fail frame write
};

/// The broadcast Hub: a process-global, thread-safe registry mapping a topic to
/// the sessions subscribed to it. `Cmd::broadcast` publishes into it and it
/// fans the payload into every subscribed session's queue (each session then
/// dispatches it through its OWN update, so no shared model, no locks in app
/// code). Sessions register/unregister as their Sub::on_topic set changes.
/// Weak pointers mean a dropped connection is reaped lazily on the next publish.
class Hub {
public:
    static Hub& instance() { static Hub h; return h; }

    /// Set the exact set of topics this session is subscribed to (idempotent).
    void set_topics(const std::shared_ptr<Session>& s, const std::vector<std::string>& topics);

    /// Publish `payload` to every session currently on `topic` (incl. sender).
    void publish(const std::string& topic, const std::string& payload);

    /// Drop a session from every topic (called on teardown).
    void remove(Session* key);

private:
    void drop(const std::string& topic, Session* key);  // caller holds m_
    std::mutex m_;
    std::unordered_map<std::string, std::vector<std::weak_ptr<Session>>> subs_;
    std::unordered_map<Session*, std::vector<std::string>> joined_;
};

/// A bounded worker pool for blocking effects (fetch/task) and one-shot timers.
/// Replaces spawning an unbounded std::thread per effect — under load (many
/// fetches, many timers) that exhausts the OS thread limit and falls over. A
/// fixed pool caps concurrency; excess work queues. Sized from hardware
/// concurrency (min 4), overridable via WAYA_WORKERS.
class Pool {
public:
    static Pool& instance() { static Pool p; return p; }

    void submit(std::function<void()> job);

private:
    Pool();
    void worker();
    std::mutex m_;
    std::condition_variable cv_;
    std::deque<std::function<void()>> jobs_;
};

/// A single-threaded timer scheduler: ONE background thread servicing a min-heap
/// of due times, instead of a dedicated std::thread per Cmd::after and per
/// subscription interval. That old design spawned O(sessions x timers) sleeping
/// OS threads — 5 timers across 1000 sessions = 5000 threads (~40 MB of stacks
/// + scheduler thrash), and a Cmd::after on every keystroke spawned a thread per
/// keystroke. Here every timed effect is a heap entry; the callbacks run on the
/// Pool (so a slow callback never delays the clock). Cancellation is a shared
/// atomic flag checked at fire time — O(1), no heap removal needed.
class Scheduler {
public:
    static Scheduler& instance() { static Scheduler s; return s; }

    /// Run `fn` once after `delay_ms`. If `alive` is given and false at fire
    /// time, the callback is skipped (a dead session's timers self-cancel).
    void after(long delay_ms, std::function<void()> fn,
               std::shared_ptr<std::atomic<bool>> alive = {});

    /// Run `fn` every `interval_ms` until `run` flips to false. `run` is the
    /// same cancel flag the caller keeps, so stopping a subscription is O(1).
    void every(long interval_ms, std::function<void()> fn,
               std::shared_ptr<std::atomic<bool>> run);

private:
    Scheduler();
    void loop();
    struct Task {
        long long due_us;                              // absolute fire time (steady, microseconds)
        long interval_ms;                              // 0 = one-shot; >0 = repeating
        std::function<void()> fn;
        std::shared_ptr<std::atomic<bool>> run;        // cancel flag (may be null)
        std::uint64_t seq;                             // tiebreaker for a stable heap order
    };
    struct Later {                                      // min-heap comparator (earliest due first)
        bool operator()(const Task& a, const Task& b) const {
            return a.due_us != b.due_us ? a.due_us > b.due_us : a.seq > b.seq;
        }
    };
    static long long now_us();
    std::mutex m_;
    std::condition_variable cv_;
    std::priority_queue<Task, std::vector<Task>, Later> heap_;
    std::uint64_t seq_ = 0;
};

/// Retained models for session resumption. When a connection drops, we stash the
/// app's Model (type-erased) under the client's session id with a timestamp; a
/// reconnect within the TTL rebinds to it instead of calling init() again — so a
/// wifi blip or a slept laptop doesn't wipe a half-filled form. Entries older
/// than the TTL are swept lazily on access. Keyed by client-supplied id (opaque
/// per-tab token from sessionStorage), so it's per-tab, not cross-user shared.
class SessionStore {
public:
    static SessionStore& instance() { static SessionStore s; return s; }

    /// Seconds a detached model is kept for a possible reconnect.
    long ttl_seconds = 900;   // 15 minutes

    template <typename Model>
    void save(const std::string& id, Model model) {
        if (id.empty()) return;
        std::lock_guard<std::mutex> l(m_);
        sweep_locked();
        store_[id] = Entry{ std::any{std::move(model)}, now() };
    }

    /// Take the retained model for `id` if present and fresh; removes it (a model
    /// belongs to exactly one live owner loop at a time).
    template <typename Model>
    std::optional<Model> take(const std::string& id) {
        if (id.empty()) return std::nullopt;
        std::lock_guard<std::mutex> l(m_);
        sweep_locked();
        auto it = store_.find(id);
        if (it == store_.end()) return std::nullopt;
        auto* mp = std::any_cast<Model>(&it->second.model);
        if (!mp) { store_.erase(it); return std::nullopt; }   // type changed (rebuild)
        Model m = std::move(*mp);
        store_.erase(it);
        return m;
    }

private:
    struct Entry { std::any model; long ts; };
    static long now();
    void sweep_locked();
    std::mutex m_;
    std::unordered_map<std::string, Entry> store_;
};

}  // namespace detail

}  // namespace waya::surface
