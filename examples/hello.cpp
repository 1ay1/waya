/// examples/hello.cpp — the smallest real waya page.
///
///   g++ -std=c++26 -Iinclude examples/hello.cpp -o hello && ./hello > page.html
///
/// Note what is absent: no CSS file, no closing tags to mismatch, no manual
/// escaping. Styles are piped in the DSL; waya emits the stylesheet itself.

#include <waya/waya.hpp>
#include <iostream>

using namespace waya::dsl;
using namespace waya::style;
using namespace waya::style::literals;

// A small reusable component is just a function returning a node.
static auto card(std::string_view title, std::string_view body) {
    return div_(
        h2_(text(title)) | size(20_px) | weight(Weight::w600) | fg(0xf1f5f9),
        p_(text(body))   | fg(0x94a3b8) | leading(1.6_rem)
    ) | col | gap(8_px) | pad(20_px) | bg(0x1e293b) | rounded(12_px);
}

int main() {
    auto page = html_(
        head_(
            meta_(),
            title_(text("waya"))
        ),
        body_(
            header_(
                h1_(text("waya")) | size(36_px) | bold | fg(0x3b82f6),
                p_(text("A C++26 web framework where invalid HTML doesn't compile."))
                    | fg(0x94a3b8)
            ) | col | gap(4_px),
            div_(
                card("Type-safe", "The HTML5 content model is enforced by the compiler."),
                card("maya-style", "Style in the DSL; waya owns the rendering, no CSS files."),
                card("Fast", "Static/dynamic diffing sends ~60-byte patches over the wire.")
            ) | row | wrap() | gap(16_px)
        ) | col | gap(32_px) | pad(48_px) | bg(0x0f172a) | min_w(100_vw)
    );

    std::cout << waya::render::render_document(page) << '\n';
    return 0;
}
