/// examples/form.cpp — a real, working form: attributes, events, layout.
///
///   cmake --build build -j && ./build/form     # http://localhost:8080
///
/// Shows the general attribute channel — any attribute, boolean attributes,
/// data-*, ARIA, and DOM events — all as clean pipes, composed with layout and
/// style. (Tier-1: inline handlers. The Tier-2 live runtime will bind events
/// to `Msg`s and re-render on the server.)

#include <waya/waya.hpp>
#include <waya/net/serve.hpp>

#include <cstring>
#include <iostream>

using namespace waya::dsl;
using namespace waya::style;
using namespace waya::style::literals;

namespace c {
constexpr auto bg = 0x0f172a, surface = 0x1e293b, border = 0x334155;
constexpr auto text = 0xe2e8f0, muted = 0x94a3b8, brand = 0x6366f1;
}

// A labelled field — a reusable component over the attribute channel.
static auto field(std::string_view label, std::string_view name,
                  std::string_view type, std::string_view placeholder) {
    return col(
        label_(text(label)) | attr_dyn("for", std::string(name))
            | fg(c::muted) | size(14_px) | weight(Weight::w500),
        input_() | id_dyn(std::string(name)) | attr_dyn("name", std::string(name))
                 | attr_dyn("type", std::string(type))
                 | attr_dyn("placeholder", std::string(placeholder))
                 | flag<"required">
                 | pad_y(10_px) | pad_x(12_px) | rounded(8_px)
                 | bg(c::bg) | fg(c::text) | size(15_px)
                 | prop<"border", "1px solid #334155">
                 | prop<"outline", "none">
                 | on<Focus>(prop<"border-color", "#6366f1">)
    ) | gap(6_px);
}

int main(int argc, char** argv) {
    auto view = [] {
        return html_(
            head_(meta_(), title_(text("Sign up · waya"))),
            body_(
                center(
                    col(
                        h1_(text("Create account")) | size(28_px) | weight(Weight::bold) | fg(c::text),
                        p_(text("A real form — every attribute is a clean pipe."))
                            | fg(c::muted) | size(15_px),

                        form_(
                            field("Full name", "name", "text", "Ada Lovelace"),
                            field("Email",     "email", "email", "ada@example.com"),
                            field("Password",  "password", "password", "••••••••"),

                            button_(text("Sign up"))
                                | attr<"type", "submit">
                                | on_<"click", "event.preventDefault(); alert('Submitted!')">
                                | pad_y(12_px) | rounded(8_px)
                                | bg(c::brand) | fg(0xffffff)
                                | size(15_px) | weight(Weight::w600)
                                | prop<"border", "none"> | pointer
                                | prop<"transition", "background .15s ease">
                                | on<Hover>(bg(0x4f46e5)),

                            p_(
                                span_(text("Already have an account? ")) | fg(c::muted),
                                a_(text("Log in")) | href<"/login"> | fg(c::brand)
                                    | prop<"text-decoration", "none">
                            ) | size(14_px) | prop<"text-align", "center">
                        ) | flex(Dir::col) | gap(16_px)
                    )
                    | gap(20_px) | pad(32_px)
                    | bg(c::surface) | rounded(16_px)
                    | width(380_px) | max_w(90_pct)
                    | prop<"border", "1px solid #334155">
                    | prop<"box-shadow", "0 20px 60px rgba(0,0,0,.4)">
                )
                | prop<"min-height", "100vh"> | bg(c::bg)
            )
        );
    };

    if (argc > 1 && std::strcmp(argv[1], "--print") == 0) {
        std::cout << waya::render::render_document(view()) << '\n';
        return 0;
    }
    return waya::serve(view);
}
