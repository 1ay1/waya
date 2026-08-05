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
#include "../render/vwalk.hpp"
#include "../render/diff.hpp"

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

/// The client shim: maintain a WebSocket-like channel (here: long-poll over the
/// dev server), forward clicks, and APPLY PATCH OPS to the DOM by path — the
/// browser-window-as-terminal client. It never re-parses the page; it walks to
/// the addressed node and mutates it, exactly like maya writing changed cells.
inline const char* live_client() {
    return
    "<script>(function(){"
    // resolve a dotted path (\"0.3.1\") to a DOM node under #waya-root's child
    "function at(path){var el=document.getElementById('waya-root').firstElementChild;"
    "if(path==='')return el;var p=path.split('.');"
    "for(var i=0;i<p.length;i++){el=el.children[+p[i]];if(!el)return null;}return el;}"
    // apply one op: [op,path,a,b]
    "function apply(op){var k=op[0],path=op[1],a=op[2],b=op[3];var el=at(path);"
    "if(k===0){if(el)el.textContent=a;}"                       // set_text
    "else if(k===1){if(el)el.setAttribute(a,b);}"              // set_attr
    "else if(k===2){if(el)el.removeAttribute(a);}"            // remove_attr
    "else if(k===3){if(el){var d=document.createElement('div');d.innerHTML=a;"
    "el.replaceWith(d.firstElementChild||document.createTextNode(a));}}" // replace
    "else if(k===4){if(el)el.remove();}"                       // remove (path=child)
    "else if(k===5){var pa=at(path);if(pa){var d=document.createElement('div');"
    "d.innerHTML=a;pa.appendChild(d.firstElementChild||document.createTextNode(a));}}" // insert
    "}"
    "function applyPatch(ops){for(var i=0;i<ops.length;i++)apply(ops[i]);}"
    // send a Msg id; receive a JSON patch; apply it. No reload, no re-render.
    "function send(id){fetch('/__waya_msg?id='+encodeURIComponent(id),{method:'POST'})"
    ".then(function(r){return r.json()}).then(applyPatch).catch(function(){});}"
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
        vdom::VNode prev;      ///< the last-rendered tree (maya's prev_cells)
        bool have_prev = false;
        std::mutex mu;
    };
    auto state = std::make_shared<State>();
    state->model = std::move(model0);

    // Build just the VNode for the current model (the Msg path needs only this;
    // the HTML for the full page is built separately below). The message table
    // is filled while P::view runs (on_msg pipes).
    auto build_vnode = [state](style::StyleSheet& sheet) {
        app::detail::begin_msg_capture<Msg>();
        auto node = P::view(state->model);
        return waya::render::to_vnode(node, sheet);
    };

    ServeConfig sc;
    sc.port = cfg.port; sc.open = cfg.open; sc.host = cfg.host;

    return serve([state, build_vnode](const Request& req) -> std::string {
        // Event endpoint: /__waya_msg?id=<n> → update → re-render → DIFF → patch.
        // No HTML is built here — only the VNode, diffed against the retained
        // prev. Only the changed nodes go on the wire.
        if (req.path.rfind("/__waya_msg", 0) == 0) {
            std::lock_guard lock(state->mu);
            auto id = app::detail::query_param(req.path, "id");
            if (auto msg = app::detail::lookup_msg<Msg>(id)) {
                auto [m2, cmd] = P::update(std::move(state->model), *msg);
                state->model = std::move(m2);
                (void)cmd;   // Cmd interpretation lands with the WS runtime
            }
            style::StyleSheet sheet;   // style classes only matter for changed nodes
            auto vnode = build_vnode(sheet);
            vdom::Patch patch = state->have_prev
                ? vdom::diff(state->prev, vnode)
                : vdom::Patch{};
            state->prev = std::move(vnode);
            state->have_prev = true;
            return vdom::to_json(patch);
        }

        // Full page: render HTML once, and remember the VNode as the baseline.
        std::lock_guard lock(state->mu);
        app::detail::begin_msg_capture<Msg>();
        auto node = P::view(state->model);
        style::StyleSheet sheet;
        std::string html;
        waya::render::detail::walk(html, sheet, node);
        state->prev = waya::render::to_vnode(node, sheet);
        state->have_prev = true;
        std::string css = sheet.render();
        std::string style = css.empty() ? "" : "<style>" + css + "</style>";
        return "<!DOCTYPE html><html><head><meta charset=\"utf-8\">" + style +
               "</head><body><div id=\"waya-root\">" + html + "</div>" +
               detail::live_client() + "</body></html>";
    }, sc);
}

} // namespace waya
