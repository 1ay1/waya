/// examples/hello.cpp — a polished waya landing page.
///
///   cmake --build build -j && ./build/hello        # serves http://localhost:8080
///   ./build/hello --print > page.html               # or dump the HTML
///
/// What this shows off, with NO .css file and NO manual escaping:
///   • reusable components — just functions returning nodes
///   • the universal style channel — hover, transitions, gradients, responsive,
///     CSS custom properties, all as clean pipes (prop / var_ / on / at)
///   • dynamic data — each / when, still type-checked by the content model
///   • style interning — repeated styles collapse to one shared class

#include <waya/waya.hpp>
#include <waya/net/serve.hpp>

#include <cstring>
#include <iostream>
#include <vector>

using namespace waya::dsl;
using namespace waya::style;
using namespace waya::style::literals;

// ── palette (local consts, not a baked-in theme — you pick your own) ────────
namespace c {
constexpr auto bg       = 0x0b1020;
constexpr auto surface  = 0x141b2e;
constexpr auto surface2 = 0x1b2440;
constexpr auto border   = 0x263156;
constexpr auto text     = 0xe8edf7;
constexpr auto muted    = 0x94a3b8;
constexpr auto brand    = 0x6366f1;
constexpr auto brand2   = 0x22d3ee;
constexpr auto ok       = 0x34d399;
constexpr auto down     = 0xf87171;
}

// ── a feature card that lifts on hover ──────────────────────────────────────
static auto feature(std::string_view icon, std::string_view title, std::string_view body) {
    return div_(
        div_(text(icon)) | size(28_px),
        h3_(text(title)) | size(18_px) | weight(Weight::w600) | fg(c::text),
        p_(text(body))   | fg(c::muted) | leading(1.6_rem) | size(15_px)
    )
    | col | gap(10_px) | pad(24_px)
    | bg(c::surface) | rounded(16_px)
    | prop<"border", "1px solid #263156">
    | prop<"transition", "transform .18s ease, box-shadow .18s ease, border-color .18s ease">
    | on<Hover>(prop<"transform", "translateY(-4px)">,
                prop<"box-shadow", "0 12px 32px rgba(0,0,0,.45)">,
                prop<"border-color", "#6366f1">)
    | prop<"flex", "1 1 240px">;   // grow, shrink, 240px basis → responsive wrap
}

// ── a status pill ───────────────────────────────────────────────────────────
static auto pill(bool up) {
    auto base = span_(text(up ? "● up" : "● down"))
        | prop<"display", "inline-flex">
        | pad_y(4_px) | pad_x(10_px) | rounded(9999_px)
        | size(13_px) | weight(Weight::w600);
    return up
        ? base | fg(c::ok)   | prop<"background", "rgba(52,211,153,.12)">
        : base | fg(c::down) | prop<"background", "rgba(248,113,113,.12)">;
}

// ── dynamic services table ──────────────────────────────────────────────────
struct Service { std::string name; int latency_ms; bool up; };

static auto services_table(const std::vector<Service>& svcs) {
    auto th = [](std::string_view t) {
        return th_(text(t))
            | prop<"text-align", "left"> | pad_y(10_px) | pad_x(14_px)
            | fg(c::muted) | size(12_px) | weight(Weight::w600)
            | prop<"text-transform", "uppercase"> | prop<"letter-spacing", ".05em">;
    };
    return table_(
        thead_(tr_(th("Service"), th("Latency"), th("Status"))),
        tbody_(each(svcs, [](const Service& s) {
            auto td = [](auto node) {
                return node | pad_y(12_px) | pad_x(14_px)
                            | prop<"border-top", "1px solid #263156">;
            };
            return tr_(
                td(td_(text(s.name)) | fg(c::text) | weight(Weight::w500)),
                td(td_(text(s.latency_ms, " ms")) | fg(c::muted)),
                td(td_(pill(s.up)))
            );
        }))
    )
    | width(100_pct)
    | prop<"border-collapse", "collapse">
    | bg(c::surface) | rounded(16_px)
    | prop<"overflow", "hidden"> | prop<"border", "1px solid #263156">;
}

