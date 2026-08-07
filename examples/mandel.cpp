/// examples/mandel.cpp — MANDEL: a live Mandelbrot set explorer, computed in
/// C++ on the server. Click to zoom in (shift-scale the viewport), right-side
/// buttons to zoom out / reset / cycle palettes. Each pixel's escape count is
/// mapped to a colour and the whole field is packed into one SVG <image> via a
/// base64 data URI \u2014 pure compute streamed to the browser as one node.
///
///   waya run mandel           # then open the printed URL

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>
#include <waya/ui.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

using namespace waya::surface;
using namespace waya::surface::literals;
using namespace waya::ui;

struct Mandel {
    static constexpr int   RW = 200, RH = 140;   // render resolution (upscaled by CSS)
    static constexpr int   MAXIT = 200;

    static constexpr std::uint32_t ink   = 0xeef2f8;
    static constexpr std::uint32_t body_c= 0x8b98af;
    static constexpr std::uint32_t faint = 0x556074;
    static constexpr std::uint32_t line  = 0x1c2436;
    static constexpr std::uint32_t brand = 0x6d7cff;

    struct Model {
        double cx = -0.6, cy = 0.0, scale = 1.6;   // viewport centre + half-height
        int    palette = 0;
        int    zooms = 0;
    };

    // Zoom re-centres on a target point (fx,fy in 0..1 of the image) and
    // tightens the scale. The 3x3 zones each tap a fixed target, so it works
    // with pure waya taps — no custom client JS.
    struct Zoom { double fx, fy; }; struct Out {}; struct Reset {}; struct Palette {};
    using Msg = std::variant<Zoom, Out, Reset, Palette>;

    static Model init() { return {}; }

    static Model update(Model m, Msg msg) {
        std::visit(overload{
            [&](Zoom z) {
                double aspect = (double)RW / RH;
                m.cx += (z.fx - 0.5) * 2 * m.scale * aspect;
                m.cy += (z.fy - 0.5) * 2 * m.scale;
                m.scale /= 2.4; m.zooms++;
            },
            [&](Out)     { m.scale *= 2.4; if (m.scale > 1.6) m.scale = 1.6; if (m.zooms) m.zooms--; },
            [&](Reset)   { m = Model{}; },
            [&](Palette) { m.palette = (m.palette + 1) % 4; },
        }, msg);
        return m;
    }

    // ── colour a pixel from its escape iteration count ──────────────────────
    static void colour(int it, int pal, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) {
        if (it >= MAXIT) { r = g = b = 6; return; }               // inside the set: near-black
        double t = (double)it / MAXIT;
        auto mix = [](double a, double b, double f){ return (std::uint8_t)(a + (b - a) * f); };
        switch (pal) {
            case 0: // indigo -> cyan -> white (waya brand)
                r = mix(20, 230, t); g = mix(30, 245, std::pow(t,0.7)); b = mix(90, 255, std::pow(t,0.5)); break;
            case 1: // fire
                r = mix(10, 255, std::pow(t,0.5)); g = mix(0, 200, t*t); b = mix(20, 40, t); break;
            case 2: // emerald
                r = mix(5, 120, t*t); g = mix(20, 255, std::pow(t,0.6)); b = mix(30, 160, t); break;
            default: { // psychedelic bands
                double a = it * 0.13;
                r = (std::uint8_t)(128 + 127 * std::sin(a));
                g = (std::uint8_t)(128 + 127 * std::sin(a + 2.09));
                b = (std::uint8_t)(128 + 127 * std::sin(a + 4.19));
            }
        }
    }

