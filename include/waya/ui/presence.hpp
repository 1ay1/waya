#pragma once
/// \file ui/presence.hpp
/// Presence — who's online / who's typing, over the pub/sub layer.
///
/// waya already has multiplayer: `Cmd::broadcast(topic, payload)` and
/// `Sub::on_topic(topic, fn)` fan state between sessions. Presence is the common
/// thing built on that — a live roster of who's here and who's typing — as model
/// state you update from the broadcasts you receive and prune by heartbeat.
///
///   struct Model { Presence room; std::string me; };
///
///   // on join / every few seconds: broadcast a heartbeat
///   Cmd::broadcast("room-42", me + "|online")       // "<user>|<state>"
///   // when the user types: broadcast typing (debounced)
///   Cmd::broadcast("room-42", me + "|typing")
///
///   // update: fold each broadcast into the roster; prune stale on a tick
///   [&](Peer p)  { m.room.mark(p.user, p.state);  return {m, Cmd::none()}; }
///   [&](Tick)    { m.room.prune(std::chrono::seconds{10}); return {m, Cmd::none()}; }
///
///   // view:
///   presence_bar(m.room, m.me)            // avatars of who's online
///   typing_line(m.room, m.me)             // "Ada and Bob are typing…"
///
/// `mark(user, state)` stamps a peer's status + a fresh timestamp; `prune(ttl)`
/// drops peers who haven't been heard from (so a crashed tab disappears).
/// `parse_peer` splits the "<user>|<state>" broadcast payload for you.

#include "../surface/node.hpp"
#include "components.hpp"

#include <cctype>
#include <chrono>
#include <string>
#include <utility>
#include <vector>

namespace waya::ui {

using namespace waya::surface;

/// The status of one peer (last-seen timestamp is monotonic ms since epoch).
struct Peer {
    std::string user;
    bool typing = false;
    long long last_ms = 0;      // when we last heard from them
    bool operator==(const Peer&) const = default;
};

/// A live roster of peers in a room. One value in your model.
struct Presence {
    std::vector<Peer> peers;
    bool operator==(const Presence&) const = default;

    /// Milliseconds since epoch (monotonic-ish; used only for relative TTL).
    static long long now_ms(){
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    /// Record `user`'s status ("online" / "typing" / "left"). Freshens their
    /// timestamp; "left" removes them immediately. Called from an on_topic Msg.
    void mark(const std::string& user, const std::string& state){
        if (state == "left"){ remove(user); return; }
        long long t = now_ms();
        for (auto& p : peers)
            if (p.user == user){ p.typing = (state == "typing"); p.last_ms = t; return; }
        peers.push_back({ user, state == "typing", t });
    }
    /// A convenience overload for a pre-parsed timestamp (testing / replay).
    void mark_at(const std::string& user, bool typing, long long at_ms){
        for (auto& p : peers)
            if (p.user == user){ p.typing = typing; p.last_ms = at_ms; return; }
        peers.push_back({ user, typing, at_ms });
    }
    void remove(const std::string& user){
        for (auto it = peers.begin(); it != peers.end(); ++it)
            if (it->user == user){ peers.erase(it); return; }
    }
    /// Drop peers not heard from within `ttl` (a crashed/closed tab vanishes).
    void prune(std::chrono::milliseconds ttl){ prune_at(ttl, now_ms()); }
    void prune_at(std::chrono::milliseconds ttl, long long nowms){
        long long cutoff = nowms - ttl.count();
        std::erase_if(peers, [&](const Peer& p){ return p.last_ms < cutoff; });
    }

    [[nodiscard]] std::size_t count() const { return peers.size(); }
    /// Everyone currently typing, excluding `me` (you don't need to see yourself).
    [[nodiscard]] std::vector<std::string> typers(const std::string& me = "") const {
        std::vector<std::string> out;
        for (auto& p : peers) if (p.typing && p.user != me) out.push_back(p.user);
        return out;
    }
};

/// Split a "<user>|<state>" presence broadcast into its parts (state defaults to
/// "online" if the payload is just a username).
inline std::pair<std::string,std::string> parse_peer(const std::string& payload){
    auto bar = payload.find('|');
    if (bar == std::string::npos) return { payload, "online" };
    return { payload.substr(0, bar), payload.substr(bar+1) };
}

/// `presence_bar(presence, me)` — a row of small avatars for who's online
/// (excluding you), overlapped like a stacked group. Empty when it's just you.
inline NodeRef presence_bar(const Presence& pr, const std::string& me = ""){
    std::vector<NodeRef> avs;
    for (auto& p : pr.peers){
        if (p.user == me) continue;
        std::string initials = p.user.empty() ? "?" : std::string(1, (char)std::toupper(p.user[0]));
        avs.push_back(avatar(initials, 26)
            | (p.typing ? ring(rgba(0x22c55e, .8f), 2) : Mod{})
            | detail::raw_css("margin-left","-6px")
            | detail::raw_css("border","2px solid var(--wa-bg, #060a14)")
            | aria_label(p.user + (p.typing ? " (typing)" : " (online)")));
    }
    if (avs.empty()) return nothing();
    return row_(std::move(avs)) | items_center | detail::raw_css("padding-left","6px");
}

/// `typing_line(presence, me)` — the "Ada is typing…" / "Ada and Bob are
/// typing…" line beneath a chat input. Empty when nobody (else) is typing.
inline NodeRef typing_line(const Presence& pr, const std::string& me = ""){
    auto who = pr.typers(me);
    if (who.empty()) return nothing();
    std::string txt;
    if (who.size() == 1) txt = who[0] + " is typing";
    else if (who.size() == 2) txt = who[0] + " and " + who[1] + " are typing";
    else txt = std::to_string(who.size()) + " people are typing";
    txt += "\xe2\x80\xa6";   // …
    return row(text(txt) | fg_muted | detail::raw_css("font-size","12px"))
        | items_center | gap(6) | role("status") | aria("live","polite");
}

} // namespace waya::ui
