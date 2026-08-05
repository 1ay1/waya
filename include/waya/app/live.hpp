#pragma once
/// \file live.hpp
/// The Tier-2 live runtime: run a `Program` as a stateful session, re-rendering
/// on the server in response to client events. maya's Elm loop, over HTTP.
///
/// This is the minimal-but-real version:
///   • The page carries a small client that forwards `data-waya-*` events to
///     the server and swaps in the returned HTML.
///   • Each request carries the message id; the server maps it back to a `Msg`,
///     runs `update`, re-renders `view`, and returns the new markup.
///
/// The full version (persistent WebSocket session + static/dynamic diff patches)
/// is Phase 4; this proves the loop end to end today over the dev server.
///
///   struct Counter { Model; Msg; init; update; view; };
///   waya::live<Counter>({.route="/"});   // http://localhost:8080

#include "program.hpp"
#include "msg.hpp"
#include "../net/serve.hpp"
#include "../render/html.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace waya {

/// Options for a live app.
struct LiveConfig {
    int         port = 8080;
    bool        open = true;
    const char* host = "127.0.0.1";
};

namespace detail {

/// The client shim: intercept clicks on [data-waya-msg], POST the id to
/// /__waya_msg, and replace the app root with the returned HTML. ~25 lines,
/// no dependencies. (Phase 4 upgrades this to a WebSocket + diff patches.)
inline const char* live_client() {
    return
    "<script>(function(){"
    "function send(id){"
    "fetch('/__waya_msg?id='+encodeURIComponent(id),{method:'POST'})"
    ".then(function(r){return r.text()}).then(function(html){"
    "var root=document.getElementById('waya-root');"
    "if(root){var d=document.createElement('div');d.innerHTML=html;"
    "root.replaceWith(d.firstElementChild||d);}"
    "});}"
    "document.addEventListener('click',function(e){"
    "var t=e.target.closest('[data-waya-msg]');"
    "if(t){e.preventDefault();send(t.getAttribute('data-waya-msg'));}"
    "});"
    "})();</script>";
}

} // namespace detail

/// Run `P` as a live session. Blocks until Ctrl-C.
///
/// One shared Model guarded by a mutex (single-session dev model; Phase 4 gives
/// each client its own session). `on_msg<P>(i)` in the view tags a click to the
/// i-th registered message.
template <typename P>
    requires Program<P>
int live(LiveConfig cfg = {}) {
    using Model = typename P::Model;
    using Msg   = typename P::Msg;

    auto [model0, _] = program_init<P>();

    struct State {
        Model model;
        std::mutex mu;
    };
    auto state = std::make_shared<State>();
    state->model = std::move(model0);

    // Render the app wrapped in <div id="waya-root">, with the message table
    // exposed so the client can address handlers by index.
    auto render_app = [state] {
        std::lock_guard lock(state->mu);
        // The message registry is filled as the view is built (see on_msg).
        app::detail::begin_msg_capture<Msg>();
        auto node = P::view(state->model);
        auto body = waya::render::render(node);   // html + css
        std::string root =
            "<div id=\"waya-root\">" + body.html + "</div>";
        std::string style = body.css.empty() ? "" : "<style>" + body.css + "</style>";
        return std::pair{root, style};
    };

    ServeConfig sc;
    sc.port = cfg.port; sc.open = cfg.open; sc.host = cfg.host;

    return serve([state, render_app](const Request& req) -> std::string {
        // Event endpoint: /__waya_msg?id=<n> → run update, return fresh root.
        if (req.path.rfind("/__waya_msg", 0) == 0) {
            auto id = app::detail::query_param(req.path, "id");
            if (auto msg = app::detail::lookup_msg<Msg>(id)) {
                std::lock_guard lock(state->mu);
                auto [m2, cmd] = P::update(std::move(state->model), *msg);
                state->model = std::move(m2);
                (void)cmd;   // Cmd interpretation lands with Phase 4 WS runtime
            }
            auto [root, style] = render_app();
            return style + root;   // client swaps #waya-root; style is idempotent
        }

        // Full page.
        auto [root, style] = render_app();
        std::string doc =
            "<!DOCTYPE html><html><head><meta charset=\"utf-8\">" + style +
            "</head><body>" + root + detail::live_client() + "</body></html>";
        return doc;
    }, sc);
}

} // namespace waya
