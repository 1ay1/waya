/// tests/test_attrs.cpp — the general attribute channel: ANY attribute, boolean
/// attributes, data-*, ARIA, and DOM events, all one clean pipe.

#include <waya/waya.hpp>

#include <iostream>
#include <string>

using namespace waya::dsl;
using namespace waya::style;
using namespace waya::style::literals;

static int g_fail = 0, g_pass = 0;
#define CHECK(cond) do { if (cond) ++g_pass; else { ++g_fail; \
    std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << "  " #cond "\n"; } } while (0)
static bool has(const std::string& h, std::string_view n) { return h.find(n) != std::string::npos; }

int main() {
    // ── arbitrary name/value attributes ─────────────────────────────────────
    {
        auto r = waya::render::render(
            input_() | attr<"type", "email"> | attr<"name", "email">
                     | attr<"placeholder", "you@x.com">);
        CHECK(has(r.html, "type=\"email\""));
        CHECK(has(r.html, "name=\"email\""));
        CHECK(has(r.html, "placeholder=\"you@x.com\""));
    }

    // ── boolean attributes render bare ──────────────────────────────────────
    {
        auto r = waya::render::render(input_() | flag<"required"> | flag<"disabled">);
        CHECK(has(r.html, "required"));
        CHECK(has(r.html, "disabled"));
        CHECK(!has(r.html, "required=\""));   // NOT required="required"
    }

    // ── data-* and ARIA are just attributes ─────────────────────────────────
    {
        auto r = waya::render::render(
            div_(text("panel")) | attr<"data-id", "42"> | attr<"role", "tabpanel">
                                | attr<"aria-label", "Details">);
        CHECK(has(r.html, "data-id=\"42\""));
        CHECK(has(r.html, "role=\"tabpanel\""));
        CHECK(has(r.html, "aria-label=\"Details\""));
    }

    // ── event handlers (generic + terse aliases) ────────────────────────────
    {
        auto r1 = waya::render::render(button_(text("Go")) | on_<"click", "go()">);
        CHECK(has(r1.html, "onclick=\"go()\""));
        auto r2 = waya::render::render(input_() | on_input<"update(this.value)">);
        CHECK(has(r2.html, "oninput=\"update(this.value)\""));
    }

    // ── runtime-valued attribute ────────────────────────────────────────────
    {
        int n = 7;
        auto r = waya::render::render(input_() | attr_dyn("value", std::to_string(n)));
        CHECK(has(r.html, "value=\"7\""));
    }

    // ── conditional flag: present only when true ────────────────────────────
    {
        auto on  = waya::render::render(button_(text("x")) | flag_if("disabled", true));
        auto off = waya::render::render(button_(text("x")) | flag_if("disabled", false));
        CHECK(has(on.html,  "disabled"));
        CHECK(!has(off.html, "disabled"));
    }

    // ── attribute VALUES are escaped (no injection) ─────────────────────────
    {
        auto r = waya::render::render(
            input_() | attr_dyn("value", std::string("\"><script>alert(1)</script>")));
        CHECK(!has(r.html, "<script>"));              // escaped
        CHECK(has(r.html, "&lt;script&gt;"));
    }

    // ── a complete, real form in a few clean pipes ──────────────────────────
    {
        auto r = waya::render::render(
            form_(
                label_(text("Email")) | attr<"for", "e">,
                input_() | id_<"e"> | attr<"type", "email"> | attr<"name", "email"> | flag<"required">,
                button_(text("Sign up")) | attr<"type", "submit">
            ) | attr<"method", "post"> | attr<"action", "/signup">);
        CHECK(has(r.html, "<form method=\"post\" action=\"/signup\">"));
        CHECK(has(r.html, "<label for=\"e\">Email</label>"));
        CHECK(has(r.html, "<input id=\"e\" type=\"email\" name=\"email\" required>"));
        CHECK(has(r.html, "<button type=\"submit\">Sign up</button>"));
    }

    // ── attributes + styles + layout compose freely ─────────────────────────
    {
        auto r = waya::render::render(
            row(input_() | attr<"name", "q">) | gap(8_px) | pad(4_px));
        CHECK(has(r.html, "name=\"q\""));
        CHECK(has(r.css, "gap:8px"));
        CHECK(has(r.css, "flex-direction:row"));
    }

    std::cout << "test_attrs: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