// ── the page ────────────────────────────────────────────────────────────────
static auto page(const std::vector<Service>& svcs) {
    return html_(
        head_(
            meta_(),
            title_(text("waya — a C++26 web framework")),
            // one place to set page-global defaults; still no external css file
            style_(text(
                "*{box-sizing:border-box;margin:0}"
                "body{font-family:ui-sans-serif,system-ui,-apple-system,Segoe UI,Roboto,sans-serif}"
                "::selection{background:#6366f1;color:#fff}"))
        ),
        body_(
            main_(
                // hero
                header_(
                    span_(text("C++26 · type-state · server-rendered"))
                        | prop<"display", "inline-block">
                        | pad_y(6_px) | pad_x(12_px) | rounded(9999_px)
                        | size(13_px) | weight(Weight::w600) | fg(c::brand2)
                        | prop<"background", "rgba(34,211,238,.1)">
                        | prop<"border", "1px solid rgba(34,211,238,.25)">,
                    h1_(text("waya"))
                        | size(64_px) | weight(Weight::w800)
                        | prop<"background", "linear-gradient(90deg,#818cf8,#22d3ee)">
                        | prop<"-webkit-background-clip", "text">
                        | prop<"background-clip", "text">
                        | prop<"color", "transparent">
                        | prop<"letter-spacing", "-.02em">,
                    p_(text("Invalid HTML doesn't compile. Style in the DSL. "
                            "Render on the server. maya's philosophy, for the web."))
                        | fg(c::muted) | size(19_px) | leading(1.7_rem)
                        | max_w(560_px)
                ) | col | gap(18_px)
                  | prop<"align-items", "flex-start">,

                // features
                section_(
                    feature("◆", "Type-safe HTML",
                            "The HTML5 content model is enforced by the compiler. "
                            "A <div> in a <p> is a one-line error, not a 3am bug."),
                    feature("✦", "Style, not CSS files",
                            "Any CSS is one clean pipe — hover, media queries, grid, "
                            "gradients. waya owns the rendering and interns it."),
                    feature("⟳", "Dynamic & fast",
                            "each / when render runtime data, still type-checked. "
                            "Repeated styles collapse to one atomic class.")
                ) | row | wrap() | gap(16_px),

                // live table
                section_(
                    div_(
                        h2_(text("Live services")) | size(22_px) | weight(Weight::bold) | fg(c::text),
                        span_(text(std::to_string(svcs.size()) + " monitored"))
                            | fg(c::muted) | size(14_px)
                    ) | row | justify(Justify::between)
                      | prop<"align-items", "baseline">,
                    services_table(svcs)
                ) | col | gap(14_px),

                // footer
                footer_(
                    span_(text("Built with waya")) | fg(c::muted) | size(14_px),
                    a_(text("github.com/1ay1/waya")) | href<"https://github.com/1ay1/waya">
                        | fg(c::brand2) | prop<"text-decoration", "none">
                        | on<Hover>(prop<"text-decoration", "underline">)
                ) | row | justify(Justify::between)
                  | prop<"border-top", "1px solid #263156"> | pad_y(24_px)
            )
            | col | gap(56_px)
            | prop<"max-width", "880px"> | prop<"margin", "0 auto">
            | pad(32_px)
            // responsive: more breathing room on wider screens
            | at<Lg>(pad_y(72_px))
        )
        | bg(c::bg) | fg(c::text)
        | prop<"min-height", "100vh">
    );
}

int main(int argc, char** argv) {
    std::vector<Service> svcs = {
        {"api-gateway", 12, true},
        {"postgres",    40, false},
        {"redis",        3, true},
        {"cdn-edge",     8, true},
    };
    auto view = [&] { return page(svcs); };

    if (argc > 1 && std::strcmp(argv[1], "--print") == 0) {
        std::cout << waya::render::render_document(view()) << '\n';
        return 0;
    }
    return waya::serve(view);
}
