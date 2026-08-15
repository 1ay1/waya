/// \file session.cpp
/// Out-of-line bodies for the non-templated Surface runtime classes declared in
/// <waya/surface/session.hpp>. Compiled once into waya_runtime; the templated
/// live<App>/perform/handle in surface/live.hpp call these through the header's
/// class declarations. (SessionStore::save/take stay inline in the header — they
/// depend on the app's Model type.)

#include "waya/surface/session.hpp"

#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iterator>

#include "waya/net/ws.hpp"   // ws::encode_text for send_text

// SIGPIPE avoidance: MSG_NOSIGNAL on Linux, harmless 0 elsewhere (SIGPIPE is
// ignored globally in live()).
#ifndef MSG_NOSIGNAL
#define WAYA_MSG_NOSIGNAL 0
#else
#define WAYA_MSG_NOSIGNAL MSG_NOSIGNAL
#endif

namespace waya::surface {
namespace detail {

// ── Session ─────────────────────────────────────────────────────────────────
void Session::push_wire(int token, std::string value) {
    { std::lock_guard<std::mutex> l(qm); Deliver d; d.token=token; d.value=std::move(value); queue.push_back(std::move(d)); }
    qcv.notify_one();
}
void Session::push_msg(std::any msg) {
    { std::lock_guard<std::mutex> l(qm); Deliver d; d.msg=std::move(msg); queue.push_back(std::move(d)); }
    qcv.notify_one();
}
void Session::push_route(std::string path) {
    { std::lock_guard<std::mutex> l(qm); Deliver d; d.is_route=true; d.value=std::move(path); queue.push_back(std::move(d)); }
    qcv.notify_one();
}
void Session::push_env(std::string report) {
    { std::lock_guard<std::mutex> l(qm); Deliver d; d.is_env=true; d.value=std::move(report); queue.push_back(std::move(d)); }
    qcv.notify_one();
}
void Session::push_storage(std::string kv) {
    { std::lock_guard<std::mutex> l(qm); Deliver d; d.is_storage=true; d.value=std::move(kv); queue.push_back(std::move(d)); }
    qcv.notify_one();
}
void Session::push_sync() {
    { std::lock_guard<std::mutex> l(qm); Deliver d; d.is_sync=true; queue.push_back(std::move(d)); }
    qcv.notify_one();
}
void Session::push_topic(std::string topic, std::string payload) {
    { std::lock_guard<std::mutex> l(qm); Deliver d; d.topic=std::move(topic); d.value=std::move(payload); queue.push_back(std::move(d)); }
    qcv.notify_one();
}
std::optional<Deliver> Session::pop() {
    std::unique_lock<std::mutex> l(qm);
    qcv.wait(l, [&]{ return !queue.empty() || !alive; });
    if (!alive && queue.empty()) return std::nullopt;
    Deliver d = std::move(queue.front()); queue.pop_front();
    return d;
}
void Session::shutdown_io() {
    alive = false;
    ::shutdown(conn, SHUT_RDWR);   // makes the blocking recv return 0/-1
    qcv.notify_all();
}
void Session::send_binary(const std::string& frame) {
    if (!alive) return;
    std::lock_guard<std::mutex> l(wm);
    if (::send(conn, frame.data(), frame.size(), WAYA_MSG_NOSIGNAL) < 0) alive = false;
}
void Session::send_text(const std::string& s) {
    if (!alive) return;
    auto f = ws::encode_text(s);
    std::lock_guard<std::mutex> l(wm);
    if (::send(conn, f.data(), f.size(), WAYA_MSG_NOSIGNAL) < 0) alive = false;
}

// ── Hub ─────────────────────────────────────────────────────────────────────
void Hub::set_topics(const std::shared_ptr<Session>& s, const std::vector<std::string>& topics) {
    std::lock_guard<std::mutex> l(m_);
    Session* key = s.get();
    auto cur = joined_[key];
    for (auto& t : cur)
        if (std::find(topics.begin(), topics.end(), t) == topics.end())
            drop(t, key);
    for (auto& t : topics)
        if (std::find(cur.begin(), cur.end(), t) == cur.end())
            subs_[t].push_back(s);
    if (topics.empty()) joined_.erase(key);
    else joined_[key] = topics;
}
void Hub::publish(const std::string& topic, const std::string& payload) {
    std::vector<std::shared_ptr<Session>> live;
    {
        std::lock_guard<std::mutex> l(m_);
        auto it = subs_.find(topic);
        if (it == subs_.end()) return;
        auto& vec = it->second;
        vec.erase(std::remove_if(vec.begin(), vec.end(),
            [&](const std::weak_ptr<Session>& w){
                if (auto sp = w.lock(); sp && sp->alive) { live.push_back(sp); return false; }
                return true;
            }), vec.end());
        if (vec.empty()) subs_.erase(it);
    }
    for (auto& sp : live) sp->push_topic(topic, payload);
}
void Hub::remove(Session* key) {
    std::lock_guard<std::mutex> l(m_);
    auto it = joined_.find(key);
    if (it == joined_.end()) return;
    for (auto& t : it->second) drop(t, key);
    joined_.erase(it);
}
void Hub::drop(const std::string& topic, Session* key) {  // caller holds m_
    auto it = subs_.find(topic);
    if (it == subs_.end()) return;
    auto& vec = it->second;
    vec.erase(std::remove_if(vec.begin(), vec.end(),
        [&](const std::weak_ptr<Session>& w){ auto sp = w.lock(); return !sp || sp.get() == key; }),
        vec.end());
    if (vec.empty()) subs_.erase(it);
}

// ── Pool ────────────────────────────────────────────────────────────────────
void Pool::submit(std::function<void()> job) {
    { std::lock_guard<std::mutex> l(m_); jobs_.push_back(std::move(job)); }
    cv_.notify_one();
}
Pool::Pool() {
    unsigned n = std::thread::hardware_concurrency();
    if (const char* w = std::getenv("WAYA_WORKERS")) n = (unsigned)std::atoi(w);
    if (n < 4) n = 4;
    for (unsigned i = 0; i < n; ++i)
        std::thread([this]{ worker(); }).detach();
}
void Pool::worker() {
    for (;;) {
        std::function<void()> job;
        { std::unique_lock<std::mutex> l(m_);
          cv_.wait(l, [this]{ return !jobs_.empty(); });
          job = std::move(jobs_.front()); jobs_.pop_front(); }
        try { job(); }
        catch (const std::exception& e) {
#ifndef NDEBUG
            std::fprintf(stderr, "waya: effect threw (dropped): %s\n", e.what());
#else
            (void)e;
#endif
        }
        catch (...) {
#ifndef NDEBUG
            std::fprintf(stderr, "waya: effect threw (dropped): unknown exception\n");
#endif
        }
    }
}

// ── SessionStore ────────────────────────────────────────────────────────────
long SessionStore::now() {
    return (long)std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
void SessionStore::sweep_locked() {
    long cutoff = now() - ttl_seconds;
    for (auto it = store_.begin(); it != store_.end();)
        it = (it->second.ts < cutoff) ? store_.erase(it) : std::next(it);
}

} // namespace detail
} // namespace waya::surface