    // ── base64 (for the data URI) ───────────────────────────────────────────
    static std::string b64(const std::vector<std::uint8_t>& in) {
        static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string o; o.reserve((in.size() + 2) / 3 * 4);
        std::size_t i = 0;
        for (; i + 3 <= in.size(); i += 3) {
            std::uint32_t v = (in[i] << 16) | (in[i+1] << 8) | in[i+2];
            o += T[(v>>18)&63]; o += T[(v>>12)&63]; o += T[(v>>6)&63]; o += T[v&63];
        }
        if (i + 1 == in.size()) { std::uint32_t v = in[i] << 16;
            o += T[(v>>18)&63]; o += T[(v>>12)&63]; o += "=="; }
        else if (i + 2 == in.size()) { std::uint32_t v = (in[i]<<16)|(in[i+1]<<8);
            o += T[(v>>18)&63]; o += T[(v>>12)&63]; o += T[(v>>6)&63]; o += "="; }
        return o;
    }

    // ── render the fractal to a BMP data URI (BMP so we need no PNG/zlib) ────
    static std::string render(const Model& m) {
        double aspect = (double)RW / RH;
        // BMP: 24-bit, bottom-up, rows padded to 4 bytes.
        int rowsz = (RW * 3 + 3) & ~3;
        int imgsz = rowsz * RH;
        int filesz = 54 + imgsz;
        std::vector<std::uint8_t> f(filesz, 0);
        auto put32 = [&](int off, std::uint32_t v){ f[off]=v; f[off+1]=v>>8; f[off+2]=v>>16; f[off+3]=v>>24; };
        f[0]='B'; f[1]='M'; put32(2, filesz); put32(10, 54);
        put32(14, 40); put32(18, RW); put32(22, RH); f[26]=1; f[28]=24; put32(34, imgsz);

        for (int y = 0; y < RH; ++y) {
            // BMP is bottom-up; map image row y to fractal row (RH-1-y)
            int fy = RH - 1 - y;
            double im = m.cy + (fy / (double)(RH-1) - 0.5) * 2 * m.scale;
            std::uint8_t* row = &f[54 + y * rowsz];
            for (int x = 0; x < RW; ++x) {
                double re = m.cx + (x / (double)(RW-1) - 0.5) * 2 * m.scale * aspect;
                double zr = 0, zi = 0; int it = 0;
                while (zr*zr + zi*zi <= 4.0 && it < MAXIT) {
                    double t = zr*zr - zi*zi + re; zi = 2*zr*zi + im; zr = t; ++it;
                }
                std::uint8_t r, g, b; colour(it, m.palette, r, g, b);
                row[x*3+0] = b; row[x*3+1] = g; row[x*3+2] = r;   // BMP is BGR
            }
        }
        return "data:image/bmp;base64," + b64(f);
    }

    static Mod bord(std::uint32_t c = line) {
        return detail::raw_css("border", "1px solid " + detail::hexstr(c));
    }
    static const char* pal_name(int p) {
        static const char* n[] = { "Nebula", "Fire", "Emerald", "Spectrum" }; return n[p];
    }

    static NodeRef btn(std::string label, Msg msg, bool primary = false) {
        auto n = text(std::move(label)) | font(13) | weight(Weight::semibold)
               | pad_x(14) | pad_y(10) | round(9) | pointer | tap(msg)
               | transition("border-color .15s ease, background-color .15s ease");
        if (primary)
            n = n | fg(0xffffff) | detail::raw_css("background", "linear-gradient(135deg,#6d7cff,#5b6cff)")
                  | on(Hover, brightness(112));
        else
            n = n | fg(ink) | detail::raw_css("background", "rgba(255,255,255,.04)") | bord(0x2a3446)
                  | on(Hover, detail::raw_css("border-color", "#3a4560"));
        return n;
    }

    static NodeRef view(const Model& m) {
        // the fractal image
        auto img = markup("<img src='" + render(m) + "' "
                          "style='width:100%;height:auto;display:block;image-rendering:pixelated'>")
            | detail::raw_css("line-height", "0");

        // a transparent 3x3 tap grid over the image: each cell zooms toward its
        // own centre. Pure waya taps, no custom JS.
        std::vector<NodeRef> zones;
        for (int gy = 0; gy < 3; ++gy)
            for (int gx = 0; gx < 3; ++gx) {
                double fx = (gx + 0.5) / 3.0, fy = (gy + 0.5) / 3.0;
                zones.push_back(box() | detail::raw_css("cursor", "crosshair")
                    | tap(Zoom{ fx, fy }));
            }

        auto stage = box(img)
            | w_full | round(14) | detail::raw_css("overflow", "hidden") | bord(0x243049)
            | detail::raw_css("box-shadow", "0 30px 80px -30px rgba(0,0,0,.85)");

        auto title = row(
            box(text("\u25C8") | font(18) | fg(0xffffff)) | square(34) | center | round(9)
                | detail::raw_css("background", "linear-gradient(135deg,#6d7cff,#00d4ff)") | glow(brand, 14),
            col(text("Mandel") | fg(ink) | font(20) | weight(Weight::black) | detail::raw_css("letter-spacing","-0.02em"),
                text("the Mandelbrot set, computed in C++") | fg(faint) | font(12)) | gap(1)
        ) | gap(12) | items_center;

        char zbuf[32]; std::snprintf(zbuf, sizeof zbuf, "%.2e", m.scale);
        auto controls = row(
            btn("Zoom out", Out{}),
            btn("Reset", Reset{}),
            btn("Palette: " + std::string(pal_name(m.palette)), Palette{}, true),
            box() | grow(),
            col(text("ZOOM") | fg(faint) | font(10) | tracking_em(0.10f) | weight(Weight::semibold),
                text(std::to_string(m.zooms) + "\u00d7  \u00b7  scale " + zbuf) | fg(ink) | font(13) | weight(Weight::bold) | tabular_nums) | gap(1)
        ) | gap(9) | items_center | wrap | w_full;

        char cbuf[64]; std::snprintf(cbuf, sizeof cbuf, "c = %.6f %+.6fi", m.cx, m.cy);
        auto readout = row(
            text(cbuf) | fg(body_c) | font(13)
                | detail::raw_css("font-family", "ui-monospace,Menlo,monospace"),
            box() | grow(),
            text(std::to_string(RW) + "\u00d7" + std::to_string(RH) + " \u00b7 " + std::to_string(MAXIT) + " iter")
                | fg(faint) | font(13)
        ) | items_center | pad(16) | round(12)
          | detail::raw_css("background", "linear-gradient(180deg,#0d1322,#0a0f1b)") | bord();

        auto hint = text("Click a third of the image to zoom in \u00b7 chase the boundary for detail")
                  | fg(faint) | font(13) | text_center;

        // overlay the 3x3 zoom zones on the stage
        auto zone_grid = box_(std::move(zones))
            | detail::raw_css("position", "absolute") | detail::raw_css("inset", "0")
            | detail::raw_css("display", "grid")
            | detail::raw_css("grid-template-columns", "1fr 1fr 1fr")
            | detail::raw_css("grid-template-rows", "1fr 1fr 1fr");

        auto interactive = stack(stage, zone_grid)
            | detail::raw_css("position", "relative");

        return col(row(title, box() | grow()) | w_full, controls, interactive, readout, hint)
            | gap(16) | pad(28) | max_w(920) | center_x | min_h(100_vh)
            | detail::raw_css("background",
                "radial-gradient(1100px 560px at 50% -5%, rgba(109,124,255,.12), transparent 55%),"
                "radial-gradient(800px 500px at 85% 20%, rgba(0,212,255,.08), transparent 55%), #070a12")
            | as_main;
    }

    static Meta meta(const Model&) {
        Meta mt; mt.title = "Mandel \u00b7 Mandelbrot explorer";
        mt.description = "A live Mandelbrot set explorer, computed pixel-by-pixel in C++ and streamed with waya.";
        return mt;
    }
};

int main() { return live<Mandel>({ .port = 8080, .page_bg = 0x070a12, .title = "Mandel" }); }
